/**********************************************************************
File name: brush.hpp
This file is part of: SCC (working title)

LICENSE

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program.  If not, see <http://www.gnu.org/licenses/>.

FEEDBACK & QUESTIONS

For feedback and questions about SCC please e-mail one of the authors named in
the AUTHORS file.
**********************************************************************/
#ifndef GAME_TERRAFORM_BRUSH_H
#define GAME_TERRAFORM_BRUSH_H

#include "fixups.hpp"

#include <cassert>
#include <vector>

#include <QImage>

#include "brush.pb.h"


/**
 * Base class for a brush for use in the map editor.
 *
 * Brushes essentially provide a field of density values (Brush::density_t) over
 * a square area with arbitrary size.
 */
class Brush
{
public:
    typedef float density_t;

public:
    Brush();
    virtual ~Brush();

public:
    /**
     * Render the brush into a square buffer of given size.
     *
     * @param size Size to use for rendering
     * @param dest Buffer to render the values in (the method resizes it as
     * needed)
     */
    virtual void preview_buffer(const unsigned int size,
                                std::vector<density_t> &dest) const = 0;

    /**
     * Render the brush into a square image of given size.
     *
     * @param size Size to use for rendering
     * @return A QImage containing the rendered image. The images properties are
     * the same as for brush_preview_to_black_alpha().
     */
    virtual QImage preview_image(const unsigned int size) const;

};


/**
 * Base class for a brush based on a function.
 */
class FunctionalBrush: public Brush
{
public:
    void preview_buffer(const unsigned int size,
                        std::vector<density_t> &dest) const override;

    /**
     * Sample the function at the given position.
     *
     * @param x Position inside the buffer, normalized to [-1.0, 1.0]
     * @param y Position inside the buffer, normalized to [-1.0, 1.0]
     * @return Density at the given position.
     */
    virtual density_t sample(const float x, const float y) const = 0;

};


/**
 * Gaussian functional brush.
 *
 * As the gaussian does not go to zero at the edges, artifacts will be
 * introduced. It works okay-ish for small changes.
 *
 * @see ParzenBrush
 */
class GaussBrush: public FunctionalBrush
{
public:
    density_t sample(const float x, const float y) const override;

};


/**
 * Brush based on the Parzen window function (de la Vallée Poussin).
 *
 * The shape is similar to GaussBrush, but as the Parzen window goes to zero at
 * the edges, it looks much nicer.
 */
class ParzenBrush: public FunctionalBrush
{
public:
    density_t sample(const float x, const float y) const override;

};


/**
 * Sharp circle brush.
 */
class CircleBrush: public FunctionalBrush
{
public:
    density_t sample(const float x, const float y) const override;

};


/**
 * Brush based on a pixel image.
 *
 * An ImageBrush can be sourced from several sources, see the constructors
 * for some options.
 */
class ImageBrush: public Brush
{
public:
    /**
     * Create a brush by copying the density values from a given buffer.
     *
     * @param size Size of the brush. The \a raw buffer must have size times
     * size values.
     * @param raw Buffer holding the densities of the brush.
     */
    ImageBrush(const unsigned int size,
               const std::vector<Brush::density_t> &raw);

    /**
     * Create a brush by moving the density values from a given buffer.
     *
     * @param size Size of the brush. The \a raw buffer must have size times
     * size values.
     * @param raw Buffer holding the densities of the brush.
     */
    ImageBrush(const unsigned int size, std::vector<Brush::density_t> &&raw);

    /**
     * Create a brush by copying the information from the Protocol Buffers
     * object gamedata::PixelBrushDef.
     *
     * @param brush Stored brush to copy from.
     *
     * @see load_gimp_brush()
     */
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
    void preview_buffer(const unsigned int size,
                        std::vector<density_t> &dest) const override;
    QImage preview_image(const unsigned int size) const override;

};


/**
 * Frontend for brush usage, which tracks the currently active brush, its size
 * and the strength.
 */
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
    /**
     * Return a buffer holding the current brush rendered at the current size.
     *
     * The BrushFrontend caches the sampled buffer until either the brush or
     * the size changes. The strength is not included in the rendered image.
     *
     * @return Buffer holding the current brush rendered at the current size.
     */
    const std::vector<Brush::density_t> &sampled();

    /**
     * Set the current brush
     *
     * @param brush Pointer to a Brush instance.
     */
    void set_brush(Brush *brush);

    /**
     * Change the brush size
     *
     * @param size Brush size
     */
    void set_brush_size(unsigned int size);

    /**
     * Change the brush strength
     *
     * @param strength Brush strength from the interval [0.0, 1.0]
     */
    void set_brush_strength(float strength);

};


/**
 * Try to load a GIMP GBR brush from a buffer.
 *
 * @param data Buffer holding the GBR brush
 * @param size Size of the buffer
 * @param dest Protocol Buffer into which the GBR brush will be stored. The
 * license is set to `"unknown"`.
 *
 * @return true if the brush loaded successfully, false otherwise.
 *
 * @see ImageBrush::ImageBrush(const gamedata::PixelBrushDef&)
 */
bool load_gimp_brush(const uint8_t *data, const unsigned int size,
                     gamedata::PixelBrushDef &dest);


/**
 * Convert a brush density into a black image with the alpha channel set to the
 * density.
 *
 * @param buffer Brush buffer with correct size
 * @param size Size of the brush
 *
 * @return New image with the rendered density.
 */
QImage brush_preview_to_black_alpha(
        const std::vector<Brush::density_t> &buffer,
        const unsigned int size);

#endif
