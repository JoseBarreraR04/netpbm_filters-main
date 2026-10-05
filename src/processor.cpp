#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Aplicacion base utilizando el paradigma orientado a objetos
class Image{
public:
    char magic[3];
    int width;
    int height;
    int max_color;
    int channels;
    int pixel_count;
    // Uso estricto de arreglos nativos en lugar de vectores
    int* pixels;

    Image() : width(0), height(0), max_color(0), channels(1), pixel_count(0), pixels(nullptr) {}

    ~Image() {
        if (pixels != nullptr) {
            free(pixels);
        }
    }

    void allocate() {
        pixel_count = width * height * channels;
        pixels = (int*)malloc(pixel_count * sizeof(int));
    }

    // Función para manejar la línea 2: los comentarios opcionales que comienzan con #
    void skipComments(FILE* file) {
        int c = getc(file);
        while (c == '#') {
            while (getc(file) != '\n'); // Avanzar hasta el final de la línea
            c = getc(file);
        }
        ungetc(c, file); // Devolver el último carácter leído que no era '#'
    }

    // Punteros a Char en lugar de strings
    bool load(const char* filepath) {
        FILE* file = fopen(filepath, "r");
        if (!file) {
            std::cout << "Error al abrir el archivo: " << filepath << "\n";
            return false;
        }

        // Línea 1: El "número mágico"
        fscanf(file, "%2s", magic);
        skipComments(file);

        // Línea 3: El ancho y el alto de la imagen
        fscanf(file, "%d %d", &width, &height);
        skipComments(file);

        // Línea 4: El valor máximo de color
        fscanf(file, "%d", &max_color);

        // Identifica si es PPM (P3) o PGM (P2)
        channels = (strcmp(magic, "P3") == 0) ? 3 : 1;
        allocate();

        // Líneas siguientes: Los valores de los píxeles
        for (int i = 0; i < pixel_count; i++) {
            fscanf(file, "%d", &pixels[i]);
        }

        fclose(file);

        std::cout << "Imagen cargada: " << filepath << " | Formato: " << magic
                  << " | Resolucion: " << width << "x" << height << "\n";

        return true;
    }
};

int main(int argc, char* argv[]) {
    // La lectura se realizará desde la entrada estándar de forma automática no interactiva
    if (argc < 2) {
        std::cout << "Faltan argumentos.\n";
        std::cout << "Uso: " << argv[0] << " lena.ppm lena2.ppm\n";
        return 1;
    }

    // Permite cargar múltiples archivos pasados por la shell
    for (int i = 1; i < argc; i++) {
        Image img;
        img.load(argv[i]);
    }

    return 0;
}