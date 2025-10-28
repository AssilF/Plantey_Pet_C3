#include "ai_client.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <cstring>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets_template.h"
#endif

namespace ai {
namespace {
constexpr const char* kOpenAiEndpoint = "https://api.openai.com/v1/chat/completions";
constexpr uint16_t kHttpTimeoutMs = 25000;

bool stringLooksPlaceholder(const char* value, const char* placeholder) {
  return value == nullptr || value[0] == '\0' || std::strcmp(value, placeholder) == 0;
}

}  // namespace

bool PlantKnowledgeClient::fetchProfile(const String& species, plant::PlantProfile& profile, String& error) {
  if (!apiKeyConfigured()) {
    error = "OpenAI API key missing";
    return false;
  }
  if (!WiFi.isConnected()) {
    error = "WiFi offline";
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(kHttpTimeoutMs);

  HTTPClient http;
  if (!http.begin(client, kOpenAiEndpoint)) {
    error = "Failed to init HTTPS";
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + secrets::OPENAI_API_KEY);

  String body = buildRequestBody(species);
  int status = http.POST(body);
  if (status <= 0) {
    error = String("HTTP POST failed: ") + http.errorToString(status);
    http.end();
    return false;
  }

  lastRawResponse_ = http.getString();
  http.end();

  if (status < 200 || status >= 300) {
    error = String("OpenAI HTTP ") + status + ": " + lastRawResponse_;
    return false;
  }

  DynamicJsonDocument doc(8192);
  DeserializationError parseErr = deserializeJson(doc, lastRawResponse_);
  if (parseErr) {
    error = String("Response parse failed: ") + parseErr.f_str();
    return false;
  }

  JsonVariant content = doc["choices"][0]["message"]["content"];
  if (content.isNull()) {
    error = "AI response missing content";
    return false;
  }

  String profileJson = content.as<String>();
  String decodeError;
  if (!plant::DecodeProfileFromJson(profileJson, profile, decodeError)) {
    error = "Profile decode failed: " + decodeError;
    return false;
  }

  if (profile.generatedAtEpoch == 0) {
    profile.generatedAtEpoch = static_cast<uint32_t>(millis() / 1000UL);
  }
  profile.valid = true;
  error = "";
  return true;
}

bool PlantKnowledgeClient::apiKeyConfigured() const {
  return !stringLooksPlaceholder(secrets::OPENAI_API_KEY, "sk-your-key");
}

String PlantKnowledgeClient::buildRequestBody(const String& species) const {
  StaticJsonDocument<6144> doc;
  doc["model"] = secrets::OPENAI_MODEL;
  doc["temperature"] = 0.35;
  doc["max_tokens"] = 600;

  JsonArray messages = doc.createNestedArray("messages");
  JsonObject systemMsg = messages.createNestedObject();
  systemMsg["role"] = "system";
  systemMsg["content"] =
      "You are a botanist who prepares care profiles for interactive smart planters. Output JSON that matches the "
      "`plant_profile` schema exactly. All thresholds must be percentages in 0-100. Soil thresholds refer to volumetric "
      "moisture as interpreted from resistive soil sensors (higher percent means wetter). Light thresholds refer to a "
      "normalized photocell reading where 0 is darkness and 100 is direct bright light. Provide actionable, concise tips.";

  JsonObject userMsg = messages.createNestedObject();
  userMsg["role"] = "user";
  userMsg["content"] =
      String("Generate the plant_profile JSON for the species: ") + species +
      ". Adapt thresholds to typical indoor care in temperate households. Include realistic ranges for soil moisture, "
      "light preference, and ideal temperature/humidity. Provide up to three practical care tips, ordered by priority.";

  JsonObject responseFormat = doc.createNestedObject("response_format");
  responseFormat["type"] = "json_schema";
  JsonObject jsonSchema = responseFormat.createNestedObject("json_schema");
  jsonSchema["name"] = "plant_profile";
  JsonObject schema = jsonSchema.createNestedObject("schema");
  schema["type"] = "object";
  schema["additionalProperties"] = false;
  JsonArray required = schema.createNestedArray("required");
  required.add("speciesCommonName");
  required.add("speciesLatinName");
  required.add("summary");
  required.add("soil");
  required.add("light");
  required.add("temperatureC");
  required.add("humidityPct");
  required.add("wateringIntervalHours");
  required.add("wateringStrategy");
  required.add("lightingStrategy");
  required.add("feedingStrategy");
  required.add("careTips");

  JsonObject properties = schema.createNestedObject("properties");

  properties.createNestedObject("speciesCommonName")["type"] = "string";
  properties.createNestedObject("speciesLatinName")["type"] = "string";
  properties.createNestedObject("summary")["type"] = "string";

  JsonObject soil = properties.createNestedObject("soil");
  soil["type"] = "object";
  soil["additionalProperties"] = false;
  JsonArray soilReq = soil.createNestedArray("required");
  soilReq.add("dryPercent");
  soilReq.add("soggyPercent");
  soilReq.add("targetPercentRange");
  JsonObject soilProps = soil.createNestedObject("properties");
  soilProps.createNestedObject("dryPercent")["type"] = "number";
  soilProps.createNestedObject("soggyPercent")["type"] = "number";
  JsonObject soilRange = soilProps.createNestedObject("targetPercentRange");
  soilRange["type"] = "array";
  soilRange["minItems"] = 2;
  soilRange["maxItems"] = 2;
  soilRange.createNestedObject("items")["type"] = "number";

  JsonObject light = properties.createNestedObject("light");
  light["type"] = "object";
  light["additionalProperties"] = false;
  JsonArray lightReq = light.createNestedArray("required");
  lightReq.add("lowPercent");
  lightReq.add("highPercent");
  lightReq.add("targetPercentRange");
  JsonObject lightProps = light.createNestedObject("properties");
  lightProps.createNestedObject("lowPercent")["type"] = "number";
  lightProps.createNestedObject("highPercent")["type"] = "number";
  JsonObject lightRange = lightProps.createNestedObject("targetPercentRange");
  lightRange["type"] = "array";
  lightRange["minItems"] = 2;
  lightRange["maxItems"] = 2;
  lightRange.createNestedObject("items")["type"] = "number";

  JsonObject temperature = properties.createNestedObject("temperatureC");
  temperature["type"] = "object";
  temperature["additionalProperties"] = false;
  JsonArray tempReq = temperature.createNestedArray("required");
  tempReq.add("minComfort");
  tempReq.add("maxComfort");
  JsonObject tempProps = temperature.createNestedObject("properties");
  tempProps.createNestedObject("minComfort")["type"] = "number";
  tempProps.createNestedObject("maxComfort")["type"] = "number";

  JsonObject humidity = properties.createNestedObject("humidityPct");
  humidity["type"] = "object";
  humidity["additionalProperties"] = false;
  JsonArray humidityReq = humidity.createNestedArray("required");
  humidityReq.add("min");
  humidityReq.add("max");
  JsonObject humidityProps = humidity.createNestedObject("properties");
  humidityProps.createNestedObject("min")["type"] = "number";
  humidityProps.createNestedObject("max")["type"] = "number";

  properties.createNestedObject("wateringIntervalHours")["type"] = "integer";
  properties.createNestedObject("wateringStrategy")["type"] = "string";
  properties.createNestedObject("lightingStrategy")["type"] = "string";
  properties.createNestedObject("feedingStrategy")["type"] = "string";

  JsonObject tips = properties.createNestedObject("careTips");
  tips["type"] = "array";
  tips["minItems"] = 1;
  tips["maxItems"] = 3;
  tips.createNestedObject("items")["type"] = "string";

  properties.createNestedObject("generatedAtEpoch")["type"] = "integer";

  String output;
  serializeJson(doc, output);
  return output;
}

}  // namespace ai
