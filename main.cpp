#define STB_IMAGE_IMPLEMENTATION
#include "BibliotecasCurso/stb_image.h"
#include <glad/glad.h>
#include <glfw/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <cmath>

#include "BibliotecasCurso/esfera.h"
#include "BibliotecasCurso/lecturaShader_0_9.h"
#include "geometria_coche.h"

// ImGui
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

void ajuste_ventana(GLFWwindow* ventana, int ancho, int alto) {
    glViewport(0, 0, ancho, alto);
}

// --- ESTRUCTURAS DE ESTADO ---
struct Coche {
    float posX = 150.0f, posY = 0.5f, posZ = -sin(150.0f * 0.05f) * 20.0f; 
    float anguloChasis = 0.0f;
    float velocidad = 0.0f;
    float giroVolante = 0.0f;
    float rodadura = 0.0f;

    int marcha = 1; // 1 to 6
    bool frenando = false;
    int vueltas = 0;
    bool cruzandoMetaAnterior = true; // Empezamos EN la meta, así que la estamos cruzando inicialmente
    float nitroCooldown = 0.0f;
    float nitroActivo = 0.0f;
};

struct Obstaculo {
    float x, z;
    float radio; // Bounding sphere radius for simple collision
};

// --- GLOBALES ---
Coche miCoche;
std::vector<Obstaculo> obstaculos;
bool farosEncendidos = false;
bool esDeNoche = false;
bool haGanado = false;
float tiempoCarrera = 0.0f;
int vistaSeleccionada = 2; // 1: Cabina, 2: Trasera, 3: Cenital

// Variables Graficas
unsigned int VAO_Cubo, VBO_Cubo;
unsigned int vaoEsfera, vboEsfera;
unsigned int VAO_Cilindro, VBO_Cilindro;
int numVerticesCilindro = 0;
unsigned int texMetal, texCristal, texRueda, texHierba, texAsfalto, texCieloDia, texCieloNoche, texMeta;

// Limites de velocidad por marcha
float limitesMarchas[6] = { 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f };

// --- FUNCIONES MATEMATICAS ---
float getDistanciaOval(float x, float z) {
    // Dividimos X entre 1.6 para estirarlo horizontalmente
    float ovalX = x / 1.6f;
    // Añadimos una onda sinusoidal a Z para crear chicanes y curvas complejas
    float waveZ = z + sin(x * 0.05f) * 20.0f;
    return sqrt(ovalX * ovalX + waveZ * waveZ);
}

// --- FUNCIONES GRAFICAS ---

unsigned int cargarTextura(const char* ruta) {
    unsigned int textura;
    glGenTextures(1, &textura);
    glBindTexture(GL_TEXTURE_2D, textura);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    int width, height, nrChannels;
    unsigned char* data = stbi_load(ruta, &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    } else {
        std::cout << "Fallo al cargar textura: " << ruta << std::endl;
    }
    stbi_image_free(data);
    return textura;
}

void inicializarObstaculos() {
    obstaculos.clear();
    // Crear muros contiguos alrededor de todo el circuito sinuoso
    // Ampliamos el radio virtual de los muros para dejar una zona amplia de hierba
    for (float angle = 0; angle < 360; angle += 1.0f) { 
        float rad = glm::radians(angle);
        
        // Muro exterior (radio 140 virtual)
        float ext_x = (140.0f * cos(rad)) * 1.6f;
        float ext_z = 140.0f * sin(rad) - sin(ext_x * 0.05f) * 20.0f;
        obstaculos.push_back({ext_x, ext_z, 2.0f});
        
        // Muro interior (radio 60 virtual)
        float int_x = (60.0f * cos(rad)) * 1.6f;
        float int_z = 60.0f * sin(rad) - sin(int_x * 0.05f) * 20.0f;
        obstaculos.push_back({int_x, int_z, 2.0f});
    }
}

void reiniciarJuego() {
    miCoche.posX = 150.0f;
    miCoche.posY = 0.5f;
    miCoche.posZ = -sin(150.0f * 0.05f) * 20.0f;
    miCoche.anguloChasis = 0.0f; 
    miCoche.velocidad = 0.0f;
    miCoche.giroVolante = 0.0f;
    miCoche.marcha = 1;
    miCoche.vueltas = 0;
    miCoche.cruzandoMetaAnterior = true; // Evitar vuelta fantasma en el frame 1
    miCoche.nitroCooldown = 0.0f;
    miCoche.nitroActivo = 0.0f;
    haGanado = false;
    tiempoCarrera = 0.0f;
    vistaSeleccionada = 2;
    std::cout << "--- JUEGO REINICIADO ---" << std::endl;
}

void procesarInput(GLFWwindow* ventana, float dt) {
    if (glfwGetKey(ventana, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(ventana, true);
        
    if (glfwGetKey(ventana, GLFW_KEY_P) == GLFW_PRESS) {
        reiniciarJuego();
        return;
    }

    miCoche.frenando = false;

    // Distancia al centro usando fórmula del óvalo para saber si estamos en asfalto
    float distOval = getDistanciaOval(miCoche.posX, miCoche.posZ);
    bool fueraDePista = (distOval < 80.0f || distOval > 120.0f);
    
    float velocidadMaxPermitida = limitesMarchas[miCoche.marcha - 1];
    
    // Si el nitro está activo, le permitimos saltarse las marchas
    if (miCoche.nitroActivo > 0.0f) {
        velocidadMaxPermitida = 100.0f;
    }
    
    if (fueraDePista) {
        velocidadMaxPermitida = 8.0f; // Castigo por salirse de pista
        if (miCoche.nitroActivo > 0.0f) {
            velocidadMaxPermitida = 20.0f; // Con nitro puedes ir a 20 en hierba
        }
        
        // Deceleración inmediata si entramos en la hierba muy rápido
        if (miCoche.velocidad > velocidadMaxPermitida) {
            miCoche.velocidad = velocidadMaxPermitida;
        }
    }
    // Sistema de Nitro
    if (glfwGetKey(ventana, GLFW_KEY_N) == GLFW_PRESS) {
        if (miCoche.nitroCooldown <= 0.0f && miCoche.nitroActivo <= 0.0f) {
            miCoche.nitroActivo = 1.5f;
            miCoche.nitroCooldown = 15.0f;
            miCoche.velocidad *= 1.5f; // Acelerón del 50%
        }
    }

    // Acelerador W
    if (glfwGetKey(ventana, GLFW_KEY_W) == GLFW_PRESS) {
        miCoche.velocidad += 15.0f * dt; // Aceleracion
        if (miCoche.velocidad > velocidadMaxPermitida) {
            miCoche.velocidad = velocidadMaxPermitida; // Capar inmediatamente a la vel. maxima actual
        }
    } 
    // Freno S
    else if (glfwGetKey(ventana, GLFW_KEY_S) == GLFW_PRESS) {
        miCoche.frenando = true;
        miCoche.velocidad -= 30.0f * dt; // Freno fuerte
        if (miCoche.velocidad < -10.0f) miCoche.velocidad = -10.0f; // Marcha atras capada
        
        // Reduccion automatica de marchas
        if (miCoche.velocidad > 0) {
            for (int i = 5; i >= 0; i--) {
                if (miCoche.velocidad <= limitesMarchas[i]) {
                    miCoche.marcha = i + 1; // Asignar marcha correspondiente
                }
            }
        }
    } 
    // Friccion (sin pulsar nada)
    else {
        if (miCoche.velocidad > 0.0f) {
            miCoche.velocidad -= 5.0f * dt;
            if (miCoche.velocidad < 0.0f) miCoche.velocidad = 0.0f;
        } else if (miCoche.velocidad < 0.0f) {
            miCoche.velocidad += 5.0f * dt;
            if (miCoche.velocidad > 0.0f) miCoche.velocidad = 0.0f;
        }
    }

    // Cambio de marchas manual
    if (glfwGetKey(ventana, GLFW_KEY_1) == GLFW_PRESS) miCoche.marcha = 1;
    if (glfwGetKey(ventana, GLFW_KEY_2) == GLFW_PRESS) miCoche.marcha = 2;
    if (glfwGetKey(ventana, GLFW_KEY_3) == GLFW_PRESS) miCoche.marcha = 3;
    if (glfwGetKey(ventana, GLFW_KEY_4) == GLFW_PRESS) miCoche.marcha = 4;
    if (glfwGetKey(ventana, GLFW_KEY_5) == GLFW_PRESS) miCoche.marcha = 5;
    if (glfwGetKey(ventana, GLFW_KEY_6) == GLFW_PRESS) miCoche.marcha = 6;

    // Volante
    bool giroIzq = glfwGetKey(ventana, GLFW_KEY_A) == GLFW_PRESS;
    bool giroDer = glfwGetKey(ventana, GLFW_KEY_D) == GLFW_PRESS;

    if (giroIzq) miCoche.giroVolante += 90.0f * dt;
    if (giroDer) miCoche.giroVolante -= 90.0f * dt;

    // Retorno automatico direccion
    if (!giroIzq && !giroDer) {
        float retorno = 150.0f * dt;
        if (miCoche.giroVolante > 0.0f) {
            miCoche.giroVolante -= retorno;
            if (miCoche.giroVolante < 0.0f) miCoche.giroVolante = 0.0f;
        } else if (miCoche.giroVolante < 0.0f) {
            miCoche.giroVolante += retorno;
            if (miCoche.giroVolante > 0.0f) miCoche.giroVolante = 0.0f;
        }
    }
    miCoche.giroVolante = glm::clamp(miCoche.giroVolante, -40.0f, 40.0f);

    // Faros y Ciclo de Dia (Toggle)
    static bool fPulsadoAntes = false;
    bool fPulsado = glfwGetKey(ventana, GLFW_KEY_F) == GLFW_PRESS;
    if (fPulsado && !fPulsadoAntes) farosEncendidos = !farosEncendidos;
    fPulsadoAntes = fPulsado;

    static bool tPulsadoAntes = false;
    bool tPulsado = glfwGetKey(ventana, GLFW_KEY_T) == GLFW_PRESS;
    if (tPulsado && !tPulsadoAntes) esDeNoche = !esDeNoche;
    tPulsadoAntes = tPulsado;

    // Control de cámaras
    if (glfwGetKey(ventana, GLFW_KEY_7) == GLFW_PRESS) vistaSeleccionada = 1;
    if (glfwGetKey(ventana, GLFW_KEY_8) == GLFW_PRESS) vistaSeleccionada = 2;
    if (glfwGetKey(ventana, GLFW_KEY_9) == GLFW_PRESS) vistaSeleccionada = 3;
}

void actualizarFisicas(float dt) {
    if (!haGanado) {
        tiempoCarrera += dt;
    }
    
    // Temporizadores del Nitro
    if (miCoche.nitroActivo > 0.0f) miCoche.nitroActivo -= dt;
    if (miCoche.nitroCooldown > 0.0f) miCoche.nitroCooldown -= dt;

    // Calculo cinematico de movimiento
    miCoche.anguloChasis += miCoche.giroVolante * (miCoche.velocidad * 0.05f) * dt;
    
    float futureX = miCoche.posX + sin(glm::radians(miCoche.anguloChasis)) * miCoche.velocidad * dt;
    float futureZ = miCoche.posZ + cos(glm::radians(miCoche.anguloChasis)) * miCoche.velocidad * dt;

    // Deteccion de Colisiones con obstaculos (Bounding sphere)
    bool colision = false;
    for (const auto& obs : obstaculos) {
        float dx = futureX - obs.x;
        float dz = futureZ - obs.z;
        float dist = sqrt(dx*dx + dz*dz);
        if (dist < (obs.radio + 1.2f)) { // 1.2f is car approx radius
            colision = true;
            break;
        }
    }

    if (!colision) {
        miCoche.posX = futureX;
        miCoche.posZ = futureZ;
    } else {
        miCoche.velocidad *= -0.5f; // Rebote simple
    }

    miCoche.rodadura += miCoche.velocidad * dt * 3.0f;

    // Deteccion de Meta 
    // Usamos Z puramente mundial para que la franja sea perfectamente recta horizontal
    float distOval = getDistanciaOval(miCoche.posX, miCoche.posZ);
    bool cruzandoMetaAhora = (distOval >= 80.0f && distOval <= 120.0f && miCoche.posX > 130.0f && miCoche.posZ >= -23.0f && miCoche.posZ <= -15.0f);
    
    if (cruzandoMetaAhora && !miCoche.cruzandoMetaAnterior) {
        // Para evitar bugs si va marcha atras, comprobamos que avanza en Z virtual (relativo a la curva)
        // Simplificado comprobando si el coche mira aprox hacia donde debe (anguloChasis cerca de 0)
        if (cos(glm::radians(miCoche.anguloChasis)) > -0.5f) {
            miCoche.vueltas++;
            std::cout << "¡Vuelta Completada! Vueltas totales: " << miCoche.vueltas << std::endl;
            
            if (miCoche.vueltas >= 2 && !haGanado) {
                haGanado = true;
                std::cout << "\n============================================\n";
                std::cout << "¡¡HAS GANADO!!\n";
                std::cout << "Tiempo de carrera: " << tiempoCarrera << " segundos.\n";
                std::cout << "Pulsa 'P' para reiniciar.\n";
                std::cout << "============================================\n\n";
                vistaSeleccionada = 3; // Cambiar a cámara cenital
            }
        }
    }
    miCoche.cruzandoMetaAnterior = cruzandoMetaAhora;
}

// Dibuja una pieza de geometría (Cubo o Cilindro)
void dibujarPieza(unsigned int shader, glm::mat4 mat, glm::vec3 col, unsigned int texID = 0, float alphaVal = 1.0f, bool esEmisivo = false, GLuint vao = VAO_Cubo, int vertCount = 36) {
    glUniformMatrix4fv(glGetUniformLocation(shader, "modelo"), 1, GL_FALSE, glm::value_ptr(mat));
    glUniform3fv(glGetUniformLocation(shader, "colorObjeto"), 1, glm::value_ptr(col));
    glUniform1i(glGetUniformLocation(shader, "usarTextura"), texID > 0);
    glUniform1f(glGetUniformLocation(shader, "alphaObj"), alphaVal);
    glUniform1i(glGetUniformLocation(shader, "esEmisivo"), esEmisivo);

    if (texID > 0) {
        glBindTexture(GL_TEXTURE_2D, texID);
    }
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, vertCount);
}

void dibujarCoche(unsigned int shader) {
    glm::mat4 mBase = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(miCoche.posX, miCoche.posY, miCoche.posZ)), glm::radians(miCoche.anguloChasis), glm::vec3(0,1,0));

    // Chasis (Rojo Competición)
    dibujarPieza(shader, glm::scale(mBase, glm::vec3(2.0f, 0.4f, 4.0f)), glm::vec3(0.9f, 0.1f, 0.1f), texMetal);
    
    // Franja blanca deportiva (arriba)
    dibujarPieza(shader, glm::scale(glm::translate(mBase, glm::vec3(0.0f, 0.22f, 0.0f)), glm::vec3(0.6f, 0.4f, 4.02f)), glm::vec3(0.9f, 0.9f, 0.9f));
    
    // Cabina
    dibujarPieza(shader, glm::scale(glm::translate(mBase, glm::vec3(0.0f, 0.6f, 0.5f)), glm::vec3(1.6f, 0.6f, 1.8f)), glm::vec3(0.8f, 0.9f, 1.0f), texCristal, 0.6f);

    // Alerón trasero
    glm::mat4 mAleronPilar1 = glm::scale(glm::translate(mBase, glm::vec3(-0.7f, 0.5f, -1.8f)), glm::vec3(0.1f, 0.5f, 0.2f));
    glm::mat4 mAleronPilar2 = glm::scale(glm::translate(mBase, glm::vec3(0.7f, 0.5f, -1.8f)), glm::vec3(0.1f, 0.5f, 0.2f));
    glm::mat4 mAleronAla = glm::scale(glm::translate(mBase, glm::vec3(0.0f, 0.75f, -1.9f)), glm::vec3(2.2f, 0.1f, 0.6f));
    dibujarPieza(shader, mAleronPilar1, glm::vec3(0.1f, 0.1f, 0.1f));
    dibujarPieza(shader, mAleronPilar2, glm::vec3(0.1f, 0.1f, 0.1f));
    dibujarPieza(shader, mAleronAla, glm::vec3(0.1f, 0.1f, 0.1f)); // Alerón negro de carbono

    // Luces de Freno (Atrás)
    bool lucesEncendidas = miCoche.frenando;
    dibujarPieza(shader, glm::scale(glm::translate(mBase, glm::vec3(-0.7f, 0.1f, -2.05f)), glm::vec3(0.4f, 0.2f, 0.1f)), glm::vec3(1.0f, 0.1f, 0.1f), 0, 1.0f, lucesEncendidas);
    dibujarPieza(shader, glm::scale(glm::translate(mBase, glm::vec3( 0.7f, 0.1f, -2.05f)), glm::vec3(0.4f, 0.2f, 0.1f)), glm::vec3(1.0f, 0.1f, 0.1f), 0, 1.0f, lucesEncendidas);

    // Llamas de Nitro
    if (miCoche.nitroActivo > 0.0f) {
        float flameScale = 1.0f + sin(glfwGetTime() * 40.0f) * 0.2f; 
        glm::mat4 mFuego1 = glm::scale(glm::translate(mBase, glm::vec3(-0.4f, -0.1f, -2.3f)), glm::vec3(0.3f*flameScale, 0.3f*flameScale, 0.8f*flameScale));
        glm::mat4 mFuego2 = glm::scale(glm::translate(mBase, glm::vec3(0.4f, -0.1f, -2.3f)), glm::vec3(0.3f*flameScale, 0.3f*flameScale, 0.8f*flameScale));
        dibujarPieza(shader, mFuego1, glm::vec3(0.1f, 0.6f, 1.0f), 0, 1.0f, true); // Llama azul claro emisiva
        dibujarPieza(shader, mFuego2, glm::vec3(0.1f, 0.6f, 1.0f), 0, 1.0f, true);
    }

    // Ruedas redondas (Cilindros)
    float matrizRuedas[4][2] = {{1.1f, 1.5f}, {-1.1f, 1.5f}, {1.1f, -1.5f}, {-1.1f, -1.5f}};
    for(int i = 0; i < 4; i++) {
        glm::mat4 mRueda = glm::translate(mBase, glm::vec3(matrizRuedas[i][0], -0.2f, matrizRuedas[i][1]));
        if(i < 2) mRueda = glm::rotate(mRueda, glm::radians(miCoche.giroVolante), glm::vec3(0,1,0)); // Direccion delantera
        mRueda = glm::rotate(mRueda, miCoche.rodadura, glm::vec3(1,0,0)); // Rodadura
        
        // Rotar el cilindro 90 grados en Z para que el eje central Y pase a ser X
        mRueda = glm::rotate(mRueda, glm::radians(90.0f), glm::vec3(0,0,1));
        
        dibujarPieza(shader, glm::scale(mRueda, glm::vec3(0.8f, 0.4f, 0.8f)), glm::vec3(1,1,1), texRueda, 1.0f, false, VAO_Cilindro, numVerticesCilindro);
    }
}

void dibujarEntorno(unsigned int shader) {
    // Dibujamos el suelo con piezas instanciadas más pequeñas (escala 3.0f)
    // Ampliamos el rango del bucle para asegurar que el terreno de hierba llega hasta los muros exteriores
    for (int x = -100; x <= 100; x++) {
        for (int z = -65; z <= 65; z++) {
            float worldX = x * 3.0f;
            float worldZ = z * 3.0f;
            float distOval = getDistanciaOval(worldX, worldZ);
            
            bool esPista = (distOval >= 80.0f && distOval <= 120.0f);
            // Dibujar la meta perfectamente recta basándose puramente en Z
            bool esMeta = (esPista && worldX > 130.0f && worldZ >= -23.0f && worldZ <= -15.0f);
            
            unsigned int texSueloActual = esPista ? texAsfalto : texHierba;
            if (esMeta) texSueloActual = texMeta;

            glm::mat4 mSuelo = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(worldX, -0.3f, worldZ)), glm::vec3(3.0f, 0.1f, 3.0f));
            dibujarPieza(shader, mSuelo, glm::vec3(1.0f, 1.0f, 1.0f), texSueloActual);
        }
    }

    // Dibujar muros
    for (const auto& obs : obstaculos) {
        // Los muros se dibujan estirados hacia arriba y entrelazados para formar una barrera continua
        glm::mat4 mObs = glm::translate(glm::mat4(1.0f), glm::vec3(obs.x, 0.5f, obs.z));
        mObs = glm::scale(mObs, glm::vec3(obs.radio*1.8f, 2.0f, obs.radio*1.8f));
        dibujarPieza(shader, mObs, glm::vec3(1.0f, 1.0f, 1.0f), texMetal); // Barrera de metal
    }
}

void generarCilindro(int segmentos) {
    std::vector<float> vertices;
    float radio = 0.5f;
    float alto = 0.5f;

    // Tapa arriba (y = alto)
    for (int i = 0; i < segmentos; i++) {
        float t1 = (float)i / segmentos * 2.0f * M_PI;
        float t2 = (float)(i + 1) / segmentos * 2.0f * M_PI;
        vertices.insert(vertices.end(), {0.0f, alto, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f});
        vertices.insert(vertices.end(), {radio * cos(t1), alto, radio * sin(t1), 0.0f, 1.0f, 0.0f, 0.5f + 0.5f*cos(t1), 0.5f + 0.5f*sin(t1)});
        vertices.insert(vertices.end(), {radio * cos(t2), alto, radio * sin(t2), 0.0f, 1.0f, 0.0f, 0.5f + 0.5f*cos(t2), 0.5f + 0.5f*sin(t2)});
    }
    
    // Tapa abajo (y = -alto)
    for (int i = 0; i < segmentos; i++) {
        float t1 = (float)i / segmentos * 2.0f * M_PI;
        float t2 = (float)(i + 1) / segmentos * 2.0f * M_PI;
        vertices.insert(vertices.end(), {0.0f, -alto, 0.0f, 0.0f, -1.0f, 0.0f, 0.5f, 0.5f});
        vertices.insert(vertices.end(), {radio * cos(t2), -alto, radio * sin(t2), 0.0f, -1.0f, 0.0f, 0.5f + 0.5f*cos(t2), 0.5f + 0.5f*sin(t2)});
        vertices.insert(vertices.end(), {radio * cos(t1), -alto, radio * sin(t1), 0.0f, -1.0f, 0.0f, 0.5f + 0.5f*cos(t1), 0.5f + 0.5f*sin(t1)});
    }

    // Lados
    for (int i = 0; i < segmentos; i++) {
        float t1 = (float)i / segmentos * 2.0f * M_PI;
        float t2 = (float)(i + 1) / segmentos * 2.0f * M_PI;
        float x1 = radio * cos(t1), z1 = radio * sin(t1);
        float x2 = radio * cos(t2), z2 = radio * sin(t2);
        float u1 = (float)i / segmentos;
        float u2 = (float)(i + 1) / segmentos;

        vertices.insert(vertices.end(), {x1, alto, z1, cos(t1), 0.0f, sin(t1), u1, 1.0f});
        vertices.insert(vertices.end(), {x1, -alto, z1, cos(t1), 0.0f, sin(t1), u1, 0.0f});
        vertices.insert(vertices.end(), {x2, -alto, z2, cos(t2), 0.0f, sin(t2), u2, 0.0f});
        
        vertices.insert(vertices.end(), {x1, alto, z1, cos(t1), 0.0f, sin(t1), u1, 1.0f});
        vertices.insert(vertices.end(), {x2, -alto, z2, cos(t2), 0.0f, sin(t2), u2, 0.0f});
        vertices.insert(vertices.end(), {x2, alto, z2, cos(t2), 0.0f, sin(t2), u2, 1.0f});
    }
    
    numVerticesCilindro = vertices.size() / 8;
    
    glGenVertexArrays(1, &VAO_Cilindro);
    glGenBuffers(1, &VBO_Cilindro);
    glBindVertexArray(VAO_Cilindro);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_Cilindro);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(3*sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(6*sizeof(float))); glEnableVertexAttribArray(2);
}

int main() {
    glfwInit();
    GLFWwindow* ventana = glfwCreateWindow(1024, 768, "Simulador Carreras - Coche", NULL, NULL);
    glfwMakeContextCurrent(ventana);
    glfwSetFramebufferSizeCallback(ventana, ajuste_ventana);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(ventana, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    unsigned int shader = setShaders("shader.vert", "shader.frag");
    glUseProgram(shader);
    glUniform1i(glGetUniformLocation(shader, "textura"), 0);

    // Buffers Cubo (geometria_coche.h)
    glGenVertexArrays(1, &VAO_Cubo);
    glGenBuffers(1, &VBO_Cubo);
    glBindVertexArray(VAO_Cubo);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_Cubo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(baseVerticesCubo), baseVerticesCubo, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(3*sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(6*sizeof(float))); glEnableVertexAttribArray(2);

    // Buffers Esfera (para SkyDome)
    glGenVertexArrays(1, &vaoEsfera);
    glGenBuffers(1, &vboEsfera);
    glBindVertexArray(vaoEsfera);
    glBindBuffer(GL_ARRAY_BUFFER, vboEsfera);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices_esfera), vertices_esfera, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(5*sizeof(float))); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)0); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(3*sizeof(float))); glEnableVertexAttribArray(2);

    // Buffer Cilindro (Ruedas redondas)
    generarCilindro(24);

    // Texturas
    texMetal = cargarTextura("Texturas/Metal.jpg");
    texCristal = cargarTextura("Texturas/Cristal.jpg");
    texRueda = cargarTextura("Texturas/Rueda.jpg");
    texHierba = cargarTextura("Texturas/Hierba.png");
    texAsfalto = cargarTextura("Texturas/Asfalto.jpg");
    texCieloDia = cargarTextura("Texturas/Cielo.jpg");
    texCieloNoche = cargarTextura("Texturas/CieloNoche.png"); 
    texMeta = cargarTextura("Texturas/Meta.jpg");

    inicializarObstaculos();

    float ultimoFrame = 0.0f;

    while (!glfwWindowShouldClose(ventana)) {
        float frameActual = glfwGetTime();
        float dt = frameActual - ultimoFrame;
        ultimoFrame = frameActual;

        // ImGui New Frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        static float terminalTimer = 0.0f;
        terminalTimer += dt;
        if (terminalTimer >= 1.0f) {
            std::cout << "[HUD] Tiempo: " << tiempoCarrera << "s | Vel: " << miCoche.velocidad << " | Marcha: " << miCoche.marcha << " | Vuelta: " << miCoche.vueltas << "/2\r" << std::flush;
            terminalTimer = 0.0f;
        }

        procesarInput(ventana, dt);
        actualizarFisicas(dt);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shader);

        // Camara Perspectiva
        glm::mat4 proyeccion = glm::perspective(glm::radians(60.0f), 1024.0f/768.0f, 0.1f, 300.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader, "proyeccion"), 1, GL_FALSE, glm::value_ptr(proyeccion));

        float radChasis = glm::radians(miCoche.anguloChasis);
        glm::vec3 posCoche(miCoche.posX, miCoche.posY, miCoche.posZ);
        glm::vec3 adelante(sin(radChasis), 0.0f, cos(radChasis));
        
        glm::mat4 vista;
        glm::vec3 posCamara;
        
        if (vistaSeleccionada == 1) {
            // Cámara Cabina (1ª Persona)
            posCamara = posCoche + glm::vec3(0.0f, 0.9f, 0.0f) + adelante * 0.5f;
            vista = glm::lookAt(posCamara, posCoche + adelante * 15.0f, glm::vec3(0,1,0));
        } else if (vistaSeleccionada == 2) {
            // Cámara Trasera (3ª Persona)
            posCamara = posCoche - adelante * 10.0f + glm::vec3(0, 4.0f, 0);
            vista = glm::lookAt(posCamara, posCoche, glm::vec3(0,1,0));
        } else {
            // Cámara Cenital Fija/Rastreadora
            posCamara = posCoche + glm::vec3(0.1f, 40.0f, 0.0f);
            vista = glm::lookAt(posCamara, posCoche, glm::vec3(0,1,0));
        }
        
        glUniformMatrix4fv(glGetUniformLocation(shader, "vista"), 1, GL_FALSE, glm::value_ptr(vista));
        glUniform3fv(glGetUniformLocation(shader, "posVista"), 1, glm::value_ptr(posCamara));

        // Setup Iluminacion Global y Faros
        glm::vec3 luzAmbiental = esDeNoche ? glm::vec3(0.15f, 0.2f, 0.25f) : glm::vec3(0.85f, 0.85f, 0.85f);
        glUniform3fv(glGetUniformLocation(shader, "luzAmbiente"), 1, glm::value_ptr(luzAmbiental));
        glUniform1i(glGetUniformLocation(shader, "farosEncendidos"), farosEncendidos);

        glm::vec3 derecha(cos(radChasis), 0.0f, -sin(radChasis));
        glm::vec3 posFaroIzq = posCoche + adelante*2.0f - derecha*0.8f + glm::vec3(0,0.2f,0);
        glm::vec3 posFaroDer = posCoche + adelante*2.0f + derecha*0.8f + glm::vec3(0,0.2f,0);
        glm::vec3 dirFaros = glm::normalize(adelante + glm::vec3(0, -0.2f, 0));

        glUniform3fv(glGetUniformLocation(shader, "posLuzFaroIzq"), 1, glm::value_ptr(posFaroIzq));
        glUniform3fv(glGetUniformLocation(shader, "posLuzFaroDer"), 1, glm::value_ptr(posFaroDer));
        glUniform3fv(glGetUniformLocation(shader, "dirFaros"), 1, glm::value_ptr(dirFaros));

        // Dibujar SkyDome
        unsigned int texCieloActual = esDeNoche ? texCieloNoche : texCieloDia;
        glm::mat4 mDomo = glm::scale(glm::translate(glm::mat4(1.0f), posCamara), glm::vec3(100.0f));
        glUniformMatrix4fv(glGetUniformLocation(shader, "modelo"), 1, GL_FALSE, glm::value_ptr(mDomo));
        glUniform1i(glGetUniformLocation(shader, "esSkyDome"), true);
        glUniform1i(glGetUniformLocation(shader, "usarTextura"), true);
        glDepthMask(GL_FALSE);
        glBindTexture(GL_TEXTURE_2D, texCieloActual);
        glBindVertexArray(vaoEsfera);
        glDrawArrays(GL_TRIANGLES, 0, 1080);
        glDepthMask(GL_TRUE);
        glUniform1i(glGetUniformLocation(shader, "esSkyDome"), false);

        // Dibujar Escena
        dibujarEntorno(shader);
        dibujarCoche(shader);

        // Render HUD (ImGui)
        if (haGanado) {
            ImGui::SetNextWindowPos(ImVec2(512.0f, 384.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::Begin("Fin de Partida", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
            ImGui::Text("¡¡HAS GANADO!!");
            ImGui::Text("Tiempo de carrera: %.3f segundos.", tiempoCarrera);
            ImGui::Text("Pulsa 'P' para reiniciar.");
            ImGui::End();
        } else {
            ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Once);
            ImGui::Begin("HUD", NULL, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("Velocidad: %.1f km/h", miCoche.velocidad);
            ImGui::Text("Marcha: %d", miCoche.marcha);
            ImGui::Text("Tiempo: %.1f s", tiempoCarrera);
            ImGui::Text("Vueltas: %d / 2", miCoche.vueltas);
            
            ImGui::Separator();
            if (miCoche.nitroCooldown <= 0.0f) {
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Nitro: LISTO (Pulsa N)");
            } else {
                float progreso = 1.0f - (miCoche.nitroCooldown / 15.0f);
                ImGui::Text("Nitro recargando:");
                ImGui::ProgressBar(progreso, ImVec2(150.0f, 0.0f));
            }
            ImGui::End();
        }
        
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(ventana);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}
