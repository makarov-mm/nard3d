#pragma once

#include "Win32.h"

#include <mmsystem.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

// Short sounds made up at start and mixed on a thread of their own, so that many can sound at once: dice shaken and
// striking the board, checkers set down, knocked off or dropped into the tray, the checkers set out for a new game,
// and the interface.
class SoundBank {
public:
    enum class Effect { Rattle, Die, Checker, Hit, Tray, Setup, Click, NoMove, Win, Count };

    SoundBank();
    ~SoundBank();
    SoundBank(const SoundBank&) = delete;
    SoundBank& operator=(const SoundBank&) = delete;

    // `volume` 0 to 1, `pan` from -1 (left) to 1 (right), `pitch` as a rate of playback.
    void Play(Effect effect, float volume = 1, float pan = 0, float pitch = 1);

    std::atomic<bool> enabled = true;

private:
    struct Voice {
        const std::vector<float>* samples = nullptr;
        double position = 0;
        double rate = 1;
        float left = 1, right = 1;
    };

    void Mix(std::int16_t* out, std::size_t frames);
    void Loop(std::stop_token stop);

    std::array<std::vector<float>, static_cast<std::size_t>(Effect::Count)> m_samples;
    std::mutex m_mutex;
    std::vector<Voice> m_voices;
    HWAVEOUT m_device = nullptr;
    HANDLE m_event = nullptr;
    static constexpr std::size_t Buffers = 4, Frames = 512;
    std::array<std::array<std::int16_t, Frames * 2>, Buffers> m_buffers{};
    std::array<WAVEHDR, Buffers> m_headers{};
    std::jthread m_thread;
};
