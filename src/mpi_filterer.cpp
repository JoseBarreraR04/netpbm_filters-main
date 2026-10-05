// Micro-Proyecto No. 2 - Programacion Paralela 2026-II
// Diseno 4: Memoria distribuida (MPI)
//
// La imagen se divide en bandas horizontales de filas y cada nodo (rank) de
// MPI recibe una. Como el kernel es 3x3, para filtrar la primera y la ultima
// fila de su banda un nodo necesita una fila de halo por encima y otra por
// debajo, y esas se las pide a sus vecinos inmediatos con MPI_Sendrecv.
//
//   height=1278, ranks=4:
//     nodo 0 -> y:[0,320)    halo de abajo: rank 1     halo de arriba: no existe
//     nodo 1 -> y:[320,640)  halo de abajo: rank 2     halo de arriba: rank 0
//     nodo 2 -> y:[640,959)  halo de abajo: rank 3     halo de arriba: rank 1
//     nodo 3 -> y:[959,1278) halo de abajo: no existe  halo de arriba: rank 2
//
// Flujo completo:
//   1. rank 0 lee la imagen y difunde la cabecera con MPI_Bcast
//   2. MPI_Scatterv reparte las filas: cada nodo recibe solo su banda
//   3. MPI_Sendrecv intercambia las filas de halo entre vecinos
//   4. cada nodo cronometra y filtra unicamente su banda
//   5. MPI_Gatherv reune las bandas y rank 0 escribe el archivo
//
// Se usan Scatterv y Gatherv, y no Scatter y Gather, porque cuando el alto no
// es multiplo del numero de nodos las bandas tienen una fila mas o menos y MPI
// necesita que se le indique el tamano y el desplazamiento de cada bloque.
//
// Uso:
//   mpiexec -n 4 ./mpi_filterer entrada.pgm salida.pgm --f blur
//   mpiexec -n 4 ./mpi_filterer entrada.ppm salida.ppm --f laplace
//   mpiexec -n 4 ./mpi_filterer salida.pgm --f sharpening   (entrada por stdin)

#include <mpi.h>

#include <cstdio>
#include <cstring>
#include <ctime>

#include "band.h"
#include "filter.h"
#include "image.h"

// ---------------------------------------------------------------------------
// Tiempos
// ---------------------------------------------------------------------------

static double secondsBetween(const struct timespec& start, const struct timespec& end) {
  return static_cast<double>(end.tv_sec - start.tv_sec) +
         static_cast<double>(end.tv_nsec - start.tv_nsec) / 1e9;
}

// Los argumentos se validan antes de tocar MPI: si un nodo se sale temprano y
// otro sigue, mpiexec se cuelga esperando. Asi que solo rank 0 valida, decide
// el codigo de salida y el filtro, y difunde ambas cosas al resto.
static int parseArguments(int argc, char* argv[], int* filter_index, const char** input_path,
                          const char** output_path, bool* error) {
  const char* paths[2] = {0, 0};
  int path_count = 0;
  *error = false;
  *filter_index = -1;

  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--f") == 0) {
      if (i + 1 >= argc) {
        std::printf("Error, falta el valor de --f\n");
        *error = true;
        return 1;
      }
      const Filter* filter = Filter::findByName(argv[++i]);
      if (filter == 0) {
        std::printf("Error, filtro desconocido: %s\n", argv[i]);
        std::printf("Filtros disponibles: blur, laplace, sharpening\n");
        *error = true;
        return 1;
      }
      *filter_index = static_cast<int>(filter - Filter::all());
    } else if (path_count < 2) {
      paths[path_count++] = argv[i];
    } else {
      std::printf("Error, demasiadas rutas.\n");
      *error = true;
      return 1;
    }
  }

  if (*filter_index < 0) {
    std::printf("Error, falta el filtro (--f blur|laplace|sharpening).\n");
    *error = true;
    return 1;
  }

  if (path_count < 1 || path_count > 2) {
    std::printf("Error, se esperaba una ruta de salida (o entrada y salida).\n");
    *error = true;
    return 1;
  }

  *output_path = paths[0];
  *input_path = 0;
  if (path_count == 2) {
    *input_path = paths[0];
    *output_path = paths[1];
  }
  return 0;
}

static void printUsage(const char* program) {
  std::printf("uso: mpiexec -n N %s entrada salida --f blur|laplace|sharpening\n", program);
  std::printf("     mpiexec -n N %s salida --f blur|laplace|sharpening   (entrada por stdin)\n",
              program);
}

int main(int argc, char* argv[]) {
  MPI_Init(&argc, &argv);

  int rank = 0;
  int ranks = 1;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &ranks);

  // --- 0. argumentos --------------------------------------------------------
  const char* input_path = 0;
  const char* output_path = 0;
  int filter_index = -1;
  bool arg_error = false;

  int exit_code = 0;
  if (rank == 0) {
    exit_code = parseArguments(argc, argv, &filter_index, &input_path, &output_path, &arg_error);
    if (arg_error) {
      printUsage(argv[0]);
    }
  }

  // Del rank 0 al resto viajan dos enteros: si los argumentos eran validos y que
  // filtro se eligio. Se difunde el indice y no el nombre porque los tres
  // filtros existen en todos los nodos por igual, y asi ningun nodo tiene que
  // volver a interpretar la cadena.
  int decision[2] = {exit_code, filter_index};
  MPI_Bcast(decision, 2, MPI_INT, 0, MPI_COMM_WORLD);
  exit_code = decision[0];
  filter_index = decision[1];
  if (exit_code != 0) {
    MPI_Finalize();
    return exit_code;
  }

  const Filter* filter = &Filter::all()[filter_index];

  // --- 1. rank 0 lee la imagen y difunde la cabecera ------------------------
  Image source;  // solo tiene datos en rank 0
  int header[4] = {0, 0, 0, 0};  // width, height, max_color, channels

  if (rank == 0) {
    FILE* input = stdin;
    if (input_path != 0) {
      input = fopen(input_path, "r");
      if (input == NULL) {
        std::printf("Error, incorrect path or incorrect file.\n");
      }
    }
    if (input != NULL && source.read(input)) {
      if (input_path != 0) {
        std::fclose(input);
      }
      header[0] = source.width;
      header[1] = source.height;
      header[2] = source.max_color;
      header[3] = source.channels;
    }
  }

  MPI_Bcast(header, 4, MPI_INT, 0, MPI_COMM_WORLD);
  if (header[0] <= 0 || header[1] <= 0) {
    // rank 0 no pudo leer la imagen: el error ya se imprimio alla. Se vacia el
    // buffer de stdout porque MPI_Abort no ejecuta la salida normal de la
    // libreria y, sin esto, el mensaje se perderia.
    std::fflush(stdout);
    std::fflush(stderr);
    MPI_Abort(MPI_COMM_WORLD, 1);
    return 1;
  }

  const int width = header[0];
  const int height = header[1];
  const int max_color = header[2];
  const int channels = header[3];
  const int row_ints = width * channels;

  const Band band = splitBands(height, rank, ranks);
  const int own_rows = band.rows();
  // Ranura 0 = fila de halo de arriba, ranuras 1..own_rows = la banda,
  // ranura own_rows+1 = fila de halo de abajo.
  const int row_offset = band.y0 - 1;
  int* my_rows = new int[static_cast<size_t>(own_rows + 2) * row_ints];

  // --- 2. Scatterv: rank 0 reparte las filas de la imagen -------------------
  // La banda se recibe en la ranura 1, no en la 0: la ranura 0 esta reservada
  // para la fila de halo de arriba, que llega despues en el paso 3. Si se
  // recibiera en la 0, todo el bloque quedaria desplazado una fila y cada nodo
  // filtraria la fila que le toca con los pixeles de la siguiente.
  int* received = my_rows + row_ints;
  if (rank == 0) {
    int* counts = new int[ranks];
    int* displs = new int[ranks];
    for (int r = 0; r < ranks; r++) {
      const Band other = splitBands(height, r, ranks);
      counts[r] = other.rows() * row_ints;
      displs[r] = other.y0 * row_ints;
    }
    MPI_Scatterv(source.pixels, counts, displs, MPI_INT, received, own_rows * row_ints, MPI_INT, 0,
                 MPI_COMM_WORLD);
    delete[] counts;
    delete[] displs;
  } else {
    MPI_Scatterv(0, 0, 0, MPI_INT, received, own_rows * row_ints, MPI_INT, 0, MPI_COMM_WORLD);
  }

  // --- 3. Halo: pedir una fila al vecino de arriba y otra al de abajo -------
  // El halo de arriba de este nodo es la ultima fila del vecino de arriba, y
  // el halo de abajo es la primera fila del vecino de abajo. A cambio, este
  // nodo tiene que enviar su primera fila hacia arriba y su ultima hacia abajo.
  //   ranura 0            = halo de arriba  (fila global y0-1)
  //   ranuras 1..own_rows = la banda
  //   ranura own_rows+1   = halo de abajo   (fila global y1)
  //
  // MPI_Sendrecv es punto a punto, asi que cada llamada envia a un vecino y
  // recibe de otro. Con MPI_PROC_NULL el primer y el ultimo nodo no esperan a
  // nadie, y las filas que no existen se descartan despues por los limites de
  // la imagen. Si a un nodo no le toca ninguna fila (imagen mas baja que el
  // numero de nodos) se envia una fila auxiliar: el vecino la recibira pero no
  // la usara, porque esa fila cae fuera de [0, height).
  int* dummy_row = new int[row_ints];
  int* send_down = (own_rows > 0) ? my_rows + own_rows * row_ints : dummy_row;
  int* send_up = (own_rows > 0) ? my_rows + row_ints : dummy_row;

  const int up = (rank > 0) ? rank - 1 : MPI_PROC_NULL;
  const int down = (rank + 1 < ranks) ? rank + 1 : MPI_PROC_NULL;

  // Mi halo de arriba viene del vecino de arriba; a cambio le mando mi ultima fila.
  MPI_Sendrecv(send_down, row_ints, MPI_INT, down, 1, my_rows, row_ints, MPI_INT, up, 1,
               MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  // Mi halo de abajo viene del vecino de abajo; a cambio le mando mi primera fila.
  MPI_Sendrecv(send_up, row_ints, MPI_INT, up, 2, my_rows + (own_rows + 1) * row_ints, row_ints,
               MPI_INT, down, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

  delete[] dummy_row;

  // --- 4. Cada nodo cronometra y filtra solo su banda -----------------------
  int* result_rows = new int[static_cast<size_t>(own_rows + 1) * row_ints];

  struct timespec wall_start, wall_end, cpu_start, cpu_end;
  clock_gettime(CLOCK_MONOTONIC, &wall_start);
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_start);

  for (int i = 0; i < own_rows; i++) {
    const int y = band.y0 + i;
    int* dst = result_rows + i * row_ints;
    for (int x = 0; x < width; x++) {
      for (int c = 0; c < channels; c++) {
        dst[x * channels + c] =
            filter->applyBand(my_rows, row_ints, width, height, channels, max_color, x, y, c,
                              row_offset);
      }
    }
  }

  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_end);
  clock_gettime(CLOCK_MONOTONIC, &wall_end);

  const double wall = secondsBetween(wall_start, wall_end);
  const double cpu = secondsBetween(cpu_start, cpu_end);

  // --- 5. Gatherv: rank 0 reune las bandas y escribe el archivo -------------
  // Estas llamadas colectivas las hacen TODOS los nodos, en el mismo orden.
  // Si solo rank 0 llamara a Gatherv, los demas empezarian antes su Gather y
  // MPI se quedaria esperando a que coincidieran: por eso la llamada esta fuera
  // del "si soy rank 0", y lo unico que se reserva en rank 0 es el buffer.
  int* gathered = 0;
  int* counts = 0;
  int* displs = 0;
  if (rank == 0) {
    gathered = new int[static_cast<size_t>(height) * row_ints];
    counts = new int[ranks];
    displs = new int[ranks];
    for (int r = 0; r < ranks; r++) {
      const Band other = splitBands(height, r, ranks);
      counts[r] = other.rows() * row_ints;
      displs[r] = other.y0 * row_ints;
    }
  }

  MPI_Gatherv(result_rows, own_rows * row_ints, MPI_INT, gathered, counts, displs, MPI_INT, 0,
              MPI_COMM_WORLD);

  // Tiempos de todos los nodos, para la tabla del enunciado.
  double* node_wall = 0;
  double* node_cpu = 0;
  if (rank == 0) {
    node_wall = new double[ranks];
    node_cpu = new double[ranks];
  }
  MPI_Gather(&wall, 1, MPI_DOUBLE, node_wall, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
  MPI_Gather(&cpu, 1, MPI_DOUBLE, node_cpu, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

  // 0 significa que todo fue bien. Todos los nodos devuelven el mismo valor:
  // solo rank 0 puede detectar un error de escritura.
  int error_code = 0;
  if (rank == 0) {
    std::printf("Filtro: %s | Nodos MPI: %d | Imagen: %s %dx%d | Canales: %d\n", filter->name,
                ranks, source.magic, width, height, channels);
    std::printf("\nReparto en bandas horizontales:\n");
    for (int r = 0; r < ranks; r++) {
      const Band b = splitBands(height, r, ranks);
      std::printf("  nodo %d -> y:[%4d,%4d)  %4d filas\n", r, b.y0, b.y1, b.rows());
    }

    std::printf("\nTiempo por nodo (solo el filtrado de su banda):\n");
    double slowest = 0.0;
    double cpu_total = 0.0;
    for (int r = 0; r < ranks; r++) {
      std::printf("  nodo %d : total %8.3f ms | CPU %8.3f ms\n", r, node_wall[r] * 1000.0,
                  node_cpu[r] * 1000.0);
      if (node_wall[r] > slowest) {
        slowest = node_wall[r];
      }
      cpu_total += node_cpu[r];
    }
    std::printf("Tiempo total del filtrado : %.6f s (%.3f ms)  [nodo mas lento]\n", slowest,
                slowest * 1000.0);
    std::printf("Tiempo de CPU del filtrado : %.6f s (%.3f ms)  [suma de los %d nodos]\n",
                cpu_total, cpu_total * 1000.0, ranks);

    if (std::strcmp(output_path, "-") == 0) {
      std::fprintf(stdout, "%s\n%d %d\n%d\n", source.magic, width, height, max_color);
      for (int i = 0; i < height * row_ints; i++) {
        std::fprintf(stdout, "%d\n", gathered[i]);
      }
    } else {
      FILE* output = fopen(output_path, "w");
      if (output == NULL) {
        std::printf("Error, incorrect path or incorrect file.\n");
        error_code = 1;
      } else {
        std::fprintf(output, "%s\n%d %d\n%d\n", source.magic, width, height, max_color);
        for (int i = 0; i < height * row_ints; i++) {
          std::fprintf(output, "%d\n", gathered[i]);
        }
        std::fclose(output);
      }
    }

    delete[] gathered;
    delete[] counts;
    delete[] displs;
    delete[] node_wall;
    delete[] node_cpu;
  }

  delete[] result_rows;
  delete[] my_rows;
  MPI_Finalize();
  return error_code;
}