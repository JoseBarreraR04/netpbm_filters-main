#ifndef IMAGE_HPP
#define IMAGE_HPP

class Image {
private:
    char magic[3];     // "P2" (PGM) o "P3" (PPM)
    int width;
    int height;
    int max_color;
    int channels;      // 1 para PGM, 3 para PPM
    int* pixels;       // Arreglo dinámico lineal de tamaño width * height * channels

    void allocate(int w, int h, int c);
    void freePixels();

public:
    Image();
    Image(int w, int h, int max_c, const char* m);
    Image(const Image& other);
    Image& operator=(const Image& other);
    ~Image();

    bool load(const char* filepath);
    bool save(const char* filepath) const;

    int getWidth() const { return width; }
    int getHeight() const { return height; }
    int getMaxColor() const { return max_color; }
    int getChannels() const { return channels; }
    const char* getMagic() const { return magic; }

    int getPixel(int x, int y, int c = 0, bool zero_pad = false) const;
    void setPixel(int x, int y, int c, int val);

    int* getPixelData() { return pixels; }
    const int* getPixelData() const { return pixels; }
    int getTotalValues() const { return width * height * channels; }
};

#endif // IMAGE_HPP
