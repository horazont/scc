#ifndef GAME_TERRAFORM_BRUSH_H
#define GAME_TERRAFORM_BRUSH_H

#include "fixups.hpp"

#include <cassert>
#include <vector>

#include <QImage>

#include "brush.pb.h"


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


class ImageBrush: public Brush
{
public:
    ImageBrush(const unsigned int size, const std::vector<Brush::density_t> &raw);
    ImageBrush(const unsigned int size, std::vector<Brush::density_t> &&raw);
    explicit ImageBrush(const gamedata::PixelBrushDef &brush);

private:
    const unsigned int m_size;
    std::vector<Brush::density_t> m_raw;
    QImage m_as_image;

protected:
    inline density_t get(unsigned int x, unsigned int y) const
    {
        return m_raw[y*m_size+x];
    }

    density_t sample(float x, float y) const;
    void update_image();

public:
    void preview_buffer(const unsigned int size, std::vector<density_t> &dest) const override;
    QImage preview_image(const unsigned int size) const override;

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
