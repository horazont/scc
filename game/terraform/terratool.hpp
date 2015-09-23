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

#include <QCursor>

#include "ffengine/math/curve.hpp"

#include "ffengine/sim/signals.hpp"
#include "ffengine/sim/terrain.hpp"
#include "ffengine/sim/world.hpp"
#include "ffengine/sim/network.hpp"

#include "ffengine/render/scenegraph.hpp"

#include "terraform/brush.hpp"
#include "terraform/drag.hpp"

namespace ffe {

class FluidSource;
class FluidSourceMaterial;
class QuadBezier3fDebug;
class Material;
class DebugNodes;

}

using ToolDragPtr = std::unique_ptr<AbstractToolDrag>;

enum class TerraToolType
{
    BRUSH = 0,
};


struct HoverState
{
    HoverState();
    HoverState(const Vector3f &world_cursor);
    HoverState(const Vector3f &world_cursor, const QCursor &cursor_override);
    HoverState(const QCursor &cursor_override);

    bool m_enable_cursor_override;
    QCursor m_cursor_override;

    bool m_world_cursor_valid;
    Vector3f m_world_cursor;
};


class ToolBackend
{
public:
    ToolBackend(BrushFrontend &brush_frontend,
                sim::SignalQueue &signal_queue,
                const sim::WorldState &world,
                ffe::scenegraph::Group &sgroot,
                ffe::scenegraph::OctreeGroup &sgoctree,
                ffe::PerspectivalCamera &camera);
    virtual ~ToolBackend();

private:
    BrushFrontend &m_brush_frontend;
    sim::SignalQueue &m_signal_queue;
    const sim::WorldState &m_world;
    ffe::scenegraph::Group &m_sgroot;
    ffe::scenegraph::OctreeGroup &m_sgoctree;
    ffe::PerspectivalCamera &m_camera;
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

    inline const ffe::PerspectivalCamera &camera() const
    {
        return m_camera;
    }

    inline const Vector2f &viewport_size() const
    {
        return m_viewport_size;
    }

    inline ffe::scenegraph::Group &sgroot()
    {
        return m_sgroot;
    }

    inline ffe::scenegraph::OctreeGroup &sgoctree()
    {
        return m_sgoctree;
    }

    inline sim::SignalQueue &signal_queue()
    {
        return m_signal_queue;
    }

    inline Ray view_ray(const Vector2f &viewport_pos) const
    {
        return m_camera.ray(viewport_pos, m_viewport_size);
    }

    std::pair<ffe::OctreeObject*, float> hittest_octree_object(
            const Ray &ray,
            const std::function<bool(const ffe::OctreeObject &)> &predicate);
    std::tuple<Vector3f, bool> hittest_terrain(const Ray &ray);

    void set_viewport_size(const Vector2f &size);

    std::pair<bool, sim::Terrain::height_t> lookup_height(
            const float x, const float y,
            const sim::Terrain::Field *field = nullptr);

    template <typename callable_t, typename... arg_ts>
    inline sig11::connection_guard<void(arg_ts...)> connect_to_signal(
            sig11::signal<void(arg_ts...)> &signal,
            callable_t &&receiver)
    {
        return m_signal_queue.connect_queued(signal, std::forward<callable_t>(receiver));
    }
};


class AbstractTerraTool: public QObject
{
    Q_OBJECT
public:
    explicit AbstractTerraTool(ToolBackend &backend);

protected:
    ToolBackend &m_backend;

    bool m_uses_brush;
    bool m_uses_hover;

public:
    inline bool uses_brush() const
    {
        return m_uses_brush;
    }

    inline bool uses_hover() const
    {
        return m_uses_hover;
    }

public:
    virtual void activate();
    virtual void deactivate();

    virtual HoverState hover(const Vector2f &viewport_cursor);

    virtual std::pair<ToolDragPtr, sim::WorldOperationPtr> primary_start(
            const Vector2f &viewport_cursor);

    virtual std::pair<ToolDragPtr, sim::WorldOperationPtr> secondary_start(
            const Vector2f &viewport_cursor);

};


class TerrainToolDrag: public AbstractToolDrag
{
public:
    using DragCallback = std::function<sim::WorldOperationPtr(const Vector2f&, const Vector3f&)>;
    using DoneCallback = std::function<sim::WorldOperationPtr(const Vector2f&, const Vector3f&)>;

public:
    TerrainToolDrag(const sim::Terrain &terrain,
                    const ffe::PerspectivalCamera &camera,
                    const Vector2f &viewport_size,
                    DragCallback &&drag_cb,
                    DoneCallback &&done_cb = nullptr);

private:
    const sim::Terrain &m_terrain;
    const ffe::PerspectivalCamera &m_camera;
    const Vector2f &m_viewport_size;
    DragCallback m_drag_cb;
    DoneCallback m_done_cb;

private:
    Vector3f raycast(const Vector2f &viewport_pos);

public:
    sim::WorldOperationPtr done(const Vector2f &viewport_pos) override;
    sim::WorldOperationPtr drag(const Vector2f &viewport_pos) override;

};


class TerrainBrushTool: public AbstractTerraTool
{
    Q_OBJECT
public:
    explicit TerrainBrushTool(ToolBackend &backend);

public:
    std::pair<ToolDragPtr, sim::WorldOperationPtr> primary_start(
            const Vector2f &viewport_cursor) override;
    virtual sim::WorldOperationPtr primary_move(
            const Vector2f &viewport_cursor,
            const Vector3f &world_cursor);

    std::pair<ToolDragPtr, sim::WorldOperationPtr> secondary_start(
            const Vector2f &viewport_cursor) override;
    virtual sim::WorldOperationPtr secondary_move(
            const Vector2f &viewport_cursor,
            const Vector3f &world_cursor);
};


class TerraRaiseLowerTool: public TerrainBrushTool
{
    Q_OBJECT
public:
    using TerrainBrushTool::TerrainBrushTool;

public:
    sim::WorldOperationPtr primary_move(const Vector2f &viewport_cursor,
                                        const Vector3f &world_cursor) override;
    sim::WorldOperationPtr secondary_move(const Vector2f &viewport_cursor,
                                          const Vector3f &world_cursor) override;

};

class TerraLevelTool: public TerrainBrushTool
{
    Q_OBJECT
public:
    explicit TerraLevelTool(ToolBackend &backend);

private:
    float m_reference_height;

public:
    sim::WorldOperationPtr primary_move(const Vector2f &viewport_cursor,
                                        const Vector3f &world_cursor) override;
    std::pair<ToolDragPtr, sim::WorldOperationPtr> secondary_start(
            const Vector2f &viewport_cursor) override;

signals:
    void reference_height_changed(float new_reference_height);

public:
    inline float reference_height() const
    {
        return m_reference_height;
    }

    void set_reference_height(float value);

};

class TerraSmoothTool: public TerrainBrushTool
{
    Q_OBJECT
public:
    using TerrainBrushTool::TerrainBrushTool;

public:
    sim::WorldOperationPtr primary_move(const Vector2f &viewport_cursor,
                                        const Vector3f &world_cursor) override;
    std::pair<ToolDragPtr, sim::WorldOperationPtr> secondary_start(
            const Vector2f &viewport_cursor) override;

};

class TerraRampTool: public TerrainBrushTool
{
    Q_OBJECT
public:
    using TerrainBrushTool::TerrainBrushTool;

private:
    Vector3f m_destination_point;
    Vector3f m_source_point;

public:
    std::pair<ToolDragPtr, sim::WorldOperationPtr> primary_start(
            const Vector2f &viewport_cursor) override;
    sim::WorldOperationPtr primary_move(const Vector2f &viewport_cursor,
                                        const Vector3f &world_cursor) override;
    std::pair<ToolDragPtr, sim::WorldOperationPtr> secondary_start(
            const Vector2f &viewport_cursor) override;

};

class TerraFluidRaiseTool: public TerrainBrushTool
{
    Q_OBJECT
public:
    using TerrainBrushTool::TerrainBrushTool;

public:
    sim::WorldOperationPtr primary_move(const Vector2f &viewport_cursor,
                                        const Vector3f &world_cursor) override;
    sim::WorldOperationPtr secondary_move(const Vector2f &viewport_cursor,
                                          const Vector3f &world_cursor) override;

};


class TerraFluidSourceTool: public AbstractTerraTool
{
    Q_OBJECT
public:
    enum Control
    {
        HEIGHT = 0,
        POSITION = 1
    };

public:
    TerraFluidSourceTool(ToolBackend &backend,
                         ffe::FluidSourceMaterial &material,
                         ffe::Material &drag_plane_material);

private:
    ffe::FluidSourceMaterial &m_material;
    ffe::Material &m_drag_plane_material;
    ffe::FluidSource *m_selected_source_vis;
    sim::object_ptr<sim::Fluid::Source> m_selected_source;

    float m_selected_height;
    float m_selected_capacity;

    ffe::scenegraph::OctGroup *m_visualisation_group;

    sim::WorldState::FluidSourceSignal::guard_t m_fluid_source_added_connection;
    sim::WorldState::FluidSourceSignal::guard_t m_fluid_source_changed_connection;
    sim::WorldState::FluidSourceSignal::guard_t m_fluid_source_removed_connection;

    std::unordered_map<sim::Object::ID, ffe::FluidSource*> m_source_visualisations;

protected:
    void add_source(const sim::Fluid::Source *source);
    std::pair<ffe::FluidSource*, Control> find_fluid_source(const Vector2f &viewport_cursor);
    void on_fluid_source_added(sim::object_ptr<sim::Fluid::Source> source);
    void on_fluid_source_changed(sim::object_ptr<sim::Fluid::Source> source);
    void on_fluid_source_removed(sim::object_ptr<sim::Fluid::Source> source);
    void remove_visualisation(ffe::FluidSource *vis);
    void select_source(ffe::FluidSource *vis, sim::object_ptr<sim::Fluid::Source> &&ptr);

signals:
    void selected_capacity_changed(float value);
    void selected_height_changed(float value);
    void selection_changed(bool has_selected);

public:
    sim::WorldOperationPtr set_selected_capacity(float value);
    sim::WorldOperationPtr set_selected_height(float value);

public:
    void activate() override;
    void deactivate() override;
    HoverState hover(const Vector2f &viewport_cursor) override;
    std::pair<ToolDragPtr, sim::WorldOperationPtr> primary_start(
            const Vector2f &viewport_cursor) override;
    std::pair<ToolDragPtr, sim::WorldOperationPtr> secondary_start(
            const Vector2f &viewport_cursor) override;

};


class TerraFluidOceanLevelTool: public AbstractTerraTool
{
    Q_OBJECT
public:
    TerraFluidOceanLevelTool(ToolBackend &backend,
                             ffe::Material &drag_plane_material);

private:
    ffe::Material &m_drag_plane_material;

public:
    HoverState hover(const Vector2f &viewport_cursor) override;
    std::pair<ToolDragPtr, sim::WorldOperationPtr> primary_start(
            const Vector2f &viewport_cursor) override;

};


class TerraTestingTool: public AbstractTerraTool
{
    Q_OBJECT
public:
    TerraTestingTool(ToolBackend &backend,
                     ffe::Material &preview_material,
                     ffe::Material &road_material,
                     ffe::Material &node_debug_material,
                     ffe::Material &edge_bundle_debug_material);

private:
    ffe::QuadBezier3fDebug *m_debug_node;
    unsigned int m_step;
    QuadBezier3f m_tmp_curve;
    sim::object_ptr<sim::PhysicalNode> m_start_node;
    sim::object_ptr<sim::PhysicalNode> m_end_node;
    ffe::Material &m_preview_material;
    ffe::Material &m_road_material;
    ffe::Material &m_edge_bundle_debug_material;
    ffe::DebugNodes &m_node_debug_node;

    sig11::connection_guard<void(sim::object_ptr<sim::PhysicalEdgeBundle>)> m_edge_bundle_created;
    sig11::connection_guard<void(sim::object_ptr<sim::PhysicalNode>)> m_node_created;

private:
    void on_edge_bundle_created(sim::object_ptr<sim::PhysicalEdgeBundle> bundle);
    void on_node_created(sim::object_ptr<sim::PhysicalNode> node);

protected:
    std::tuple<bool, sim::object_ptr<sim::PhysicalNode>, Vector3f> snapped_point(const Vector2f &viewport_cursor);

public:
    HoverState hover(const Vector2f &viewport_cursor) override;
    std::pair<ToolDragPtr, sim::WorldOperationPtr> primary_start(
            const Vector2f &viewport_cursor) override;
    std::pair<ToolDragPtr, sim::WorldOperationPtr> secondary_start(
            const Vector2f &viewport_cursor) override;

};

#endif
