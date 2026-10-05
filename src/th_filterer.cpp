// Micro-Proyecto No. 2 - Programacion Paralela 2026-II
// Diseno 3: Memoria compartida
//
// La imagen de entrada se divide en cuatro regiones (arriba-izquierda,
// arriba-derecha, abajo-izquierda y abajo-derecha) y cada region se le
// entrega a un hilo (std::thread) junto con el filtro que debe aplicar.
// Los cuatro hilos escriben sobre un unico buffer de salida, en el rango
// que les corresponde, por lo que no hace falta ningun mecanismo de
// sincronizacion: la imagen de entrada solo se lee y la de salida solo
// se escribe en posiciones disjuntas.
//
// Uso:
//   ./th_filterer entrada.pgm salida.pgm --f blur
//   ./th_filterer entrada.ppm salida.ppm --f laplace
//   ./th_filterer salida.pgm --f sharpening        (entrada por stdin)

#include <cctype>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <thread>

#define NUM_THREADS 4

// ---------------------------------------------------------------------------
// helpers de lectura
// ---------------------------------------------------------------------------

// El formato netpbm admite comentarios que empiezan con # y terminan en fin de
// linea, en cualquier punto entre los valores. skipBlanks() los descarta y se
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

// ---------------------------------------------------------------------------
// Image
// ---------------------------------------------------------------------------

class Image {
 public:
  char magic[3];
  int width;
  int height;
  int max_color;
  int channels;
  int* pixels;

  Image() : magic("P2"), width(0), height(0), max_color(255), channels(1), pixels(0) {}

  ~Image() { delete[] pixels; }

  int count() const { return width * height * channels; }

  void allocate(int w, int h, int c) {
    width = w;
    height = h;
    channels = c;
    delete[] pixels;
    pixels = new int[count()];
  }

  // Copia en esta imagen los parametros de otra (magic, dimensiones, canales y
  // valor maximo de color) y reserva el buffer de pixeles.
  void allocateLike(const Image& other) {
    magic[0] = other.magic[0];
    magic[1] = other.magic[1];
    magic[2] = other.magic[2];
    max_color = other.max_color;
    allocate(other.width, other.height, other.channels);
  }

  bool read(FILE* file) {
    if (skipBlanks(file) == EOF || fscanf(file, "%2s", magic) != 1) {
      std::printf("Error, no se pudo leer el numero magico.\n");
      return false;
    }

    if (strcmp(magic, "P2") == 0) {
      channels = 1;
    } else if (strcmp(magic, "P3") == 0) {
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

// ---------------------------------------------------------------------------
// Filter
// ---------------------------------------------------------------------------

#define KERNEL_SIZE 3

class Filter {
 public:
  const char* name;
  float kernel[KERNEL_SIZE * KERNEL_SIZE];
  // renormalize: true solo para blur, donde en los bordes se divide por la
  //              suma de los pesos realmente utilizados.
  // absolute:    true para laplace y realce, para no truncar a negativo.
  bool renormalize;
  bool absolute;

  Filter(const char* filter_name, const float* filter_kernel, bool renorm, bool abs)
      : name(filter_name), renormalize(renorm), absolute(abs) {
    for (int i = 0; i < KERNEL_SIZE * KERNEL_SIZE; i++) {
      kernel[i] = filter_kernel[i];
    }
  }

  static const Filter* all() {
    static const float blur_kernel[KERNEL_SIZE * KERNEL_SIZE] = {
        1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f,
        1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f,
        1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f};
    static const float laplace_kernel[KERNEL_SIZE * KERNEL_SIZE] = {
        0.0f, -1.0f, 0.0f,
        -1.0f, 4.0f, -1.0f,
        0.0f, -1.0f, 0.0f};
    static const float sharpen_kernel[KERNEL_SIZE * KERNEL_SIZE] = {
        0.0f, -1.0f, 0.0f,
        -1.0f, 5.0f, -1.0f,
        0.0f, -1.0f, 0.0f};

    static Filter filters[3] = {Filter("blur", blur_kernel, true, false),
                                Filter("laplace", laplace_kernel, false, true),
                                Filter("sharpening", sharpen_kernel, false, true)};
    return filters;
  }

  static int count() { return 3; }

  static const Filter* findByName(const char* filter_name) {
    const Filter* filters = all();
    for (int i = 0; i < count(); i++) {
      if (strcmp(filters[i].name, filter_name) == 0) {
        return &filters[i];
      }
    }
    return 0;
  }
};

// ---------------------------------------------------------------------------
// Region
// ---------------------------------------------------------------------------

class Region {
 public:
  int x0;
  int y0;
  int x1;
  int y1;

  // Reparte la imagen en cuatro cuadrantes. Cuando el ancho o el alto es
  // impar el pixel extra va a la region derecha / inferior.
  static void splitQuadrants(int width, int height, Region out[NUM_THREADS]) {
    int half_w = width / 2;
    int half_h = height / 2;
    out[0] = Region{0, 0, half_w, half_h};
    out[1] = Region{half_w, 0, width, half_h};
    out[2] = Region{0, half_h, half_w, height};
    out[3] = Region{half_w, half_h, width, height};
  }

  static const char* label(int index) {
    static const char* labels[NUM_THREADS] = {
        "arriba-izquierda", "arriba-derecha", "abajo-izquierda", "abajo-derecha"};
    return labels[index];
  }
};

// ---------------------------------------------------------------------------
// FilterJob
// ---------------------------------------------------------------------------

// Trabajo que ejecuta un hilo: una region de la imagen mas el filtro a aplicar.
class FilterJob {
 public:
  const Image* source;
  Image* destination;
  Region region;
  const Filter* filter;

  void run() const {
    const int channels = source->channels;
    const int width = source->width;
    const int height = source->height;
    const int max_color = source->max_color;
    const int* src = source->pixels;
    int* dst = destination->pixels;

    for (int y = region.y0; y < region.y1; y++) {
      for (int x = region.x0; x < region.x1; x++) {
        for (int c = 0; c < channels; c++) {
          float sum = 0.0f;
          float weight_sum = 0.0f;

          for (int ky = -1; ky <= 1; ky++) {
            for (int kx = -1; kx <= 1; kx++) {
              int nx = x + kx;
              int ny = y + ky;
              if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
                continue;
              }
              float weight = filter->kernel[(ky + 1) * KERNEL_SIZE + (kx + 1)];
              sum += src[(ny * width + nx) * channels + c] * weight;
              if (filter->renormalize) {
                weight_sum += weight;
              }
            }
          }

          float value = filter->renormalize ? sum / weight_sum : sum;
          if (filter->absolute && value < 0.0f) {
            value = -value;
          }

          int result = static_cast<int>(value);
          if (result < 0) {
            result = 0;
          }
          if (result > max_color) {
            result = max_color;
          }

          dst[(y * width + x) * channels + c] = result;
        }
      }
    }
  }
};

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static void printUsage(const char* program) {
  std::printf("uso: %s entrada salida --f blur|laplace|sharpening\n", program);
  std::printf("  %s salida --f blur|laplace|sharpening   (lee la entrada desde stdin)\n", program);
  std::printf("ejemplo: %s images/damma.pgm /tmp/damma_blur.pgm --f blur\n", program);
}

static double secondsBetween(const struct timespec& start, const struct timespec& end) {
  return static_cast<double>(end.tv_sec - start.tv_sec) +
         static_cast<double>(end.tv_nsec - start.tv_nsec) / 1e9;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
  const char* filter_name = 0;
  const char* paths[2] = {0, 0};
  int path_count = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--f") == 0) {
      if (i + 1 >= argc) {
        std::printf("Error, falta el valor de --f\n");
        printUsage(argv[0]);
        return 1;
      }
      filter_name = argv[++i];
    } else if (path_count < 2) {
      paths[path_count++] = argv[i];
    } else {
      std::printf("Error, demasiadas rutas.\n");
      printUsage(argv[0]);
      return 1;
    }
  }

  if (filter_name == 0) {
    std::printf("Error, falta el filtro (--f blur|laplace|sharpening).\n");
    printUsage(argv[0]);
    return 1;
  }

  const Filter* filter = Filter::findByName(filter_name);
  if (filter == 0) {
    std::printf("Error, filtro desconocido: %s\n", filter_name);
    std::printf("Filtros disponibles: blur, laplace, sharpening\n");
    return 1;
  }

  if (path_count < 1 || path_count > 2) {
    std::printf("Error, se esperaba una ruta de salida (o entrada y salida).\n");
    printUsage(argv[0]);
    return 1;
  }

  const char* output_path = paths[0];
  const char* input_path = 0;
  if (path_count == 2) {
    input_path = paths[0];
    output_path = paths[1];
  }

  Image source;
  FILE* input = stdin;
  if (input_path != 0) {
    input = fopen(input_path, "r");
    if (input == NULL) {
      std::printf("Error, incorrect path or incorrect file.\n");
      return 1;
    }
  }

  if (!source.read(input)) {
    if (input_path != 0) {
      std::fclose(input);
    }
    return 1;
  }
  if (input_path != 0) {
    std::fclose(input);
  }

  Image destination;
  destination.allocateLike(source);

  Region regions[NUM_THREADS];
  Region::splitQuadrants(source.width, source.height, regions);

  FilterJob jobs[NUM_THREADS] = {
      FilterJob{&source, &destination, regions[0], filter},
      FilterJob{&source, &destination, regions[1], filter},
      FilterJob{&source, &destination, regions[2], filter},
      FilterJob{&source, &destination, regions[3], filter}};

  std::printf("Filtro: %s | Hilos: %d | Imagen: %s %dx%d | Regiones: %dx%d\n", filter->name,
              NUM_THREADS, source.magic, source.width, source.height, source.width / 2,
              source.height / 2);
  for (int i = 0; i < NUM_THREADS; i++) {
    std::printf("  hilo %d -> %-15s x:[%d,%d) y:[%d,%d)\n", i, Region::label(i), regions[i].x0,
                regions[i].x1, regions[i].y0, regions[i].y1);
  }

  struct timespec wall_start, wall_end, cpu_start, cpu_end;
  clock_gettime(CLOCK_MONOTONIC, &wall_start);
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_start);

  std::thread threads[NUM_THREADS];
  for (int i = 0; i < NUM_THREADS; i++) {
    threads[i] = std::thread(&FilterJob::run, &jobs[i]);
  }
  for (int i = 0; i < NUM_THREADS; i++) {
    threads[i].join();
  }

  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_end);
  clock_gettime(CLOCK_MONOTONIC, &wall_end);

  const double wall = secondsBetween(wall_start, wall_end);
  const double cpu = secondsBetween(cpu_start, cpu_end);
  std::printf("Tiempo total del filtrado : %.6f s (%.3f ms)\n", wall, wall * 1000.0);
  std::printf("Tiempo de CPU del filtrado : %.6f s (%.3f ms)\n", cpu, cpu * 1000.0);

  if (!destination.write(output_path)) {
    return 1;
  }
  return 0;
}
