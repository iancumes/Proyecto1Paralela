# PLINKO 3D PARALELO

Simulación continua de pelotas en un tablero Plinko tridimensional, preparada para comparar posteriormente la actualización física secuencial con una implementación paralela en OpenMP.

## Primer avance

### API gráfica

SDL2 crea la ventana y procesa eventos; OpenGL renderiza la escena 3D con double buffering.

### Elementos almacenados en memoria

La estructura `Ball` está declarada en `include/Ball.h`. La colección principal `std::vector<Ball> balls`, ubicada en `src/main.cpp`, almacena las 12 pelotas iniciales. La cantidad se controla con `INITIAL_BALL_COUNT`.

### Actualización

`updateBall(...)` y `updateBallsSerial(...)` están en `src/Ball.cpp`. Aplican gravedad, calculan la siguiente posición usando delta time y resuelven rebotes simples contra el piso y los límites laterales.

### Renderizado

`renderBall(...)` y `renderBalls(...)` están en `src/Renderer.cpp`. Cada pelota se dibuja como una esfera sencilla en su posición 3D almacenada.

### Ejecución

Requisitos: CMake 3.16 o posterior, compilador compatible con C++17, SDL2 y OpenGL.

```bash
cmake -S . -B build
cmake --build build
./build/plinko3d
```

Cierra la ventana con el botón del sistema o con la tecla `ESC`.
