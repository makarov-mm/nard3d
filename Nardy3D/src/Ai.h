#pragma once

#include "Nardy.h"
#include "Network.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <utility>

namespace nardy {

// The computer player. It judges the position each way of playing the roll leaves by the network; at the top level
// it also weighs every roll the opponent may answer with and their best reply. The lower levels misjudge on purpose,
// the lowest by much.
class Ai {
public:
    static Move Choose(const Network& net, const Position& position, Variant variant, Roll roll, bool firstMove, bool opponentFirst, int level,
                       std::uint64_t seed, std::stop_token stop);
};

// Runs a computation on a thread of its own; the window polls for the result.
template <class Result>
class Worker {
public:
    Worker() = default;
    ~Worker() { Cancel(); }
    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;

    template <class Function>
    void Start(Function function)
    {
        Cancel();
        m_thread = std::jthread([this, function = std::move(function)](std::stop_token stop) {
            Result result = function(stop);
            if (!stop.stop_requested()) {
                std::scoped_lock lock(m_mutex);
                m_result = std::move(result);
            }
        });
    }

    void Cancel()
    {
        if (m_thread.joinable()) {
            m_thread.request_stop();
            m_thread.join();
        }
        std::scoped_lock lock(m_mutex);
        m_result.reset();
    }

    std::optional<Result> Take()
    {
        std::scoped_lock lock(m_mutex);
        return std::exchange(m_result, std::nullopt);
    }

private:
    std::jthread m_thread;
    std::mutex m_mutex;
    std::optional<Result> m_result;
};

} // namespace nardy
