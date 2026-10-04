#include "Image.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <iostream>

void Image::allocate(int w, int h, int c) {
    width = w;
    height = h;
    channels = c;
    int total = width * height * channels;
    if (total > 0) {
        pixels = new int[total];
        std::memset(pixels, 0, total * sizeof(int));
    } else {
        pixels = nullptr;
    }
}

void Image::freePixels() {
    if (pixels != nullptr) {
        delete[] pixels;
        pixels = nullptr;
    }
}

Image::Image() : width(0), height(0), max_color(255), channels(1), pixels(nullptr) {
    magic[0] = 'P';
    magic[1] = '2';
    magic[2] = '\0';
}

Image::Image(int w, int h, int max_c, const char* m)
    : width(w), height(h), max_color(max_c), pixels(nullptr) {
    magic[0] = m[0];
    magic[1] = m[1];
    magic[2] = '\0';
    channels = (std::strcmp(magic, "P3") == 0) ? 3 : 1;
    allocate(width, height, channels);
}

Image::Image(const Image& other)
    : width(other.width), height(other.height), max_color(other.max_color),
      channels(other.channels), pixels(nullptr) {
    magic[0] = other.magic[0];
    magic[1] = other.magic[1];
    magic[2] = other.magic[2];
    allocate(width, height, channels);
    if (pixels != nullptr && other.pixels != nullptr) {
        int total = width * height * channels;
        std::memcpy(pixels, other.pixels, total * sizeof(int));
    }
}

Image& Image::operator=(const Image& other) {
    if (this != &other) {
        freePixels();
        width = other.width;
        height = other.height;
        max_color = other.max_color;
        channels = other.channels;
        magic[0] = other.magic[0];
        magic[1] = other.magic[1];
        magic[2] = other.magic[2];
        allocate(width, height, channels);
        if (pixels != nullptr && other.pixels != nullptr) {
            int total = width * height * channels;
            std::memcpy(pixels, other.pixels, total * sizeof(int));
        }
    }
    return *this;
}

Image::~Image() {
    freePixels();
}

static const char* skipWhitespaceAndComments(const char* p) {
    while (*p) {
        if (std::isspace((unsigned char)*p)) {
            p++;
        } else if (*p == '#') {
            while (*p && *p != '\n' && *p != '\r') {
                p++;
            }
        } else {
            break;
        }
    }
    return p;
}

static const char* readToken(const char* p, char* token, int max_len) {
    p = skipWhitespaceAndComments(p);
    if (!*p) return nullptr;
    int idx = 0;
    while (*p && !std::isspace((unsigned char)*p) && *p != '#' && idx < max_len - 1) {
        token[idx++] = *p++;
    }
    token[idx] = '\0';
    return p;
}

static const char* readInt(const char* p, int* value) {
    p = skipWhitespaceAndComments(p);
    if (!*p) return nullptr;
    int sign = 1;
    if (*p == '-') {
        sign = -1;
        p++;
    } else if (*p == '+') {
        p++;
    }
    int val = 0;
    bool has_digits = false;
    while (*p >= '0' && *p <= '9') {
        val = val * 10 + (*p - '0');
        p++;
        has_digits = true;
    }
    if (!has_digits) return nullptr;
    *value = val * sign;
    return p;
}

bool Image::load(const char* filepath) {
    FILE* file = std::fopen(filepath, "rb");
    if (!file) {
        std::cerr << "Error: No se pudo abrir el archivo de entrada: " << filepath << std::endl;
        return false;
    }

    std::fseek(file, 0, SEEK_END);
    long file_size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);

    if (file_size <= 0) {
        std::cerr << "Error: Archivo vacio o invalido: " << filepath << std::endl;
        std::fclose(file);
        return false;
    }

    char* buffer = new char[file_size + 1];
    size_t bytes_read = std::fread(buffer, 1, file_size, file);
    buffer[bytes_read] = '\0';
    std::fclose(file);

    const char* ptr = buffer;

    char magic_token[16];
    ptr = readToken(ptr, magic_token, sizeof(magic_token));
    if (!ptr || (std::strcmp(magic_token, "P2") != 0 && std::strcmp(magic_token, "P3") != 0)) {
        std::cerr << "Error: Formato magico no soportado (se esperaba P2 o P3): "
                  << (ptr ? magic_token : "EOF") << std::endl;
        delete[] buffer;
        return false;
    }

    magic[0] = magic_token[0];
    magic[1] = magic_token[1];
    magic[2] = '\0';
    int detected_channels = (std::strcmp(magic, "P3") == 0) ? 3 : 1;

    int w = 0, h = 0, mc = 0;
    ptr = readInt(ptr, &w);
    if (!ptr) {
        std::cerr << "Error: No se pudo leer el ancho de la imagen." << std::endl;
        delete[] buffer;
        return false;
    }
    ptr = readInt(ptr, &h);
    if (!ptr) {
        std::cerr << "Error: No se pudo leer el alto de la imagen." << std::endl;
        delete[] buffer;
        return false;
    }
    ptr = readInt(ptr, &mc);
    if (!ptr) {
        std::cerr << "Error: No se pudo leer el valor maximo de color." << std::endl;
        delete[] buffer;
        return false;
    }

    if (w <= 0 || h <= 0 || mc <= 0) {
        std::cerr << "Error: Dimensiones o max_color invalidos (" << w << "x" << h << ", max=" << mc << ")" << std::endl;
        delete[] buffer;
        return false;
    }

    freePixels();
    max_color = mc;
    allocate(w, h, detected_channels);

    int total_values = width * height * channels;
    for (int i = 0; i < total_values; ++i) {
        int val = 0;
        ptr = readInt(ptr, &val);
        if (!ptr) {
            std::cerr << "Error: Datos de pixeles truncados en el indice " << i 
                      << " de " << total_values << std::endl;
            delete[] buffer;
            return false;
        }
        pixels[i] = val;
    }

    delete[] buffer;
    return true;
}

bool Image::save(const char* filepath) const {
    FILE* file = std::fopen(filepath, "w");
    if (!file) {
        std::cerr << "Error: No se pudo abrir el archivo para escritura: " << filepath << std::endl;
        return false;
    }

    std::setvbuf(file, nullptr, _IOFBF, 65536);

    std::fprintf(file, "%s\n", magic);
    std::fprintf(file, "%d %d\n", width, height);
    std::fprintf(file, "%d\n", max_color);

    if (channels == 1) {
        // PGM: escala de grises
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                std::fprintf(file, "%d%c", pixels[y * width + x], (x + 1 == width) ? '\n' : ' ');
            }
        }
    } else {
        // PPM: color RGB
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int base = (y * width + x) * 3;
                std::fprintf(file, "%d %d %d%c", pixels[base], pixels[base + 1], pixels[base + 2],
                             (x + 1 == width) ? '\n' : ' ');
            }
        }
    }

    std::fclose(file);
    return true;
}

int Image::getPixel(int x, int y, int c, bool zero_pad) const {
    if (x < 0 || x >= width || y < 0 || y >= height) {
        if (zero_pad) return 0;
        return 0;
    }
    if (c < 0 || c >= channels) return 0;
    return pixels[(y * width + x) * channels + c];
}

void Image::setPixel(int x, int y, int c, int val) {
    if (x >= 0 && x < width && y >= 0 && y < height && c >= 0 && c < channels) {
        pixels[(y * width + x) * channels + c] = val;
    }
}
