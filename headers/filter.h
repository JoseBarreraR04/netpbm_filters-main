// Micro-Proyecto No. 2 - Programacion Paralela 2026-II
// Diseno 4: Memoria distribuida
//
// Filter: los tres filtros del enunciado (blur, laplace y sharpening) sobre un
// kernel 3x3. La aritmetica es identica a la del Diseno 3, para que las
// imagenes produced por los dos diseños sean comparables byte a byte.

#ifndef FILTER_H
#define FILTER_H

#include <cstring>

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

  Filter(const char* filter_name, const float* filter_kernel, bool renorm, bool abs_flag)
      : name(filter_name), renormalize(renorm), absolute(abs_flag) {
    for (int i = 0; i < KERNEL_SIZE * KERNEL_SIZE; i++) {
      kernel[i] = filter_kernel[i];
    }
  }

  static const float* blurKernel() {
    static const float k[KERNEL_SIZE * KERNEL_SIZE] = {
        1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f,
        1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f,
        1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f};
    return k;
  }

  static const float* laplaceKernel() {
    static const float k[KERNEL_SIZE * KERNEL_SIZE] = {
        0.0f, -1.0f, 0.0f,
        -1.0f, 4.0f, -1.0f,
        0.0f, -1.0f, 0.0f};
    return k;
  }

  static const float* sharpenKernel() {
    static const float k[KERNEL_SIZE * KERNEL_SIZE] = {
        0.0f, -1.0f, 0.0f,
        -1.0f, 5.0f, -1.0f,
        0.0f, -1.0f, 0.0f};
    return k;
  }

  static int count() { return 3; }

  static const Filter* all() {
    static Filter filters[3] = {
        Filter("blur", blurKernel(), true, false),
        Filter("laplace", laplaceKernel(), false, true),
        Filter("sharpening", sharpenKernel(), false, true)};
    return filters;
  }

  static const Filter* findByName(const char* filter_name) {
    if (filter_name == 0) {
      return 0;
    }
    const Filter* filters = all();
    for (int i = 0; i < count(); i++) {
      if (std::strcmp(filters[i].name, filter_name) == 0) {
        return &filters[i];
      }
    }
    return 0;
  }

  // Aplica el filtro al pixel global (x, y), canal c.
  //
  // En vez de la imagen completa recibe solo la franja de filas del nodo:
  //   rows      apunta a la ranura 0 del buffer local
  //   row_ints  pixeles por fila (width * channels)
  //   row_offset numero global de la fila que hay en la ranura 0
  //
  // El nodo guarda su banda en las ranuras 1..own_rows, con una fila de halo
  // en la ranura 0 (la de arriba) y otra en la ranura own_rows+1 (la de
  // abajo), de modo que para la fila global y el kernel completo cabe en
  // rows[(y-1-row_offset) .. (y+1-row_offset)]. Las filas fuera de [0, height)
  // se saltan, igual que en la version secuencial.
  int applyBand(const int* rows, int row_ints, int width, int height, int channels, int max_color,
                int x, int y, int c, int row_offset) const {
    float sum = 0.0f;
    float weight_sum = 0.0f;

    for (int ky = -1; ky <= 1; ky++) {
      for (int kx = -1; kx <= 1; kx++) {
        int nx = x + kx;
        int ny = y + ky;
        if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
          continue;
        }
        const int slot = ny - row_offset;
        float weight = kernel[(ky + 1) * KERNEL_SIZE + (kx + 1)];
        sum += rows[slot * row_ints + nx * channels + c] * weight;
        if (renormalize) {
          weight_sum += weight;
        }
      }
    }

    float value = renormalize ? sum / weight_sum : sum;
    if (absolute && value < 0.0f) {
      value = -value;
    }

    int result = static_cast<int>(value);
    if (result < 0) {
      result = 0;
    }
    if (result > max_color) {
      result = max_color;
    }
    return result;
  }
};

#endif  // FILTER_H