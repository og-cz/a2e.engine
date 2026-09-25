#pragma once

#include "a2e/update_system.hpp"

#include <cstdint>
#include <memory>
#include <unordered_set>
#include <vector>

namespace a2e {

// Mono 16-bit PCM sample data. Clips are shared resources and can live in ResourceManager.
struct AudioClip {
    int sample_rate = 22050;
    std::vector<std::int16_t> samples;

    // Generates a sine tone with short fades, useful for prototypes that have no audio assets yet.
    static AudioClip tone(double frequency, double duration_seconds, double amplitude = 0.25,
                          int sample_rate = 22050);
    double duration() const;
    void validate() const;
};

using SoundHandle = std::uint64_t;

// Replaceable playback device. Volume is applied when a sound starts.
class AudioBackend {
public:
    virtual ~AudioBackend() = default;
    virtual SoundHandle play(std::shared_ptr<const AudioClip> clip, double volume, bool loop) = 0;
    virtual void stop(SoundHandle handle) = 0;
    virtual void stop_all() = 0;
    virtual bool is_playing(SoundHandle handle) const = 0;
    // Releases finished voices. Called once per frame by AudioManager::update.
    virtual void update() {}
};

// Silent backend for headless runs, batch simulation, and tests. Looping sounds report as
// playing until stopped; one-shot sounds finish immediately.
class NullAudioBackend final : public AudioBackend {
public:
    SoundHandle play(std::shared_ptr<const AudioClip> clip, double volume, bool loop) override;
    void stop(SoundHandle handle) override { looping_.erase(handle); }
    void stop_all() override { looping_.clear(); }
    bool is_playing(SoundHandle handle) const override { return looping_.count(handle) != 0; }

private:
    SoundHandle next_handle_ = 1;
    std::unordered_set<SoundHandle> looping_;
};

// Returns the platform backend, or NullAudioBackend when no audio device is available.
std::unique_ptr<AudioBackend> create_audio_backend();

class AudioManager {
public:
    explicit AudioManager(std::unique_ptr<AudioBackend> backend);
    ~AudioManager();

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    SoundHandle play_sound(std::shared_ptr<const AudioClip> clip, double volume = 1.0, bool loop = false);
    // Replaces any current music track.
    SoundHandle play_music(std::shared_ptr<const AudioClip> clip, double volume = 1.0, bool loop = true);
    void stop(SoundHandle handle);
    void stop_music();
    void stop_all();
    bool is_playing(SoundHandle handle) const;
    SoundHandle music() const { return music_; }
    void update();

    void set_master_volume(double volume) { master_volume_ = clamp_volume(volume); }
    void set_sound_volume(double volume) { sound_volume_ = clamp_volume(volume); }
    void set_music_volume(double volume) { music_volume_ = clamp_volume(volume); }
    double master_volume() const { return master_volume_; }
    double sound_volume() const { return sound_volume_; }
    double music_volume() const { return music_volume_; }
    AudioBackend& backend() { return *backend_; }

private:
    static double clamp_volume(double volume);

    std::unique_ptr<AudioBackend> backend_;
    SoundHandle music_ = 0;
    double master_volume_ = 1.0;
    double sound_volume_ = 1.0;
    double music_volume_ = 1.0;
};

// Entity component. Gameplay requests playback by setting flags; AudioSystem talks to the device.
struct AudioSource {
    std::shared_ptr<const AudioClip> clip;
    double volume = 1.0;
    bool loop = false;
    bool play_requested = false;
    bool stop_requested = false;
    SoundHandle handle = 0;

    void play() { play_requested = true; }
    void stop() { stop_requested = true; }
};

class AudioSystem final : public UpdateSystem {
public:
    explicit AudioSystem(AudioManager& audio) : audio_(audio) {}
    void update(Scene& scene, const InputState& input, double delta_seconds) override;

private:
    AudioManager& audio_;
};

} // namespace a2e
