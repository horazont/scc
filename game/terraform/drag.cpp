/**********************************************************************
File name: drag.cpp
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
#include "drag.hpp"

#include "ffengine/math/intersect.hpp"


/* AbstractToolDrag */

AbstractToolDrag::AbstractToolDrag(bool continuous):
    m_continuous(continuous)
{

}

AbstractToolDrag::~AbstractToolDrag()
{

}


/* PlaneToolDrag */

PlaneToolDrag::PlaneToolDrag(const Plane &plane,
                             const ffe::PerspectivalCamera &camera,
                             const Vector2f &viewport_size,
                             PlaneToolDrag::DragCallback &&drag_cb,
                             PlaneToolDrag::DoneCallback &&done_cb,
                             bool continuous):
    AbstractToolDrag(continuous),
    m_plane(plane),
    m_camera(camera),
    m_viewport_size(viewport_size),
    m_drag_cb(std::move(drag_cb)),
    m_done_cb(std::move(done_cb))
{

}

Vector3f PlaneToolDrag::raycast(const Vector2f &viewport_pos) const
{
    const Ray ray(m_camera.ray(viewport_pos, m_viewport_size));
    float t;
    PlaneSide side;
    std::tie(t, side) = isect_plane_ray(m_plane, ray);
    if (side != PlaneSide::BOTH) {
        return Vector3f(NAN, NAN, NAN);
    }

    return ray.origin + ray.direction*t;
}

sim::WorldOperationPtr PlaneToolDrag::done(const Vector2f &viewport_pos)
{
    if (!m_done_cb) {
        return nullptr;
    }
    return m_done_cb(viewport_pos, raycast(viewport_pos));
}

sim::WorldOperationPtr PlaneToolDrag::drag(const Vector2f &viewport_pos)
{
    return m_drag_cb(viewport_pos, raycast(viewport_pos));
}
