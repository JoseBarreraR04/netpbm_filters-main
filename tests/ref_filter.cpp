// Referencia secuencial de un solo hilo para el Diseno 3.
//
// No forma parte de la entrega: es el oraculo de correccion que usa
// tests/selftest.sh. Aplica el filtro sobre toda la imagen, sin dividirla en
// regiones ni usar hilos, con exactamente la misma aritmetica que
// src/th_filterer.cpp. Si las dos imagenes coinciden byte a byte, el reparto en
// cuatro regiones y el halo entre hilos estan bien hechos.
//
//   ref_filter entrada salida --f blur|laplace|sharpening
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#define KERNEL_SIZE 3

// Salta espacios en blanco y comentarios (# hasta el fin de linea).
static int skip_blanks(FILE* f) {
  int c = fgetc(f);
  while (c != EOF) {
    if (c == '#') {
      while (c != EOF && c != '\n') c = fgetc(f);
      continue;
    }
    if (!isspace(c)) {
      ungetc(c, f);
      return c;
    }
    c = fgetc(f);
  }
  return EOF;
}

static int read_int(FILE* f) {
  int v;
  if (skip_blanks(f) == EOF) return -1;
  if (fscanf(f, "%d", &v) != 1) return -1;
  return v;
}

int main(int argc, char* argv[]) {
  const char* filter_name = 0;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--f") == 0 && i + 1 < argc) filter_name = argv[i + 1];
  }
  if (filter_name == 0 || argc < 3) {
    fprintf(stderr, "uso: ref_filter entrada salida --f blur|laplace|sharpening\n");
    return 2;
  }

  static const float blur_kernel[KERNEL_SIZE * KERNEL_SIZE] = {
      1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f,
      1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f};
  static const float laplace_kernel[KERNEL_SIZE * KERNEL_SIZE] = {
      0.0f, -1.0f, 0.0f, -1.0f, 4.0f, -1.0f, 0.0f, -1.0f, 0.0f};
  static const float sharpen_kernel[KERNEL_SIZE * KERNEL_SIZE] = {
      0.0f, -1.0f, 0.0f, -1.0f, 5.0f, -1.0f, 0.0f, -1.0f, 0.0f};

  const float* kernel;
  int renormalize;
  int use_abs;
  if (strcmp(filter_name, "blur") == 0) {
    kernel = blur_kernel;
    renormalize = 1;
    use_abs = 0;
  } else if (strcmp(filter_name, "laplace") == 0) {
    kernel = laplace_kernel;
    renormalize = 0;
    use_abs = 1;
  } else if (strcmp(filter_name, "sharpening") == 0) {
    kernel = sharpen_kernel;
    renormalize = 0;
    use_abs = 1;
  } else {
    fprintf(stderr, "filtro desconocido: %s\n", filter_name);
    return 2;
  }

  FILE* f = fopen(argv[1], "r");
  if (f == NULL) {
    fprintf(stderr, "no se pudo abrir %s\n", argv[1]);
    return 1;
  }
  char magic[3];
  if (skip_blanks(f) == EOF || fscanf(f, "%2s", magic) != 1) {
    fprintf(stderr, "no se pudo leer el numero magico\n");
    return 1;
  }
  int width = read_int(f);
  int height = read_int(f);
  int max_color = read_int(f);
  if (width <= 0 || height <= 0 || max_color <= 0) {
    fprintf(stderr, "cabecera invalida\n");
    return 1;
  }
  int channels = (strcmp(magic, "P3") == 0) ? 3 : 1;
  int total = width * height * channels;
  int* src = (int*)malloc(total * sizeof(int));
  int* dst = (int*)malloc(total * sizeof(int));
  if (src == NULL || dst == NULL) return 1;
  for (int i = 0; i < total; i++) {
    int v = read_int(f);
    if (v < 0) {
      fprintf(stderr, "pixeles incompletos en %s (%d de %d)\n", argv[1], i, total);
      return 1;
    }
    src[i] = v;
  }
  fclose(f);

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      for (int c = 0; c < channels; c++) {
        float sum = 0.0f, weight_sum = 0.0f;
        for (int ky = -1; ky <= 1; ky++) {
          for (int kx = -1; kx <= 1; kx++) {
            int nx = x + kx, ny = y + ky;
            if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
            float w = kernel[(ky + 1) * KERNEL_SIZE + (kx + 1)];
            sum += src[(ny * width + nx) * channels + c] * w;
            if (renormalize) weight_sum += w;
          }
        }
        float value = renormalize ? sum / weight_sum : sum;
        if (use_abs && value < 0.0f) value = -value;
        int result = (int)value;
        if (result < 0) result = 0;
        if (result > max_color) result = max_color;
        dst[(y * width + x) * channels + c] = result;
      }
    }
  }

  FILE* o = fopen(argv[2], "w");
  if (o == NULL) {
    fprintf(stderr, "no se pudo escribir %s\n", argv[2]);
    return 1;
  }
  fprintf(o, "%s\n%d %d\n%d\n", magic, width, height, max_color);
  for (int i = 0; i < total; i++) fprintf(o, "%d\n", dst[i]);
  fclose(o);
  free(src);
  free(dst);
  return 0;
}