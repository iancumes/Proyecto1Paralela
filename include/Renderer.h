#pragma once

#include "Ball.h"

#include <vector>

void initializeRenderer(int width, int height);
void resizeRenderer(int width, int height);
void renderBall(const Ball& ball);
void renderBalls(const std::vector<Ball>& balls);
void renderScene(const std::vector<Ball>& balls);
