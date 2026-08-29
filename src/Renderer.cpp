#include "Renderer.h"

#include <SDL_opengl.h>

#include <algorithm>
#include <cmath>

namespace {
constexpr float PI = 3.14159265358979323846F;

void setPerspective(float fieldOfViewDegrees, float aspect, float nearPlane, float farPlane) {
    const float top = nearPlane * std::tan(fieldOfViewDegrees * PI / 360.0F);
    const float right = top * aspect;
    glFrustum(-right, right, -top, top, nearPlane, farPlane);
}

void setBallColor(BallColor color) {
    switch (color) {
        case BallColor::Cyan: glColor3f(0.10F, 0.85F, 1.00F); break;
        case BallColor::Magenta: glColor3f(1.00F, 0.20F, 0.75F); break;
        case BallColor::Yellow: glColor3f(1.00F, 0.80F, 0.15F); break;
        case BallColor::Green: glColor3f(0.30F, 1.00F, 0.50F); break;
    }
}

void renderBoard() {
    glColor3f(0.08F, 0.11F, 0.20F);
    glBegin(GL_QUADS);
    glVertex3f(-7.0F, -4.0F, -2.2F);
    glVertex3f(7.0F, -4.0F, -2.2F);
    glVertex3f(7.0F, 6.0F, -2.2F);
    glVertex3f(-7.0F, 6.0F, -2.2F);
    glEnd();

    glColor3f(0.25F, 0.30F, 0.48F);
    glBegin(GL_LINES);
    for (int row = 0; row < 6; ++row) {
        const float y = 3.7F - static_cast<float>(row) * 1.25F;
        for (int column = -4; column <= 4; ++column) {
            const float x = static_cast<float>(column) * 1.35F + (row % 2 == 0 ? 0.0F : 0.675F);
            glVertex3f(x - 0.13F, y, -1.9F);
            glVertex3f(x + 0.13F, y, -1.9F);
        }
    }
    glEnd();

    glColor3f(0.18F, 0.65F, 0.95F);
    glBegin(GL_LINES);
    glVertex3f(-7.0F, -4.0F, -1.8F);
    glVertex3f(7.0F, -4.0F, -1.8F);
    glEnd();
}
}

void initializeRenderer(int width, int height) {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.015F, 0.02F, 0.07F, 1.0F);
    resizeRenderer(width, height);
}

void resizeRenderer(int width, int height) {
    height = std::max(height, 1);
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    setPerspective(45.0F, static_cast<float>(width) / static_cast<float>(height), 0.1F, 100.0F);
    glMatrixMode(GL_MODELVIEW);
}

void renderBall(const Ball& ball) {
    if (!ball.active) {
        return;
    }

    constexpr int LATITUDE_SEGMENTS = 12;
    constexpr int LONGITUDE_SEGMENTS = 18;

    glPushMatrix();
    glTranslatef(ball.position.x, ball.position.y, ball.position.z);
    setBallColor(ball.color);

    for (int latitude = 0; latitude < LATITUDE_SEGMENTS; ++latitude) {
        const float angle1 = -PI / 2.0F + PI * latitude / LATITUDE_SEGMENTS;
        const float angle2 = -PI / 2.0F + PI * (latitude + 1) / LATITUDE_SEGMENTS;
        glBegin(GL_QUAD_STRIP);
        for (int longitude = 0; longitude <= LONGITUDE_SEGMENTS; ++longitude) {
            const float longitudeAngle = 2.0F * PI * longitude / LONGITUDE_SEGMENTS;
            for (float latitudeAngle : {angle1, angle2}) {
                const float nx = std::cos(latitudeAngle) * std::cos(longitudeAngle);
                const float ny = std::sin(latitudeAngle);
                const float nz = std::cos(latitudeAngle) * std::sin(longitudeAngle);
                glVertex3f(ball.radius * nx, ball.radius * ny, ball.radius * nz);
            }
        }
        glEnd();
    }
    glPopMatrix();
}

void renderBalls(const std::vector<Ball>& balls) {
    for (const Ball& ball : balls) {
        renderBall(ball);
    }
}

void renderScene(const std::vector<Ball>& balls) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glTranslatef(0.0F, -0.6F, -18.0F);
    renderBoard();
    renderBalls(balls);
}
