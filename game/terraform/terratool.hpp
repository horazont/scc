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

#include "ffengine/sim/terrain.hpp"
#include "ffengine/sim/world.hpp"

#include "ffengine/render/scenegraph.hpp"
#include "ffengine/render/curve.hpp"

#include "terraform/brush.hpp"


enum class TerraToolType
{
    BRUSH = 0,
};


class ToolBackend
{
public:
    ToolBackend(BrushFrontend &brush_frontend,
                const sim::WorldState &world);
    virtual ~ToolBackend();

private:
    BrushFrontend &m_brush_frontend;
    const sim::WorldState &m_world;
    engine::scenegraph::OctGroup *m_sgnode;

public:
    inline BrushFrontend &brush_frontend()
    {
        return m_brush_frontend;
    }

    inline const sim::WorldState &world() const
    {
        return m_world;
    }

    inline engine::scenegraph::OctGroup *sgnode()
    {
        return m_sgnode;
    }

    void set_sgnode(engine::scenegraph::OctGroup *sgnode);

    std::pair<bool, sim::Terrain::height_t> lookup_height(
            const float x, const float y,
            const sim::Terrain::HeightField *field = nullptr);

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
    virtual sim::WorldOperationPtr primary_start(const float x0,
                                                 const float y0);
    virtual sim::WorldOperationPtr primary(const float x0,
                                           const float y0);
    virtual sim::WorldOperationPtr secondary_start(const float x0,
                                                   const float y0);
    virtual sim::WorldOperationPtr secondary(const float x0,
                                             const float y0);

};


class TerraRaiseLowerTool: public TerraTool
{
public:
    using TerraTool::TerraTool;

public:
    sim::WorldOperationPtr primary(const float x0, const float y0) override;
    sim::WorldOperationPtr secondary(const float x0, const float y0) override;

};

class TerraLevelTool: public TerraTool
{
public:
    TerraLevelTool(ToolBackend &backend);

public:
    sim::WorldOperationPtr primary(const float x0, const float y0) override;
    sim::WorldOperationPtr secondary_start(const float x0, const float y0) override;

};

class TerraSmoothTool: public TerraTool
{
public:
    using TerraTool::TerraTool;

public:
    sim::WorldOperationPtr primary(const float x0, const float y0) override;

};

class TerraRampTool: public TerraTool
{
public:
    using TerraTool::TerraTool;

private:
    Vector3f m_destination_point;
    Vector3f m_source_point;

protected:
    void reference_point(const float x0, const float y0, Vector3f &dest);

public:
    sim::WorldOperationPtr primary_start(const float x0, const float y0) override;
    sim::WorldOperationPtr primary(const float x0, const float y0) override;
    sim::WorldOperationPtr secondary_start(const float x0, const float y0) override;

};

class TerraFluidRaiseTool: public TerraTool
{
public:
    using TerraTool::TerraTool;

public:
    sim::WorldOperationPtr primary(const float x0, const float y0) override;
    sim::WorldOperationPtr secondary(const float x0, const float y0) override;

};

class TerraTestingTool: public TerraTool
{
public:
    TerraTestingTool(ToolBackend &backend);

private:
    engine::QuadBezier3fDebug *m_debug_node;
    unsigned int m_step;
    QuadBezier3f m_tmp_curve;
    engine::Material *m_preview_material;
    engine::Material *m_road_material;

public:
    void set_preview_material(engine::Material &material);
    void set_road_material(engine::Material &material);

public:
    sim::WorldOperationPtr primary_start(const float x0, const float y0) override;
    sim::WorldOperationPtr secondary_start(const float x0, const float y0) override;

};

#endif
