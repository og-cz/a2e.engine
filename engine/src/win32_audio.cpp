#include "a2e/audio.hpp"
#include "a2e/logging.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>

namespace a2e {
namespace {

// One waveOut stream per playing sound; Windows mixes concurrent streams.
struct Voice {
    HWAVEOUT device = nullptr;
    WAVEHDR header{};
    std::vector<std::int16_t> buffer;
};

class Win32AudioBackend final : public AudioBackend {
public:
    ~Win32AudioBackend() override { stop_all(); }

    SoundHandle play(std::shared_ptr<const AudioClip> clip, double volume, bool loop) override {
        if (!clip) throw std::invalid_argument("audio clip cannot be null");
        auto voice = std::make_unique<Voice>();
        voice->buffer.reserve(clip->samples.size());
        for (const auto sample : clip->samples) {
            voice->buffer.push_back(static_cast<std::int16_t>(sample * std::clamp(volume, 0.0, 1.0)));
        }

        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = 1;
        format.nSamplesPerSec = static_cast<DWORD>(clip->sample_rate);
        format.wBitsPerSample = 16;
        format.nBlockAlign = static_cast<WORD>(format.nChannels * format.wBitsPerSample / 8);
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
        if (waveOutOpen(&voice->device, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
            return 0;
        }

        voice->header.lpData = reinterpret_cast<LPSTR>(voice->buffer.data());
        voice->header.dwBufferLength = static_cast<DWORD>(voice->buffer.size() * sizeof(std::int16_t));
        if (loop) {
            voice->header.dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP;
            voice->header.dwLoops = 0xFFFFFFFFu;
        }
        if (waveOutPrepareHeader(voice->device, &voice->header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR ||
            waveOutWrite(voice->device, &voice->header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
            release(*voice);
            return 0;
        }
        const SoundHandle handle = next_handle_++;
        voices_.emplace(handle, std::move(voice));
        return handle;
    }

    void stop(SoundHandle handle) override {
        const auto found = voices_.find(handle);
        if (found == voices_.end()) return;
        release(*found->second);
        voices_.erase(found);
    }

    void stop_all() override {
        for (auto& [handle, voice] : voices_) release(*voice);
        voices_.clear();
    }

    bool is_playing(SoundHandle handle) const override {
        const auto found = voices_.find(handle);
        return found != voices_.end() && (found->second->header.dwFlags & WHDR_DONE) == 0;
    }

    void update() override {
        for (auto voice = voices_.begin(); voice != voices_.end();) {
            if (voice->second->header.dwFlags & WHDR_DONE) {
                release(*voice->second);
                voice = voices_.erase(voice);
            } else {
                ++voice;
            }
        }
    }

private:
    static void release(Voice& voice) {
        if (!voice.device) return;
        waveOutReset(voice.device);
        if (voice.header.dwFlags & WHDR_PREPARED) {
            waveOutUnprepareHeader(voice.device, &voice.header, sizeof(WAVEHDR));
        }
        waveOutClose(voice.device);
        voice.device = nullptr;
    }

    SoundHandle next_handle_ = 1;
    std::unordered_map<SoundHandle, std::unique_ptr<Voice>> voices_;
};

} // namespace

std::unique_ptr<AudioBackend> create_audio_backend() {
    if (waveOutGetNumDevs() == 0) {
        log_info("no audio output device found; using silent audio backend");
        return std::make_unique<NullAudioBackend>();
    }
    return std::make_unique<Win32AudioBackend>();
}

} // namespace a2e
#else

namespace a2e {
std::unique_ptr<AudioBackend> create_audio_backend() { return std::make_unique<NullAudioBackend>(); }
} // namespace a2e
#endif
