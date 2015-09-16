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

#include "ffengine/render/curve.hpp"
#include "ffengine/render/fancyterraindata.hpp"
#include "ffengine/render/fluidsource.hpp"
#include "ffengine/render/renderpass.hpp"


ToolBackend::ToolBackend(BrushFrontend &brush_frontend,
                         const sim::WorldState &world,
                         ffe::scenegraph::OctreeGroup &sgoctree,
                         ffe::PerspectivalCamera &camera,
                         std::mutex &queue_mutex,
                         std::vector<std::function<void()> > &queue_vector):
    m_brush_frontend(brush_frontend),
    m_world(world),
    m_sgnode(sgoctree),
    m_camera(camera),
    m_queue_mutex(queue_mutex),
    m_queue(queue_vector)
{

}

ToolBackend::~ToolBackend()
{

}

ffe::OctreeObject *ToolBackend::hittest_octree_object(const Ray &ray,
        const std::function<bool (const ffe::OctreeObject &)> &predicate)
{
    m_sgnode.octree().select_nodes_by_ray(ray, m_ray_hitset);
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
            }
        }
    }

    m_ray_hitset.clear();

    return closest_object;
}

void ToolBackend::set_viewport_size(const Vector2f &size)
{
    m_viewport_size = size;
}

std::pair<bool, sim::Terrain::height_t> ToolBackend::lookup_height(
        const float x, const float y,
        const sim::Terrain::HeightField *field)
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

    return std::make_pair(true, (*field)[terrainy*terrain_size+terrainx]);
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

std::pair<bool, Vector3f> AbstractTerraTool::hover(
        const Vector2f &,
        const Vector3f &world_cursor)
{
    return std::make_pair(true, world_cursor);
}

std::pair<ToolDragPtr, sim::WorldOperationPtr> AbstractTerraTool::primary_start(
        const Vector2f &, const Vector3f &)
{
    return std::make_pair(nullptr, nullptr);
}

std::pair<ToolDragPtr, sim::WorldOperationPtr> AbstractTerraTool::secondary_start(
        const Vector2f &, const Vector3f &)
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
        const sim::Terrain::HeightField *field;
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

std::pair<ToolDragPtr, sim::WorldOperationPtr> TerrainBrushTool::primary_start(
        const Vector2f &, const Vector3f &)
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

std::pair<ToolDragPtr, sim::WorldOperationPtr> TerrainBrushTool::secondary_start(
        const Vector2f &, const Vector3f &)
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
        const Vector2f &, const Vector3f &world_cursor)
{
    set_reference_height(world_cursor[eZ]);
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

std::pair<ToolDragPtr, sim::WorldOperationPtr> TerraSmoothTool::secondary_start(
        const Vector2f &,
        const Vector3f &)
{
    return std::make_pair(nullptr, nullptr);
}


/* TerraRampTool */

std::pair<ToolDragPtr, sim::WorldOperationPtr> TerraRampTool::primary_start(
        const Vector2f &viewport_pos,
        const Vector3f &world_cursor)
{
    m_source_point = world_cursor;
    return TerrainBrushTool::primary_start(viewport_pos, world_cursor);
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
        const Vector2f &,
        const Vector3f &world_cursor)
{
    m_destination_point = world_cursor;
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


/* TerraFluidSourceTool */

TerraFluidSourceTool::TerraFluidSourceTool(ToolBackend &backend,
                                           ffe::FluidSourceMaterial &material):
    AbstractTerraTool(backend),
    m_material(material),
    m_selected_source(nullptr),
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
}

ffe::FluidSource *TerraFluidSourceTool::find_fluid_source(const Vector2f &viewport_cursor)
{
    const Ray r = m_backend.view_ray(viewport_cursor);

    return static_cast<ffe::FluidSource*>(
                m_backend.hittest_octree_object(r, [](const ffe::OctreeObject &obj){ return bool(dynamic_cast<const ffe::FluidSource*>(&obj)); })
                );
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

void TerraFluidSourceTool::on_fluid_source_removed(sim::object_ptr<sim::Fluid::Source> source)
{
    auto iter = m_source_visualisations.find(source.object_id());
    if (iter == m_source_visualisations.end()) {
        return;
    }

    remove_visualisation(iter->second);
    if (m_selected_source == iter->second) {
        m_selected_source = nullptr;
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

void TerraFluidSourceTool::activate()
{
    m_visualisation_group = &m_backend.sgnode().root().emplace<ffe::scenegraph::OctGroup>();

    for (sim::Fluid::Source *source: m_backend.world().fluid().sources())
    {
        add_source(source);
    }

    m_fluid_source_added_connection = m_backend.connect_to_signal(
                m_backend.world().fluid_source_added(),
                std::bind(&TerraFluidSourceTool::on_fluid_source_added,
                          this,
                          std::placeholders::_1));

    m_fluid_source_removed_connection = m_backend.connect_to_signal(
                m_backend.world().fluid_source_removed(),
                std::bind(&TerraFluidSourceTool::on_fluid_source_removed,
                          this,
                          std::placeholders::_1));
}

void TerraFluidSourceTool::deactivate()
{
    m_fluid_source_added_connection = nullptr;
    m_fluid_source_removed_connection = nullptr;
    ffe::scenegraph::OctGroup &parent = m_backend.sgnode().root();
    auto iter = std::find_if(parent.begin(),
                             parent.end(),
                             [this](const ffe::scenegraph::OctNode &item){return &item == m_visualisation_group;});
    parent.erase(iter);
    m_visualisation_group = nullptr;
    m_source_visualisations.clear();
}

std::pair<bool, Vector3f> TerraFluidSourceTool::hover(
        const Vector2f &viewport_cursor,
        const Vector3f &world_cursor)
{
    if (m_selected_source) {
        m_selected_source->set_ui_state(ffe::UI_STATE_SELECTED);
    }

    ffe::FluidSource *obj = find_fluid_source(viewport_cursor);

    if (obj) {
        if (obj != m_selected_source) {
            obj->set_ui_state(ffe::UI_STATE_HOVER);
        }
        return std::make_pair(true, world_cursor);
    }

    return std::make_pair(false, world_cursor);
}

std::pair<ToolDragPtr, sim::WorldOperationPtr> TerraFluidSourceTool::primary_start(
        const Vector2f &viewport_cursor,
        const Vector3f &world_cursor)
{
    ffe::FluidSource *obj = find_fluid_source(viewport_cursor);

    if (obj) {
        obj->set_ui_state(ffe::UI_STATE_SELECTED);
        m_selected_source = obj;
        return std::make_pair(nullptr, nullptr);
    } else {
        return std::make_pair(nullptr,
                              std::make_unique<sim::ops::FluidSourceCreate>(
                                  world_cursor[eX], world_cursor[eY],
                                  5.f,
                                  world_cursor[eZ] + 2.f,
                                  1.f));
    }


}

std::pair<ToolDragPtr, sim::WorldOperationPtr> TerraFluidSourceTool::secondary_start(
        const Vector2f &viewport_cursor,
        const Vector3f &)
{
    ffe::FluidSource *obj = find_fluid_source(viewport_cursor);

    if (obj) {
        if (obj == m_selected_source) {
            m_selected_source = nullptr;
        }
        if (obj->source()) {
            return std::make_pair(nullptr, std::make_unique<sim::ops::FluidSourceDestroy>(obj->source()->object_id()));
        }
    }

    return std::make_pair(nullptr, nullptr);
}


/* TerraTestingTool::TerraTestingTool */

TerraTestingTool::TerraTestingTool(ToolBackend &backend,
                                   ffe::Material &preview_material,
                                   ffe::Material &road_material):

    AbstractTerraTool(backend),
    m_debug_node(nullptr),
    m_step(0),
    m_preview_material(preview_material),
    m_road_material(road_material)
{
    m_uses_brush = false;
    m_uses_hover = true;
}

void TerraTestingTool::add_segment(const QuadBezier3f &curve)
{
    ffe::scenegraph::OctGroup &group = m_backend.sgnode().root();
    group.emplace<ffe::QuadBezier3fRoadTest>(m_road_material, 20).set_curve(curve);
}

void TerraTestingTool::add_segmentized()
{
    const float segment_length = 10;
    const float min_length = 5;
    std::vector<float> ts;
    // we use the autosampled points as a reference for where we can approximate
    // the curve using line segments. inside those segments, we split as
    // neccessary
    autosample_quadbezier(m_tmp_curve, std::back_inserter(ts));

    std::vector<float> segment_ts;

    float len_accum = 0.f;
    float prev_t = 0.f;
    Vector3f prev_point = m_tmp_curve[0.];
    for (float sampled_t: ts) {
        Vector3f curr_point = m_tmp_curve[sampled_t];
        float segment_len = (prev_point - curr_point).length();
        float existing_len = len_accum;
        float split_t = 0.f;
        len_accum += segment_len;

        std::cout << "segment: t = " << sampled_t
                  << "; len = " << segment_len
                  << "; existing len = " << existing_len
                  << "; len accum = " << len_accum
                  << std::endl;

        if (len_accum >= segment_length) {
            // special handling for re-using the existing length
            float local_len = segment_length - existing_len;
            split_t = prev_t + (sampled_t - prev_t) * local_len / segment_len;
            segment_ts.push_back(split_t);
            std::cout << split_t << " (first)"<< std::endl;

            len_accum -= segment_length;
        }

        while (len_accum >= segment_length) {
            split_t += (sampled_t - prev_t) * segment_length / segment_len;
            len_accum -= segment_length;
            segment_ts.push_back(split_t);
            std::cout << split_t << " (more)"<< std::endl;
        }

        prev_t = sampled_t;
        prev_point = curr_point;
    }

    // drop the last segment if it would otherwise result in a very short piece
    if (len_accum < min_length) {
        segment_ts.pop_back();
    }

    std::vector<QuadBezier3f> segments;
    m_tmp_curve.segmentize(segment_ts.begin(), segment_ts.end(),
                           std::back_inserter(segments));
    for (auto &segment: segments) {
        add_segment(segment);
    }
}

std::pair<bool, Vector3f> TerraTestingTool::snapped_point(const Vector2f &viewport_cursor,
                                                          const Vector3f &world_cursor)
{
    const Ray r = m_backend.view_ray(viewport_cursor);

    ffe::QuadBezier3fRoadTest *obj = static_cast<ffe::QuadBezier3fRoadTest*>(
                m_backend.hittest_octree_object(r, [](const ffe::OctreeObject &obj){ return bool(dynamic_cast<const ffe::QuadBezier3fRoadTest*>(&obj)); })
                );

    if (obj) {
        return std::make_pair(true, obj->curve().p3);
    }

    return std::make_pair(false, world_cursor);
}

std::pair<bool, Vector3f> TerraTestingTool::hover(
        const Vector2f &viewport_cursor,
        const Vector3f &world_cursor)
{
    switch (m_step)
    {
    case 0:
    {
        auto potential_result = snapped_point(viewport_cursor, world_cursor);;
        if (potential_result.first) {
            return potential_result;
        }
        break;
    }
    case 1:
    {
        m_tmp_curve.p2 = world_cursor;
        m_tmp_curve.p3 = world_cursor;
        m_debug_node->set_curve(m_tmp_curve);
        break;
    }
    case 2:
    {
        bool valid;
        Vector3f p;
        std::tie(valid, p) = snapped_point(viewport_cursor, world_cursor);
        m_tmp_curve.p3 = p;
        m_debug_node->set_curve(m_tmp_curve);
        if (valid) {
            return std::make_pair(valid, p);
        }
        break;
    }
    default:;
    }
    return AbstractTerraTool::hover(viewport_cursor, world_cursor);
}

std::pair<ToolDragPtr, sim::WorldOperationPtr> TerraTestingTool::primary_start(
        const Vector2f &viewport_cursor,
        const Vector3f &world_cursor)
{
    switch (m_step)
    {
    case 1:
    {
        m_tmp_curve.p2 = world_cursor;
        m_tmp_curve.p3 = world_cursor;
        m_debug_node->set_curve(m_tmp_curve);
        m_step += 1;
        break;
    }
    case 2:
    {
        bool valid;
        Vector3f p;
        std::tie(valid, p) = snapped_point(viewport_cursor, world_cursor);

        ffe::scenegraph::OctGroup &group = m_backend.sgnode().root();

        m_tmp_curve.p3 = p;
        m_debug_node->set_curve(m_tmp_curve);
        auto iter = std::find_if(group.begin(), group.end(), [this](ffe::scenegraph::OctNode &node){ return &node == m_debug_node; });
        group.erase(iter);
        add_segmentized();
        m_debug_node = nullptr;
        m_step = 0;
        break;
    }
    default:
    {
        bool valid;
        Vector3f p;
        std::tie(valid, p) = snapped_point(viewport_cursor, world_cursor);

        assert(!m_debug_node);
        assert(m_preview_material);
        m_debug_node = &m_backend.sgnode().root().emplace<ffe::QuadBezier3fDebug>(m_preview_material, 20);
        m_tmp_curve = QuadBezier3f(p, p, p);
        m_debug_node->set_curve(m_tmp_curve);
        m_step += 1;
        break;
    }
    }

    return std::make_pair(nullptr, nullptr);
}

std::pair<ToolDragPtr, sim::WorldOperationPtr> TerraTestingTool::secondary_start(
        const Vector2f &,
        const Vector3f &)
{
    if (m_step > 0) {
        assert(m_debug_node);
        ffe::scenegraph::OctGroup &group = m_backend.sgnode().root();
        auto iter = std::find_if(group.begin(), group.end(), [this](ffe::scenegraph::OctNode &node){ return &node == m_debug_node; });
        group.erase(iter);
        m_step = 0;
        m_debug_node = nullptr;
    }
    return std::make_pair(nullptr, nullptr);
}
