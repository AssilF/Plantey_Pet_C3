#include "audio_engine.h"

#include <algorithm>

namespace audio {
namespace {

constexpr MelodyStep kBootMelody[] = {
    {415.3f, 180, 45},
    {554.4f, 200, 30},
    {659.3f, 240, 60},
    {830.6f, 260, 140},
};

constexpr MelodyStep kAmbientMelody[] = {
    {246.9f, 180, 30},
    {329.6f, 170, 40},
    {415.3f, 200, 70},
    {0.0f, 0, 160},
    {311.1f, 170, 30},
    {466.2f, 210, 80},
    {0.0f, 0, 240},
};

}  // namespace

void AudioEngine::begin() {
  ledcSetup(hw::BUZZER_LEDC_CHANNEL, 2000, hw::BUZZER_LEDC_RESOLUTION);
  ledcAttachPin(hw::PIN_BUZZER, hw::BUZZER_LEDC_CHANNEL);
  stop();
}

void AudioEngine::playTone(float frequencyHz, uint16_t durationMs) {
  stopMelody();
  ambientMode_ = false;
  if (frequencyHz <= 0.0f) {
    stop();
    return;
  }
  startTonePlayback(frequencyHz, durationMs);
}

void AudioEngine::playChord(std::initializer_list<float> frequenciesHz, uint16_t durationMs, uint16_t cycleMs) {
  stopMelody();
  ambientMode_ = false;
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

void AudioEngine::playMelody(const MelodyStep* steps, size_t count, bool loop, bool ambient) {
  stopMelody();
  ambientMode_ = ambient;
  if (steps == nullptr || count == 0) {
    return;
  }
  melody_ = steps;
  melodyCount_ = count;
  melodyLoop_ = loop;
  melodyIndex_ = 0;
  activeMelodyIndex_ = 0;
  melodyActive_ = true;
  melodyInPause_ = false;
  melodyCurrentPauseMs_ = 0;
  handleMelody(millis());
}

void AudioEngine::playBootSequence() {
  playMelody(kBootMelody, sizeof(kBootMelody) / sizeof(MelodyStep), false, false);
}

void AudioEngine::playAmbientLoop() {
  if (!ambientMode_) {
    playMelody(kAmbientMelody, sizeof(kAmbientMelody) / sizeof(MelodyStep), true, true);
  }
}

void AudioEngine::stopAmbient() {
  if (ambientMode_) {
    stop();
    ambientMode_ = false;
  }
}

void AudioEngine::update() {
  uint32_t now = millis();

  if (playing_) {
    if (playbackDurationMs_ > 0 && (now - playbackStartMs_) >= playbackDurationMs_) {
      finishPlayback();
      if (melodyActive_) {
        melodyInPause_ = true;
        melodyPauseStartMs_ = now;
      } else {
        return;
      }
    }

    if (chordMode_ && chordNoteCount_ > 1 && (now - lastChordSwitchMs_) >= chordCycleMs_) {
      currentChordIndex_ = (currentChordIndex_ + 1) % chordNoteCount_;
      applyFrequency(chordFrequencies_[currentChordIndex_]);
      lastChordSwitchMs_ = now;
    }
  }

  if (melodyActive_) {
    handleMelody(now);
  }
}

void AudioEngine::stop() {
  finishPlayback();
  stopMelody();
  ambientMode_ = false;
}

void AudioEngine::startTonePlayback(float frequencyHz, uint16_t durationMs) {
  playbackDurationMs_ = durationMs;
  chordMode_ = false;
  chordNoteCount_ = 1;
  melodyInPause_ = false;
  startPlayback();
  applyFrequency(frequencyHz);
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

void AudioEngine::finishPlayback() {
  ledcWrite(hw::BUZZER_LEDC_CHANNEL, 0);
  playing_ = false;
  chordMode_ = false;
  chordNoteCount_ = 0;
  currentChordIndex_ = 0;
  currentFrequency_ = 0.0f;
  playbackDurationMs_ = 0;
}

void AudioEngine::stopMelody() {
  melodyActive_ = false;
  melody_ = nullptr;
  melodyCount_ = 0;
  melodyIndex_ = 0;
  activeMelodyIndex_ = 0;
  melodyLoop_ = false;
  melodyInPause_ = false;
  melodyCurrentPauseMs_ = 0;
  ambientMode_ = false;
}

void AudioEngine::startMelodyStep(size_t index) {
  if (melody_ == nullptr || index >= melodyCount_) {
    stopMelody();
    return;
  }
  const MelodyStep& step = melody_[index];
  activeMelodyIndex_ = index;
  melodyIndex_ = index + 1;
  melodyCurrentPauseMs_ = step.pauseMs;
  if (step.frequencyHz <= 0.0f || step.durationMs == 0) {
    melodyInPause_ = true;
    melodyPauseStartMs_ = millis();
    return;
  }
  startTonePlayback(step.frequencyHz, step.durationMs);
}

void AudioEngine::handleMelody(uint32_t now) {
  if (!melodyActive_ || melody_ == nullptr) {
    return;
  }

  if (melodyInPause_) {
    if (melodyCurrentPauseMs_ == 0 || (now - melodyPauseStartMs_) >= melodyCurrentPauseMs_) {
      melodyInPause_ = false;
    } else {
      return;
    }
  }

  if (playing_) {
    return;
  }

  if (melodyIndex_ >= melodyCount_) {
    if (melodyLoop_) {
      melodyIndex_ = 0;
      activeMelodyIndex_ = 0;
    } else {
      stopMelody();
      return;
    }
  }

  startMelodyStep(melodyIndex_);
}

}  // namespace audio

