// Micro-Proyecto No. 2 - Programacion Paralela 2026-II
// Diseno 4: Memoria distribuida
//
// Image: lectura y escritura de imagenes PGM (P2) y PPM (P3) en texto plano.
// Los datos viven en un arreglo plano de int, indiceado como
//   pixels[(y * width + x) * channels + c]
// El formato netpbm admite comentarios que empiezan con # y terminan en fin de
// linea, en cualquier punto entre los valores; skipBlanks() los descarta.

#ifndef IMAGE_H
#define IMAGE_H

#include <cctype>
#include <cstdio>
#include <cstring>

class Image {
 public:
  char magic[3];
  int width;
  int height;
  int max_color;
  int channels;
  int* pixels;

  Image() : width(0), height(0), max_color(255), channels(1), pixels(0) {
    magic[0] = 'P';
    magic[1] = '2';
    magic[2] = '\0';
  }

  // No se copia la imagen: cada nodo trabaja sobre su propia banda y el
  // buffer se libera solo.
  ~Image() { delete[] pixels; }

  int count() const { return width * height * channels; }

  void allocate(int w, int h, int c) {
    width = w;
    height = h;
    channels = c;
    delete[] pixels;
    pixels = new int[count()];
  }

  // Salta espacios en blanco y comentarios (# hasta el fin de linea), y se
  // queda en el primer caracter que si es parte de un valor.
  static int skipBlanks(FILE* file) {
    int c = fgetc(file);
    while (c != EOF) {
      if (c == '#') {
        while (c != EOF && c != '\n') {
          c = fgetc(file);
        }
        continue;
      }
      if (!std::isspace(c)) {
        ungetc(c, file);
        return c;
      }
      c = fgetc(file);
    }
    return EOF;
  }

  static bool readInt(FILE* file, int* out) {
    if (skipBlanks(file) == EOF) {
      return false;
    }
    return fscanf(file, "%d", out) == 1;
  }

  bool read(FILE* file) {
    if (skipBlanks(file) == EOF || fscanf(file, "%2s", magic) != 1) {
      std::printf("Error, no se pudo leer el numero magico.\n");
      return false;
    }

    if (std::strcmp(magic, "P2") == 0) {
      channels = 1;
    } else if (std::strcmp(magic, "P3") == 0) {
      channels = 3;
    } else {
      std::printf("Error, formato no soportado: %s (solo P2 y P3).\n", magic);
      return false;
    }

    if (!readInt(file, &width) || !readInt(file, &height) || !readInt(file, &max_color)) {
      std::printf("Error, no se pudo leer la cabecera de la imagen.\n");
      return false;
    }

    if (width <= 0 || height <= 0 || max_color <= 0) {
      std::printf("Error, dimensiones invalidas.\n");
      return false;
    }

    allocate(width, height, channels);

    for (int i = 0; i < count(); i++) {
      int value;
      if (!readInt(file, &value)) {
        std::printf("Error reading pixels.\n");
        return false;
      }
      pixels[i] = value;
    }
    return true;
  }

  // Escribe la cabecera y un pixel por linea, igual que el resto del proyecto.
  bool write(const char* path) const {
    FILE* output = fopen(path, "w");
    if (output == NULL) {
      std::printf("Error, incorrect path or incorrect file.\n");
      return false;
    }
    std::fprintf(output, "%s\n%d %d\n%d\n", magic, width, height, max_color);
    for (int i = 0; i < count(); i++) {
      std::fprintf(output, "%d\n", pixels[i]);
    }
    std::fclose(output);
    return true;
  }
};

#endif  // IMAGE_H