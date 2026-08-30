#include "BallThreadSystem.h"

#include "Simulation.h"

BallThreadSystem::BallThreadSystem(std::size_t ballCount) {
    workers_.reserve(ballCount);
    try {
        for (std::size_t index = 0; index < ballCount; ++index) {
            workers_.emplace_back(&BallThreadSystem::workerLoop, this, index);
        }
    } catch (...) {
        // Si el sistema operativo rechaza la creacion de un hilo se detienen
        // ordenadamente los que ya existen antes de propagar la excepcion.
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
        workers_.clear();
        throw;
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

void BallThreadSystem::runRound(const Ball* readBalls, Ball* writeBalls, std::size_t ballCount,
                                const Peg* pegs, std::size_t pegCount,
                                const Modifier* modifiers, std::size_t modifierCount,
                                const SimulationParams& params,
                                float time, float dt,
                                int* binHits) {
    if (workers_.empty()) {
        return;
    }

    std::unique_lock<std::mutex> lock(mutex_);
    round_.readBalls = readBalls;
    round_.writeBalls = writeBalls;
    round_.ballCount = ballCount;
    round_.pegs = pegs;
    round_.pegCount = pegCount;
    round_.modifiers = modifiers;
    round_.modifierCount = modifierCount;
    round_.params = &params;
    round_.time = time;
    round_.dt = dt;
    round_.binHits = binHits;

    pendingWorkers_ = workers_.size();
    ++generation_;
    // Primera fase de la barrera: se libera el mutex y se despierta a todos.
    lock.unlock();
    startCondition_.notify_all();

    // Segunda fase: el coordinador no continua hasta que el ultimo worker
    // decremente el contador. Esto garantiza que nadie lea "writeBalls"
    // mientras otro hilo todavia lo esta escribiendo.
    lock.lock();
    finishedCondition_.wait(lock, [this] { return pendingWorkers_ == 0; });
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

        const RoundData round = round_;
        completedGeneration = generation_;
        lock.unlock();

        // Fuera del mutex: cada worker escribe unicamente su propio indice, de
        // modo que no existe solapamiento de escrituras entre hilos.
        if (ballIndex < round.ballCount) {
            int binHit = -1;
            round.writeBalls[ballIndex] = advanceBall(
                ballIndex, round.readBalls, round.ballCount,
                round.pegs, round.pegCount,
                round.modifiers, round.modifierCount,
                *round.params, round.time, round.dt, binHit);
            round.binHits[ballIndex] = binHit;
        }

        lock.lock();
        if (--pendingWorkers_ == 0) {
            // Solo el ultimo worker despierta al coordinador.
            lock.unlock();
            finishedCondition_.notify_one();
        }
    }
}
