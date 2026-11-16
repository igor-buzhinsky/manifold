#include "platform/audio.hpp"

#include <cstddef>
#include <cstdint>

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>

#include "audio/sound.hpp"
#include "util/log.hpp"

namespace manifold {

namespace {

/// The format of the samples of `sound`, as SDL describes it.
SDL_AudioSpec sound_spec(const Sound& sound) {
    return SDL_AudioSpec{.format = SDL_AUDIO_S16, .channels = sound.channels, .freq = sound.sample_rate};
}

}  // namespace

AudioOutput::AudioOutput() {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        log_warning("No sound: SDL audio can't be started: {}", SDL_GetError());
        return;
    }
    // The format is only a start: play() sets the format of each sound. The device keeps its own format, and the
    // stream converts to it.
    const SDL_AudioSpec spec{.format = SDL_AUDIO_S16, .channels = 2, .freq = 48000};
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (stream_ == nullptr) {
        log_warning("No sound: the audio output device can't be opened: {}", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return;
    }
    // A device stream starts paused.
    SDL_ResumeAudioStreamDevice(stream_);
    const char* name = SDL_GetAudioDeviceName(SDL_GetAudioStreamDevice(stream_));
    log_info("Audio: {} ({} driver)", name != nullptr ? name : "unknown device", SDL_GetCurrentAudioDriver());
}

AudioOutput::~AudioOutput() {
    if (stream_ != nullptr) {
        SDL_DestroyAudioStream(stream_);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

void AudioOutput::play(const Sound& sound) {
    if (stream_ == nullptr || sound.samples.empty()) {
        return;
    }
    const SDL_AudioSpec spec = sound_spec(sound);
    const std::size_t bytes = sound.samples.size() * sizeof(std::int16_t);
    if (!SDL_ClearAudioStream(stream_) || !SDL_SetAudioStreamFormat(stream_, &spec, nullptr) ||
        !SDL_PutAudioStreamData(stream_, sound.samples.data(), static_cast<int>(bytes))) {
        log_warning("Can't play a sound: {}", SDL_GetError());
    }
}

}  // namespace manifold
