# Plantey Pet C3

An expressive plant companion built on the ESP32-C3 Super Mini. The firmware samples soil moisture, ambient light, temperature, and humidity, then renders a characterful face on an SH1106 OLED. Two buttons drive a lightweight menu and calibration flow, while a piezo buzzer plays chord-like cues using LEDC time-slicing.

## Hardware Map

| Peripheral             | Pin / Channel                | Notes                                              |
|------------------------|------------------------------|----------------------------------------------------|
| Capacitive soil sensor | GPIO0 (ADC1_CH0)             | Powered from 3.3 V, calibrate via Sensor toolkit   |
| LDR divider            | GPIO1 (ADC1_CH1)             | Dark/bright thresholds set via Sensor toolkit      |
| DHT11                  | GPIO3                        | Provides air temperature and humidity              |
| Battery divider        | GPIO4 (ADC1_CH4)             | High-value divider feeding pack voltage            |
| Button left/back       | GPIO20 (INPUT_PULLUP)        | Active-low                                         |
| Button right/next      | GPIO21 (INPUT_PULLUP)        | Active-low                                         |
| Piezo buzzer           | GPIO2 / LEDC channel 0       | Cycles tones quickly to simulate simple chords     |
| RGB cathodes (R,G,B)   | GPIO5 / GPIO6 / GPIO7        | PWM-capable outputs for glow effects               |
| SH1106 OLED            | I2C SDA GPIO8, SCL GPIO9     | Uses U8G2 hardware I2C driver                      |

## Using the Interface

- **Navigation**: From the face view, tap either button to open the menu. Inside the menu the left button steps upward, the right button steps downward, and pressing both together confirms (`OK`) or backs out of a submenu.  
- **Main menu**: Items include Face view, Plant insights, Sensor toolkit, Plant toolkit, Sound & calm, and Diagnostics. The title bar labels each menu and scroll arrows appear only when there are items above or below.  
- **Sensor toolkit**: Capture the current soil value as dry or wet and the current light level as dark or bright to recalibrate the sensors. Each action plays a confirmation tone so you know the sample registered.  
- **Plant toolkit**: Fetch or cycle plant profiles via ChatGPT, advance to the next preset species (fetching immediately), or clear the cached profile to return to defaults.  
- **Sound & calm**: Trigger the layered chord demo to confirm audio hardware and let Plantey slip back into its ambient loop afterwards.  
- **Screens**: Face view shows the animated plant with a corner clock; Plant insights gathers readings, Wi-Fi state, and AI tips; Diagnostics surfaces raw values. From any screen, pressing both buttons returns you to the main menu.

## Behaviour

- Soil and light channels use an exponential moving average to smooth noisy readings.  
- Expression logic maps environment data into moods (thirsty, overwatered, sleepy, too bright, comfortable, etc.) and drives subtitles, indicator overlays, and audio cues.  
- Eye blinks, leaf sway, and gentle breathing are jittered so the face feels alive even when idle, while Plant insights now carries the guidance text off the main face.  
- Booting shows the Plantey logo with a short welcome melody, then hands off to a soft ambient loop that continues whenever no other sound is playing.

## AI-assisted Plant Profiles

The firmware can ask ChatGPT for species-specific care recommendations. Copy `src/secrets_template.h` to `src/secrets.h`, then fill in your Wi-Fi credentials and OpenAI API key. Once connected:

- Use **Plant tools -> Fetch plant profile** to pull care thresholds for the active species.
- Pick **Plant tools -> Next preset + fetch** to cycle through common houseplants and pull the relevant profile automatically.
- Serial helpers remain available: `plant:<name>` sets a custom species query, `profile:fetch` queues a fetch, and `profile:clear` deletes the stored profile and reverts to default thresholds.

The retrieved profile is cached in NVS so the pot boots with your latest configuration, and thresholds immediately drive the mood/expression logic.

## Wi-Fi & Web API

- The ESP32 brings up a hotspot called `PlanteyPet` (password `planteypet`) while also attempting to join the STA network specified in `secrets.h`. Both radios run at max transmit power so you can connect locally even if your home Wi-Fi is unavailable.
- Browse to `http://192.168.4.1/api/status` when attached to the hotspot to read live sensor data, thresholds, and Wi-Fi state.
- POST JSON commands to:
  - `POST /api/plant` - e.g. `{ "species": "Monstera", "fetch": true }` or `{ "nextPreset": true }` for ChatGPT-assisted updates.
  - `POST /api/calibrate` - `{ "target": "soilDry" }`, `soilWet`, `lightDark`, or `lightBright` to capture live readings.
  - `POST /api/display` - `{ "playDemo": true }` for a quick audio check. (Contrast control is not supported on the SH1106 panel.)
  - `POST /api/profile/reset` - wipe the cached profile and revert to defaults.
- All endpoints include permissive CORS headers so a companion mobile or web app can call them without extra firmware changes.

## Build and Test

1. Copy `src/secrets_template.h` -> `src/secrets.h` and populate your credentials.  
2. Install dependencies and build:

```
platformio run
```

Use the serial monitor to follow sensor snapshots, AI fetch results, and any network warnings. For bench testing you can tweak `lastReadings` inside `loop()` or feed the analog inputs with a potentiometer to confirm each mood transition.


