#pragma once

#include <vector>

struct Vec3 {
    float x;
    float y;
    float z;

    Vec3& operator+=(const Vec3& other);
};

Vec3 operator*(const Vec3& vector, float scalar);

enum class BallColor {
    Cyan,
    Magenta,
    Yellow,
    Green
};

struct Ball {
    Vec3 position;
    Vec3 velocity;
    float radius;
    float mass;
    BallColor color;
    bool active;
};

void updateBall(Ball& ball, float dt);
void updateBallsSerial(std::vector<Ball>& balls, float dt);
