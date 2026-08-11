#include "Ball.h"

#include <algorithm>

namespace {
constexpr float GRAVITY = -9.81F;
constexpr float FLOOR_Y = -4.0F;
constexpr float SIDE_LIMIT = 6.5F;
constexpr float DEPTH_LIMIT = 2.0F;
constexpr float BOUNCE_DAMPING = 0.78F;
}

Vec3& Vec3::operator+=(const Vec3& other) {
    x += other.x;
    y += other.y;
    z += other.z;
    return *this;
}

Vec3 operator*(const Vec3& vector, float scalar) {
    return {vector.x * scalar, vector.y * scalar, vector.z * scalar};
}

void updateBall(Ball& ball, float dt) {
    if (!ball.active) {
        return;
    }

    ball.velocity.y += GRAVITY * dt;
    ball.position += ball.velocity * dt;

    if (ball.position.y - ball.radius < FLOOR_Y) {
        ball.position.y = FLOOR_Y + ball.radius;
        ball.velocity.y = -ball.velocity.y * BOUNCE_DAMPING;
    }

    const float horizontalLimit = SIDE_LIMIT - ball.radius;
    if (ball.position.x < -horizontalLimit || ball.position.x > horizontalLimit) {
        ball.position.x = std::clamp(ball.position.x, -horizontalLimit, horizontalLimit);
        ball.velocity.x = -ball.velocity.x * BOUNCE_DAMPING;
    }

    const float depthLimit = DEPTH_LIMIT - ball.radius;
    if (ball.position.z < -depthLimit || ball.position.z > depthLimit) {
        ball.position.z = std::clamp(ball.position.z, -depthLimit, depthLimit);
        ball.velocity.z = -ball.velocity.z * BOUNCE_DAMPING;
    }
}

void updateBallsSerial(std::vector<Ball>& balls, float dt) {
    for (Ball& ball : balls) {
        updateBall(ball, dt);
    }
}
