# Plantey Pet C3

An expressive plant companion built on the ESP32-C3 Super Mini. The firmware samples soil moisture, ambient light, temperature, and humidity, then renders a characterful face on an SH1106 OLED. Two buttons drive a lightweight menu and calibration flow, while a piezo buzzer plays chord-like cues using LEDC time-slicing.

## Hardware Map

| Peripheral             | Pin / Channel                | Notes                                              |
|------------------------|------------------------------|----------------------------------------------------|
| Capacitive soil sensor | GPIO0 (ADC1_CH0)             | Powered from 3.3 V, calibrate via Stats page       |
| LDR divider            | GPIO1 (ADC1_CH1)             | Dark/bright thresholds set from Tips page          |
| DHT11                  | GPIO3                        | Provides air temperature and humidity              |
| Button left/back       | GPIO6 (INPUT_PULLUP)         | Active-low                                         |
| Button right/next      | GPIO7 (INPUT_PULLUP)         | Active-low                                         |
| Piezo buzzer           | GPIO2 / LEDC channel 0       | Cycles tones quickly to simulate simple chords     |
| SH1106 OLED            | I2C SDA GPIO8, SCL GPIO9     | Uses U8G2 hardware I2C driver                      |

## Using the Interface

- **Pages**: Mood face -> Stats -> Care tips -> Debug (wraps around). Tap right for next, left for previous.  
- **Calibrations**: On the Stats page, long-press left to capture the current soil reading as "dry", long-press right for "wet". On the Tips page, long-press left for "dark" light level, right for "bright". Values take effect immediately.  
- **Contrast tweak**: On the Debug page, long-press left/right to adjust OLED contrast in small steps.  
- **Audio demo**: Long-press on the Mood page to trigger a celebratory chord. Automatic hydration and celebration cues also fire when thresholds change.

## Behaviour

- Soil and light channels use an exponential moving average to smooth noisy readings.  
- Expression logic maps environment data into moods (thirsty, overwatered, sleepy, too bright, comfortable, etc.) and selects matching subtitles and optional audio.  
- Eye blinks run on a jittered timer so the face feels alive, and the Stats/Tips pages overlay contextual guidance without hiding the underlying readings.

## Build and Test

```
platformio run
```

Use a 115200 baud serial monitor to watch calibration captures and sensor snapshots. For bench testing you can tweak `lastReadings` inside `loop()` or feed the analog inputs with a potentiometer to confirm each mood transition.
