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

#include "ffengine/math/curve.hpp"

#include "ffengine/sim/terrain.hpp"
#include "ffengine/sim/world.hpp"

#include "ffengine/render/scenegraph.hpp"

#include "terraform/brush.hpp"

namespace ffe {

class QuadBezier3fDebug;
class Material;

}

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
    ffe::scenegraph::OctreeGroup *m_sgnode;
    ffe::PerspectivalCamera *m_camera;
    Vector2f m_viewport_size;

    std::vector<ffe::OctreeRayHitInfo> m_ray_hitset;

public:
    inline BrushFrontend &brush_frontend()
    {
        return m_brush_frontend;
    }

    inline const sim::WorldState &world() const
    {
        return m_world;
    }

    inline ffe::scenegraph::OctreeGroup *sgnode()
    {
        return m_sgnode;
    }

    inline Ray view_ray(const Vector2f &viewport_pos) const
    {
        return m_camera->ray(viewport_pos, m_viewport_size);
    }

    ffe::OctreeObject *hittest_octree_object(
            const Ray &ray,
            const std::function<bool(const ffe::OctreeObject &)> &predicate);

    void set_camera(ffe::PerspectivalCamera &camera);
    void set_sgnode(ffe::scenegraph::OctreeGroup &sgnode);
    void set_viewport_size(const Vector2f &size);

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

protected:
    bool m_uses_brushes;
    bool m_uses_hover;

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

    inline bool uses_brushes() const
    {
        return m_uses_brushes;
    }

    inline bool uses_hover() const
    {
        return m_uses_hover;
    }

public:
    virtual void set_value(float new_value);

public:
    virtual std::pair<bool, Vector3f> hover(const Vector2f &viewport_cursor,
                                            const Vector3f &world_cursor);
    virtual sim::WorldOperationPtr primary_start(const Vector2f &viewport_cursor,
                                                 const Vector3f &world_cursor);
    virtual sim::WorldOperationPtr primary(const Vector2f &viewport_cursor,
                                           const Vector3f &world_cursor);
    virtual sim::WorldOperationPtr secondary_start(const Vector2f &viewport_cursor,
                                                   const Vector3f &world_cursor);
    virtual sim::WorldOperationPtr secondary(const Vector2f &viewport_cursor,
                                             const Vector3f &world_cursor);

};


class TerrainBrushTool: public TerraTool
{
public:
    explicit TerrainBrushTool(ToolBackend &backend);

};


class TerraRaiseLowerTool: public TerrainBrushTool
{
public:
    using TerrainBrushTool::TerrainBrushTool;

public:
    sim::WorldOperationPtr primary(const Vector2f &viewport_cursor,
                                   const Vector3f &world_cursor) override;
    sim::WorldOperationPtr secondary(const Vector2f &viewport_cursor,
                                     const Vector3f &world_cursor) override;

};

class TerraLevelTool: public TerrainBrushTool
{
public:
    explicit TerraLevelTool(ToolBackend &backend);

public:
    sim::WorldOperationPtr primary(const Vector2f &viewport_cursor,
                                   const Vector3f &world_cursor) override;
    sim::WorldOperationPtr secondary_start(const Vector2f &viewport_cursor,
                                           const Vector3f &world_cursor) override;

};

class TerraSmoothTool: public TerrainBrushTool
{
public:
    using TerrainBrushTool::TerrainBrushTool;

public:
    sim::WorldOperationPtr primary(const Vector2f &viewport_cursor,
                                   const Vector3f &world_cursor) override;

};

class TerraRampTool: public TerrainBrushTool
{
public:
    using TerrainBrushTool::TerrainBrushTool;

private:
    Vector3f m_destination_point;
    Vector3f m_source_point;

public:
    sim::WorldOperationPtr primary_start(const Vector2f &viewport_cursor,
                                         const Vector3f &world_cursor) override;
    sim::WorldOperationPtr primary(const Vector2f &viewport_cursor,
                                   const Vector3f &world_cursor) override;
    sim::WorldOperationPtr secondary_start(const Vector2f &viewport_cursor,
                                           const Vector3f &world_cursor) override;

};

class TerraFluidRaiseTool: public TerrainBrushTool
{
public:
    using TerrainBrushTool::TerrainBrushTool;

public:
    sim::WorldOperationPtr primary(const Vector2f &viewport_cursor,
                                   const Vector3f &world_cursor) override;
    sim::WorldOperationPtr secondary(const Vector2f &viewport_cursor,
                                     const Vector3f &world_cursor) override;

};

class TerraTestingTool: public TerraTool
{
public:
    TerraTestingTool(ToolBackend &backend);

private:
    ffe::QuadBezier3fDebug *m_debug_node;
    unsigned int m_step;
    QuadBezier3f m_tmp_curve;
    ffe::Material *m_preview_material;
    ffe::Material *m_road_material;

protected:
    void add_segment(const QuadBezier3f &curve);
    void add_segmentized();
    std::pair<bool, Vector3f> snapped_point(const Vector2f &viewport_cursor,
                                            const Vector3f &world_cursor);

public:
    void set_preview_material(ffe::Material &material);
    void set_road_material(ffe::Material &material);

public:
    std::pair<bool, Vector3f> hover(const Vector2f &viewport_cursor,
                                    const Vector3f &world_cursor) override;
    sim::WorldOperationPtr primary_start(const Vector2f &viewport_cursor,
                                         const Vector3f &world_cursor) override;
    sim::WorldOperationPtr secondary_start(const Vector2f &viewport_cursor,
                                           const Vector3f &world_cursor) override;

};

#endif
