#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out vec3 Normal;
out vec3 PosFrag;
out vec2 TexCoord;

uniform mat4 modelo;
uniform mat4 vista;
uniform mat4 proyeccion;

void main() {
    PosFrag  = vec3(modelo * vec4(aPos, 1.0));
    Normal   = mat3(transpose(inverse(modelo))) * aNormal;
    TexCoord = aTexCoord;
    gl_Position = proyeccion * vista * vec4(PosFrag, 1.0);
}
