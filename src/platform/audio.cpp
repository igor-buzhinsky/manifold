#include "platform/audio.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>

#include "audio/sound.hpp"
#include "util/log.hpp"

namespace manifold {

namespace {

/// How many seconds of sound `stream` has left to play, or none if SDL can't tell.
std::optional<double> seconds_left(SDL_AudioStream* stream) {
    SDL_AudioSpec spec{};
    const int queued = SDL_GetAudioStreamQueued(stream);  // Bytes, in the format of the sound put last.
    if (queued < 0 || !SDL_GetAudioStreamFormat(stream, &spec, nullptr) || spec.channels <= 0 || spec.freq <= 0) {
        return std::nullopt;
    }
    const double bytes_per_second = static_cast<double>(SDL_AUDIO_FRAMESIZE(spec)) * spec.freq;
    return queued / bytes_per_second;
}

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
            // An unbound stream would never be played, so it isn't kept as a voice.
            log_warning("Can't make an audio stream: {}", SDL_GetError());
            SDL_DestroyAudioStream(voice);
            voice = nullptr;
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

void AudioOutput::play(const Sound& sound, float gain) {
    if (device_ == 0 || sound.samples.empty()) {
        return;
    }
    // The voice with the least time left to play: an idle one if there is one.
    SDL_AudioStream* voice = nullptr;
    double least_left = 0.0;
    for (SDL_AudioStream* candidate : voices_) {
        const std::optional<double> left = candidate != nullptr ? seconds_left(candidate) : std::nullopt;
        if (left && (voice == nullptr || *left < least_left)) {
            voice = candidate;
            least_left = *left;
        }
    }
    if (voice == nullptr) {
        return;
    }
    const SDL_AudioSpec spec = sound_spec(sound);
    const std::size_t bytes = sound.samples.size() * sizeof(std::int16_t);
    if (!SDL_ClearAudioStream(voice) || !SDL_SetAudioStreamFormat(voice, &spec, nullptr) ||
        !SDL_SetAudioStreamGain(voice, gain) ||
        !SDL_PutAudioStreamData(voice, sound.samples.data(), static_cast<int>(bytes))) {
        log_warning("Can't play a sound: {}", SDL_GetError());
    }
}

}  // namespace manifold
