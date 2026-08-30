#include "Simulation.h"

#include "BallThreadSystem.h"
#include "Random.h"

#include <algorithm>
#include <cmath>
#include <system_error>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace {

// Constantes fisicas que no dependen de la configuracion del usuario.
constexpr float AIR_DRAG          = 0.16F; // Amortiguamiento del aire por segundo.
constexpr float PEG_JITTER        = 0.55F; // Desviacion lateral maxima al golpear una clavija.
constexpr float MAX_SPEED         = 26.0F; // Rapidez maxima; evita que la escena "explote".
constexpr float SPAWN_SPREAD      = 0.92F; // Fraccion del ancho usada al reaparecer.
constexpr float PEG_RADIUS_FACTOR = 1.30F; // Radio de clavija relativo al radio de la pelota.

// Convierte un color HSV a RGB. Se usa para generar colores pseudoaleatorios
// vistosos: al fijar saturacion y valor altos se evitan los tonos apagados que
// produciria sortear las tres componentes RGB de forma independiente.
// Entradas: "hue" en [0,1); "saturation" y "value" en [0,1].
// Salida: Vec3 con las componentes rojo, verde y azul en [0,1].
Vec3 hsvToRgb(float hue, float saturation, float value) {
    const float sector = hue * 6.0F;
    const int index = static_cast<int>(sector) % 6;
    const float fraction = sector - std::floor(sector);
    const float p = value * (1.0F - saturation);
    const float q = value * (1.0F - saturation * fraction);
    const float t = value * (1.0F - saturation * (1.0F - fraction));

    switch (index) {
        case 0: return {value, t, p};
        case 1: return {q, value, p};
        case 2: return {p, value, t};
        case 3: return {p, q, value};
        case 4: return {t, p, value};
        default: return {value, p, q};
    }
}

// Recoloca una pelota en la parte alta del tablero con color, posicion y
// velocidad nuevas. Consume unicamente el generador propio de la pelota, por lo
// que puede ejecutarse dentro de un ciclo paralelo sin sincronizacion.
void respawnAtTop(Ball& ball, const SimulationParams& params) {
    const float usableHalfWidth = params.halfWidth() * SPAWN_SPREAD - ball.radius;
    ball.position.x = nextRandomInRange(ball.seed, -usableHalfWidth, usableHalfWidth);
    ball.position.y = params.ceilingY() - ball.radius -
                      nextRandomInRange(ball.seed, 0.0F, 0.6F);
    ball.position.z = nextRandomInRange(ball.seed,
                                        -params.halfDepth() + ball.radius,
                                        params.halfDepth() - ball.radius);
    ball.velocity.x = nextRandomInRange(ball.seed, -1.1F, 1.1F);
    ball.velocity.y = nextRandomInRange(ball.seed, -1.4F, -0.2F);
    ball.velocity.z = nextRandomInRange(ball.seed, -0.5F, 0.5F);
    ball.color = hsvToRgb(nextRandomFloat(ball.seed), 0.78F, 1.0F);
}

// Limita la rapidez de la pelota sin alterar su direccion.
void clampSpeed(Vec3& velocity) {
    const float speedSquared = lengthSquared(velocity);
    if (speedSquared > MAX_SPEED * MAX_SPEED) {
        velocity = velocity * (MAX_SPEED / std::sqrt(speedSquared));
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// Nucleo de fisica. Es el 100% del trabajo que se reparte entre hilos.
// Complejidad por pelota: O(N) por las interacciones pelota-pelota, O(M) por
// las clavijas y O(K) por los modificadores; en total O(N^2 + N*M + N*K) por
// paso de simulacion.
// ---------------------------------------------------------------------------
Ball advanceBall(std::size_t index,
                 const Ball* balls, std::size_t ballCount,
                 const Peg* pegs, std::size_t pegCount,
                 const Modifier* modifiers, std::size_t modifierCount,
                 const SimulationParams& params,
                 float time, float dt,
                 int& binHit) {
    Ball ball = balls[index];
    binHit = -1;
    if (!ball.active) {
        return ball;
    }

    // --- 1. Aceleracion: gravedad mas zonas modificadoras -------------------
    Vec3 acceleration {0.0F, params.gravity, 0.0F};
    for (std::size_t k = 0; k < modifierCount; ++k) {
        const Modifier& modifier = modifiers[k];
        const Vec3 toCenter = modifier.center - ball.position;
        const float distanceSquared = lengthSquared(toCenter);
        if (distanceSquared > modifier.radius * modifier.radius) {
            continue;
        }
        // La influencia decae linealmente desde el centro hacia el borde.
        const float distance = std::sqrt(distanceSquared);
        const float falloff = 1.0F - distance / modifier.radius;

        switch (modifier.kind) {
            case ModifierKind::GravityWell:
                acceleration += normalized(toCenter) * (modifier.strength * falloff);
                break;
            case ModifierKind::SpeedBoost:
                acceleration.y -= modifier.strength * falloff;
                break;
            case ModifierKind::Turbulence: {
                // Perturbacion lateral trigonometrica dependiente de la posicion
                // y del tiempo: produce un movimiento ondulante reproducible.
                const float wave = std::sin(time * 2.7F + ball.position.y * 1.9F);
                const float sway = std::cos(time * 2.1F + ball.position.x * 1.5F);
                acceleration.x += modifier.strength * falloff * wave;
                acceleration.z += modifier.strength * falloff * sway * 0.4F;
                break;
            }
        }
    }

    // --- 2. Colisiones pelota-pelota (ciclo dominante, O(N)) ----------------
    // Se leen exclusivamente los estados del cuadro anterior, por lo que el
    // resultado no depende del orden en que los hilos procesen los indices.
    Vec3 separation {0.0F, 0.0F, 0.0F};
    if (params.ballInteraction) {
        const float restitution = params.restitution;
        for (std::size_t other = 0; other < ballCount; ++other) {
            if (other == index) {
                continue;
            }
            const Ball& neighbor = balls[other];
            if (!neighbor.active) {
                continue;
            }
            const Vec3 delta = ball.position - neighbor.position;
            const float distanceSquared = lengthSquared(delta);
            const float contactDistance = ball.radius + neighbor.radius;
            if (distanceSquared >= contactDistance * contactDistance ||
                distanceSquared < 1.0e-10F) {
                continue;  // Sin contacto: se evita la raiz cuadrada.
            }

            const float distance = std::sqrt(distanceSquared);
            const Vec3 normal = delta * (1.0F / distance);
            const float totalMass = ball.mass + neighbor.mass;
            const float otherShare = neighbor.mass / totalMass;

            // Correccion de posicion proporcional a la masa del vecino.
            separation += normal * ((contactDistance - distance) * otherShare);

            // Impulso normal: solo si las pelotas se estan acercando.
            const float approachSpeed = dot(ball.velocity - neighbor.velocity, normal);
            if (approachSpeed < 0.0F) {
                const float impulse = -(1.0F + restitution) * approachSpeed * otherShare;
                ball.velocity += normal * impulse;
            }
        }
    }

    // --- 3. Integracion semi-implicita de Euler -----------------------------
    ball.velocity += acceleration * dt;
    ball.velocity = ball.velocity * std::max(0.0F, 1.0F - AIR_DRAG * dt);
    clampSpeed(ball.velocity);
    ball.position += ball.velocity * dt;
    ball.position += separation;

    // --- 4. Colisiones contra las clavijas (O(M)) ---------------------------
    // Las clavijas se modelan como cilindros paralelos al eje Z, por lo que la
    // distancia se evalua unicamente en el plano XY.
    for (std::size_t p = 0; p < pegCount; ++p) {
        const Peg& peg = pegs[p];
        const Vec3 center = pegPositionAt(peg, time);
        const float dx = ball.position.x - center.x;
        const float dy = ball.position.y - center.y;
        const float contactDistance = ball.radius + peg.radius;
        const float distanceSquared = dx * dx + dy * dy;
        if (distanceSquared >= contactDistance * contactDistance) {
            continue;
        }

        const float distance = std::sqrt(std::max(distanceSquared, 1.0e-10F));
        const float normalX = dx / distance;
        const float normalY = dy / distance;

        // Se empuja la pelota fuera de la clavija.
        ball.position.x = center.x + normalX * contactDistance;
        ball.position.y = center.y + normalY * contactDistance;

        // Reflexion de la velocidad respecto de la normal, con perdida de energia.
        const float normalSpeed = ball.velocity.x * normalX + ball.velocity.y * normalY;
        if (normalSpeed < 0.0F) {
            const float impulse = -(1.0F + params.restitution) * normalSpeed;
            ball.velocity.x += normalX * impulse;
            ball.velocity.y += normalY * impulse;
            // Pequena desviacion pseudoaleatoria: es lo que hace que dos
            // pelotas identicas terminen en casillas distintas.
            ball.velocity.x += nextRandomInRange(ball.seed, -PEG_JITTER, PEG_JITTER);
            ball.velocity.z += nextRandomInRange(ball.seed, -PEG_JITTER, PEG_JITTER) * 0.35F;
        }
    }

    // --- 5. Paredes laterales, fondo de profundidad y techo -----------------
    const float horizontalLimit = params.halfWidth() - ball.radius;
    if (ball.position.x < -horizontalLimit) {
        ball.position.x = -horizontalLimit;
        ball.velocity.x = std::fabs(ball.velocity.x) * params.restitution;
    } else if (ball.position.x > horizontalLimit) {
        ball.position.x = horizontalLimit;
        ball.velocity.x = -std::fabs(ball.velocity.x) * params.restitution;
    }

    const float depthLimit = params.halfDepth() - ball.radius;
    if (ball.position.z < -depthLimit) {
        ball.position.z = -depthLimit;
        ball.velocity.z = std::fabs(ball.velocity.z) * params.restitution;
    } else if (ball.position.z > depthLimit) {
        ball.position.z = depthLimit;
        ball.velocity.z = -std::fabs(ball.velocity.z) * params.restitution;
    }

    const float ceiling = params.ceilingY() - ball.radius;
    if (ball.position.y > ceiling) {
        ball.position.y = ceiling;
        ball.velocity.y = -std::fabs(ball.velocity.y) * params.restitution;
    }

    // --- 6. Llegada al fondo: se anota la casilla y la pelota se recicla -----
    if (ball.position.y - ball.radius <= params.floorY()) {
        const float normalizedX =
            (ball.position.x + params.halfWidth()) / std::max(params.boardWidth, 1.0e-4F);
        const int bin = std::clamp(
            static_cast<int>(normalizedX * static_cast<float>(params.binCount)),
            0, params.binCount - 1);
        binHit = bin;
        respawnAtTop(ball, params);
    }

    return ball;
}

// ---------------------------------------------------------------------------
// Simulation
// ---------------------------------------------------------------------------

Simulation::Simulation(const SimulationParams& params) : params_(params) {
    reset(params.seed);
}

Simulation::~Simulation() {
    // El unique_ptr destruye el sistema de hilos, que a su vez detiene y une
    // ordenadamente a todos los workers antes de liberar la memoria.
    releaseStdThreads();
}

void Simulation::reset(std::uint32_t seed) {
    releaseStdThreads();

    seed_ = seed;
    params_.seed = seed;
    time_ = 0.0F;
    recycledBalls_ = 0;
    lastLoadImbalance_ = 1.0;

    std::uint32_t seedState = mixSeed(seed, 0x5EEDu);
    buildPegs(seedState);
    buildModifiers(seedState);

    const std::size_t ballCount = static_cast<std::size_t>(std::max(params_.ballCount, 0));
    current_.assign(ballCount, Ball{});
    for (std::size_t index = 0; index < ballCount; ++index) {
        spawnBall(current_[index], static_cast<std::uint32_t>(index), seedState);
    }
    next_ = current_;
    binHits_.assign(ballCount, -1);
    binCounts_.assign(static_cast<std::size_t>(params_.binCount), 0LL);
}

void Simulation::resizeBallCount(int ballCount) {
    const int clamped = std::clamp(ballCount, 1, 200000);
    if (clamped == params_.ballCount) {
        return;
    }
    params_.ballCount = clamped;
    reset(seed_);
}

void Simulation::buildPegs(std::uint32_t& seedState) {
    pegs_.clear();
    const int rows = std::max(params_.pegRows, 0);
    const int columns = std::max(params_.pegColumns, 0);
    if (rows == 0 || columns == 0) {
        return;
    }

    pegs_.reserve(static_cast<std::size_t>(rows) * static_cast<std::size_t>(columns));
    const float pegRadius = params_.ballRadius * PEG_RADIUS_FACTOR;
    // Se deja un margen arriba (zona de caida) y abajo (zona de casillas).
    const float topY = params_.ceilingY() - params_.boardHeight * 0.14F;
    const float bottomY = params_.floorY() + params_.boardHeight * 0.16F;
    const float rowSpacing = rows > 1 ? (topY - bottomY) / static_cast<float>(rows - 1) : 0.0F;
    const float columnSpacing = params_.boardWidth / static_cast<float>(columns + 1);

    for (int row = 0; row < rows; ++row) {
        // Las filas impares se desplazan medio paso: es la rejilla clasica de
        // un tablero Plinko y garantiza que la pelota siempre encuentre clavija.
        const float offset = (row % 2 == 0) ? 0.0F : columnSpacing * 0.5F;
        for (int column = 0; column < columns; ++column) {
            Peg peg {};
            peg.basePosition = {
                -params_.halfWidth() + columnSpacing * static_cast<float>(column + 1) + offset,
                topY - rowSpacing * static_cast<float>(row),
                0.0F
            };
            peg.radius = pegRadius;
            // Solo una parte de las clavijas oscila, para que el tablero tenga
            // zonas predecibles y zonas cambiantes.
            const bool oscillates = (nextRandomFloat(seedState) < 0.35F);
            peg.amplitude = oscillates ? nextRandomInRange(seedState, 0.18F, 0.45F) : 0.0F;
            peg.angularSpeed = nextRandomInRange(seedState, 0.6F, 1.9F);
            peg.phase = nextRandomInRange(seedState, 0.0F, 6.2831853F);
            // Las clavijas que oscilan se pintan en violeta y las fijas en cian,
            // para que se distinga a simple vista cual parte del tablero cambia.
            peg.color = oscillates ? hsvToRgb(0.78F, 0.55F, 0.95F)
                                   : hsvToRgb(0.53F, 0.45F, 0.78F);
            pegs_.push_back(peg);
        }
    }
}

void Simulation::buildModifiers(std::uint32_t& seedState) {
    modifiers_.clear();
    const int count = std::max(params_.modifierCount, 0);
    modifiers_.reserve(static_cast<std::size_t>(count));

    for (int index = 0; index < count; ++index) {
        Modifier modifier {};
        modifier.center = {
            nextRandomInRange(seedState, -params_.halfWidth() * 0.8F, params_.halfWidth() * 0.8F),
            nextRandomInRange(seedState, params_.floorY() * 0.6F, params_.ceilingY() * 0.7F),
            nextRandomInRange(seedState, -params_.halfDepth() * 0.5F, params_.halfDepth() * 0.5F)
        };
        modifier.radius = nextRandomInRange(seedState, 0.9F, 1.9F);
        modifier.kind = static_cast<ModifierKind>(nextRandomUint(seedState) % 3u);
        switch (modifier.kind) {
            case ModifierKind::GravityWell:
                modifier.strength = nextRandomInRange(seedState, 6.0F, 14.0F);
                modifier.color = {0.35F, 0.55F, 1.0F};
                break;
            case ModifierKind::SpeedBoost:
                modifier.strength = nextRandomInRange(seedState, 8.0F, 18.0F);
                modifier.color = {1.0F, 0.45F, 0.25F};
                break;
            case ModifierKind::Turbulence:
                modifier.strength = nextRandomInRange(seedState, 5.0F, 12.0F);
                modifier.color = {0.55F, 1.0F, 0.45F};
                break;
        }
        modifiers_.push_back(modifier);
    }
}

void Simulation::spawnBall(Ball& ball, std::uint32_t index, std::uint32_t& seedState) const {
    // Cada pelota recibe una semilla derivada de la global y de su indice: asi
    // la escena es reproducible y ningun hilo comparte estado de generacion.
    ball.seed = mixSeed(seedState, index);
    ball.radius = params_.ballRadius;
    ball.mass = 1.0F;
    ball.bin = -1;
    ball.active = true;
    respawnAtTop(ball, params_);
    // En el arranque las pelotas se reparten por toda la altura del tablero
    // para que la escena se vea poblada desde el primer cuadro.
    ball.position.y = nextRandomInRange(ball.seed,
                                        params_.floorY() + ball.radius * 3.0F,
                                        params_.ceilingY() - ball.radius);
}

void Simulation::releaseStdThreads() {
    stdThreads_.reset();
}

void Simulation::ensureStdThreads() {
    if (stdThreads_ && stdThreads_->threadCount() == current_.size()) {
        return;
    }
    releaseStdThreads();
    stdThreadsAvailable_ = true;
    stdThreadsError_.clear();
    try {
        stdThreads_ = std::make_unique<BallThreadSystem>(current_.size());
    } catch (const std::exception& error) {
        // Un hilo por pelota deja de ser viable cuando N es grande: el sistema
        // operativo rechaza la creacion. Se informa y se continua sin abortar.
        stdThreadsAvailable_ = false;
        stdThreadsError_ = error.what();
        stdThreads_.reset();
    }
}

std::size_t Simulation::stdThreadCount() const {
    return stdThreads_ ? stdThreads_->threadCount() : 0U;
}

void Simulation::step(float dt, ExecutionMode mode, int threadCount) {
    switch (mode) {
        case ExecutionMode::Sequential:   stepSequential(dt); break;
        case ExecutionMode::StdThreads:   stepStdThreads(dt); break;
        case ExecutionMode::OpenMpStatic: stepOpenMpStatic(dt, threadCount); break;
        case ExecutionMode::OpenMpTuned:  stepOpenMpTuned(dt, threadCount); break;
    }
}

void Simulation::stepSequential(float dt) {
    const std::size_t ballCount = current_.size();
    if (ballCount == 0) {
        return;
    }
    const int substeps = std::max(params_.substeps, 1);
    const float subDelta = dt / static_cast<float>(substeps);
    const float startTime = time_;

    for (int sub = 0; sub < substeps; ++sub) {
        // El tiempo del sub-paso se calcula siempre con la misma formula en los
        // cuatro modos para que los resultados sean comparables bit a bit.
        const float subTime = startTime + static_cast<float>(sub) * subDelta;
        const Ball* readBalls = current_.data();
        Ball* writeBalls = next_.data();

        for (std::size_t index = 0; index < ballCount; ++index) {
            int binHit = -1;
            writeBalls[index] = advanceBall(index, readBalls, ballCount,
                                            pegs_.data(), pegs_.size(),
                                            modifiers_.data(), modifiers_.size(),
                                            params_, subTime, subDelta, binHit);
            if (binHit >= 0) {
                ++binCounts_[static_cast<std::size_t>(binHit)];
                ++recycledBalls_;
            }
        }
        current_.swap(next_);
    }
    time_ = startTime + static_cast<float>(substeps) * subDelta;
    lastOpenMpThreads_ = 1;
    lastLoadImbalance_ = 1.0;
}

void Simulation::stepStdThreads(float dt) {
    const std::size_t ballCount = current_.size();
    if (ballCount == 0) {
        return;
    }
    ensureStdThreads();
    if (!stdThreads_) {
        // Degradacion controlada: si no hay hilos disponibles se usa la version
        // secuencial en lugar de dejar la escena congelada.
        stepSequential(dt);
        return;
    }

    const int substeps = std::max(params_.substeps, 1);
    const float subDelta = dt / static_cast<float>(substeps);
    const float startTime = time_;

    for (int sub = 0; sub < substeps; ++sub) {
        const float subTime = startTime + static_cast<float>(sub) * subDelta;
        stdThreads_->runRound(current_.data(), next_.data(), ballCount,
                              pegs_.data(), pegs_.size(),
                              modifiers_.data(), modifiers_.size(),
                              params_, subTime, subDelta, binHits_.data());

        // Tras la barrera el hilo coordinador es el unico activo, por lo que
        // puede acumular las casillas sin necesidad de proteccion adicional.
        for (std::size_t index = 0; index < ballCount; ++index) {
            const int binHit = binHits_[index];
            if (binHit >= 0) {
                ++binCounts_[static_cast<std::size_t>(binHit)];
                ++recycledBalls_;
            }
        }
        current_.swap(next_);
    }
    time_ = startTime + static_cast<float>(substeps) * subDelta;
    lastOpenMpThreads_ = static_cast<int>(stdThreads_->threadCount());
    lastLoadImbalance_ = 1.0;
}

void Simulation::stepOpenMpStatic(float dt, int threadCount) {
    const std::size_t ballCount = current_.size();
    if (ballCount == 0) {
        return;
    }

    const int substeps = std::max(params_.substeps, 1);
    const float subDelta = dt / static_cast<float>(substeps);
    const float startTime = time_;
    const int elements = static_cast<int>(ballCount);

    // Copias locales: permiten escribir pragmas sin depender del puntero "this"
    // y evitan accesos indirectos dentro del ciclo caliente.
    const SimulationParams params = params_;
    const Peg* pegData = pegs_.data();
    const std::size_t pegCount = pegs_.size();
    const Modifier* modifierData = modifiers_.data();
    const std::size_t modifierCount = modifiers_.size();
    long long* binData = binCounts_.data();

#ifdef _OPENMP
    const int threads = threadCount > 0 ? threadCount : omp_get_max_threads();
#else
    const int threads = 1;
    (void)threadCount;
#endif
    long long recycled = 0;

    for (int sub = 0; sub < substeps; ++sub) {
        const float subTime = startTime + static_cast<float>(sub) * subDelta;
        const Ball* readBalls = current_.data();
        Ball* writeBalls = next_.data();

        // Reparto estatico: cada hilo recibe un bloque contiguo de indices.
        // Es la traduccion directa del ciclo secuencial y la primera version
        // OpenMP que se midio en el proyecto.
#pragma omp parallel for schedule(static) num_threads(threads) \
    shared(readBalls, writeBalls, pegData, modifierData, binData, params) \
    firstprivate(elements, pegCount, modifierCount, subTime, subDelta) \
    reduction(+ : recycled)
        for (int index = 0; index < elements; ++index) {
            int binHit = -1;
            writeBalls[index] = advanceBall(static_cast<std::size_t>(index),
                                            readBalls, static_cast<std::size_t>(elements),
                                            pegData, pegCount,
                                            modifierData, modifierCount,
                                            params, subTime, subDelta, binHit);
            if (binHit >= 0) {
                // Varios hilos pueden anotar en la misma casilla: la
                // actualizacion se protege con una operacion atomica.
#pragma omp atomic
                binData[binHit] += 1LL;
                recycled += 1;
            }
        }
        // El "omp parallel for" incorpora una barrera implicita al final, que es
        // justo la sincronia que exige el siguiente sub-paso: nadie puede leer
        // el buffer nuevo antes de que todos hayan terminado de escribirlo.
        current_.swap(next_);
    }

    recycledBalls_ += recycled;
    time_ = startTime + static_cast<float>(substeps) * subDelta;
    lastOpenMpThreads_ = threads;
    lastLoadImbalance_ = 1.0;
}

void Simulation::stepOpenMpTuned(float dt, int threadCount) {
    const std::size_t ballCount = current_.size();
    if (ballCount == 0) {
        return;
    }

    const int substeps = std::max(params_.substeps, 1);
    const float subDelta = dt / static_cast<float>(substeps);
    const float startTime = time_;
    const int elements = static_cast<int>(ballCount);
    const int bins = params_.binCount;

    const SimulationParams params = params_;
    const Peg* pegData = pegs_.data();
    const std::size_t pegCount = pegs_.size();
    const Modifier* modifierData = modifiers_.data();
    const std::size_t modifierCount = modifiers_.size();

#ifdef _OPENMP
    const int threads = threadCount > 0 ? threadCount : omp_get_max_threads();
#else
    const int threads = 1;
    (void)threadCount;
#endif

    // Punteros crudos a los dos buffers. Dentro de la region paralela no se
    // puede intercambiar los vectores, asi que la alternancia se resuelve por
    // la paridad del sub-paso.
    Ball* bufferA = current_.data();
    Ball* bufferB = next_.data();

    threadBusyTime_.assign(static_cast<std::size_t>(std::max(threads, 1)), PaddedTime{});
    PaddedTime* busyTime = threadBusyTime_.data();

    long long recycled = 0;
    std::vector<long long> sharedBins(static_cast<std::size_t>(bins), 0LL);
    long long* sharedBinData = sharedBins.data();

    // Una sola region paralela para todos los sub-pasos: se paga el costo de
    // crear el equipo de hilos una vez por cuadro en lugar de una vez por
    // sub-paso, que era la principal perdida de la version "omp-static".
#pragma omp parallel num_threads(threads) \
    shared(bufferA, bufferB, pegData, modifierData, params, sharedBinData, busyTime) \
    firstprivate(elements, bins, substeps, subDelta, startTime, pegCount, modifierCount) \
    reduction(+ : recycled)
    {
#ifdef _OPENMP
        const int threadId = omp_get_thread_num();
        const double threadStart = omp_get_wtime();
#else
        const int threadId = 0;
        const double threadStart = 0.0;
#endif
        // Contadores privados: cada hilo acumula en su propia copia y solo al
        // final se fusionan. Evita la contencion de la operacion atomica que
        // usa la version "omp-static".
        std::vector<long long> localBins(static_cast<std::size_t>(bins), 0LL);

        for (int sub = 0; sub < substeps; ++sub) {
            const float subTime = startTime + static_cast<float>(sub) * subDelta;
            const Ball* readBalls = (sub % 2 == 0) ? bufferA : bufferB;
            Ball* writeBalls = (sub % 2 == 0) ? bufferB : bufferA;

            // Reparto guiado: los bloques grandes del inicio amortizan el costo
            // de planificacion y los pequenos del final nivelan la carga cuando
            // unas pelotas colisionan mas que otras.
#pragma omp for schedule(guided)
            for (int index = 0; index < elements; ++index) {
                int binHit = -1;
                writeBalls[index] = advanceBall(static_cast<std::size_t>(index),
                                                readBalls, static_cast<std::size_t>(elements),
                                                pegData, pegCount,
                                                modifierData, modifierCount,
                                                params, subTime, subDelta, binHit);
                if (binHit >= 0) {
                    localBins[static_cast<std::size_t>(binHit)] += 1LL;
                    recycled += 1;
                }
            }
            // Barrera implicita del "omp for": el sub-paso siguiente lee lo que
            // este acaba de escribir, de modo que la barrera es obligatoria.
        }

#ifdef _OPENMP
        busyTime[threadId].seconds = omp_get_wtime() - threadStart;
#else
        busyTime[threadId].seconds = threadStart;
#endif

        // Fusion de los contadores privados. La seccion critica se ejecuta una
        // sola vez por hilo y por cuadro, no una vez por pelota.
#pragma omp critical(binCountMerge)
        {
            for (int bin = 0; bin < bins; ++bin) {
                sharedBinData[bin] += localBins[static_cast<std::size_t>(bin)];
            }
        }

        // Todos los hilos deben haber publicado su tiempo antes de que uno solo
        // calcule la metrica de desbalance de carga.
#pragma omp barrier
#pragma omp single
        {
            double maximum = 0.0;
            double total = 0.0;
            const int active = threads;
            for (int t = 0; t < active; ++t) {
                maximum = std::max(maximum, busyTime[t].seconds);
                total += busyTime[t].seconds;
            }
            const double average = active > 0 ? total / static_cast<double>(active) : 0.0;
            lastLoadImbalance_ = average > 0.0 ? maximum / average : 1.0;
        }
    }

    for (int bin = 0; bin < bins; ++bin) {
        binCounts_[static_cast<std::size_t>(bin)] += sharedBins[static_cast<std::size_t>(bin)];
    }
    recycledBalls_ += recycled;

    // Si la cantidad de sub-pasos es impar el resultado quedo en "next_".
    if (substeps % 2 == 1) {
        current_.swap(next_);
    }
    time_ = startTime + static_cast<float>(substeps) * subDelta;
    lastOpenMpThreads_ = threads;
}
