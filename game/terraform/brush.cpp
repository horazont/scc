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


BitmapBrush::BitmapBrush(const unsigned int base_size):
    m_base_size(base_size),
    m_pixels(base_size*base_size, 0)
{
    assert(m_base_size < (unsigned int)std::numeric_limits<int>::max());
}

BitmapBrush::density_t BitmapBrush::sample(float x, float y) const
{
    /* std::cout << x << " " << y << " "; */

    x = (x + 1.0)*(m_base_size-1) / 2.0;
    y = (y + 1.0)*(m_base_size-1) / 2.0;

    /* std::cout << x << " " << y << std::endl; */

    int x0 = std::trunc(x);
    int y0 = std::trunc(y);
    float xfrac = frac(x);
    float yfrac = frac(y);
    assert(x0 >= 0 && x0 < (int)m_base_size && y0 >= 0 && y0 < (int)m_base_size);
    assert(xfrac == 0 || x0 < (int)m_base_size-1);
    assert(yfrac == 0 || y0 < (int)m_base_size-1);

    density_t x0y0 = get(x0, y0);
    density_t x1y0 = get(std::ceil(x0 + xfrac), y0);
    density_t x0y1 = get(x0, std::ceil(y0 + yfrac));
    density_t x1y1 = get(std::ceil(x0 + xfrac), std::ceil(y0 + yfrac));

    density_t density_x0 = interp_linear(x0y0, x0y1, yfrac);
    density_t density_x1 = interp_linear(x1y0, x1y1, yfrac);

    return interp_linear(density_x0, density_x1, xfrac);
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


BrushFrontend::BrushFrontend():
    m_curr_brush(nullptr),
    m_brush_size(1),
    m_sampling_valid(false)
{

}

void BrushFrontend::resample()
{
    m_sampled.resize(m_brush_size*m_brush_size, 0.);
    if (!m_curr_brush) {
        return;
    }

    const float factor = 2./m_brush_size;

    Brush::density_t *dest_ptr = &m_sampled.front();
    for (unsigned int y = 0; y < m_brush_size; y++) {
        const float yf = y*factor - 1.0f;
        for (unsigned int x = 0; x < m_brush_size; x++) {
            const float xf = x*factor - 1.0f;
            *dest_ptr++ = m_curr_brush->sample(xf, yf);
        }
    }
}

const std::vector<Brush::density_t> &BrushFrontend::sampled()
{
    if (m_sampling_valid) {
        return m_sampled;
    }
    resample();
    m_sampling_valid = true;
    return m_sampled;
}

void BrushFrontend::set_brush(Brush *brush)
{
    if (brush == m_curr_brush) {
        return;
    }
    m_curr_brush = brush;
    m_sampling_valid = false;
}

void BrushFrontend::set_brush_size(unsigned int size)
{
    if (size == m_brush_size) {
        return;
    }
    m_brush_size = size;
    m_sampling_valid = false;
}

void BrushFrontend::set_brush_strength(float strength)
{
    m_brush_strength = strength;
}

