#pragma once

#include <array>

#include <SDL3/SDL_audio.h>

#include "audio/sound.hpp"

namespace manifold {

/// Plays sounds on the default audio output device, up to kVoices at a time, mixed together.
///
/// Initializes the SDL audio subsystem on creation and shuts it down on destruction. If there is no audio output,
/// a warning is logged and play() does nothing, so the game runs without sound.
class AudioOutput {
public:
    /// How many sounds can play at the same time.
    static constexpr int kVoices = 4;

    AudioOutput();
    ~AudioOutput();
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;
    AudioOutput(AudioOutput&&) = delete;
    AudioOutput& operator=(AudioOutput&&) = delete;

    /// Starts playing `sound` from its start, along with the sounds that are playing. If kVoices sounds are playing
    /// already, the one with the least time left stops.
    ///
    /// Returns at once: the sound plays in the background. Its samples are copied, so `sound` may be destroyed.
    void play(const Sound& sound);

private:
    /// The opened device, or 0 without audio output.
    SDL_AudioDeviceID device_ = 0;
    /// One stream for each sound that can play at once, bound to the device, which mixes them. Each converts the
    /// format of its sound to the device's. All null without audio output.
    std::array<SDL_AudioStream*, kVoices> voices_{};
};

}  // namespace manifold
