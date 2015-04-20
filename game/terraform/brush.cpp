#include "terraform/brush.hpp"

#include <cmath>
#include <limits>

#include "engine/math/algo.hpp"


static io::Logger &logger = io::logging().get_logger("app.terraform.brush");

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


ImageBrush::ImageBrush(const unsigned int size,
                       const std::vector<Brush::density_t> &raw):
    m_size(size),
    m_raw(raw),
    m_as_image(size, size, QImage::Format_ARGB32)
{
    update_image();
}

ImageBrush::ImageBrush(const unsigned int size,
                       std::vector<Brush::density_t> &&raw):
    m_size(size),
    m_raw(std::move(raw)),
    m_as_image(size, size, QImage::Format_ARGB32)
{
    update_image();
}

ImageBrush::ImageBrush(const gamedata::PixelBrushDef &brush):
    m_size(brush.size()),
    m_raw(m_size*m_size),
    m_as_image()
{
    memcpy(&m_raw.front(), brush.data().data(), sizeof(float)*m_size*m_size);
    update_image();
}

Brush::density_t ImageBrush::sample(float x, float y) const
{
    x = (x + 1.0)*(m_size-1) / 2.0;
    y = (y + 1.0)*(m_size-1) / 2.0;

    /* std::cout << x << " " << y << std::endl; */

    int x0 = std::trunc(x);
    int y0 = std::trunc(y);
    float xfrac = frac(x);
    float yfrac = frac(y);
    assert(x0 >= 0 && x0 < (int)m_size && y0 >= 0 && y0 < (int)m_size);
    assert(xfrac == 0 || x0 < (int)m_size-1);
    assert(yfrac == 0 || y0 < (int)m_size-1);

    density_t x0y0 = get(x0, y0);
    density_t x1y0 = get(std::ceil(x0 + xfrac), y0);
    density_t x0y1 = get(x0, std::ceil(y0 + yfrac));
    density_t x1y1 = get(std::ceil(x0 + xfrac), std::ceil(y0 + yfrac));

    density_t density_x0 = interp_cos(x0y0, x0y1, yfrac);
    density_t density_x1 = interp_cos(x1y0, x1y1, yfrac);
    return interp_cos(density_x0, density_x1, xfrac);
}

void ImageBrush::update_image()
{
    m_as_image = brush_preview_to_black_alpha(m_raw, m_size);
}

void ImageBrush::preview_buffer(const unsigned int size, std::vector<density_t> &dest) const
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

QImage ImageBrush::preview_image(const unsigned int size) const
{
    return m_as_image.scaled(size, size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
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
