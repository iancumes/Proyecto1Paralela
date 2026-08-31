#include "Renderer.h"

#include "Font5x7.h"
#include "Random.h"

#include <SDL_opengl.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

constexpr float PI = 3.14159265358979323846F;
constexpr int SPRITE_TEXTURE_SIZE = 64;   // Lado de la textura procedural.
constexpr float FIELD_OF_VIEW = 45.0F;    // Campo de vision vertical en grados.
constexpr float FIT_MARGIN = 1.02F;       // Holgura alrededor del contenido.
// Acercamiento deliberado. El solver de arriba encuadra el cilindro entero, pero
// el punto que obliga a alejar la camara es el borde cercano de la corona de
// sectores, que al inclinar la vista queda mucho mas proximo que el centro.
// Respetarlo al pie de la letra desperdiciaba casi una cuarta parte del ancho
// del lienzo en franjas negras, que es justo lo que no se quiere en un protector
// de pantalla. Con este factor la escena llena el ancho y solo se recorta el
// borde mas cercano, lo que ademas hace que la arena parezca continuar mas alla
// de la pantalla. El valor se ajusto midiendo los pixeles de capturas reales a
// pantalla completa, no a ojo.
constexpr float FILL_ZOOM = 0.92F;

GLuint g_sphereTexture = 0;  // Textura de esfera preiluminada (pelotas).
GLuint g_pegTexture = 0;     // Esfera con reborde oscuro, exclusiva de las clavijas.
GLuint g_glowTexture = 0;    // Halo aditivo que da el aspecto de neon.
int g_viewportWidth = 1280;  // Ancho actual del lienzo.
int g_viewportHeight = 720;  // Alto actual del lienzo.

// Estrella del fondo. Se generan una sola vez, en coordenadas normalizadas, de
// modo que el campo se adapta a cualquier resolucion sin regenerarse.
struct Estrella {
    float x;           // Posicion horizontal en [0, 1].
    float y;           // Posicion vertical en [0, 1].
    float brillo;      // Brillo base en [0, 1].
    float tamano;      // Lado en pixeles a 1080p.
    float fase;        // Desfase del parpadeo.
    float velocidad;   // Velocidad del parpadeo.
};
std::vector<Estrella> g_estrellas;

// Construye una perspectiva sin depender de GLU, que no siempre esta presente.
// Entradas: campo de vision vertical en grados, relacion de aspecto y planos.
void setPerspective(float fieldOfViewDegrees, float aspect, float nearPlane, float farPlane) {
    const float top = nearPlane * std::tan(fieldOfViewDegrees * PI / 360.0F);
    const float right = top * aspect;
    glFrustum(-right, right, -top, top, nearPlane, farPlane);
}

// Genera la textura de una esfera iluminada desde arriba a la izquierda.
// El canal alfa define el recorte circular; los canales RGB llevan el termino
// difuso mas un realce especular. Al multiplicarla por el color de la pelota se
// obtiene una esfera creible con un solo cuadrilatero.
GLuint createSphereTexture() {
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(SPRITE_TEXTURE_SIZE) * SPRITE_TEXTURE_SIZE * 4, 0);

    const float half = SPRITE_TEXTURE_SIZE * 0.5F;
    for (int y = 0; y < SPRITE_TEXTURE_SIZE; ++y) {
        for (int x = 0; x < SPRITE_TEXTURE_SIZE; ++x) {
            const float nx = (static_cast<float>(x) + 0.5F - half) / half;
            const float ny = (static_cast<float>(y) + 0.5F - half) / half;
            const float radiusSquared = nx * nx + ny * ny;
            const std::size_t offset =
                (static_cast<std::size_t>(y) * SPRITE_TEXTURE_SIZE + static_cast<std::size_t>(x)) * 4;

            if (radiusSquared > 1.0F) {
                pixels[offset + 3] = 0;  // Fuera del circulo: totalmente transparente.
                continue;
            }

            // Normal de la esfera reconstruida a partir de la posicion.
            const float nz = std::sqrt(std::max(0.0F, 1.0F - radiusSquared));
            const float lightX = -0.45F;
            const float lightY = -0.55F;
            const float lightZ = 0.70F;
            const float diffuse = std::max(0.0F, nx * lightX + ny * lightY + nz * lightZ);
            // Realce especular estrecho.
            const float specular = std::pow(diffuse, 22.0F);
            const float intensity = std::min(1.0F, 0.24F + 0.86F * diffuse);

            const float red = intensity + specular * 0.85F;
            const float green = intensity + specular * 0.85F;
            const float blue = intensity + specular * 0.85F;

            // Suavizado del borde para evitar el aliasing del circulo.
            const float edge = std::min(1.0F, (1.0F - std::sqrt(radiusSquared)) * 12.0F);

            pixels[offset + 0] = static_cast<std::uint8_t>(std::min(255.0F, red * 255.0F));
            pixels[offset + 1] = static_cast<std::uint8_t>(std::min(255.0F, green * 255.0F));
            pixels[offset + 2] = static_cast<std::uint8_t>(std::min(255.0F, blue * 255.0F));
            pixels[offset + 3] = static_cast<std::uint8_t>(std::max(0.0F, edge) * 255.0F);
        }
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SPRITE_TEXTURE_SIZE, SPRITE_TEXTURE_SIZE, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    return texture;
}

// Genera la textura de las clavijas: la misma esfera iluminada, pero con un
// reborde oscuro bien marcado y un realce especular mucho mas tenue.
//
// El objetivo es que clavijas y pelotas nunca se confundan, aunque el color de
// una pelota se acerque al de una clavija. Se separan por cuatro rasgos a la
// vez: las clavijas son mas grandes, usan tonos frios poco saturados, llevan
// este contorno oscuro y no emiten halo.
GLuint createPegTexture() {
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(SPRITE_TEXTURE_SIZE) * SPRITE_TEXTURE_SIZE * 4, 0);

    const float half = SPRITE_TEXTURE_SIZE * 0.5F;
    for (int y = 0; y < SPRITE_TEXTURE_SIZE; ++y) {
        for (int x = 0; x < SPRITE_TEXTURE_SIZE; ++x) {
            const float nx = (static_cast<float>(x) + 0.5F - half) / half;
            const float ny = (static_cast<float>(y) + 0.5F - half) / half;
            const float radiusSquared = nx * nx + ny * ny;
            const std::size_t offset =
                (static_cast<std::size_t>(y) * SPRITE_TEXTURE_SIZE + static_cast<std::size_t>(x)) * 4;

            if (radiusSquared > 1.0F) {
                pixels[offset + 3] = 0;
                continue;
            }

            const float radius = std::sqrt(radiusSquared);
            const float nz = std::sqrt(std::max(0.0F, 1.0F - radiusSquared));
            const float diffuse = std::max(0.0F, nx * -0.42F + ny * -0.52F + nz * 0.74F);
            float intensity = 0.30F + 0.78F * diffuse + std::pow(diffuse, 30.0F) * 0.35F;

            // Reborde: el ultimo 18 % del radio se oscurece con fuerza, lo que
            // da a la clavija un contorno nitido contra el fondo y contra
            // cualquier pelota que pase por detras.
            if (radius > 0.82F) {
                const float borde = (radius - 0.82F) / 0.18F;
                intensity *= (1.0F - 0.82F * borde);
            }

            const float edge = std::min(1.0F, (1.0F - radius) * 14.0F);
            const std::uint8_t canal =
                static_cast<std::uint8_t>(std::min(255.0F, intensity * 255.0F));
            pixels[offset + 0] = canal;
            pixels[offset + 1] = canal;
            pixels[offset + 2] = canal;
            pixels[offset + 3] = static_cast<std::uint8_t>(std::max(0.0F, edge) * 255.0F);
        }
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SPRITE_TEXTURE_SIZE, SPRITE_TEXTURE_SIZE, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    return texture;
}

// Genera un halo radial suave que se dibuja en modo aditivo alrededor de cada
// pelota. Es lo que produce la estetica de neon del screensaver.
GLuint createGlowTexture() {
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(SPRITE_TEXTURE_SIZE) * SPRITE_TEXTURE_SIZE * 4, 0);

    const float half = SPRITE_TEXTURE_SIZE * 0.5F;
    for (int y = 0; y < SPRITE_TEXTURE_SIZE; ++y) {
        for (int x = 0; x < SPRITE_TEXTURE_SIZE; ++x) {
            const float nx = (static_cast<float>(x) + 0.5F - half) / half;
            const float ny = (static_cast<float>(y) + 0.5F - half) / half;
            const float distance = std::sqrt(nx * nx + ny * ny);
            const float falloff = std::max(0.0F, 1.0F - distance);
            const float alpha = falloff * falloff * falloff;
            const std::size_t offset =
                (static_cast<std::size_t>(y) * SPRITE_TEXTURE_SIZE + static_cast<std::size_t>(x)) * 4;
            pixels[offset + 0] = 255;
            pixels[offset + 1] = 255;
            pixels[offset + 2] = 255;
            pixels[offset + 3] = static_cast<std::uint8_t>(alpha * 255.0F);
        }
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SPRITE_TEXTURE_SIZE, SPRITE_TEXTURE_SIZE, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    return texture;
}

// Base de la camara en coordenadas de mundo. Como la camara ahora orbita, un
// cuadrilatero fijo en el plano XY dejaria de mirarla en cuanto girara: los
// billboards se construyen sobre estos dos vectores.
struct CameraBasis {
    Vec3 right;  // Eje horizontal de la pantalla, llevado a coordenadas de mundo.
    Vec3 up;     // Eje vertical de la pantalla, llevado a coordenadas de mundo.
};

// Invierte la rotacion de la vista para obtener los ejes de pantalla en mundo.
// La matriz de vista es Rx(pitch) * Ry(yaw), asi que la inversa que se aplica a
// los ejes canonicos es Ry(-yaw) * Rx(-pitch).
CameraBasis cameraBasisFor(float yawDegrees, float pitchDegrees) {
    const float yaw = yawDegrees * PI / 180.0F;
    const float pitch = pitchDegrees * PI / 180.0F;
    const float cosYaw = std::cos(yaw);
    const float sinYaw = std::sin(yaw);
    const float cosPitch = std::cos(pitch);
    const float sinPitch = std::sin(pitch);
    return {
        {cosYaw, 0.0F, sinYaw},
        {sinPitch * sinYaw, cosPitch, -sinPitch * cosYaw}
    };
}

// Emite los cuatro vertices de un billboard centrado en "center", construido
// sobre los ejes de pantalla para que quede siempre de frente a la camara.
inline void emitBillboard(const Vec3& center, float radius, const CameraBasis& basis) {
    const Vec3 r = basis.right * radius;
    const Vec3 u = basis.up * radius;
    const Vec3 superiorIzq = center - r + u;
    const Vec3 superiorDer = center + r + u;
    const Vec3 inferiorDer = center + r - u;
    const Vec3 inferiorIzq = center - r - u;
    glTexCoord2f(0.0F, 0.0F); glVertex3f(superiorIzq.x, superiorIzq.y, superiorIzq.z);
    glTexCoord2f(1.0F, 0.0F); glVertex3f(superiorDer.x, superiorDer.y, superiorDer.z);
    glTexCoord2f(1.0F, 1.0F); glVertex3f(inferiorDer.x, inferiorDer.y, inferiorDer.z);
    glTexCoord2f(0.0F, 1.0F); glVertex3f(inferiorIzq.x, inferiorIzq.y, inferiorIzq.z);
}

// Genera el campo de estrellas del fondo.
//
// Sin fondo, una arena circular vista de frente deja negra mas de la mitad de
// un lienzo 16:9, sobre todo las esquinas y la franja superior. El campo de
// estrellas y el degradado hacen que la escena ocupe el lienzo entero, que es
// lo que se espera de un protector de pantalla.
void generarEstrellas(std::uint32_t semilla) {
    g_estrellas.clear();
    g_estrellas.reserve(420);
    std::uint32_t estado = semilla;
    for (int indice = 0; indice < 420; ++indice) {
        Estrella estrella {};
        estrella.x = nextRandomFloat(estado);
        estrella.y = nextRandomFloat(estado);
        // El brillo sigue una potencia para que haya muchas tenues y pocas
        // brillantes, que es como se ve un cielo real.
        const float base = nextRandomFloat(estado);
        estrella.brillo = 0.18F + 0.82F * base * base * base;
        estrella.tamano = 1.0F + 2.2F * base * base;
        estrella.fase = nextRandomInRange(estado, 0.0F, 2.0F * PI);
        estrella.velocidad = nextRandomInRange(estado, 0.25F, 1.1F);
        g_estrellas.push_back(estrella);
    }
}

// Dibuja el fondo a pantalla completa: un degradado vertical mas el campo de
// estrellas. Se dibuja antes que la escena 3D, sin escribir profundidad.
void renderFondo(float time) {
    const float ancho = static_cast<float>(g_viewportWidth);
    const float alto = static_cast<float>(g_viewportHeight);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, ancho, alto, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);

    // Degradado: violeta profundo arriba, casi negro en el horizonte y un
    // rescoldo azulado abajo, donde se apoya la arena.
    glBegin(GL_QUADS);
    glColor3f(0.055F, 0.030F, 0.115F);
    glVertex2f(0.0F, 0.0F);
    glVertex2f(ancho, 0.0F);
    glColor3f(0.012F, 0.016F, 0.052F);
    glVertex2f(ancho, alto * 0.62F);
    glVertex2f(0.0F, alto * 0.62F);

    glColor3f(0.012F, 0.016F, 0.052F);
    glVertex2f(0.0F, alto * 0.62F);
    glVertex2f(ancho, alto * 0.62F);
    glColor3f(0.030F, 0.045F, 0.105F);
    glVertex2f(ancho, alto);
    glVertex2f(0.0F, alto);
    glEnd();

    // Estrellas con parpadeo suave.
    const float escala = alto / 1080.0F;
    glBegin(GL_QUADS);
    for (const Estrella& estrella : g_estrellas) {
        const float parpadeo =
            0.72F + 0.28F * std::sin(time * estrella.velocidad + estrella.fase);
        const float intensidad = estrella.brillo * parpadeo;
        const float lado = std::max(1.0F, estrella.tamano * escala);
        const float px = estrella.x * ancho;
        const float py = estrella.y * alto;
        glColor4f(intensidad * 0.85F, intensidad * 0.90F, intensidad, 1.0F);
        glVertex2f(px, py);
        glVertex2f(px + lado, py);
        glVertex2f(px + lado, py + lado);
        glVertex2f(px, py + lado);
    }
    glEnd();

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_ALPHA_TEST);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// Calcula a que distancia debe colocarse la camara para que toda la escena
// quepa en el lienzo, sea cual sea la inclinacion y la relacion de aspecto.
//
// Un calculo basado en el tamano aparente del plano central no sirve: al
// inclinar la camara, el borde cercano del anillo de sectores queda mucho mas
// proximo que el centro y se sale del frustum aunque el centro quepa de sobra.
// Por eso se muestrean puntos del cilindro que contiene la escena, se les
// aplica la rotacion de la vista y para cada uno se despeja la distancia
// minima que lo mantiene dentro: como z_ojo = z_rotado - d, la condicion
// |y_ojo| <= tan(fov/2) * (d - z_rotado) da d >= z_rotado + |y_rotado|/tan.
// La distancia final es el maximo sobre todos los puntos.
//
// Entradas: "params" dimensiones de la escena; "aspect" relacion del lienzo;
//           "yawDegrees" y "pitchDegrees" orientacion de la camara.
// Salida: distancia en unidades de mundo sobre el eje Z.
float cameraDistanceFor(const SimulationParams& params, float aspect,
                        float yawDegrees, float pitchDegrees) {
    const float tangent = std::tan(FIELD_OF_VIEW * PI / 360.0F);
    const float tangenteHorizontal = tangent * std::max(aspect, 0.1F);
    const float pitch = pitchDegrees * PI / 180.0F;
    const float cosPitch = std::cos(pitch);
    const float sinPitch = std::sin(pitch);

    const float radio = params.boardRadius * FIT_MARGIN;
    const float alturas[2] = {params.floorY() * FIT_MARGIN, params.ceilingY() * FIT_MARGIN};
    constexpr int MUESTRAS = 24;

    float distancia = 1.0F;
    for (int muestra = 0; muestra < MUESTRAS; ++muestra) {
        // El giro alrededor del eje vertical no cambia el conjunto de puntos del
        // cilindro, asi que basta con recorrer el borde una vez.
        const float angulo = 2.0F * PI * static_cast<float>(muestra) / MUESTRAS;
        const float x = std::cos(angulo) * radio;
        const float z = std::sin(angulo) * radio;
        for (const float y : alturas) {
            // Rotacion de la vista: Rx(pitch) aplicada al punto ya girado.
            const float yRotado = y * cosPitch - z * sinPitch;
            const float zRotado = y * sinPitch + z * cosPitch;
            distancia = std::max(distancia, zRotado + std::fabs(yRotado) / tangent);
            distancia = std::max(distancia, zRotado + std::fabs(x) / tangenteHorizontal);
        }
    }
    (void)yawDegrees;
    return distancia * FILL_ZOOM;
}

// Dibuja el piso de la escena como un disco oscuro con anillos concentricos.
// Los anillos dan una referencia de profundidad que hace evidente el giro.
void renderGround(const SimulationParams& params) {
    const float floorY = params.floorY();
    const float radius = params.boardRadius;
    constexpr int SEGMENTS = 72;

    glDisable(GL_TEXTURE_2D);
    // Disco relleno, como abanico de triangulos desde el centro.
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(0.045F, 0.055F, 0.130F, 1.0F);
    glVertex3f(0.0F, floorY, 0.0F);
    glColor4f(0.020F, 0.025F, 0.070F, 1.0F);
    for (int segment = 0; segment <= SEGMENTS; ++segment) {
        const float angle = 2.0F * PI * static_cast<float>(segment) / SEGMENTS;
        glVertex3f(std::cos(angle) * radius, floorY, std::sin(angle) * radius);
    }
    glEnd();

    // Anillos concentricos tenues.
    glColor4f(0.16F, 0.26F, 0.48F, 0.30F);
    for (int ring = 1; ring <= 4; ++ring) {
        const float ringRadius = radius * static_cast<float>(ring) / 4.0F;
        glBegin(GL_LINE_LOOP);
        for (int segment = 0; segment < SEGMENTS; ++segment) {
            const float angle = 2.0F * PI * static_cast<float>(segment) / SEGMENTS;
            glVertex3f(std::cos(angle) * ringRadius, floorY + 0.01F,
                       std::sin(angle) * ringRadius);
        }
        glEnd();
    }
}

// Dibuja la jaula cilindrica que contiene la escena: dos circulos y unas
// columnas verticales. Es lo que permite percibir el giro de la camara.
void renderCage(const SimulationParams& params) {
    const float radius = params.boardRadius;
    const float bottom = params.floorY();
    const float top = params.ceilingY();
    constexpr int SEGMENTS = 72;
    constexpr int COLUMNS = 12;

    (void)top;
    glDisable(GL_TEXTURE_2D);
    // Solo el aro del piso: un cilindro completo dibujaba una linea horizontal
    // en la parte alta del encuadre que distraia sin aportar profundidad.
    glColor4f(0.24F, 0.55F, 0.92F, 0.75F);
    glBegin(GL_LINE_LOOP);
    for (int segment = 0; segment < SEGMENTS; ++segment) {
        const float angle = 2.0F * PI * static_cast<float>(segment) / SEGMENTS;
        glVertex3f(std::cos(angle) * radius, bottom, std::sin(angle) * radius);
    }
    glEnd();

    // Puntales cortos hacia arriba: marcan la vertical sin cerrar la escena.
    glColor4f(0.20F, 0.42F, 0.76F, 0.35F);
    glBegin(GL_LINES);
    for (int column = 0; column < COLUMNS; ++column) {
        const float angle = 2.0F * PI * static_cast<float>(column) / COLUMNS;
        const float x = std::cos(angle) * radius;
        const float z = std::sin(angle) * radius;
        glVertex3f(x, bottom, z);
        glVertex3f(x, bottom + (top - bottom) * 0.06F, z);
    }
    glEnd();
}

// Dibuja las aristas de la piramide: lineas desde el vertice hasta el anillo
// base, mas el contorno del anillo base. Refuerzan la lectura de la forma en 3D.
void renderPyramidEdges(const std::vector<Peg>& pegs, const SimulationParams& params) {
    if (pegs.empty()) {
        return;
    }
    const Vec3 apex = pegs.front().basePosition;
    const float baseRadius = params.pyramidRadius();
    // El nivel mas bajo es el ultimo que se genero, asi que su altura es la de
    // la ultima clavija del arreglo.
    const float baseY = pegs.back().basePosition.y;
    constexpr int EDGES = 8;
    constexpr int SEGMENTS = 64;

    glDisable(GL_TEXTURE_2D);
    glColor4f(0.38F, 0.74F, 1.0F, 0.55F);
    glBegin(GL_LINES);
    for (int edge = 0; edge < EDGES; ++edge) {
        const float angle = 2.0F * PI * static_cast<float>(edge) / EDGES;
        glVertex3f(apex.x, apex.y, apex.z);
        glVertex3f(std::cos(angle) * baseRadius, baseY, std::sin(angle) * baseRadius);
    }
    glEnd();

    glColor4f(0.38F, 0.74F, 1.0F, 0.65F);
    glBegin(GL_LINE_LOOP);
    for (int segment = 0; segment < SEGMENTS; ++segment) {
        const float angle = 2.0F * PI * static_cast<float>(segment) / SEGMENTS;
        glVertex3f(std::cos(angle) * baseRadius, baseY, std::sin(angle) * baseRadius);
    }
    glEnd();
}

// Dibuja un aro que une las clavijas de cada nivel de la piramide.
//
// Sin estos aros, las clavijas se leen como una nube de esferas sueltas; con
// ellos la piramide escalonada se reconoce de inmediato, incluso cuando las
// pelotas cubren parte de la estructura.
// Entradas: "pegs" clavijas ordenadas por nivel; "time" reloj de simulacion.
void renderPyramidRings(const std::vector<Peg>& pegs, float time) {
    if (pegs.empty()) {
        return;
    }
    glDisable(GL_TEXTURE_2D);

    std::size_t inicio = 0;
    while (inicio < pegs.size()) {
        // Las clavijas se generan nivel por nivel, asi que basta con avanzar
        // mientras el nivel no cambie.
        std::size_t fin = inicio;
        while (fin < pegs.size() && pegs[fin].level == pegs[inicio].level) {
            ++fin;
        }
        const std::size_t cantidad = fin - inicio;
        if (cantidad >= 3) {
            const Vec3& color = pegs[inicio].color;
            glColor4f(color.x, color.y, color.z, 0.45F);
            glBegin(GL_LINE_LOOP);
            for (std::size_t indice = inicio; indice < fin; ++indice) {
                const Vec3 centro = pegPositionAt(pegs[indice], time);
                glVertex3f(centro.x, centro.y, centro.z);
            }
            glEnd();
        }
        inicio = fin;
    }
}

// Dibuja el histograma de las casillas como una corona de sectores alrededor
// del piso: cada sector se levanta en proporcion a las pelotas que recogio.
// Es la salida de resultados de la simulacion.
void renderBins(const SimulationParams& params, const std::vector<long long>& counts) {
    if (counts.empty()) {
        return;
    }
    // Normalizacion entre el minimo y el maximo observados, no entre cero y el
    // maximo. Motivo: por simetria de revolucion la distribucion angular es
    // uniforme, asi que con la normalizacion desde cero todas las barras se
    // pegan al tope y el histograma no muestra nada. Repartiendo el rango real
    // entre la altura minima y la maxima, las diferencias se vuelven visibles y
    // las barras crecen desde un tocon cuando la escena arranca de cero.
    // La dispersion real se despliega en el HUD para que la amplificacion no
    // induzca a error.
    const long long maximum = *std::max_element(counts.begin(), counts.end());
    const long long minimum = *std::min_element(counts.begin(), counts.end());
    const float rango = static_cast<float>(std::max(1LL, maximum - minimum));
    const float innerRadius = params.boardRadius * 0.80F;
    const float outerRadius = params.boardRadius * 0.99F;
    const float bottom = params.floorY() + 0.02F;
    const float maximumHeight = params.boardHeight * 0.10F;
    const int sectors = static_cast<int>(counts.size());
    constexpr int SUBDIVISIONS = 4;  // Segmentos por sector, para curvar el arco.

    glDisable(GL_TEXTURE_2D);
    for (int sector = 0; sector < sectors; ++sector) {
        const long long conteo = counts[static_cast<std::size_t>(sector)];
        // "fraction" ubica al sector dentro del rango observado: 0 el que menos
        // recibio, 1 el que mas.
        const float fraction = maximum > 0
            ? static_cast<float>(conteo - minimum) / rango
            : 0.0F;
        // Un tocon minimo garantiza que el anillo se vea completo desde el
        // primer cuadro, antes de que caiga ninguna pelota.
        const float height = maximumHeight * (0.16F + 0.84F * fraction);
        // El sector empieza en -PI para coincidir con el calculo de la casilla
        // que hace la fisica, que parte de atan2 desplazado.
        const float angle0 = -PI + 2.0F * PI * static_cast<float>(sector) /
                                   static_cast<float>(sectors);
        const float angle1 = -PI + 2.0F * PI * static_cast<float>(sector + 1) /
                                   static_cast<float>(sectors);
        // Se deja un hueco angular entre sectores vecinos para que se distingan.
        const float gap = (angle1 - angle0) * 0.12F;
        const float inicio = angle0 + gap;
        const float fin = angle1 - gap;

        // El color pasa de azul (poco) a magenta (mucho).
        glColor4f(0.28F + 0.68F * fraction, 0.26F - 0.14F * fraction,
                  0.92F - 0.22F * fraction, 0.88F);

        // Cara superior del sector.
        glBegin(GL_QUAD_STRIP);
        for (int step = 0; step <= SUBDIVISIONS; ++step) {
            const float t = static_cast<float>(step) / SUBDIVISIONS;
            const float angle = inicio + (fin - inicio) * t;
            const float cosA = std::cos(angle);
            const float sinA = std::sin(angle);
            glVertex3f(cosA * innerRadius, bottom + height, sinA * innerRadius);
            glVertex3f(cosA * outerRadius, bottom + height, sinA * outerRadius);
        }
        glEnd();

        // Cara exterior, la que se ve de frente al girar.
        glColor4f(0.20F + 0.58F * fraction, 0.20F - 0.10F * fraction,
                  0.78F - 0.18F * fraction, 0.92F);
        glBegin(GL_QUAD_STRIP);
        for (int step = 0; step <= SUBDIVISIONS; ++step) {
            const float t = static_cast<float>(step) / SUBDIVISIONS;
            const float angle = inicio + (fin - inicio) * t;
            const float cosA = std::cos(angle);
            const float sinA = std::sin(angle);
            glVertex3f(cosA * outerRadius, bottom, sinA * outerRadius);
            glVertex3f(cosA * outerRadius, bottom + height, sinA * outerRadius);
        }
        glEnd();
    }
}

// Dibuja las zonas modificadoras como tres anillos ortogonales, de modo que se
// lean como esferas al girar la camara.
void renderModifiers(const std::vector<Modifier>& modifiers, float time) {
    if (modifiers.empty()) {
        return;
    }
    glDisable(GL_TEXTURE_2D);
    constexpr int SEGMENTS = 26;
    for (const Modifier& modifier : modifiers) {
        // La pulsacion sinusoidal deja claro cual zona esta activa.
        const float pulse = 0.85F + 0.15F * std::sin(time * 2.4F + modifier.center.x);
        const float radius = modifier.radius * pulse;
        glColor4f(modifier.color.x, modifier.color.y, modifier.color.z, 0.26F);
        for (int plane = 0; plane < 3; ++plane) {
            glBegin(GL_LINE_LOOP);
            for (int segment = 0; segment < SEGMENTS; ++segment) {
                const float angle = 2.0F * PI * static_cast<float>(segment) / SEGMENTS;
                const float a = std::cos(angle) * radius;
                const float b = std::sin(angle) * radius;
                if (plane == 0) {
                    glVertex3f(modifier.center.x + a, modifier.center.y + b, modifier.center.z);
                } else if (plane == 1) {
                    glVertex3f(modifier.center.x + a, modifier.center.y, modifier.center.z + b);
                } else {
                    glVertex3f(modifier.center.x, modifier.center.y + a, modifier.center.z + b);
                }
            }
            glEnd();
        }
    }
}

}  // namespace

void initializeRenderer(int width, int height) {
    generarEstrellas(0x51A45u);
    g_sphereTexture = createSphereTexture();
    g_pegTexture = createPegTexture();
    g_glowTexture = createGlowTexture();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // La prueba de alfa descarta el exterior del circulo de cada sprite, de
    // modo que el buffer de profundidad sigue ordenando correctamente.
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.35F);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glClearColor(0.010F, 0.014F, 0.045F, 1.0F);
    resizeRenderer(width, height);
}

void shutdownRenderer() {
    // Liberacion explicita de los recursos de OpenGL antes de destruir el
    // contexto: es parte del manejo ordenado de objetos que pide el proyecto.
    if (g_sphereTexture != 0) {
        glDeleteTextures(1, &g_sphereTexture);
        g_sphereTexture = 0;
    }
    if (g_pegTexture != 0) {
        glDeleteTextures(1, &g_pegTexture);
        g_pegTexture = 0;
    }
    if (g_glowTexture != 0) {
        glDeleteTextures(1, &g_glowTexture);
        g_glowTexture = 0;
    }
}

void resizeRenderer(int width, int height) {
    g_viewportWidth = std::max(width, 1);
    g_viewportHeight = std::max(height, 1);
    glViewport(0, 0, g_viewportWidth, g_viewportHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    setPerspective(FIELD_OF_VIEW,
                   static_cast<float>(g_viewportWidth) / static_cast<float>(g_viewportHeight),
                   0.1F, 200.0F);
    glMatrixMode(GL_MODELVIEW);
}

void drawText(const std::string& text, float x, float y, float scale,
              float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    float cursorX = x;
    for (const char character : text) {
        if (character == ' ') {
            cursorX += (font5x7::GLYPH_WIDTH + 1) * scale;
            continue;
        }
        const std::uint8_t* glyph = font5x7::glyphFor(character);
        for (int column = 0; column < font5x7::GLYPH_WIDTH; ++column) {
            const std::uint8_t bits = glyph[column];
            for (int row = 0; row < font5x7::GLYPH_HEIGHT; ++row) {
                if ((bits & (1u << row)) == 0u) {
                    continue;
                }
                const float px = cursorX + static_cast<float>(column) * scale;
                const float py = y + static_cast<float>(row) * scale;
                glVertex2f(px, py);
                glVertex2f(px + scale, py);
                glVertex2f(px + scale, py + scale);
                glVertex2f(px, py + scale);
            }
        }
        cursorX += (font5x7::GLYPH_WIDTH + 1) * scale;
    }
    glEnd();
}

void renderScene(const Simulation& simulation, const HudInfo& hud, const CameraState& camera) {
    const SimulationParams& params = simulation.params();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderFondo(simulation.time());

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    const float aspect =
        static_cast<float>(g_viewportWidth) / static_cast<float>(g_viewportHeight);

    // Camara en orbita: se aleja, se inclina y luego gira alrededor del eje
    // vertical. El giro es puramente visual, no toca la fisica, de modo que las
    // mediciones de rendimiento siguen siendo comparables.
    glTranslatef(0.0F, 0.0F,
                 -cameraDistanceFor(params, aspect, camera.yawDegrees, camera.pitchDegrees));
    glRotatef(camera.pitchDegrees, 1.0F, 0.0F, 0.0F);
    glRotatef(camera.yawDegrees, 0.0F, 1.0F, 0.0F);

    const CameraBasis basis = cameraBasisFor(camera.yawDegrees, camera.pitchDegrees);

    renderGround(params);
    renderBins(params, simulation.binCounts());
    renderCage(params);
    renderPyramidEdges(simulation.pegs(), params);
    renderPyramidRings(simulation.pegs(), simulation.time());
    renderModifiers(simulation.modifiers(), simulation.time());

    // --- Clavijas ----------------------------------------------------------
    // Se dibujan con su propia textura, de contorno oscuro y sin halo, para que
    // nunca se confundan con las pelotas.
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_pegTexture);
    glBegin(GL_QUADS);
    for (const Peg& peg : simulation.pegs()) {
        const Vec3 center = pegPositionAt(peg, simulation.time());
        glColor4f(peg.color.x, peg.color.y, peg.color.z, 1.0F);
        emitBillboard(center, peg.radius, basis);
    }
    glEnd();

    // --- Halo aditivo de las pelotas ---------------------------------------
    const std::vector<Ball>& balls = simulation.balls();
    glDepthMask(GL_FALSE);
    glDisable(GL_ALPHA_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glBindTexture(GL_TEXTURE_2D, g_glowTexture);
    glBegin(GL_QUADS);
    for (const Ball& ball : balls) {
        if (!ball.active) {
            continue;
        }
        glColor4f(ball.color.x, ball.color.y, ball.color.z, 0.38F);
        emitBillboard(ball.position, ball.radius * 2.4F, basis);
    }
    glEnd();

    // --- Pelotas -----------------------------------------------------------
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_ALPHA_TEST);
    glDepthMask(GL_TRUE);
    glBindTexture(GL_TEXTURE_2D, g_sphereTexture);
    glBegin(GL_QUADS);
    for (const Ball& ball : balls) {
        if (!ball.active) {
            continue;
        }
        glColor4f(ball.color.x, ball.color.y, ball.color.z, 1.0F);
        emitBillboard(ball.position, ball.radius, basis);
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);

    // --- HUD en proyeccion ortografica -------------------------------------
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(g_viewportWidth),
            static_cast<double>(g_viewportHeight), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_ALPHA_TEST);

    const float scale = std::max(2.0F, static_cast<float>(g_viewportWidth) / 640.0F);
    const float lineHeight = (font5x7::GLYPH_HEIGHT + 3) * scale;
    const float marginX = 12.0F * scale * 0.5F;
    float cursorY = 10.0F * scale * 0.5F;

    // Panel semitransparente detras del texto para que el HUD sea legible
    // aunque una pelota clara pase justo por debajo. El ancho se calcula a
    // partir del avance del tipo de letra, no con un numero fijo.
    const float glyphAdvance = (font5x7::GLYPH_WIDTH + 1) * scale;
    const float panelWidth = marginX * 2.0F + glyphAdvance * 20.5F;
    const float panelHeight = lineHeight * 8.6F + marginX;
    glColor4f(0.02F, 0.03F, 0.09F, 0.72F);
    glBegin(GL_QUADS);
    glVertex2f(4.0F, 4.0F);
    glVertex2f(4.0F + panelWidth, 4.0F);
    glVertex2f(4.0F + panelWidth, 4.0F + panelHeight);
    glVertex2f(4.0F, 4.0F + panelHeight);
    glEnd();
    glColor4f(0.25F, 0.55F, 0.90F, 0.55F);
    glBegin(GL_LINE_LOOP);
    glVertex2f(4.0F, 4.0F);
    glVertex2f(4.0F + panelWidth, 4.0F);
    glVertex2f(4.0F + panelWidth, 4.0F + panelHeight);
    glVertex2f(4.0F, 4.0F + panelHeight);
    glEnd();

    // Los FPS se muestran en grande y cambian de color segun la fluidez: verde
    // sobre 60, ambar entre 30 y 60, rojo por debajo.
    const bool smooth = hud.framesPerSecond >= 58.0;
    const bool acceptable = hud.framesPerSecond >= 30.0;
    const float fpsRed = smooth ? 0.35F : (acceptable ? 1.0F : 1.0F);
    const float fpsGreen = smooth ? 1.0F : (acceptable ? 0.75F : 0.30F);
    const float fpsBlue = smooth ? 0.55F : (acceptable ? 0.20F : 0.30F);

    char buffer[160];
    std::snprintf(buffer, sizeof(buffer), "FPS %5.1f", hud.framesPerSecond);
    drawText(buffer, marginX, cursorY, scale * 1.6F, fpsRed, fpsGreen, fpsBlue, 1.0F);
    cursorY += lineHeight * 1.7F;

    std::snprintf(buffer, sizeof(buffer), "MODO %s", hud.modeName.c_str());
    drawText(buffer, marginX, cursorY, scale, 0.85F, 0.92F, 1.0F, 1.0F);
    cursorY += lineHeight;

    std::snprintf(buffer, sizeof(buffer), "N %d  HILOS %d", hud.ballCount, hud.threads);
    drawText(buffer, marginX, cursorY, scale, 0.85F, 0.92F, 1.0F, 1.0F);
    cursorY += lineHeight;

    std::snprintf(buffer, sizeof(buffer), "FISICA %6.2f MS", hud.physicsMilliseconds);
    drawText(buffer, marginX, cursorY, scale, 0.75F, 0.85F, 1.0F, 1.0F);
    cursorY += lineHeight;

    std::snprintf(buffer, sizeof(buffer), "CUADRO %6.2f MS", hud.frameMilliseconds);
    drawText(buffer, marginX, cursorY, scale, 0.75F, 0.85F, 1.0F, 1.0F);
    cursorY += lineHeight;

    if (hud.speedupEstimate > 0.0) {
        std::snprintf(buffer, sizeof(buffer), "SPEEDUP %4.2fX", hud.speedupEstimate);
    } else {
        std::snprintf(buffer, sizeof(buffer), "SPEEDUP  --");
    }
    drawText(buffer, marginX, cursorY, scale, 1.0F, 0.85F, 0.45F, 1.0F);
    cursorY += lineHeight;

    std::snprintf(buffer, sizeof(buffer), "RECICLADAS %lld", hud.recycled);
    drawText(buffer, marginX, cursorY, scale, 0.65F, 0.78F, 0.95F, 1.0F);
    cursorY += lineHeight;

    // La altura de las barras esta amplificada para que las diferencias se
    // vean; esta cifra dice cuanta variacion real hay entre sectores.
    std::snprintf(buffer, sizeof(buffer), "DISPERSION %4.1f%%", hud.sectorSpread);
    drawText(buffer, marginX, cursorY, scale, 0.65F, 0.78F, 0.95F, 1.0F);

    // Ayuda de teclado en la esquina inferior izquierda.
    drawText("0-3 MODO   ESPACIO ROTA   +/- HILOS   , . GIRO   F PANTALLA   R REINICIA   ESC SALE",
             marginX, static_cast<float>(g_viewportHeight) - lineHeight - 6.0F,
             scale * 0.75F, 0.55F, 0.68F, 0.85F, 0.95F);

    // Aviso temporal (por ejemplo, cuando un modo no esta disponible).
    if (!hud.notice.empty()) {
        drawText(hud.notice, marginX,
                 static_cast<float>(g_viewportHeight) - lineHeight * 2.2F - 6.0F,
                 scale * 0.85F, 1.0F, 0.55F, 0.45F, 1.0F);
    }

    glEnable(GL_ALPHA_TEST);
    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}
