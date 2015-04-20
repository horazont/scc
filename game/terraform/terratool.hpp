/**********************************************************************
File name: terratool.hpp
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
#ifndef SCC_GAME_TERRATOOL_H
#define SCC_GAME_TERRATOOL_H

#include "fixups.hpp"

#include <sigc++/sigc++.h>

#include "engine/sim/terrain.hpp"

#include "terraform/brush.hpp"


enum class TerraToolType
{
    BRUSH = 0,
};


class ToolBackend
{
public:
    ToolBackend(BrushFrontend &brush_frontend,
                sim::Terrain &terrain);
    virtual ~ToolBackend();

private:
    BrushFrontend &m_brush_frontend;
    sim::Terrain &m_terrain;

public:
    inline BrushFrontend &brush_frontend()
    {
        return m_brush_frontend;
    }

    unsigned int terrain_size() const
    {
        return m_terrain.size();;
    }

    inline sim::Terrain &terrain()
    {
        return m_terrain;
    }

};


class TerraTool
{
public:
    TerraTool(ToolBackend &backend);
    virtual ~TerraTool();

protected:
    ToolBackend &m_backend;

    TerraToolType m_type;
    bool m_has_value;
    float m_value;
    std::string m_value_name;

    sigc::signal<void, float> m_value_changed;

public:
    inline sigc::signal<void, float> &value_changed()
    {
        return m_value_changed;
    }

    inline bool has_value() const
    {
        return m_has_value;
    }

    inline const std::string &value_name() const
    {
        return m_value_name;
    }

public:
    virtual void set_value(float new_value);

public:
    virtual void primary(const float x0,
                         const float y0);
    virtual void secondary(const float x0,
                           const float y0);

};


class TerraRaiseLowerTool: public TerraTool
{
public:
    using TerraTool::TerraTool;

public:
    void primary(const float x0, const float y0) override;
    void secondary(const float x0, const float y0) override;

};

class TerraLevelTool: public TerraTool
{
public:
    TerraLevelTool(ToolBackend &backend);

public:
    void primary(const float x0, const float y0) override;
    void secondary(const float x0, const float y0) override;

};

#endif
