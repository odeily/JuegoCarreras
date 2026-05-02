# Makefile para el Simulador de Carreras OpenGL

CC = g++
IMGUI_DIR = imgui
IMGUI_SOURCES = $(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_draw.cpp $(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/imgui_widgets.cpp
IMGUI_BACKENDS = $(IMGUI_DIR)/backends/imgui_impl_glfw.cpp $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp

CFLAGS = -Wall -I. -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends
LDFLAGS = -lglfw -lGL -lX11 -lpthread -lXrandr -lXi -ldl

TARGET = simulador

all: $(TARGET)

$(TARGET): main.cpp glad/glad.c $(IMGUI_SOURCES) $(IMGUI_BACKENDS)
	$(CC) $(CFLAGS) main.cpp glad/glad.c $(IMGUI_SOURCES) $(IMGUI_BACKENDS) -o $(TARGET) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
