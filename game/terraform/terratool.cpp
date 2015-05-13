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
#define QT_NO_KEYWORDS

#include "terraform/terratool.hpp"

#include "engine/math/algo.hpp"

#include "engine/sim/world_ops.hpp"


ToolBackend::ToolBackend(BrushFrontend &brush_frontend,
                         const sim::WorldState &world):
    m_brush_frontend(brush_frontend),
    m_world(world)
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

sim::WorldOperationPtr TerraTool::primary_start(const float, const float)
{
    return nullptr;
}

sim::WorldOperationPtr TerraTool::primary(const float, const float)
{
    return nullptr;
}

sim::WorldOperationPtr TerraTool::secondary_start(const float, const float)
{
    return nullptr;
}

sim::WorldOperationPtr TerraTool::secondary(const float, const float)
{
    return nullptr;
}


sim::WorldOperationPtr TerraRaiseLowerTool::primary(const float x0, const float y0)
{
    return std::make_unique<sim::ops::TerraformRaise>(
                x0, y0,
                m_backend.brush_frontend().brush_size(),
                m_backend.brush_frontend().sampled(),
                m_backend.brush_frontend().brush_strength());
}

sim::WorldOperationPtr TerraRaiseLowerTool::secondary(const float x0, const float y0)
{
    return std::make_unique<sim::ops::TerraformRaise>(
                x0, y0,
                m_backend.brush_frontend().brush_size(),
                m_backend.brush_frontend().sampled(),
                -m_backend.brush_frontend().brush_strength());
}


TerraLevelTool::TerraLevelTool(ToolBackend &backend):
    TerraTool(backend)
{
    m_has_value = true;
    m_value_name = "Level";
    m_value = 10.f;
}

sim::WorldOperationPtr TerraLevelTool::primary(const float x0, const float y0)
{
    return std::make_unique<sim::ops::TerraformLevel>(
                x0, y0,
                m_backend.brush_frontend().brush_size(),
                m_backend.brush_frontend().sampled(),
                m_backend.brush_frontend().brush_strength(),
                m_value);
}

sim::WorldOperationPtr TerraLevelTool::secondary_start(const float x0, const float y0)
{
    const unsigned int terrain_size = m_backend.world().terrain().size();

    const int terrainx = std::round(x0);
    const int terrainy = std::round(y0);

    if (terrainx < 0 || terrainx >= (int)terrain_size ||
            terrainy < 0 || terrainy >= (int)terrain_size)
    {
        return nullptr;
    }

    float new_height;
    {
        const sim::Terrain::HeightField *field = nullptr;
        auto lock = m_backend.world().terrain().readonly_field(field);
        new_height = (*field)[terrainy*m_backend.world().terrain().size()+terrainx];
    }
    set_value(new_height);

    return nullptr;
}


sim::WorldOperationPtr TerraSmoothTool::primary(const float x0, const float y0)
{
    return std::make_unique<sim::ops::TerraformSmooth>(
                x0, y0,
                m_backend.brush_frontend().brush_size(),
                m_backend.brush_frontend().sampled(),
                m_backend.brush_frontend().brush_strength());
}


sim::WorldOperationPtr TerraFluidRaiseTool::primary(const float x0, const float y0)
{
    return std::make_unique<sim::ops::FluidRaise>(
                x0-0.5, y0-0.5,
                m_backend.brush_frontend().brush_size(),
                m_backend.brush_frontend().sampled(),
                m_backend.brush_frontend().brush_strength());
}

sim::WorldOperationPtr TerraFluidRaiseTool::secondary(const float x0, const float y0)
{
    return std::make_unique<sim::ops::FluidRaise>(
                x0-0.5, y0-0.5,
                m_backend.brush_frontend().brush_size(),
                m_backend.brush_frontend().sampled(),
                -m_backend.brush_frontend().brush_strength());
}
