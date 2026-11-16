#pragma once

#include <SDL3/SDL_audio.h>

#include "audio/sound.hpp"

namespace manifold {

/// Plays sounds on the default audio output device, one at a time.
///
/// Initializes the SDL audio subsystem on creation and shuts it down on destruction. If there is no audio output,
/// a warning is logged and play() does nothing, so the game runs without sound.
class AudioOutput {
public:
    AudioOutput();
    ~AudioOutput();
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;
    AudioOutput(AudioOutput&&) = delete;
    AudioOutput& operator=(AudioOutput&&) = delete;

    /// Starts playing `sound` from its start. A sound that is still playing stops.
    ///
    /// Returns at once: the sound plays in the background. Its samples are copied, so `sound` may be destroyed.
    void play(const Sound& sound);

private:
    /// The stream into the device, which converts sounds to the device's format. Null without audio output.
    SDL_AudioStream* stream_ = nullptr;
};

}  // namespace manifold
