#pragma once

#include <Arduino.h>

#include "plant_profile.h"

namespace ai {

class PlantKnowledgeClient {
 public:
  bool fetchProfile(const String& species, plant::PlantProfile& profile, String& error);
  const String& lastRawResponse() const { return lastRawResponse_; }

 private:
  bool apiKeyConfigured() const;
  String buildRequestBody(const String& species) const;

  String lastRawResponse_;
};

}  // namespace ai
