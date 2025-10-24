#pragma once

#include <Arduino.h>
#include <array>
#include <initializer_list>

#include "hardware_config.h"

namespace audio {

class AudioEngine {
 public:
  void begin();
  void update();

  void playTone(float frequencyHz, uint16_t durationMs);
  void playChord(std::initializer_list<float> frequenciesHz, uint16_t durationMs, uint16_t cycleMs = 12);
  void stop();

  bool isPlaying() const { return playing_; }

 private:
  void applyFrequency(float frequencyHz);
  void startPlayback();

  static constexpr size_t kMaxChordNotes = 4;
  std::array<float, kMaxChordNotes> chordFrequencies_{};
  size_t chordNoteCount_ = 0;
  size_t currentChordIndex_ = 0;

  bool playing_ = false;
  bool chordMode_ = false;
  uint32_t playbackStartMs_ = 0;
  uint32_t playbackDurationMs_ = 0;
  uint16_t chordCycleMs_ = 12;
  uint32_t lastChordSwitchMs_ = 0;
  float currentFrequency_ = 0.0f;
};

}  // namespace audio
