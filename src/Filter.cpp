#include "Filter.hpp"
#include <cstring>
#include <cmath>

ConvolutionFilter::ConvolutionFilter(const char* filter_name, const float k[3][3], float div, float off)
    : divisor(div), offset(off) {
    std::strncpy(name, filter_name, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';

    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            kernel[r][c] = k[r][c];
        }
    }
}

const char* ConvolutionFilter::getName() const {
    return name;
}

void ConvolutionFilter::apply(const Image& src, Image& dst) const {
    int w = src.getWidth();
    int h = src.getHeight();
    int channels = src.getChannels();
    int max_c = src.getMaxColor();

    dst = Image(w, h, max_c, src.getMagic());

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            for (int c = 0; c < channels; ++c) {
                float sum = 0.0f;

                for (int ky = -1; ky <= 1; ++ky) {
                    for (int kx = -1; kx <= 1; ++kx) {
                        int nx = x + kx;
                        int ny = y + ky;

                        // Zero-padding: valores fuera de la imagen se tratan como 0
                        int pixel_val = 0;
                        if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                            pixel_val = src.getPixel(nx, ny, c);
                        }

                        sum += pixel_val * kernel[ky + 1][kx + 1];
                    }
                }

                float computed = (sum / divisor) + offset;
                int result = static_cast<int>(std::round(computed));

                // Clamping directo a [0, max_color]
                if (result < 0) {
                    result = 0;
                } else if (result > max_c) {
                    result = max_c;
                }

                dst.setPixel(x, y, c, result);
            }
        }
    }
}

ConvolutionFilter ConvolutionFilter::createBlur() {
    const float k[3][3] = {
        {1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f}
    };
    return ConvolutionFilter("blur", k, 9.0f, 0.0f);
}

ConvolutionFilter ConvolutionFilter::createSharpen() {
    const float k[3][3] = {
        { 0.0f, -1.0f,  0.0f},
        {-1.0f,  5.0f, -1.0f},
        { 0.0f, -1.0f,  0.0f}
    };
    return ConvolutionFilter("sharpen", k, 1.0f, 0.0f);
}

ConvolutionFilter ConvolutionFilter::createLaplace() {
    const float k[3][3] = {
        {-1.0f, -1.0f, -1.0f},
        {-1.0f,  8.0f, -1.0f},
        {-1.0f, -1.0f, -1.0f}
    };
    return ConvolutionFilter("laplace", k, 1.0f, 0.0f);
}
