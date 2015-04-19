#include "terraform/terratool.hpp"

#include "engine/math/algo.hpp"


template <typename impl_t>
void apply_brush_masked_tool(sim::Terrain::HeightField &field,
                             BrushFrontend &brush_frontend,
                             const unsigned int terrain_size,
                             const float x0,
                             const float y0,
                             const impl_t &impl)
{
    const int size = brush_frontend.brush_size();
    const float radius = size / 2.f;
    const int terrain_xbase = std::round(x0 - radius);
    const int terrain_ybase = std::round(y0 - radius);

    const std::vector<Brush::density_t> &sampled = brush_frontend.sampled();
    Brush::density_t density_factor = brush_frontend.brush_strength();

    for (int y = 0; y < size; y++) {
        const int yterrain = y + terrain_ybase;
        if (yterrain < 0) {
            continue;
        }
        if (yterrain >= (int)terrain_size) {
            break;
        }
        for (int x = 0; x < size; x++) {
            const int xterrain = x + terrain_xbase;
            if (xterrain < 0) {
                continue;
            }
            if (xterrain >= (int)terrain_size) {
                break;
            }

            sim::Terrain::height_t &h = field[yterrain*terrain_size+xterrain];

            h = std::max(sim::Terrain::min_height,
                         std::min(sim::Terrain::max_height,
                                  impl.paint(h, density_factor*sampled[y*size+x])));
        }
    }
}

void update_terrain_rect(unsigned int brush_size,
                         sim::Terrain &terrain,
                         const float x0,
                         const float y0)
{
    const float brush_radius = brush_size/2.f;
    sim::TerrainRect r(x0, y0, std::ceil(x0+brush_radius), std::ceil(y0+brush_radius));
    if (x0 < std::ceil(brush_radius)) {
        r.set_x0(0);
    } else {
        r.set_x0(x0-std::ceil(brush_radius));
    }
    if (y0 < std::ceil(brush_radius)) {
        r.set_y0(0);
    } else {
        r.set_y0(y0-std::ceil(brush_radius));
    }
    if (r.x1() > terrain.size()) {
        r.set_x1(terrain.size());
    }
    if (r.y1() > terrain.size()) {
        r.set_y1(terrain.size());
    }

    terrain.notify_heightmap_changed(r);
}


struct raise_tool
{
    sim::Terrain::height_t paint(sim::Terrain::height_t h,
                                 Brush::density_t brush_density) const
    {
        return h + brush_density;
    }
};


struct lower_tool
{
    sim::Terrain::height_t paint(sim::Terrain::height_t h,
                                 Brush::density_t brush_density) const
    {
        return h + brush_density;
    }
};


struct flatten_tool
{
    flatten_tool(sim::Terrain::height_t new_value):
        new_value(new_value)
    {

    }

    sim::Terrain::height_t new_value;

    sim::Terrain::height_t paint(sim::Terrain::height_t h,
                                 Brush::density_t brush_density) const
    {
        return interp_linear(new_value, h, brush_density);
    }
};


ToolBackend::ToolBackend(BrushFrontend &brush_frontend,
                         sim::Terrain &terrain):
    m_brush_frontend(brush_frontend),
    m_terrain(terrain)
{

}

ToolBackend::~ToolBackend()
{

}


TerraTool::TerraTool(ToolBackend &backend):
    m_backend(backend),
    m_has_value(false)
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

void TerraTool::primary(const float, const float)
{

}

void TerraTool::secondary(const float, const float)
{

}


void TerraRaiseLowerTool::primary(const float x0, const float y0)
{
    {
        sim::Terrain::HeightField *field = nullptr;
        auto lock = m_backend.terrain().writable_field(field);
        apply_brush_masked_tool(*field,
                                m_backend.brush_frontend(),
                                m_backend.terrain_size(),
                                x0, y0,
                                raise_tool());
    }
    update_terrain_rect(m_backend.brush_frontend().brush_size(),
                        m_backend.terrain(),
                        x0, y0);
}

void TerraRaiseLowerTool::secondary(const float x0, const float y0)
{
    {
        sim::Terrain::HeightField *field = nullptr;
        auto lock = m_backend.terrain().writable_field(field);
        apply_brush_masked_tool(*field,
                                m_backend.brush_frontend(),
                                m_backend.terrain_size(),
                                x0, y0,
                                lower_tool());
    }
    update_terrain_rect(m_backend.brush_frontend().brush_size(),
                        m_backend.terrain(),
                        x0, y0);
}


TerraLevelTool::TerraLevelTool(ToolBackend &backend):
    TerraTool(backend)
{
    m_value = 10.f;
}

void TerraLevelTool::primary(const float x0, const float y0)
{
    {
        sim::Terrain::HeightField *field = nullptr;
        auto lock = m_backend.terrain().writable_field(field);
        apply_brush_masked_tool(*field,
                                m_backend.brush_frontend(),
                                m_backend.terrain_size(),
                                x0, y0,
                                flatten_tool(m_value));
    }
    update_terrain_rect(m_backend.brush_frontend().brush_size(),
                        m_backend.terrain(),
                        x0, y0);
}

void TerraLevelTool::secondary(const float x0, const float y0)
{
    const int terrainx = std::round(x0);
    const int terrainy = std::round(y0);

    if (terrainx < 0 || terrainx >= (int)m_backend.terrain_size() ||
            terrainy < 0 || terrainy >= (int)m_backend.terrain_size())
    {
        return;
    }

    const sim::Terrain::HeightField *field = nullptr;
    auto lock = m_backend.terrain().readonly_field(field);
    set_value((*field)[terrainy*m_backend.terrain_size()+terrainx]);
}
