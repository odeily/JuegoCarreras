#version 330 core
out vec4 FragColor;

in vec3 Normal;
in vec3 PosFrag;
in vec2 TexCoord;

uniform sampler2D textura;
uniform bool  usarTextura;
uniform float alphaObj;
uniform vec3  colorObjeto;

uniform bool  esSkyDome;
uniform bool  esEmisivo;

// Luces
uniform vec3 posVista;
uniform bool farosEncendidos;
uniform vec3 posLuzFaroIzq;
uniform vec3 posLuzFaroDer;
uniform vec3 dirFaros;
uniform vec3 luzAmbiente;

vec3 calcularFocoPhong(vec3 posLuz, vec3 dirFoco, float cutoff, float outerCutoff, vec3 colorLuz, vec3 norm, vec3 dirVista, vec3 colorBase) {
    vec3 dirLuz     = normalize(posLuz - PosFrag);
    float theta     = dot(dirLuz, normalize(-dirFoco));
    
    float epsilon   = cutoff - outerCutoff;
    float intensidad = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);
    
    float diff      = max(dot(norm, dirLuz), 0.0);
    vec3 difusa     = diff * colorLuz;

    float ks        = 0.4;
    vec3 dirReflejo = reflect(-dirLuz, norm);
    float spec      = pow(max(dot(dirVista, dirReflejo), 0.0), 32);
    vec3 especular  = ks * spec * colorLuz;

    float distancia = length(posLuz - PosFrag);
    // Atenuación mucho más débil para que la luz viaje más lejos
    float atenuacion = 1.0 / (1.0 + 0.01 * distancia + 0.001 * distancia * distancia);

    // Multiplicamos por un factor (ej. 3.0) para "sobrequemar" y hacer el foco más intenso
    return (difusa + especular) * colorBase * intensidad * atenuacion * 4.0;
}

void main() {
    if (esSkyDome) {
        FragColor = texture(textura, TexCoord);
        return;
    }

    vec3 colorBase;
    if (usarTextura) {
        colorBase = texture(textura, TexCoord).rgb;
    } else {
        colorBase = colorObjeto;
    }

    if (esEmisivo) {
        FragColor = vec4(colorBase, alphaObj); // Muestra el color puro especificado, sin iluminacion
        return;
    }

    vec3 norm     = normalize(Normal);
    vec3 dirVista = normalize(posVista - PosFrag);

    // Luz general de base (depende de si es de dia o de noche)
    vec3 resultado = luzAmbiente * colorBase;

    // Luz de ambos faros
    if (farosEncendidos) {
        float cut = cos(radians(15.0));
        float outCut = cos(radians(25.0));
        resultado += calcularFocoPhong(posLuzFaroIzq, dirFaros, cut, outCut, vec3(1.0, 0.95, 0.8), norm, dirVista, colorBase);
        resultado += calcularFocoPhong(posLuzFaroDer, dirFaros, cut, outCut, vec3(1.0, 0.95, 0.8), norm, dirVista, colorBase);
    }

    resultado = min(resultado, vec3(1.0));
    FragColor = vec4(resultado, alphaObj);
}
