#ifndef EATSCHARTED_AUDIO_FOLEY_HPP
#define EATSCHARTED_AUDIO_FOLEY_HPP

#include "eatscharted/chart_types.hpp"
#include <atomic>
#include <cstdint>

namespace eatscharted {

namespace FoleySound {
    constexpr uint16_t ParchmentStep = 0;
    constexpr uint16_t SwordSlash    = 1;
    constexpr uint16_t ArcaneNova    = 2;
    constexpr uint16_t OceanSplash   = 3;
    constexpr uint16_t DungeonPortal = 4;
} // namespace FoleySound

/**
 * Procedural Cartographic & Action-RPG Foley Synthesizer built on miniaudio.
 */
class AudioFoleyEngine {
public:
    AudioFoleyEngine() = default;
    ~AudioFoleyEngine();

    bool init(bool enableHardwareDevice = true);
    void shutdown();

    void triggerSound(uint16_t soundId, float intensity = 1.0f);
    void mixSamples(float* outInterleavedStereo, uint32_t frameCount, uint32_t sampleRate = 48000) noexcept;

private:
    struct Voice {
        std::atomic<bool> active{false};
        uint16_t soundId{0};
        float phase1{0.0f};
        float phase2{0.0f};
        float freq1{440.0f};
        float freq2{880.0f};
        float env{0.0f};
        float decay{0.999f};
        float noiseMix{0.2f};
        float filterState{0.0f};
    };

    static constexpr int kMaxVoices = 8;
    Voice voices_[kMaxVoices]{};
    uint32_t rngState_{0x9E3779B9u};
    bool initialized_{false};
    void* maDevicePtr_{nullptr};
};

} // namespace eatscharted

#endif // EATSCHARTED_AUDIO_FOLEY_HPP
