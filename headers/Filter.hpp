#ifndef FILTER_HPP
#define FILTER_HPP

#include "Image.hpp"

class Filter {
public:
    virtual ~Filter() {}
    virtual const char* getName() const = 0;
    virtual void apply(const Image& src, Image& dst) const = 0;
};

class ConvolutionFilter : public Filter {
private:
    char name[32];
    float kernel[3][3];
    float divisor;
    float offset;

public:
    ConvolutionFilter(const char* filter_name, const float k[3][3], float div = 1.0f, float off = 0.0f);
    virtual ~ConvolutionFilter() {}

    virtual const char* getName() const;
    virtual void apply(const Image& src, Image& dst) const;

    static ConvolutionFilter createBlur();
    static ConvolutionFilter createSharpen();
    static ConvolutionFilter createLaplace();
};

#endif // FILTER_HPP
