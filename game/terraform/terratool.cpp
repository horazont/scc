/**********************************************************************
File name: terratool.cpp
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
#define QT_NO_EMIT

#include "terraform/terratool.hpp"

#include "ffengine/math/algo.hpp"
#include "ffengine/math/intersect.hpp"

#include "ffengine/sim/world_ops.hpp"
#include "ffengine/sim/network.hpp"

#include "ffengine/render/curve.hpp"
#include "ffengine/render/fancyterraindata.hpp"
#include "ffengine/render/fluidsource.hpp"
#include "ffengine/render/renderpass.hpp"
#include "ffengine/render/network_debug.hpp"


HoverState::HoverState():
    m_enable_cursor_override(false),
    m_world_cursor_valid(false)
{

}

HoverState::HoverState(const Vector3f &world_cursor):
    m_enable_cursor_override(false),
    m_world_cursor_valid(true),
    m_world_cursor(world_cursor)
{

}

HoverState::HoverState(const QCursor &cursor_override):
    m_enable_cursor_override(true),
    m_cursor_override(cursor_override),
    m_world_cursor_valid(false)
{

}


ToolBackend::ToolBackend(BrushFrontend &brush_frontend,
                         sim::SignalQueue &signal_queue,
                         const sim::WorldState &world,
                         ffe::scenegraph::Group &sgroot,
                         ffe::scenegraph::OctreeGroup &sgoctree,
                         ffe::PerspectivalCamera &camera):
    m_brush_frontend(brush_frontend),
    m_signal_queue(signal_queue),
    m_world(world),
    m_sgroot(sgroot),
    m_sgoctree(sgoctree),
    m_camera(camera)
{

}

ToolBackend::~ToolBackend()
{

}

std::pair<ffe::OctreeObject *, float> ToolBackend::hittest_octree_object(const Ray &ray,
        const std::function<bool (const ffe::OctreeObject &)> &predicate)
{
    m_sgoctree.octree().select_nodes_by_ray(ray, m_ray_hitset);
    std::sort(m_ray_hitset.begin(), m_ray_hitset.end());

    ffe::OctreeObject *closest_object = nullptr;
    float closest = std::numeric_limits<float>::max();

    for (auto iter = m_ray_hitset.begin();
         iter != m_ray_hitset.end();
         ++iter)
    {
        if (iter->tmin >= closest) {
            continue;
        }

        for (auto obj_iter = iter->node->cbegin();
             obj_iter != iter->node->cend();
             ++obj_iter)
        {
            ffe::OctreeObject *obj = *obj_iter;
            if (!predicate(*obj)) {
                continue;
            }

            float tmin = 0;
            bool hit = obj->isect_ray(ray, tmin);
            if (!hit) {
                continue;
            }

            if (tmin < closest) {
                closest_object = obj;
                closest = tmin;
            }
        }
    }

    m_ray_hitset.clear();

    return std::make_pair(closest_object, closest);
}

std::tuple<Vector3f, bool> ToolBackend::hittest_terrain(const Ray &ray)
{
    const sim::Terrain::Field *field;
    auto lock = m_world.terrain().readonly_field(field);
    return ffe::isect_terrain_ray(ray, m_world.terrain().size(), *field);
}

void ToolBackend::set_viewport_size(const Vector2f &size)
{
    m_viewport_size = size;
}

std::pair<bool, sim::Terrain::height_t> ToolBackend::lookup_height(
        const float x, const float y,
        const sim::Terrain::Field *field)
{
    const unsigned int terrain_size = m_world.terrain().size();

    const int terrainx = std::round(x);
    const int terrainy = std::round(y);

    if (terrainx < 0 || terrainx >= (int)terrain_size ||
            terrainy < 0 || terrainy >= (int)terrain_size)
    {
        return std::make_pair(false, 0);
    }

    std::shared_lock<std::shared_timed_mutex> lock;
    if (!field) {
        lock = m_world.terrain().readonly_field(field);
    }

    return std::make_pair(true, (*field)[terrainy*terrain_size+terrainx][sim::Terrain::HEIGHT_ATTR]);
}

/* AbstractTerraTool */

AbstractTerraTool::AbstractTerraTool(ToolBackend &backend):
    m_backend(backend),
    m_uses_brush(false),
    m_uses_hover(false)
{

}

void AbstractTerraTool::activate()
{

}

void AbstractTerraTool::deactivate()
{

}

HoverState AbstractTerraTool::hover(const Vector2f &)
{
    return HoverState();
}

std::pair<ToolDragPtr, sim::WorldOperationPtr> AbstractTerraTool::primary_start(const Vector2f &)
{
    return std::make_pair(nullptr, nullptr);
}

std::pair<ToolDragPtr, sim::WorldOperationPtr> AbstractTerraTool::secondary_start(const Vector2f &)
{
    return std::make_pair(nullptr, nullptr);
}


/* TerrainToolDrag */

TerrainToolDrag::TerrainToolDrag(const sim::Terrain &terrain,
                                 const ffe::PerspectivalCamera &camera,
                                 const Vector2f &viewport_size,
                                 TerrainToolDrag::DragCallback &&drag_cb,
                                 TerrainToolDrag::DoneCallback &&done_cb):
    AbstractToolDrag(true),
    m_terrain(terrain),
    m_camera(camera),
    m_viewport_size(viewport_size),
    m_drag_cb(std::move(drag_cb)),
    m_done_cb(std::move(done_cb))
{

}

Vector3f TerrainToolDrag::raycast(const Vector2f &viewport_pos)
{
    const Ray ray(m_camera.ray(viewport_pos, m_viewport_size));
    Vector3f pos;
    bool hit;
    {
        const sim::Terrain::Field *field;
        auto lock = m_terrain.readonly_field(field);
        std::tie(pos, hit) = ffe::isect_terrain_ray(ray, m_terrain.size(), *field);
    }

    if (hit) {
        return pos;
    }

    return Vector3f(NAN, NAN, NAN);
}

sim::WorldOperationPtr TerrainToolDrag::done(const Vector2f &viewport_pos)
{
    if (!m_done_cb) {
        return nullptr;
    }
    return m_done_cb(viewport_pos, raycast(viewport_pos));
}

sim::WorldOperationPtr TerrainToolDrag::drag(const Vector2f &viewport_pos)
{
    return m_drag_cb(viewport_pos, raycast(viewport_pos));
}


/* TerrainBrushTool */

TerrainBrushTool::TerrainBrushTool(ToolBackend &backend):
    AbstractTerraTool(backend)
{
    m_uses_brush = true;
    m_uses_hover = true;
}

std::pair<ToolDragPtr, sim::WorldOperationPtr> TerrainBrushTool::primary_start(const Vector2f &)
{
    return std::make_pair(std::make_unique<TerrainToolDrag>(
                              m_backend.world().terrain(),
                              m_backend.camera(),
                              m_backend.viewport_size(),
                              std::bind(&TerrainBrushTool::primary_move,
                                        this,
                                        std::placeholders::_1,
                                        std::placeholders::_2),
                              nullptr),
                          nullptr);
}

sim::WorldOperationPtr TerrainBrushTool::primary_move(
        const Vector2f &,
        const Vector3f &)
{
    return nullptr;
}

std::pair<ToolDragPtr, sim::WorldOperationPtr> TerrainBrushTool::secondary_start(const Vector2f &)
{
    return std::make_pair(std::make_unique<TerrainToolDrag>(
                              m_backend.world().terrain(),
                              m_backend.camera(),
                              m_backend.viewport_size(),
                              std::bind(&TerrainBrushTool::secondary_move,
                                        this,
                                        std::placeholders::_1,
                                        std::placeholders::_2),
                              nullptr),
                          nullptr);
}

sim::WorldOperationPtr TerrainBrushTool::secondary_move(
        const Vector2f &,
        const Vector3f &)
{
    return nullptr;
}


sim::WorldOperationPtr TerraRaiseLowerTool::primary_move(
        const Vector2f &, const Vector3f &world_cursor)
{
    if (!m_backend.brush_frontend().curr_brush()) {
        return nullptr;
    }

    return std::make_unique<sim::ops::TerraformRaise>(
                world_cursor[eX], world_cursor[eY],
                m_backend.brush_frontend().brush_size(),
                m_backend.brush_frontend().sampled(),
                m_backend.brush_frontend().brush_strength());
}

sim::WorldOperationPtr TerraRaiseLowerTool::secondary_move(
        const Vector2f &, const Vector3f &world_cursor)
{
    if (!m_backend.brush_frontend().curr_brush()) {
        return nullptr;
    }

    return std::make_unique<sim::ops::TerraformRaise>(
                world_cursor[eX], world_cursor[eY],
                m_backend.brush_frontend().brush_size(),
                m_backend.brush_frontend().sampled(),
                -m_backend.brush_frontend().brush_strength());
}


/* TerraLevelTool */

TerraLevelTool::TerraLevelTool(ToolBackend &backend):
    TerrainBrushTool(backend)
{

}

sim::WorldOperationPtr TerraLevelTool::primary_move(
        const Vector2f &, const Vector3f &world_cursor)
{
    if (!m_backend.brush_frontend().curr_brush()) {
        return nullptr;
    }

    return std::make_unique<sim::ops::TerraformLevel>(
                world_cursor[eX], world_cursor[eY],
                m_backend.brush_frontend().brush_size(),
                m_backend.brush_frontend().sampled(),
                m_backend.brush_frontend().brush_strength(),
                m_reference_height);
}

std::pair<ToolDragPtr, sim::WorldOperationPtr> TerraLevelTool::secondary_start(
        const Vector2f &viewport_pos)
{
    bool valid;
    Vector3f world_pos;
    std::tie(world_pos, valid) = m_backend.hittest_terrain(m_backend.view_ray(viewport_pos));
    if (valid) {
        set_reference_height(world_pos[eZ]);
    }
    return std::make_pair(nullptr, nullptr);
}

void TerraLevelTool::set_reference_height(float value)
{
    if (m_reference_height == value) {
        return;
    }
    m_reference_height = value;
    Q_EMIT(reference_height_changed(value));
}


/* TerraSmoothTool */

sim::WorldOperationPtr TerraSmoothTool::primary_move(
        const Vector2f &,
        const Vector3f &world_cursor)
{
    if (!m_backend.brush_frontend().curr_brush()) {
        return nullptr;
    }

    return std::make_unique<sim::ops::TerraformSmooth>(
                world_cursor[eX], world_cursor[eY],
                m_backend.brush_frontend().brush_size(),
                m_backend.brush_frontend().sampled(),
                m_backend.brush_frontend().brush_strength());
}

std::pair<ToolDragPtr, sim::WorldOperationPtr> TerraSmoothTool::secondary_start(const Vector2f &)
{
    return std::make_pair(nullptr, nullptr);
}


/* TerraRampTool */

std::pair<ToolDragPtr, sim::WorldOperationPtr> TerraRampTool::primary_start(
        const Vector2f &viewport_pos)
{
    bool valid;
    Vector3f world_pos;
    std::tie(world_pos, valid) = m_backend.hittest_terrain(m_backend.view_ray(viewport_pos));
    if (valid) {
        m_source_point = world_pos;
        return TerrainBrushTool::primary_start(viewport_pos);
    } else {
        return std::make_pair(nullptr, nullptr);
    }
}

sim::WorldOperationPtr TerraRampTool::primary_move(
        const Vector2f &,
        const Vector3f &world_cursor)
{
    if (!m_backend.brush_frontend().curr_brush()) {
        return nullptr;
    }

    return std::make_unique<sim::ops::TerraformRamp>(
                world_cursor[eX], world_cursor[eY],
                m_backend.brush_frontend().brush_size(),
                m_backend.brush_frontend().sampled(),
                m_backend.brush_frontend().brush_strength(),
                Vector2f(m_source_point),
                m_source_point[eZ],
                Vector2f(m_destination_point),
                m_destination_point[eZ]);
}

std::pair<ToolDragPtr, sim::WorldOperationPtr> TerraRampTool::secondary_start(
        const Vector2f &viewport_pos)
{
    bool valid;
    Vector3f world_pos;
    std::tie(world_pos, valid) = m_backend.hittest_terrain(m_backend.view_ray(viewport_pos));
    if (valid) {
        m_destination_point = world_pos;
    }
    return std::make_pair(nullptr, nullptr);
}


/* TerraFluidRaiseTool */

sim::WorldOperationPtr TerraFluidRaiseTool::primary_move(
        const Vector2f &,
        const Vector3f &world_cursor)
{
    if (!m_backend.brush_frontend().curr_brush()) {
        return nullptr;
    }

    return std::make_unique<sim::ops::FluidRaise>(
                world_cursor[eX]-0.5, world_cursor[eY]-0.5,
                m_backend.brush_frontend().brush_size(),
                m_backend.brush_frontend().sampled(),
                m_backend.brush_frontend().brush_strength());
}

sim::WorldOperationPtr TerraFluidRaiseTool::secondary_move(
        const Vector2f &,
        const Vector3f &world_cursor)
{
    if (!m_backend.brush_frontend().curr_brush()) {
        return nullptr;
    }

    return std::make_unique<sim::ops::FluidRaise>(
                world_cursor[eX]-0.5, world_cursor[eY]-0.5,
                m_backend.brush_frontend().brush_size(),
                m_backend.brush_frontend().sampled(),
                -m_backend.brush_frontend().brush_strength());
}


/* FluidSourceDrag */

class FluidSourceDrag: public TerrainToolDrag
{
public:
    FluidSourceDrag(ToolBackend &backend,
                    const sim::object_ptr<sim::Fluid::Source> &source,
                    const unsigned int terrain_size):
        TerrainToolDrag(backend.world().terrain(),
                        backend.camera(),
                        backend.viewport_size(),
                        std::bind(&FluidSourceDrag::plane_drag,
                                  this,
                                  std::placeholders::_1,
                                  std::placeholders::_2)),
        m_backend(backend),
        m_source(std::move(source)),
        m_terrain_size(terrain_size)
    {
        set_cursor(Qt::SizeAllCursor);
    }

private:
    ToolBackend m_backend;
    sim::object_ptr<sim::Fluid::Source> m_source;
    unsigned int m_terrain_size;

private:
    sim::WorldOperationPtr plane_drag(const Vector2f &,
                                      const Vector3f &world_pos)
    {
        if (std::isnan(world_pos[eX])) {
            return nullptr;
        }

        if (!m_source) {
            return nullptr;
        }

        if (m_source->m_pos == Vector2f(world_pos)) {
            return nullptr;
        }

        return std::make_unique<sim::ops::FluidSourceMove>(
                    m_source.object_id(), world_pos[eX], world_pos[eY]);
    }

};


/* FluidSourceResize */

class FluidSourceResize: public PlaneToolDrag
{
public:
    FluidSourceResize(ToolBackend &backend,
                      const Vector3f &plane_normal,
                      const Vector2f &viewport_cursor,
                      ffe::Material &plane_material,
                      const sim::object_ptr<sim::Fluid::Source> &source,
                      const unsigned int terrain_size):
        PlaneToolDrag(Plane(Vector3f(source->m_pos, 0), plane_normal),
                      backend.camera(),
                      backend.viewport_size(),
                      std::bind(&FluidSourceResize::plane_drag,
                                this,
                                std::placeholders::_1,
                                std::placeholders::_2)),
        m_backend(backend),
        m_plane(&backend.sgroot().emplace<ffe::PlaneNode>(Plane(Vector3f(source->m_pos, source->m_absolute_height), Vector3f(0, 0, 1)), plane_material)),
        m_source(std::move(source)),
        m_terrain_size(terrain_size),
        m_original_height(m_source->m_absolute_height),
        m_original_pos(raycast(viewport_cursor))
    {
        set_cursor(Qt::SizeVerCursor);
    }

    ~FluidSourceResize()
    {
        delete_plane();
    }

private:
    ToolBackend m_backend;
    ffe::PlaneNode *m_plane;
    sim::object_ptr<sim::Fluid::Source> m_source;
    unsigned int m_terrain_size;
    float m_original_height;
    Vector3f m_original_pos;

private:
    void delete_plane()
    {
        if (!m_plane) {
            return;
        }

        ffe::scenegraph::Group &group = m_backend.sgroot();

        auto iter = std::find_if(group.begin(),
                                 group.end(),
                                 [this](const ffe::scenegraph::Node &node){ return &node == m_plane; });
        assert(iter != group.end());
        group.erase(iter);
        m_plane = nullptr;
    }

private:
    sim::WorldOperationPtr plane_drag(const Vector2f &,
                                      const Vector3f &world_pos)
    {
        if (!m_source) {
            return nullptr;
        }

        bool _;
        float curr_terrain_height;
        {
            const sim::Terrain::Field *field;
            auto lock = m_backend.world().terrain().readonly_field(field);
            std::tie(_, curr_terrain_height) = m_backend.lookup_height(
                        m_source->m_pos[eX],
                        m_source->m_pos[eY],
                        field);
        }

        float new_height = std::max(m_original_height + world_pos[eZ] - m_original_pos[eZ], curr_terrain_height);

        m_plane->set_plane(Plane(Vector3f(m_source->m_pos, new_height), Vector3f(0, 0, 1)));

        if (m_source->m_absolute_height == new_height) {
            return nullptr;
        }

        return std::make_unique<sim::ops::FluidSourceSetHeight>(
                    m_source.object_id(),
                    new_height);
    }

public:
    sim::WorldOperationPtr done(const Vector2f &viewport_pos) override
    {
        delete_plane();
        return PlaneToolDrag::done(viewport_pos);
    }

};


/* TerraFluidSourceTool */

TerraFluidSourceTool::TerraFluidSourceTool(ToolBackend &backend,
                                           ffe::FluidSourceMaterial &material,
                                           ffe::Material &drag_plane_material):
    AbstractTerraTool(backend),
    m_material(material),
    m_drag_plane_material(drag_plane_material),
    m_selected_source_vis(nullptr),
    m_selected_height(0.f),
    m_selected_capacity(1.f),
    m_visualisation_group(nullptr)
{
    m_uses_hover = true;
    m_uses_brush = false;
}

void TerraFluidSourceTool::add_source(const sim::Fluid::Source *source)
{
    ffe::FluidSource *vis = &m_visualisation_group->emplace<ffe::FluidSource>(m_material);
    vis->set_source(source);
    vis->update_from_source();
    m_source_visualisations[source->object_id()] = vis;

    if (!m_selected_source) {
        select_source(vis, m_backend.world().objects().share<sim::Fluid::Source>(source->object_id()));
    }
}

std::pair<ffe::FluidSource *, TerraFluidSourceTool::Control> TerraFluidSourceTool::find_fluid_source(const Vector2f &viewport_cursor)
{
    const Ray r = m_backend.view_ray(viewport_cursor);

    ffe::OctreeObject *hit;
    float t;

    std::tie(hit, t) = m_backend.hittest_octree_object(r, [](const ffe::OctreeObject &obj){ return bool(dynamic_cast<const ffe::FluidSource*>(&obj)); });

    if (hit) {
        ffe::FluidSource *source_vis = static_cast<ffe::FluidSource*>(hit);
        const sim::Fluid::Source *source = source_vis->source();
        const Vector3f pos = r.origin + r.direction*t;
        const Vector3f center_pos = Vector3f(source->m_pos, pos[eZ]);

        const Vector3f from_center = pos - center_pos;

        Control ctrl;
        if (from_center.length() < source->m_radius / 3.f) {
            ctrl = POSITION;
        } else {
            const Vector3f viewdir = (-r.direction).normalized();
            float angle = from_center.normalized() * viewdir;
            // std::cout << from_center.normalized() << " " << angle << " " << viewdir << std::endl;
            ctrl = (angle > 0.5 ? HEIGHT : POSITION);
        }
        return std::make_pair(source_vis, ctrl);
    }

    return std::make_pair(nullptr, HEIGHT);
}

void TerraFluidSourceTool::on_fluid_source_added(sim::object_ptr<sim::Fluid::Source> source)
{
    if (!m_fluid_source_added_connection) {
        return;
    }
    if (!source) {
        return;
    }
    add_source(source.get());
}

void TerraFluidSourceTool::on_fluid_source_changed(sim::object_ptr<sim::Fluid::Source> source)
{
    if (!m_fluid_source_added_connection) {
        return;
    }
    if (!source) {
        return;
    }
    auto iter = m_source_visualisations.find(source.object_id());
    if (iter == m_source_visualisations.end()) {
        return;
    }

    iter->second->update_from_source();

    if (m_selected_source.object_id() == source.object_id()) {
        if (m_selected_height != source->m_absolute_height) {
            m_selected_height = source->m_absolute_height;
            Q_EMIT(selected_height_changed(m_selected_height));
        }
        if (m_selected_capacity != source->m_capacity) {
            m_selected_capacity = source->m_capacity;
            Q_EMIT(selected_capacity_changed(m_selected_capacity));
        }
    }
}

void TerraFluidSourceTool::on_fluid_source_removed(sim::object_ptr<sim::Fluid::Source> source)
{
    auto iter = m_source_visualisations.find(source.object_id());
    if (iter == m_source_visualisations.end()) {
        return;
    }

    remove_visualisation(iter->second);
    if (m_selected_source_vis == iter->second) {
        select_source(nullptr, nullptr);
    }
    m_source_visualisations.erase(iter);
}

void TerraFluidSourceTool::remove_visualisation(ffe::FluidSource *vis)
{
    auto iter = std::find_if(m_visualisation_group->begin(),
                             m_visualisation_group->end(),
                             [vis](const ffe::scenegraph::OctNode &item){ return &item == vis; });
    m_visualisation_group->erase(iter);
}

void TerraFluidSourceTool::select_source(
        ffe::FluidSource *vis,
        sim::object_ptr<sim::Fluid::Source> &&ptr)
{
    m_selected_source_vis = vis;
    m_selected_source = std::move(ptr);

    if (m_selected_source) {
        Q_EMIT(selection_changed(true));
        m_selected_height = m_selected_source->m_absolute_height;
        Q_EMIT(selected_height_changed(m_selected_height));
        m_selected_capacity = m_selected_source->m_capacity;
        Q_EMIT(selected_capacity_changed(m_selected_height));
    } else {
        Q_EMIT(selection_changed(false));
    }
}

sim::WorldOperationPtr TerraFluidSourceTool::set_selected_capacity(float value)
{
    if (!m_selected_source) {
        return nullptr;
    }

    return std::make_unique<sim::ops::FluidSourceSetCapacity>(m_selected_source.object_id(), value);
}

sim::WorldOperationPtr TerraFluidSourceTool::set_selected_height(float value)
{
    if (!m_selected_source) {
        return nullptr;
    }

    return std::make_unique<sim::ops::FluidSourceSetHeight>(m_selected_source.object_id(), value);
}

void TerraFluidSourceTool::activate()
{
    m_visualisation_group = &m_backend.sgoctree().root().emplace<ffe::scenegraph::OctGroup>();

    for (sim::Fluid::Source *source: m_backend.world().fluid().sources())
    {
        add_source(source);
    }

    m_fluid_source_added_connection = m_backend.connect_to_signal(
                m_backend.world().fluid_source_added(),
                std::bind(&TerraFluidSourceTool::on_fluid_source_added,
                          this,
                          std::placeholders::_1));

    m_fluid_source_changed_connection = m_backend.connect_to_signal(
                m_backend.world().fluid_source_changed(),
                std::bind(&TerraFluidSourceTool::on_fluid_source_changed,
                          this,
                          std::placeholders::_1));

    m_fluid_source_removed_connection = m_backend.connect_to_signal(
                m_backend.world().fluid_source_removed(),
                std::bind(&TerraFluidSourceTool::on_fluid_source_removed,
                          this,
                          std::placeholders::_1));

    Q_EMIT(selected_capacity_changed(m_selected_capacity));
}

void TerraFluidSourceTool::deactivate()
{
    m_fluid_source_added_connection = nullptr;
    m_fluid_source_changed_connection = nullptr;
    m_fluid_source_removed_connection = nullptr;
    m_selected_source = nullptr;
    m_selected_source_vis = nullptr;
    ffe::scenegraph::OctGroup &parent = m_backend.sgoctree().root();
    auto iter = std::find_if(parent.begin(),
                             parent.end(),
                             [this](const ffe::scenegraph::OctNode &item){return &item == m_visualisation_group;});
    parent.erase(iter);
    m_visualisation_group = nullptr;
    m_source_visualisations.clear();
}

HoverState TerraFluidSourceTool::hover(const Vector2f &viewport_cursor)
{
    if (m_selected_source_vis) {
        m_selected_source_vis->set_ui_state(ffe::UI_STATE_SELECTED);
    }

    ffe::FluidSource *obj;
    Control control;
    std::tie(obj, control) = find_fluid_source(viewport_cursor);

    if (obj) {
        if (obj != m_selected_source_vis) {
            obj->set_ui_state(ffe::UI_STATE_HOVER);
            return HoverState(Qt::PointingHandCursor);
        }
        switch (control)
        {
        case POSITION:
        {
            return HoverState(Qt::SizeAllCursor);
        }
        case HEIGHT:
        {
            return HoverState(Qt::SizeVerCursor);
        }
        }
    }

    return HoverState();
}

std::pair<ToolDragPtr, sim::WorldOperationPtr> TerraFluidSourceTool::primary_start(
        const Vector2f &viewport_cursor)
{
    ffe::FluidSource *obj;
    Control control;
    std::tie(obj, control) = find_fluid_source(viewport_cursor);

    if (obj) {
        obj->set_ui_state(ffe::UI_STATE_SELECTED);
        if (obj != m_selected_source_vis) {
            select_source(obj, m_backend.world().objects().share<sim::Fluid::Source>(obj->source()->object_id()));
            return std::make_pair(nullptr, nullptr);
        }

        switch (control)
        {
        case HEIGHT:
        {
            Vector3f plane_normal(-m_backend.view_ray(viewport_cursor).direction);
            plane_normal[eZ] = 0;
            plane_normal.normalize();
            return std::make_pair(std::make_unique<FluidSourceResize>(m_backend,
                                                                      plane_normal,
                                                                      viewport_cursor,
                                                                      m_drag_plane_material,
                                                                      m_selected_source,
                                                                      m_backend.world().terrain().size()),
                                  nullptr);
        }
        case POSITION:
        {
            return std::make_pair(std::make_unique<FluidSourceDrag>(m_backend,
                                                                    m_selected_source,
                                                                    m_backend.world().terrain().size()),
                                  nullptr);
        }
        }

    } else {
        bool valid;
        Vector3f world_pos;
        std::tie(world_pos, valid) = m_backend.hittest_terrain(m_backend.view_ray(viewport_cursor));

        if (valid) {
            // clear selection so that new source will be selected automatically
            select_source(nullptr, nullptr);
            return std::make_pair(nullptr,
                                  std::make_unique<sim::ops::FluidSourceCreate>(
                                      world_pos[eX], world_pos[eY],
                                      5.f,
                                      world_pos[eZ] + 2.f,
                                      m_selected_capacity));
        }
    }


    return std::make_pair(nullptr, nullptr);
}

std::pair<ToolDragPtr, sim::WorldOperationPtr> TerraFluidSourceTool::secondary_start(
        const Vector2f &viewport_cursor)
{
    ffe::FluidSource *obj;
    Control _;
    std::tie(obj, _) = find_fluid_source(viewport_cursor);

    if (obj) {
        if (obj->source()) {
            return std::make_pair(nullptr, std::make_unique<sim::ops::FluidSourceDestroy>(obj->source()->object_id()));
        }
    }

    return std::make_pair(nullptr, nullptr);
}


/* FluidOceanLevelMove */

class FluidOceanLevelMove: public PlaneToolDrag
{
public:
    FluidOceanLevelMove(ToolBackend &backend,
                        const Vector3f &plane_origin,
                        const Vector3f &plane_normal,
                        const Vector2f &viewport_cursor,
                        ffe::Material &plane_material,
                        const float initial_level):
        PlaneToolDrag(Plane(plane_origin, plane_normal),
                      backend.camera(),
                      backend.viewport_size(),
                      std::bind(&FluidOceanLevelMove::plane_drag,
                                this,
                                std::placeholders::_1,
                                std::placeholders::_2)),
        m_backend(backend),
        m_plane(&backend.sgroot().emplace<ffe::PlaneNode>(Plane(Vector3f(0, 0, initial_level), Vector3f(0, 0, 1)), plane_material)),
        m_original_height(initial_level),
        m_original_pos(raycast(viewport_cursor))
    {
        set_cursor(Qt::SizeVerCursor);
    }

    ~FluidOceanLevelMove()
    {
        delete_plane();
    }

private:
    ToolBackend m_backend;
    ffe::PlaneNode *m_plane;
    float m_original_height;
    Vector3f m_original_pos;

private:
    void delete_plane()
    {
        if (!m_plane) {
            return;
        }

        ffe::scenegraph::Group &group = m_backend.sgroot();

        auto iter = std::find_if(group.begin(),
                                 group.end(),
                                 [this](const ffe::scenegraph::Node &node){ return &node == m_plane; });
        assert(iter != group.end());
        group.erase(iter);
        m_plane = nullptr;
    }

private:
    sim::WorldOperationPtr plane_drag(const Vector2f &,
                                      const Vector3f &world_pos)
    {
        float new_height = m_original_height - m_original_pos[eZ] + world_pos[eZ];

        new_height = clamp(new_height,
                           sim::Terrain::min_height - 1.f,
                           sim::Terrain::max_height);

        m_plane->set_plane(Plane(Vector3f(0, 0, new_height), Vector3f(0, 0, 1)));

        return std::make_unique<sim::ops::FluidOceanLevelSetHeight>(new_height);
    }

public:
    sim::WorldOperationPtr done(const Vector2f &viewport_pos) override
    {
        delete_plane();
        return PlaneToolDrag::done(viewport_pos);
    }

};


/* TerraFluidOceanLevelTool */

TerraFluidOceanLevelTool::TerraFluidOceanLevelTool(ToolBackend &backend,
                                                   ffe::Material &drag_plane_material):
    AbstractTerraTool(backend),
    m_drag_plane_material(drag_plane_material)
{
    m_uses_hover = true;
}

HoverState TerraFluidOceanLevelTool::hover(const Vector2f &)
{
    return HoverState(Qt::SizeVerCursor);
}

std::pair<ToolDragPtr, sim::WorldOperationPtr> TerraFluidOceanLevelTool::primary_start(
        const Vector2f &viewport_cursor)
{
    const Ray &ray = m_backend.view_ray(viewport_cursor);
    const float curr_level = m_backend.world().fluid().ocean_level();
    float t;
    PlaneSide side;
    std::tie(t, side) = isect_plane_ray(Plane(Vector3f(0, 0, curr_level),
                                              Vector3f(0, 0, 1)),
                                        ray);
    const Vector3f plane_origin(ray.origin + ray.direction*t);


    Vector3f plane_normal(-ray.direction);
    plane_normal[eZ] = 0;
    plane_normal.normalize();
    return std::make_pair(
                std::make_unique<FluidOceanLevelMove>(m_backend,
                                                      plane_origin,
                                                      plane_normal,
                                                      viewport_cursor,
                                                      m_drag_plane_material,
                                                      curr_level),
                nullptr);
}


/* TerraTestingTool::TerraTestingTool */

TerraTestingTool::TerraTestingTool(ToolBackend &backend,
                                   ffe::Material &preview_material,
                                   ffe::Material &road_material,
                                   ffe::Material &node_debug_material,
                                   ffe::Material &edge_bundle_debug_material):

    AbstractTerraTool(backend),
    m_debug_node(nullptr),
    m_step(0),
    m_preview_material(preview_material),
    m_road_material(road_material),
    m_edge_bundle_debug_material(edge_bundle_debug_material),
    m_node_debug_node(m_backend.sgoctree().root().emplace<ffe::DebugNodes>(node_debug_material)),
    m_edge_bundle_created(backend.connect_to_signal(
                              backend.world().graph().edge_bundle_created(),
                              std::bind(&TerraTestingTool::on_edge_bundle_created,
                                        this,
                                        std::placeholders::_1))),
    m_node_created(backend.connect_to_signal(
                       backend.world().graph().node_created(),
                       std::bind(&TerraTestingTool::on_node_created,
                                 this,
                                 std::placeholders::_1)))
{
    m_uses_brush = false;
    m_uses_hover = true;
}

void TerraTestingTool::on_edge_bundle_created(sim::object_ptr<sim::PhysicalEdgeBundle> bundle)
{
    ffe::scenegraph::OctGroup &group = m_backend.sgoctree().root();
    group.emplace<ffe::DebugEdgeBundle>(m_edge_bundle_debug_material, m_backend.signal_queue(), m_backend.world().graph(), bundle);
    std::cout << "edge bundle created" << std::endl;
}

void TerraTestingTool::on_node_created(sim::object_ptr<sim::PhysicalNode> node)
{
    m_node_debug_node.register_node(node);
    std::cout << "node created" << std::endl;
}

std::tuple<bool, sim::object_ptr<sim::PhysicalNode>, Vector3f> TerraTestingTool::snapped_point(const Vector2f &viewport_cursor)
{
    const Ray r = m_backend.view_ray(viewport_cursor);

    ffe::OctreeObject *abstract_obj;
    float t;
    std::tie(abstract_obj, t) = m_backend.hittest_octree_object(r, [](const ffe::OctreeObject &obj){ return bool(dynamic_cast<const ffe::DebugNode*>(&obj)); });

    ffe::DebugNode *obj = static_cast<ffe::DebugNode*>(abstract_obj);
    if (obj && obj->node()) {
        return std::make_tuple(true, obj->node(), obj->node()->position());
    }

    bool valid;
    Vector3f p;
    std::tie(p, valid) = m_backend.hittest_terrain(r);
    return std::make_tuple(valid, nullptr, p);
}

HoverState TerraTestingTool::hover(const Vector2f &viewport_cursor)
{
    switch (m_step)
    {
    case 0:
    {
        auto potential_result = snapped_point(viewport_cursor);
        if (std::get<0>(potential_result)) {
            return HoverState(std::get<2>(potential_result));
        }
        break;
    }
    case 1:
    {
        bool valid;
        Vector3f world_pos;
        const Ray r = m_backend.view_ray(viewport_cursor);
        std::tie(world_pos, valid) = m_backend.hittest_terrain(r);
        if (valid) {
            m_tmp_curve.p_control = world_pos;
            m_tmp_curve.p_end = world_pos;
            m_debug_node->set_curve(m_tmp_curve);
        }
        break;
    }
    case 2:
    {
        auto potential_result = snapped_point(viewport_cursor);
        if (std::get<0>(potential_result)) {
            m_tmp_curve.p_end = std::get<2>(potential_result);
            m_debug_node->set_curve(m_tmp_curve);
            return HoverState(std::get<2>(potential_result));
        }
        break;
    }
    default:;
    }
    return AbstractTerraTool::hover(viewport_cursor);
}

std::pair<ToolDragPtr, sim::WorldOperationPtr> TerraTestingTool::primary_start(const Vector2f &viewport_cursor)
{
    switch (m_step)
    {
    case 1:
    {
        bool valid;
        Vector3f world_pos;
        const Ray r = m_backend.view_ray(viewport_cursor);
        std::tie(world_pos, valid) = m_backend.hittest_terrain(r);
        if (valid) {
            m_tmp_curve.p_control = world_pos;
            m_tmp_curve.p_end = world_pos;
            m_debug_node->set_curve(m_tmp_curve);
        }
        m_step += 1;
        break;
    }
    case 2:
    {
        bool valid;
        Vector3f p;
        std::tie(valid, m_end_node, p) = snapped_point(viewport_cursor);
        if (!valid) {
            return std::make_pair(nullptr, nullptr);
        }

        ffe::scenegraph::OctGroup &group = m_backend.sgoctree().root();

        m_tmp_curve.p_end = p;
        m_debug_node->set_curve(m_tmp_curve);
        auto iter = std::find_if(group.begin(), group.end(), [this](ffe::scenegraph::OctNode &node){ return &node == m_debug_node; });
        group.erase(iter);
        m_debug_node = nullptr;
        m_step = 0;
        return std::make_pair(nullptr, std::make_unique<sim::ops::ConstructNewCurve>(
                                  m_tmp_curve.p_start,
                                  m_start_node,
                                  m_tmp_curve.p_control,
                                  m_tmp_curve.p_end,
                                  m_end_node));
        break;
    }
    default:
    {
        bool valid;
        Vector3f p;
        std::tie(valid, m_start_node, p) = snapped_point(viewport_cursor);
        if (!valid) {
            return std::make_pair(nullptr, nullptr);
        }

        assert(!m_debug_node);
        assert(m_preview_material);
        m_debug_node = &m_backend.sgoctree().root().emplace<ffe::QuadBezier3fDebug>(m_preview_material, 20);
        m_tmp_curve = QuadBezier3f(p, p, p);
        m_debug_node->set_curve(m_tmp_curve);
        m_step += 1;
        break;
    }
    }

    return std::make_pair(nullptr, nullptr);
}

std::pair<ToolDragPtr, sim::WorldOperationPtr> TerraTestingTool::secondary_start(const Vector2f &)
{
    if (m_step > 0) {
        assert(m_debug_node);
        ffe::scenegraph::OctGroup &group = m_backend.sgoctree().root();
        auto iter = std::find_if(group.begin(), group.end(), [this](ffe::scenegraph::OctNode &node){ return &node == m_debug_node; });
        group.erase(iter);
        m_step = 0;
        m_debug_node = nullptr;
    }
    return std::make_pair(nullptr, nullptr);
}
