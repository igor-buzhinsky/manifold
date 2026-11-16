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
    device_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (device_ == 0) {
        log_warning("No sound: the audio output device can't be opened: {}", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return;
    }
    // The format is only a start: play() sets the format of each sound. Binding sets the output format to the
    // device's.
    const SDL_AudioSpec spec{.format = SDL_AUDIO_S16, .channels = 2, .freq = 48000};
    for (SDL_AudioStream*& voice : voices_) {
        voice = SDL_CreateAudioStream(&spec, nullptr);
        if (voice == nullptr || !SDL_BindAudioStream(device_, voice)) {
            log_warning("Can't make an audio stream: {}", SDL_GetError());
        }
    }
    const char* name = SDL_GetAudioDeviceName(device_);
    log_info("Audio: {} ({} driver)", name != nullptr ? name : "unknown device", SDL_GetCurrentAudioDriver());
}

AudioOutput::~AudioOutput() {
    if (device_ == 0) {
        return;
    }
    for (SDL_AudioStream* voice : voices_) {
        SDL_DestroyAudioStream(voice);  // Unbinds it. Null is allowed.
    }
    SDL_CloseAudioDevice(device_);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void AudioOutput::play(const Sound& sound) {
    if (device_ == 0 || sound.samples.empty()) {
        return;
    }
    // The voice with the least left to play: an idle one if there is one.
    SDL_AudioStream* voice = nullptr;
    int least_queued = 0;
    for (SDL_AudioStream* candidate : voices_) {
        if (candidate == nullptr) {
            continue;
        }
        const int queued = SDL_GetAudioStreamQueued(candidate);
        if (voice == nullptr || queued < least_queued) {
            voice = candidate;
            least_queued = queued;
        }
    }
    if (voice == nullptr) {
        return;
    }
    const SDL_AudioSpec spec = sound_spec(sound);
    const std::size_t bytes = sound.samples.size() * sizeof(std::int16_t);
    if (!SDL_ClearAudioStream(voice) || !SDL_SetAudioStreamFormat(voice, &spec, nullptr) ||
        !SDL_PutAudioStreamData(voice, sound.samples.data(), static_cast<int>(bytes))) {
        log_warning("Can't play a sound: {}", SDL_GetError());
    }
}

}  // namespace manifold
