#include "audio_engine.h"

#include <algorithm>

namespace audio {

void AudioEngine::begin() {
  ledcSetup(hw::BUZZER_LEDC_CHANNEL, 2000, hw::BUZZER_LEDC_RESOLUTION);
  ledcAttachPin(hw::PIN_BUZZER, hw::BUZZER_LEDC_CHANNEL);
  stop();
}

void AudioEngine::playTone(float frequencyHz, uint16_t durationMs) {
  if (frequencyHz <= 0.0f) {
    stop();
    return;
  }
  chordMode_ = false;
  chordNoteCount_ = 1;
  chordFrequencies_[0] = frequencyHz;
  playbackDurationMs_ = durationMs;
  startPlayback();
  applyFrequency(frequencyHz);
}

void AudioEngine::playChord(std::initializer_list<float> frequenciesHz, uint16_t durationMs, uint16_t cycleMs) {
  chordNoteCount_ = std::min(frequenciesHz.size(), chordFrequencies_.size());
  if (chordNoteCount_ == 0) {
    stop();
    return;
  }
  size_t index = 0;
  for (float freq : frequenciesHz) {
    if (index >= chordNoteCount_) {
      break;
    }
    chordFrequencies_[index++] = freq;
  }
  playbackDurationMs_ = durationMs;
  chordCycleMs_ = std::max<uint16_t>(cycleMs, 4);
  chordMode_ = true;
  currentChordIndex_ = 0;
  startPlayback();
  applyFrequency(chordFrequencies_[currentChordIndex_]);
  lastChordSwitchMs_ = millis();
}

void AudioEngine::update() {
  if (!playing_) {
    return;
  }

  uint32_t now = millis();
  if (playbackDurationMs_ > 0 && (now - playbackStartMs_) >= playbackDurationMs_) {
    stop();
    return;
  }

  if (chordMode_ && chordNoteCount_ > 1 && (now - lastChordSwitchMs_) >= chordCycleMs_) {
    currentChordIndex_ = (currentChordIndex_ + 1) % chordNoteCount_;
    applyFrequency(chordFrequencies_[currentChordIndex_]);
    lastChordSwitchMs_ = now;
  }
}

void AudioEngine::stop() {
  ledcWrite(hw::BUZZER_LEDC_CHANNEL, 0);
  playing_ = false;
  chordMode_ = false;
  chordNoteCount_ = 0;
  currentChordIndex_ = 0;
  currentFrequency_ = 0.0f;
}

void AudioEngine::applyFrequency(float frequencyHz) {
  if (frequencyHz <= 0.0f) {
    ledcWrite(hw::BUZZER_LEDC_CHANNEL, 0);
    currentFrequency_ = 0.0f;
    return;
  }
  ledcWriteTone(hw::BUZZER_LEDC_CHANNEL, frequencyHz);
  ledcWrite(hw::BUZZER_LEDC_CHANNEL, (1 << hw::BUZZER_LEDC_RESOLUTION) / 2);
  currentFrequency_ = frequencyHz;
}

void AudioEngine::startPlayback() {
  playbackStartMs_ = millis();
  playing_ = true;
}

}  // namespace audio
