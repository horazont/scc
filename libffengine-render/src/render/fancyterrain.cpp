/**********************************************************************
File name: fancyterrain.cpp
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
#include "ffengine/render/fancyterrain.hpp"

#include "ffengine/common/utils.hpp"
#include "ffengine/math/intersect.hpp"
#include "ffengine/io/log.hpp"

// #define TIMELOG_SYNC
// #define TIMELOG_RENDER

#if defined(TIMELOG_SYNC) || defined(TIMELOG_RENDER)
#include <chrono>
typedef std::chrono::steady_clock timelog_clock;
#define ms_cast(x) std::chrono::duration_cast<std::chrono::duration<float, std::ratio<1, 1000> > >(x)
#endif

namespace engine {

static io::Logger &logger = io::logging().get_logger("render.fancyterrain");

static const Vector3f fake_viewpoint(30, 30, 200);


FancyTerrainNode::FancyTerrainNode(const unsigned int terrain_size,
                                   const unsigned int grid_size,
                                   FancyTerrainInterface &terrain_interface,
                                   GLResourceManager &resources):
    FullTerrainRenderer(terrain_size, grid_size),
    m_resources(resources),
    m_eval_context(resources.shader_library()),
    m_terrain_interface(terrain_interface),
    m_terrain(terrain_interface.terrain()),
    m_terrain_nt(terrain_interface.ntmap()),
    m_invalidate_cache_conn(terrain_interface.field_updated().connect(
                           sigc::mem_fun(*this,
                                         &FancyTerrainNode::invalidate_cache))),
    m_linear_filter(true),
    m_heightmap(GL_R32F,
                m_terrain.size(),
                m_terrain.size(),
                GL_RED,
                GL_FLOAT),
    m_normalt(GL_RGBA32F,
                m_terrain.size(),
                m_terrain.size(),
                GL_RGBA,
                GL_FLOAT),
    m_vbo(VBOFormat({
                        VBOAttribute(2)
                    })),
    m_material(m_vbo, m_ibo),
    m_normal_debug_material(m_vbo, m_ibo),
    m_vbo_allocation(m_vbo.allocate(terrain_interface.grid_size()*terrain_interface.grid_size())),
    m_ibo_allocation(m_ibo.allocate((terrain_interface.grid_size()-1)*(terrain_interface.grid_size()-1)*4)),
    m_cache_invalidation(0, 0, m_terrain.size(), m_terrain.size())
{
    uint16_t *dest = m_ibo_allocation.get();
    for (unsigned int y = 0; y < grid_size-1; y++) {
        for (unsigned int x = 0; x < grid_size-1; x++) {
            const unsigned int curr_base = y*grid_size + x;
            *dest++ = curr_base;
            *dest++ = curr_base + grid_size;
            *dest++ = curr_base + grid_size + 1;
            *dest++ = curr_base + 1;
        }
    }

    m_ibo_allocation.mark_dirty();

    const float heightmap_factor = 1.f / m_terrain_interface.size();

    m_eval_context.define1f("HEIGHTMAP_FACTOR", heightmap_factor);

    // sub-context for defining the z-offset
    spp::EvaluationContext ctx(m_eval_context);
    ctx.define1f("ZOFFSET", 0.);

    bool success = true;

    success = success && m_material.shader().attach(
                resources.load_shader_checked(":/shaders/terrain/main.vert"),
                ctx,
                GL_VERTEX_SHADER);
    success = success && m_material.shader().attach(
                resources.load_shader_checked(":/shaders/terrain/main.geom"),
                ctx,
                GL_GEOMETRY_SHADER);
    success = success && m_material.shader().attach(
                resources.load_shader_checked(":/shaders/terrain/main.frag"),
                ctx,
                GL_FRAGMENT_SHADER);

    m_material.declare_attribute("position", 0);

    success = success && m_material.link();

    success = success && m_normal_debug_material.shader().attach(
                resources.load_shader_checked(":/shaders/terrain/main.vert"),
                ctx,
                GL_VERTEX_SHADER);
    success = success && m_normal_debug_material.shader().attach(
                resources.load_shader_checked(":/shaders/terrain/normal_debug.geom"),
                ctx,
                GL_GEOMETRY_SHADER);
    success = success && m_normal_debug_material.shader().attach(
                resources.load_shader_checked(":/shaders/generic/normal_debug.frag"),
                ctx,
                GL_FRAGMENT_SHADER);

    m_normal_debug_material.declare_attribute("position", 0);

    success = success && m_normal_debug_material.link();

    if (!success) {
        throw std::runtime_error("failed to compile or link shader");
    }

    m_material.shader().bind();
    glUniform2f(m_material.shader().uniform_location("chunk_translation"),
                0.0, 0.0);
    m_material.attach_texture("heightmap", &m_heightmap);
    m_material.attach_texture("normalt", &m_normalt);

    m_normal_debug_material.shader().bind();
    glUniform2f(m_normal_debug_material.shader().uniform_location("chunk_translation"),
                0.0, 0.0);
    m_normal_debug_material.attach_texture("heightmap", &m_heightmap);
    m_normal_debug_material.attach_texture("normalt", &m_normalt);
    glUniform1f(m_normal_debug_material.shader().uniform_location("normal_length"),
                2.);

    {
        auto slice = VBOSlice<Vector2f>(m_vbo_allocation, 0);
        unsigned int index = 0;
        for (unsigned int y = 0; y < grid_size; y++) {
            for (unsigned int x = 0; x < grid_size; x++) {
                slice[index++] = Vector2f(x, y) / (grid_size-1);
            }
        }
    }
    m_vbo_allocation.mark_dirty();

    m_vbo.sync();
    m_ibo.sync();
}

FancyTerrainNode::~FancyTerrainNode()
{
    m_invalidate_cache_conn.disconnect();
}

inline void render_slice(RenderContext &context,
                         Material &material,
                         IBOAllocation &ibo_allocation,
                         const float x, const float y,
                         const float scale)
{
    /*const float xtex = (float(slot_index % texture_cache_size) + 0.5/grid_size) / texture_cache_size;
    const float ytex = (float(slot_index / texture_cache_size) + 0.5/grid_size) / texture_cache_size;*/

    glUniform1f(material.shader().uniform_location("chunk_size"),
                scale);
    glUniform2f(material.shader().uniform_location("chunk_translation"),
                x, y);
    /*std::cout << "rendering" << std::endl;
    std::cout << "  translationx  = " << x << std::endl;
    std::cout << "  translationy  = " << y << std::endl;
    std::cout << "  scale         = " << scale << std::endl;*/
    context.draw_elements(GL_LINES_ADJACENCY, material, ibo_allocation);
}

void FancyTerrainNode::render_all(RenderContext &context, Material &material,
                                  const FullTerrainNode::Slices &slices_to_render)
{
    for (auto &slice: slices_to_render) {
        const float x = slice.basex;
        const float y = slice.basey;
        const float scale = slice.lod;

        render_slice(context, material, m_ibo_allocation,
                     x, y, scale);
    }
}

void FancyTerrainNode::attach_blend_texture(Texture2D *tex)
{
    m_material.attach_texture("blend", tex);
}

void FancyTerrainNode::attach_grass_texture(Texture2D *tex)
{
    m_material.attach_texture("grass", tex);
}

void FancyTerrainNode::attach_rock_texture(Texture2D *tex)
{
    m_material.attach_texture("rock", tex);
}

void FancyTerrainNode::configure_overlay(
        Material &mat,
        const sim::TerrainRect &clip_rect)
{
    OverlayConfig &conf = m_overlays[&mat];
    conf.clip_rect = clip_rect;
}

bool FancyTerrainNode::configure_overlay_material(Material &mat)
{
    // sub-context for redefining the zoffset
    spp::EvaluationContext ctx(m_eval_context);
    ctx.define1f("ZOFFSET", 1.);

    bool success = mat.shader().attach(
                m_resources.load_shader_checked(":/shaders/terrain/main.vert"),
                ctx,
                GL_VERTEX_SHADER);
    success = success && mat.shader().attach(
                m_resources.load_shader_checked(":/shaders/terrain/main.geom"),
                ctx,
                GL_GEOMETRY_SHADER);

    mat.declare_attribute("position", 0);

    success = success && mat.link();
    if (!success) {
        return false;
    }

    mat.attach_texture("heightmap", &m_heightmap);
    mat.attach_texture("normalt", &m_normalt);

    return true;
}

void FancyTerrainNode::remove_overlay(Material &mat)
{
    auto iter = m_overlays.find(&mat);
    if (iter == m_overlays.end()) {
        return;
    }

    m_overlays.erase(iter);
}

void FancyTerrainNode::invalidate_cache(sim::TerrainRect part)
{
    logger.log(io::LOG_INFO, "GPU terrain data cache invalidated");
    std::lock_guard<std::mutex> lock(m_cache_invalidation_mutex);
    m_cache_invalidation = bounds(part, m_cache_invalidation);
}

void FancyTerrainNode::render(RenderContext &context,
                              const FullTerrainNode &render_terrain)
{
#ifdef TIMELOG_RENDER
    const timelog_clock::time_point t0 = timelog_clock::now();
    timelog_clock::time_point t_solid, t_overlay;
#endif
    /* glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); */
    m_material.bind();
    glUniform3fv(m_material.shader().uniform_location("lod_viewpoint"),
                 1, context.viewpoint()/*fake_viewpoint*/.as_array);
    render_all(context, m_material, render_terrain.slices_to_render());
    /* glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); */
#ifdef TIMELOG_RENDER
    t_solid = timelog_clock::now();
#endif

    /* m_normal_debug_material.shader().bind();
    glUniform3fv(m_normal_debug_material.shader().uniform_location("lod_viewpoint"),
                 1, context.scene().viewpoint().as_array);
    render_all(context, *m_nd_vao, m_normal_debug_material); */

    glDepthMask(GL_FALSE);
    for (auto &overlay: m_render_overlays)
    {
        Material &material = *overlay.material;
        material.bind();
        glUniform3fv(material.shader().uniform_location("lod_viewpoint"),
                     1, context.viewpoint()/*fake_viewpoint*/.as_array);
        for (auto &slice: render_terrain.slices_to_render())
        {
            const unsigned int x = slice.basex;
            const unsigned int y = slice.basey;
            const unsigned int scale = slice.lod;

            sim::TerrainRect slice_rect(x, y, x+scale, y+scale);
            if (slice_rect.overlaps(overlay.clip_rect)) {
                render_slice(context,
                             material, m_ibo_allocation,
                             x, y, scale);
            }
        }
    }
    glDepthMask(GL_TRUE);
#ifdef TIMELOG_RENDER
    t_overlay = timelog_clock::now();
    logger.logf(io::LOG_DEBUG, "render: solid time: %.2f ms",
                std::chrono::duration_cast<std::chrono::duration<float, std::ratio<1, 1000> > >(t_solid-t0).count());
    logger.logf(io::LOG_DEBUG, "render: overlay time: %.2f ms",
                std::chrono::duration_cast<std::chrono::duration<float, std::ratio<1, 1000> > >(t_overlay-t_solid).count());
#endif
}

void FancyTerrainNode::sync(RenderContext &context,
                            const FullTerrainNode &render_terrain)
{
    // FIXME: use SceneStorage here!

#ifdef TIMELOG_SYNC
    const timelog_clock::time_point t0 = timelog_clock::now();
    timelog_clock::time_point t_overlays, t_setup, t_allocate, t_upload;
#endif

    m_render_overlays.clear();
    m_render_overlays.reserve(m_overlays.size());
    for (auto &item: m_overlays) {
        item.first->shader().bind();
        glUniform1f(item.first->shader().uniform_location("scale_to_radius"),
                    render_terrain.scale_to_radius());
        m_render_overlays.emplace_back(
                    RenderOverlay{item.first, item.second.clip_rect}
                    );
    }

#ifdef TIMELOG_SYNC
    t_overlays = timelog_clock::now();
#endif

    m_material.shader().bind();
    glUniform1f(m_material.shader().uniform_location("scale_to_radius"),
                render_terrain.scale_to_radius());
    m_normal_debug_material.shader().bind();
    glUniform1f(m_normal_debug_material.shader().uniform_location("scale_to_radius"),
                render_terrain.scale_to_radius());

    m_heightmap.bind();
    if (m_linear_filter) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    m_normalt.bind();
    if (m_linear_filter) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

#ifdef TIMELOG_SYNC
    t_setup = timelog_clock::now();
#endif

#ifdef TIMELOG_SYNC
    t_allocate = timelog_clock::now();
#endif

    sim::TerrainRect updated;
    {
        std::lock_guard<std::mutex> lock(m_cache_invalidation_mutex);
        updated = m_cache_invalidation;
        m_cache_invalidation = NotARect;
    }
    if (updated.is_a_rect())
    {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, m_terrain.size());
        m_heightmap.bind();
        {
            const sim::Terrain::HeightField *heightfield = nullptr;
            auto hf_lock = m_terrain.readonly_field(heightfield);

            glTexSubImage2D(GL_TEXTURE_2D, 0,
                            updated.x0(),
                            updated.y0(),
                            updated.x1() - updated.x0(),
                            updated.y1() - updated.y0(),
                            GL_RED, GL_FLOAT,
                            &heightfield->data()[updated.y0()*m_terrain.size()+updated.x0()]);

        }

        m_normalt.bind();
        {
            const NTMapGenerator::NTField *ntfield = nullptr;
            auto nt_lock = m_terrain_nt.readonly_field(ntfield);

            glTexSubImage2D(GL_TEXTURE_2D, 0,
                            updated.x0(),
                            updated.y0(),
                            updated.x1() - updated.x0(),
                            updated.y1() - updated.y0(),
                            GL_RGBA, GL_FLOAT,
                            &ntfield->data()[updated.y0()*m_terrain.size()+updated.x0()]);
        }
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }

#ifdef TIMELOG_SYNC
    t_upload = timelog_clock::now();
    logger.logf(io::LOG_DEBUG, "sync: t_overlays = %.2f ms", ms_cast(t_overlays - t0).count());
    logger.logf(io::LOG_DEBUG, "sync: t_setup    = %.2f ms", ms_cast(t_setup - t_overlays).count());
    logger.logf(io::LOG_DEBUG, "sync: t_allocate = %.2f ms", ms_cast(t_allocate - t_setup).count());
    logger.logf(io::LOG_DEBUG, "sync: t_upload   = %.2f ms", ms_cast(t_upload - t_allocate).count());
#endif
}

}
