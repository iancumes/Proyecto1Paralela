#pragma once

#include "Ball.h"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

// Mantiene un worker persistente por pelota. El vector no debe cambiar de
// tamano durante la vida de este objeto.
class BallThreadSystem {
public:
    explicit BallThreadSystem(std::vector<Ball>& balls);
    ~BallThreadSystem();

    BallThreadSystem(const BallThreadSystem&) = delete;
    BallThreadSystem& operator=(const BallThreadSystem&) = delete;

    void update(float dt);
    std::size_t threadCount() const;

private:
    void workerLoop(std::size_t ballIndex);

    std::vector<Ball>& balls_;
    std::vector<std::thread> workers_;
    std::mutex mutex_;
    std::condition_variable startCondition_;
    std::condition_variable finishedCondition_;
    std::uint64_t generation_ {0};
    std::size_t pendingWorkers_ {0};
    float frameDeltaTime_ {0.0F};
    bool stopping_ {false};
};
