/**********************************************************************
File name: drag.hpp
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
#ifndef GAME_TERRAFORM_DRAG_HPP
#define GAME_TERRAFORM_DRAG_HPP

#include <functional>

#include "ffengine/math/shapes.hpp"
#include "ffengine/math/vector.hpp"

#include "ffengine/sim/world_ops.hpp"

#include "ffengine/render/camera.hpp"


class AbstractToolDrag
{
public:
    explicit AbstractToolDrag(bool continuous);
    virtual ~AbstractToolDrag();

private:
    bool m_continuous;

public:
    inline bool is_continuous() const
    {
        return m_continuous;
    }

public:
    virtual sim::WorldOperationPtr done(const Vector2f &viewport_pos) = 0;
    virtual sim::WorldOperationPtr drag(const Vector2f &viewport_pos) = 0;

};


class CustomToolDrag: public AbstractToolDrag
{
public:
    using DragCallback = std::function<sim::WorldOperationPtr(const Vector2f&)>;
    using DoneCallback = std::function<sim::WorldOperationPtr(const Vector2f&)>;

public:
    explicit CustomToolDrag(DragCallback &&drag_cb,
                            DoneCallback &&done_cb = nullptr,
                            bool continuous = false);

private:
    DragCallback m_drag_cb;
    DoneCallback m_done_cb;

public:
    sim::WorldOperationPtr done(const Vector2f &viewport_pos);
    sim::WorldOperationPtr drag(const Vector2f &viewport_pos);


};


class PlaneToolDrag: public AbstractToolDrag
{
public:
    using DragCallback = std::function<sim::WorldOperationPtr(const Vector2f&, const Vector3f&)>;
    using DoneCallback = std::function<sim::WorldOperationPtr(const Vector2f&, const Vector3f&)>;

public:
    PlaneToolDrag(const Plane &plane,
                  const ffe::PerspectivalCamera &camera,
                  const Vector2f &viewport_size,
                  DragCallback &&drag_cb,
                  DoneCallback &&done_cb = nullptr,
                  bool continuous = false);

private:
    const Plane m_plane;
    const ffe::PerspectivalCamera &m_camera;
    const Vector2f &m_viewport_size;
    DragCallback m_drag_cb;
    DoneCallback m_done_cb;

private:
    Vector3f raycast(const Vector2f &viewport_pos) const;

public:
    sim::WorldOperationPtr done(const Vector2f &viewport_pos) override;
    sim::WorldOperationPtr drag(const Vector2f &viewport_pos) override;

};


#endif
