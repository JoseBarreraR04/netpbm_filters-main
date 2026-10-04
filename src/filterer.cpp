#include "Image.hpp"
#include "Filter.hpp"

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <chrono>

#define MAX_FILTERS 32
#define MAX_STR_LEN 256

static void printUsage(const char* progName) {
    std::cout << "Uso: " << progName << " <imagen_entrada> <imagen_salida> --f <filtro1> [--f <filtro2> ...]\n"
              << "     o tambien: " << progName << " <imagen_entrada> <imagen_salida> --f <filtro1,filtro2,...>\n\n"
              << "Filtros disponibles:\n"
              << "  blur     : Suavizado (filtro de media 3x3)\n"
              << "  sharpen  : Realce / enfoque de bordes 3x3\n"
              << "  laplace  : Deteccion de bordes laplaciano 8-vecinos 3x3\n\n"
              << "Ejemplos:\n"
              << "  " << progName << " images/fruit.ppm images/fruit_blur.ppm --f blur\n"
              << "  " << progName << " images/lena.pgm images/lena_sharpen.pgm --f sharpen\n"
              << "  " << progName << " images/puj.ppm images/puj_pipeline.ppm --f blur,sharpen\n"
              << std::endl;
}

static Filter* createFilterByName(const char* name) {
    if (std::strcmp(name, "blur") == 0) {
        return new ConvolutionFilter(ConvolutionFilter::createBlur());
    } else if (std::strcmp(name, "sharpen") == 0) {
        return new ConvolutionFilter(ConvolutionFilter::createSharpen());
    } else if (std::strcmp(name, "laplace") == 0) {
        return new ConvolutionFilter(ConvolutionFilter::createLaplace());
    }
    return nullptr;
}

static void getCurrentTimestamp(char* out_buf, size_t buf_size) {
    std::time_t now = std::time(nullptr);
    std::tm* tm_info = std::localtime(&now);
    if (tm_info != nullptr) {
        std::strftime(out_buf, buf_size, "%Y-%m-%d %H:%M:%S", tm_info);
    } else {
        std::strncpy(out_buf, "N/A", buf_size);
    }
}

static void logTimeToCsv(const char* input_path,
                         const char* magic,
                         int width,
                         int height,
                         int channels,
                         const char* filters_str,
                         double cpu_sec,
                         double wall_sec) {
    const char* csv_path = "tiempos.csv";
    FILE* check = std::fopen(csv_path, "r");
    bool write_header = (check == nullptr);
    if (check != nullptr) {
        std::fclose(check);
    }

    FILE* csv = std::fopen(csv_path, "a");
    if (!csv) {
        std::cerr << "Advertencia: No se pudo abrir '" << csv_path << "' para registrar tiempos.\n";
        return;
    }

    if (write_header) {
        std::fprintf(csv, "fecha_hora,archivo_entrada,formato,ancho,alto,canales,filtros,tiempo_cpu_s,tiempo_total_s\n");
    }

    char timestamp[64];
    getCurrentTimestamp(timestamp, sizeof(timestamp));

    const char* fmt_str = (std::strcmp(magic, "P3") == 0) ? "PPM (P3)" : "PGM (P2)";

    std::fprintf(csv, "%s,%s,%s,%d,%d,%d,\"%s\",%.6f,%.6f\n",
                 timestamp, input_path, fmt_str, width, height, channels,
                 filters_str, cpu_sec, wall_sec);

    std::fclose(csv);
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Error: Argumentos insuficientes.\n\n";
        printUsage(argv[0]);
        return 1;
    }

    const char* input_path = argv[1];
    const char* output_path = argv[2];

    Filter* filters[MAX_FILTERS];
    int filter_count = 0;
    char filters_applied_str[MAX_STR_LEN];
    filters_applied_str[0] = '\0';

    int arg_idx = 3;
    while (arg_idx < argc) {
        if (std::strcmp(argv[arg_idx], "--f") == 0 || std::strcmp(argv[arg_idx], "-f") == 0) {
            arg_idx++;
            while (arg_idx < argc && argv[arg_idx][0] != '-') {
                char temp[MAX_STR_LEN];
                std::strncpy(temp, argv[arg_idx], sizeof(temp) - 1);
                temp[sizeof(temp) - 1] = '\0';

                char* token = std::strtok(temp, ",");
                while (token != nullptr) {
                    if (filter_count >= MAX_FILTERS) {
                        std::cerr << "Error: Se ha alcanzado el numero maximo de filtros (" 
                                  << MAX_FILTERS << ").\n";
                        for (int k = 0; k < filter_count; ++k) delete filters[k];
                        return 1;
                    }

                    Filter* f = createFilterByName(token);
                    if (!f) {
                        std::cerr << "Error: Filtro desconocido '" << token << "'.\n\n";
                        printUsage(argv[0]);
                        for (int k = 0; k < filter_count; ++k) delete filters[k];
                        return 1;
                    }

                    filters[filter_count++] = f;

                    if (filters_applied_str[0] != '\0') {
                        std::strncat(filters_applied_str, ", ", sizeof(filters_applied_str) - std::strlen(filters_applied_str) - 1);
                    }
                    std::strncat(filters_applied_str, token, sizeof(filters_applied_str) - std::strlen(filters_applied_str) - 1);

                    token = std::strtok(nullptr, ",");
                }
                arg_idx++;
            }
        } else {
            std::cerr << "Error: Opcion no reconocida '" << argv[arg_idx] << "'.\n\n";
            printUsage(argv[0]);
            for (int k = 0; k < filter_count; ++k) delete filters[k];
            return 1;
        }
    }

    if (filter_count == 0) {
        std::cerr << "Error: Debe especificar al menos un filtro con la opcion --f <filtro>.\n\n";
        printUsage(argv[0]);
        return 1;
    }

    Image current_image;
    if (!current_image.load(input_path)) {
        for (int k = 0; k < filter_count; ++k) delete filters[k];
        return 1;
    }

    // Medicion de tiempo de filtrado
    std::clock_t cpu_start = std::clock();
    auto wall_start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < filter_count; ++i) {
        Image next_image;
        filters[i]->apply(current_image, next_image);
        current_image = next_image;
    }

    std::clock_t cpu_end = std::clock();
    auto wall_end = std::chrono::high_resolution_clock::now();

    double cpu_sec = static_cast<double>(cpu_end - cpu_start) / CLOCKS_PER_SEC;
    std::chrono::duration<double> wall_duration = wall_end - wall_start;
    double wall_sec = wall_duration.count();

    if (!current_image.save(output_path)) {
        for (int k = 0; k < filter_count; ++k) delete filters[k];
        return 1;
    }

    // Salida clara en consola
    std::cout << "\n======================================================\n";
    std::cout << "           Filtrado Secuencial (Diseno 2)             \n";
    std::cout << "======================================================\n";
    std::cout << "  Archivo de entrada : " << input_path << "\n";
    std::cout << "  Archivo de salida  : " << output_path << "\n";
    std::cout << "  Formato            : " 
              << ((std::strcmp(current_image.getMagic(), "P3") == 0) ? "PPM (P3 - Color)" : "PGM (P2 - Escala de grises)") << "\n";
    std::cout << "  Dimensiones        : " << current_image.getWidth() << " x " << current_image.getHeight() << " px\n";
    std::cout << "  Canales            : " << current_image.getChannels() << "\n";
    std::cout << "  Filtros aplicados  : " << filters_applied_str << "\n";
    std::cout << "------------------------------------------------------\n";
    std::cout << "  Tiempo CPU         : " << cpu_sec << " s\n";
    std::cout << "  Tiempo Total (Wall): " << wall_sec << " s (" << (wall_sec * 1000.0) << " ms)\n";
    std::cout << "======================================================\n\n";

    // Registro en tiempos.csv
    logTimeToCsv(input_path, current_image.getMagic(), current_image.getWidth(), current_image.getHeight(),
                 current_image.getChannels(), filters_applied_str, cpu_sec, wall_sec);

    // Liberar memoria de filtros
    for (int k = 0; k < filter_count; ++k) {
        delete filters[k];
    }

    return 0;
}