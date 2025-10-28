#pragma once

#include <Arduino.h>
#include <array>
#include <initializer_list>

#include "hardware_config.h"

namespace audio {

struct MelodyStep {
  float frequencyHz;
  uint16_t durationMs;
  uint16_t pauseMs;
};

class AudioEngine {
 public:
  void begin();
  void update();

  void playTone(float frequencyHz, uint16_t durationMs);
  void playChord(std::initializer_list<float> frequenciesHz, uint16_t durationMs, uint16_t cycleMs = 12);
  void playMelody(const MelodyStep* steps, size_t count, bool loop = false, bool ambient = false);
  void playBootSequence();
  void playAmbientLoop();
  void stopAmbient();
  void stop();

  bool isPlaying() const { return playing_; }
  bool isAmbientActive() const { return ambientMode_; }

 private:
  void startTonePlayback(float frequencyHz, uint16_t durationMs);
  void applyFrequency(float frequencyHz);
  void startPlayback();
  void finishPlayback();
  void stopMelody();
  void startMelodyStep(size_t index);
  void handleMelody(uint32_t now);

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

  const MelodyStep* melody_ = nullptr;
  size_t melodyCount_ = 0;
  size_t melodyIndex_ = 0;
  size_t activeMelodyIndex_ = 0;
  bool melodyLoop_ = false;
  bool melodyActive_ = false;
  bool melodyInPause_ = false;
  uint16_t melodyCurrentPauseMs_ = 0;
  uint32_t melodyPauseStartMs_ = 0;
  bool ambientMode_ = false;
};

}  // namespace audio
