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
#include "ffengine/render/fluidsource.hpp"
#include "ffengine/render/renderpass.hpp"


ToolBackend::ToolBackend(BrushFrontend &brush_frontend,
                         const sim::WorldState &world):
    m_brush_frontend(brush_frontend),
    m_world(world)
{

}

ToolBackend::~ToolBackend()
{

}

ffe::OctreeObject *ToolBackend::hittest_octree_object(const Ray &ray,
        const std::function<bool (const ffe::OctreeObject &)> &predicate)
{
    m_sgnode->octree().select_nodes_by_ray(ray, m_ray_hitset);
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

void ToolBackend::set_camera(ffe::PerspectivalCamera &camera)
{
    m_camera = &camera;
}

void ToolBackend::set_sgnode(ffe::scenegraph::OctreeGroup &sgnode)
{
    m_sgnode = &sgnode;
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


TerraTool::TerraTool(ToolBackend &backend):
    m_backend(backend),
    m_has_value(false),
    m_uses_brushes(false),
    m_uses_hover(false)
{

}

TerraTool::~TerraTool()
{

}

void TerraTool::set_value(float new_value)
{
    if (!m_has_value) {
        return;
    }
    if (m_value == new_value) {
        return;
    }
    m_value = new_value;
    m_value_changed.emit(new_value);
}

std::pair<bool, Vector3f> TerraTool::hover(const Vector2f &viewport_cursor,
                                           const Vector3f &world_cursor)
{
    return std::make_pair(true, world_cursor);
}

sim::WorldOperationPtr TerraTool::primary_start(const Vector2f &viewport_cursor, const Vector3f &world_cursor)
{
    return nullptr;
}

sim::WorldOperationPtr TerraTool::primary(const Vector2f &viewport_cursor, const Vector3f &world_cursor)
{
    return nullptr;
}

sim::WorldOperationPtr TerraTool::secondary_start(const Vector2f &viewport_cursor, const Vector3f &world_cursor)
{
    return nullptr;
}

sim::WorldOperationPtr TerraTool::secondary(const Vector2f &viewport_cursor, const Vector3f &world_cursor)
{
    return nullptr;
}


sim::WorldOperationPtr TerraRaiseLowerTool::primary(const Vector2f &viewport_cursor, const Vector3f &world_cursor)
{
    return std::make_unique<sim::ops::TerraformRaise>(
                world_cursor[eX], world_cursor[eY],
                m_backend.brush_frontend().brush_size(),
                m_backend.brush_frontend().sampled(),
                m_backend.brush_frontend().brush_strength());
}

sim::WorldOperationPtr TerraRaiseLowerTool::secondary(const Vector2f &viewport_cursor, const Vector3f &world_cursor)
{
    return std::make_unique<sim::ops::TerraformRaise>(
                world_cursor[eX], world_cursor[eY],
                m_backend.brush_frontend().brush_size(),
                m_backend.brush_frontend().sampled(),
                -m_backend.brush_frontend().brush_strength());
}


TerrainBrushTool::TerrainBrushTool(ToolBackend &backend):
    TerraTool(backend)
{
    m_uses_brushes = true;
    m_uses_hover = true;
}


TerraLevelTool::TerraLevelTool(ToolBackend &backend):
    TerrainBrushTool(backend)
{
    m_has_value = true;
    m_value_name = "Level";
    m_value = 10.f;
}

sim::WorldOperationPtr TerraLevelTool::primary(const Vector2f &viewport_cursor, const Vector3f &world_cursor)
{
    return std::make_unique<sim::ops::TerraformLevel>(
                world_cursor[eX], world_cursor[eY],
                m_backend.brush_frontend().brush_size(),
                m_backend.brush_frontend().sampled(),
                m_backend.brush_frontend().brush_strength(),
                m_value);
}

sim::WorldOperationPtr TerraLevelTool::secondary_start(const Vector2f &viewport_cursor, const Vector3f &world_cursor)
{
    set_value(world_cursor[eZ]);
    return nullptr;
}


sim::WorldOperationPtr TerraSmoothTool::primary(const Vector2f &viewport_cursor, const Vector3f &world_cursor)
{
    return std::make_unique<sim::ops::TerraformSmooth>(
                world_cursor[eX], world_cursor[eY],
                m_backend.brush_frontend().brush_size(),
                m_backend.brush_frontend().sampled(),
                m_backend.brush_frontend().brush_strength());
}


sim::WorldOperationPtr TerraRampTool::primary_start(const Vector2f &viewport_cursor, const Vector3f &world_cursor)
{
    m_source_point = world_cursor;
    return nullptr;
}

sim::WorldOperationPtr TerraRampTool::primary(const Vector2f &viewport_cursor, const Vector3f &world_cursor)
{
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

sim::WorldOperationPtr TerraRampTool::secondary_start(const Vector2f &viewport_cursor, const Vector3f &world_cursor)
{
    m_destination_point = world_cursor;
    return nullptr;
}


sim::WorldOperationPtr TerraFluidRaiseTool::primary(const Vector2f &viewport_cursor, const Vector3f &world_cursor)
{
    return std::make_unique<sim::ops::FluidRaise>(
                world_cursor[eX]-0.5, world_cursor[eY]-0.5,
                m_backend.brush_frontend().brush_size(),
                m_backend.brush_frontend().sampled(),
                m_backend.brush_frontend().brush_strength());
}

sim::WorldOperationPtr TerraFluidRaiseTool::secondary(const Vector2f &viewport_cursor, const Vector3f &world_cursor)
{
    return std::make_unique<sim::ops::FluidRaise>(
                world_cursor[eX]-0.5, world_cursor[eY]-0.5,
                m_backend.brush_frontend().brush_size(),
                m_backend.brush_frontend().sampled(),
                -m_backend.brush_frontend().brush_strength());
}


/* TerraFluidSourceTool */

TerraFluidSourceTool::TerraFluidSourceTool(ToolBackend &backend):
    TerraTool(backend),
    m_selected_source(nullptr)
{
    m_uses_hover = true;
    m_uses_brushes = false;
}

ffe::FluidSource *TerraFluidSourceTool::find_fluid_source(const Vector2f &viewport_cursor)
{
    const Ray r = m_backend.view_ray(viewport_cursor);

    return static_cast<ffe::FluidSource*>(
                m_backend.hittest_octree_object(r, [](const ffe::OctreeObject &obj){ return bool(dynamic_cast<const ffe::FluidSource*>(&obj)); })
                );
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

sim::WorldOperationPtr TerraFluidSourceTool::primary_start(
        const Vector2f &viewport_cursor,
        const Vector3f&)
{
    ffe::FluidSource *obj = find_fluid_source(viewport_cursor);

    if (obj) {
        obj->set_ui_state(ffe::UI_STATE_SELECTED);
        m_selected_source = obj;
    }

    return nullptr;
}


/* TerraTestingTool::TerraTestingTool */

TerraTestingTool::TerraTestingTool(ToolBackend &backend):
    TerraTool(backend),
    m_debug_node(nullptr),
    m_step(0),
    m_preview_material(nullptr),
    m_road_material(nullptr)
{
    m_uses_brushes = false;
    m_uses_hover = true;
}

void TerraTestingTool::add_segment(const QuadBezier3f &curve)
{
    ffe::scenegraph::OctGroup &group = m_backend.sgnode()->root();
    group.emplace<ffe::QuadBezier3fRoadTest>(*m_road_material, 20).set_curve(curve);
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

void TerraTestingTool::set_preview_material(ffe::Material &material)
{
    m_preview_material = &material;
}

void TerraTestingTool::set_road_material(ffe::Material &material)
{
    m_road_material = &material;
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
    return TerraTool::hover(viewport_cursor, world_cursor);
}

sim::WorldOperationPtr TerraTestingTool::primary_start(const Vector2f &viewport_cursor, const Vector3f &world_cursor)
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

        ffe::scenegraph::OctGroup &group = m_backend.sgnode()->root();

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
        m_debug_node = &m_backend.sgnode()->root().emplace<ffe::QuadBezier3fDebug>(*m_preview_material, 20);
        m_tmp_curve = QuadBezier3f(p, p, p);
        m_debug_node->set_curve(m_tmp_curve);
        m_step += 1;
        break;
    }
    }

    return nullptr;
}

sim::WorldOperationPtr TerraTestingTool::secondary_start(const Vector2f &viewport_cursor, const Vector3f &world_cursor)
{
    if (m_step > 0) {
        assert(m_debug_node);
        ffe::scenegraph::OctGroup &group = m_backend.sgnode()->root();
        auto iter = std::find_if(group.begin(), group.end(), [this](ffe::scenegraph::OctNode &node){ return &node == m_debug_node; });
        group.erase(iter);
        m_step = 0;
        m_debug_node = nullptr;
    }
    return nullptr;
}
