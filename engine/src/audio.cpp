#include "a2e/audio.hpp"

#include "a2e/scene.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace a2e {

AudioClip AudioClip::tone(double frequency, double duration_seconds, double amplitude, int sample_rate) {
    if (frequency <= 0.0) throw std::invalid_argument("tone frequency must be positive");
    if (duration_seconds <= 0.0) throw std::invalid_argument("tone duration must be positive");
    if (sample_rate <= 0) throw std::invalid_argument("sample rate must be positive");
    amplitude = std::clamp(amplitude, 0.0, 1.0);

    constexpr double two_pi = 6.283185307179586;
    const auto count = static_cast<std::size_t>(duration_seconds * sample_rate);
    const auto fade = std::max<std::size_t>(1, std::min<std::size_t>(count / 10, sample_rate / 100));
    AudioClip clip;
    clip.sample_rate = sample_rate;
    clip.samples.resize(std::max<std::size_t>(count, 1));
    for (std::size_t index = 0; index < clip.samples.size(); ++index) {
        const double envelope = std::min({1.0, static_cast<double>(index) / fade,
                                          static_cast<double>(clip.samples.size() - index) / fade});
        const double value = std::sin(two_pi * frequency * static_cast<double>(index) / sample_rate);
        clip.samples[index] = static_cast<std::int16_t>(value * amplitude * envelope * 32767.0);
    }
    return clip;
}

double AudioClip::duration() const {
    return sample_rate > 0 ? static_cast<double>(samples.size()) / sample_rate : 0.0;
}

void AudioClip::validate() const {
    if (sample_rate <= 0) throw std::invalid_argument("sample rate must be positive");
    if (samples.empty()) throw std::invalid_argument("audio clip has no samples");
}

SoundHandle NullAudioBackend::play(std::shared_ptr<const AudioClip> clip, double, bool loop) {
    if (!clip) throw std::invalid_argument("audio clip cannot be null");
    const SoundHandle handle = next_handle_++;
    if (loop) looping_.insert(handle);
    return handle;
}

AudioManager::AudioManager(std::unique_ptr<AudioBackend> backend) : backend_(std::move(backend)) {
    if (!backend_) throw std::invalid_argument("audio backend cannot be null");
}

AudioManager::~AudioManager() { backend_->stop_all(); }

SoundHandle AudioManager::play_sound(std::shared_ptr<const AudioClip> clip, double volume, bool loop) {
    if (!clip) throw std::invalid_argument("audio clip cannot be null");
    clip->validate();
    return backend_->play(std::move(clip), master_volume_ * sound_volume_ * clamp_volume(volume), loop);
}

SoundHandle AudioManager::play_music(std::shared_ptr<const AudioClip> clip, double volume, bool loop) {
    if (!clip) throw std::invalid_argument("audio clip cannot be null");
    clip->validate();
    stop_music();
    music_ = backend_->play(std::move(clip), master_volume_ * music_volume_ * clamp_volume(volume), loop);
    return music_;
}

void AudioManager::stop(SoundHandle handle) {
    if (handle == 0) return;
    backend_->stop(handle);
    if (handle == music_) music_ = 0;
}

void AudioManager::stop_music() { stop(music_); }

void AudioManager::stop_all() {
    backend_->stop_all();
    music_ = 0;
}

bool AudioManager::is_playing(SoundHandle handle) const { return handle != 0 && backend_->is_playing(handle); }

void AudioManager::update() { backend_->update(); }

double AudioManager::clamp_volume(double volume) { return std::clamp(volume, 0.0, 1.0); }

void AudioSystem::update(Scene& scene, const InputState&, double) {
    for (const auto& entity : scene.entities()) {
        auto* source = entity->get_component<AudioSource>();
        if (!source) continue;
        if (source->stop_requested || (!entity->active() && source->handle != 0)) {
            audio_.stop(source->handle);
            source->handle = 0;
            source->stop_requested = false;
        }
        if (source->play_requested && entity->active() && source->clip) {
            if (source->loop) audio_.stop(source->handle);
            source->handle = audio_.play_sound(source->clip, source->volume, source->loop);
        }
        source->play_requested = false;
    }
    audio_.update();
}

} // namespace a2e
