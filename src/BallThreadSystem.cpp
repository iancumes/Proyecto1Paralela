#include "BallThreadSystem.h"

BallThreadSystem::BallThreadSystem(std::vector<Ball>& balls) : balls_(balls) {
    workers_.reserve(balls_.size());
    for (std::size_t index = 0; index < balls_.size(); ++index) {
        workers_.emplace_back(&BallThreadSystem::workerLoop, this, index);
    }
}

BallThreadSystem::~BallThreadSystem() {
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    startCondition_.notify_all();

    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void BallThreadSystem::update(float dt) {
    if (workers_.empty()) {
        return;
    }

    std::unique_lock<std::mutex> lock(mutex_);
    frameDeltaTime_ = dt;
    pendingWorkers_ = workers_.size();
    ++generation_;
    startCondition_.notify_all();

    finishedCondition_.wait(lock, [this] {
        return pendingWorkers_ == 0;
    });
}

std::size_t BallThreadSystem::threadCount() const {
    return workers_.size();
}

void BallThreadSystem::workerLoop(std::size_t ballIndex) {
    std::uint64_t completedGeneration = 0;

    while (true) {
        std::unique_lock<std::mutex> lock(mutex_);
        startCondition_.wait(lock, [this, completedGeneration] {
            return stopping_ || generation_ > completedGeneration;
        });

        if (stopping_) {
            return;
        }

        const float dt = frameDeltaTime_;
        const std::uint64_t currentGeneration = generation_;
        lock.unlock();

        // Este worker es el unico que escribe su pelota durante este frame.
        updateBall(balls_[ballIndex], dt);

        lock.lock();
        completedGeneration = currentGeneration;
        if (--pendingWorkers_ == 0) {
            finishedCondition_.notify_one();
        }
    }
}
