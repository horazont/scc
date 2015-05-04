/**********************************************************************
File name: plane.hpp
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
#ifndef SCC_ENGINE_RENDER_PLANE_H
#define SCC_ENGINE_RENDER_PLANE_H

#include "engine/render/scenegraph.hpp"

namespace engine {

/**
 * Render a simple z-up plane.
 */
class ZUpPlaneNode: public scenegraph::Node
{
public:
    ZUpPlaneNode(const float width, const float height,
                 const unsigned int cells);

private:
    VBO m_vbo;
    IBO m_ibo;
    Material m_material;
    std::unique_ptr<VAO> m_vao;

    VBOAllocation m_vbo_alloc;
    IBOAllocation m_ibo_alloc;

public:
    Material &material();
    void setup_vao();

public:
    void render(RenderContext &context) override;
    void sync(RenderContext &context) override;

};

}

#endif // SCC_ENGINE_RENDER_GRID_H
