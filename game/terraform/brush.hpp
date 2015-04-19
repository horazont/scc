#ifndef GAME_TERRAFORM_BRUSH_H
#define GAME_TERRAFORM_BRUSH_H

#include "fixups.hpp"

#include <cassert>
#include <vector>


class Brush
{
public:
    typedef float density_t;

public:
    Brush();
    virtual ~Brush();

public:
    virtual density_t sample(const float x, const float y) const = 0;

};


class BitmapBrush: public Brush
{
public:
    BitmapBrush(const unsigned int base_size);

private:
    unsigned int m_base_size;
    std::vector<density_t> m_pixels;

public:
    inline unsigned int base_size() const
    {
        return m_base_size;
    }

    inline void set(unsigned int x, unsigned int y, density_t value)
    {
        assert(x < m_base_size && y < m_base_size);
        m_pixels[y*m_base_size+x] = value;
    }

    inline density_t get(unsigned int x, unsigned int y) const
    {
        assert(x < m_base_size && y < m_base_size);
        return m_pixels[y*m_base_size+x];
    }

    inline density_t *scanline(unsigned int y)
    {
        return &m_pixels[y*m_base_size];
    }

public:
    density_t sample(float x, float y) const override;

};


class GaussBrush: public Brush
{
public:
    density_t sample(float x, float y) const override;

};


class ParzenBrush: public Brush
{
public:
    density_t sample(float x, float y) const override;

};


class BrushFrontend
{
public:
    BrushFrontend();

private:
    Brush *m_curr_brush;
    unsigned int m_brush_size;
    float m_brush_strength;

    bool m_sampling_valid;
    std::vector<Brush::density_t> m_sampled;

protected:
    void resample();

public:
    inline Brush *curr_brush()
    {
        return m_curr_brush;
    }

    inline unsigned int brush_size() const
    {
        return m_brush_size;
    }

    inline float brush_strength() const
    {
        return m_brush_strength;
    }

public:
    const std::vector<Brush::density_t> &sampled();

public:
    void set_brush(Brush *brush);
    void set_brush_size(unsigned int size);
    void set_brush_strength(float strength);

};

#endif
