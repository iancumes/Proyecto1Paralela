#include "Ball.h"
#include "BallThreadSystem.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace {
constexpr std::size_t BALL_COUNT = 12;
constexpr int STEP_COUNT = 600;
constexpr float DT = 1.0F / 120.0F;
constexpr float TOLERANCE = 0.000001F;

std::vector<Ball> createBalls() {
    std::vector<Ball> balls;
    balls.reserve(BALL_COUNT);
    for (std::size_t index = 0; index < BALL_COUNT; ++index) {
        const float value = static_cast<float>(index);
        balls.push_back({
            {-4.0F + value * 0.6F, 4.0F + value * 0.2F, -0.5F + value * 0.05F},
            {0.2F + value * 0.03F, 0.0F, -0.1F + value * 0.01F},
            0.3F,
            1.0F,
            BallColor::Cyan,
            true
        });
    }
    return balls;
}

bool close(float left, float right) {
    return std::fabs(left - right) <= TOLERANCE;
}

bool sameState(const Ball& left, const Ball& right) {
    return close(left.position.x, right.position.x)
        && close(left.position.y, right.position.y)
        && close(left.position.z, right.position.z)
        && close(left.velocity.x, right.velocity.x)
        && close(left.velocity.y, right.velocity.y)
        && close(left.velocity.z, right.velocity.z)
        && left.active == right.active;
}
}

int main() {
    std::vector<Ball> serialBalls = createBalls();
    std::vector<Ball> parallelBalls = serialBalls;
    BallThreadSystem parallelSystem(parallelBalls);

    if (parallelSystem.threadCount() != parallelBalls.size()) {
        std::cerr << "No se creo exactamente un hilo por pelota.\n";
        return 1;
    }

    for (int step = 0; step < STEP_COUNT; ++step) {
        updateBallsSerial(serialBalls, DT);
        parallelSystem.update(DT);
    }

    for (std::size_t index = 0; index < serialBalls.size(); ++index) {
        if (!sameState(serialBalls[index], parallelBalls[index])) {
            std::cerr << "Los modos difieren en la pelota " << index << ".\n";
            return 1;
        }
    }

    std::cout << "OK: " << parallelSystem.threadCount()
              << " pelotas produjeron el mismo resultado en ambos modos.\n";
    return 0;
}
