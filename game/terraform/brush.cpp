#include "terraform/brush.hpp"

#include <cmath>
#include <limits>

#include "engine/math/algo.hpp"


Brush::Brush()
{

}

Brush::~Brush()
{

}

QImage Brush::preview_image(const unsigned int size) const
{
    std::vector<Brush::density_t> brush;
    preview_buffer(size, brush);
    return brush_preview_to_black_alpha(brush, size);
}


void FunctionalBrush::preview_buffer(
        const unsigned int size,
        std::vector<Brush::density_t> &dest) const
{
    dest.resize(size*size);
    const float factor = 2./size;

    Brush::density_t *dest_ptr = &dest.front();
    for (unsigned int y = 0; y < size; y++) {
        const float yf = y*factor - 1.0f;
        for (unsigned int x = 0; x < size; x++) {
            const float xf = x*factor - 1.0f;
            *dest_ptr++ = sample(xf, yf);
        }
    }
}


Brush::density_t GaussBrush::sample(float x, float y) const
{
    static constexpr float sigma = 0.3f;

    const float r_sqr = sqr(x)+sqr(y);
    /* if (r_sqr > 1.0) {
        return 0.;
    }*/

    return std::exp(-r_sqr/(2.*sqr(sigma)));
}


Brush::density_t ParzenBrush::sample(float x, float y) const
{
    const float r = std::sqrt(sqr(x)+sqr(y));

    if (r > 1) {
        return 0;
    } else if (r >= 0.5) {
        return 2.f*pow(1.f-r, 3.f);
    } else {
        return 1.f+6.f*(pow(r, 2.f)*(r-1.f));
    }
}


Brush::density_t CircleBrush::sample(float x, float y) const
{
    const float r = std::sqrt(sqr(x)+sqr(y));

    if (r > 1) {
        return 0;
    } else {
        return 1;
    }
}


BrushFrontend::BrushFrontend():
    m_curr_brush(nullptr),
    m_brush_size(1),
    m_sampled_valid(false)
{

}

const std::vector<Brush::density_t> &BrushFrontend::sampled()
{
    if (m_sampled_valid) {
        return m_sampled;
    }
    if (!m_curr_brush) {
        m_sampled.resize(0);
        return m_sampled;
    }
    m_curr_brush->preview_buffer(m_brush_size, m_sampled);
    m_sampled_valid = true;
    return m_sampled;
}

void BrushFrontend::set_brush(Brush *brush)
{
    if (brush == m_curr_brush) {
        return;
    }
    m_curr_brush = brush;
    m_sampled_valid = false;
}

void BrushFrontend::set_brush_size(unsigned int size)
{
    if (size == m_brush_size) {
        return;
    }
    m_brush_size = size;
    m_sampled_valid = false;
}

void BrushFrontend::set_brush_strength(float strength)
{
    m_brush_strength = strength;
}


QImage brush_preview_to_black_alpha(const std::vector<Brush::density_t> &buffer,
                                    const unsigned int size)
{
    QImage result(size, size, QImage::Format_ARGB32);
    uint8_t *bits = result.bits();
    const Brush::density_t *density_ptr = buffer.data();
    for (int row = 0; row < (int)size; row++) {
        for (int col = 0; col < (int)size; col++) {
            const uint8_t density = std::round(clamp(*density_ptr++, 0.f, 1.f) * 255);
            *bits++ = 0; // R
            *bits++ = 0; // G
            *bits++ = 0; // B
            *bits++ = density; // alpha
        }
    }
    return result;
}
