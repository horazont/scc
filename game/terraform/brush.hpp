#ifndef GAME_TERRAFORM_BRUSH_H
#define GAME_TERRAFORM_BRUSH_H

#include "fixups.hpp"

#include <cassert>
#include <vector>

#include <QImage>


class Brush
{
public:
    typedef float density_t;

public:
    Brush();
    virtual ~Brush();

public:
    virtual void preview_buffer(const unsigned int size,
                                std::vector<density_t> &dest) const = 0;
    virtual QImage preview_image(const unsigned int size) const;

};


class FunctionalBrush: public Brush
{
public:
    void preview_buffer(const unsigned int size,
                        std::vector<density_t> &dest) const override;
    virtual density_t sample(const float x, const float y) const = 0;

};


class GaussBrush: public FunctionalBrush
{
public:
    density_t sample(const float x, const float y) const override;

};


class ParzenBrush: public FunctionalBrush
{
public:
    density_t sample(const float x, const float y) const override;

};


class CircleBrush: public FunctionalBrush
{
public:
    density_t sample(const float x, const float y) const override;

};


class BrushFrontend
{
public:
    BrushFrontend();

private:
    Brush *m_curr_brush;
    unsigned int m_brush_size;
    float m_brush_strength;

    bool m_sampled_valid;
    std::vector<Brush::density_t> m_sampled;

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
    void set_brush(Brush *brush);
    void set_brush_size(unsigned int size);
    void set_brush_strength(float strength);

};


QImage brush_preview_to_black_alpha(const std::vector<Brush::density_t> &buffer,
                                    const unsigned int size);

#endif
