#include "engine/math/intersect.hpp"

#include <limits>
#include <tuple>


template <typename Ta>
Ta sqr(Ta v)
{
    return v*v;
}


const float ISECT_EPSILON = 0.00001;

std::tuple<float, bool> isect_ray_triangle(
        const Ray &ray,
        const Vector3f &p0,
        const Vector3f &p1,
        const Vector3f &p2)
{
    const Vector3f edge1 = p1 - p0;
    const Vector3f edge2 = p2 - p0;

    const Vector3f pvec = ray.direction % edge2;

    const float det = edge1 * pvec;

    if (det > -ISECT_EPSILON && det < ISECT_EPSILON) {
        return std::make_tuple(NAN, false);
    }

    const float inv_det = 1/det;

    const Vector3f tvec = ray.origin - p0;

    const float u = (tvec * pvec) * inv_det;
    if (u < 0 || u > 1) {
        return std::make_tuple(NAN, false);
    }

    const Vector3f qvec = tvec % edge1;

    const float v = (ray.direction * qvec) * inv_det;
    if (v < 0 || v > 1) {
        return std::make_tuple(NAN, false);
    }

    const float t = (edge2 * qvec) * inv_det;

    return std::make_tuple(t, t >= 0);
}

std::tuple<float, PlaneSide> isect_plane_ray(
        const Plane &plane,
        const Ray &ray)
{
    const float normal_dir = ray.direction * plane.normal;

    if (normal_dir > -ISECT_EPSILON && normal_dir < ISECT_EPSILON) {
        // parallel
        return std::make_tuple(0, plane.side_of(ray.origin));
    }

    const float t = (plane.dist - plane.normal*ray.origin) / normal_dir;

    return std::make_tuple(t, PlaneSide::BOTH);
}

bool isect_aabb_sphere(const AABB &aabb, const Sphere &sphere)
{
    /* Jim Arvo, A Simple Method for Box-Sphere Intersection Testing, Graphics Gems, pp. 247-250 */

    float dmin = 0;
    for (unsigned int i = 0; i < 3; i++) {
        const float Ci = sphere.center.as_array[i];
        if (Ci < aabb.min.as_array[i]) {
            dmin += sqr(Ci - aabb.min.as_array[i]);
        } else if (Ci > aabb.max.as_array[i]) {
            dmin += sqr(Ci - aabb.max.as_array[i]);
        }
    }
    return dmin <= sqr(sphere.radius);
}

bool isect_aabb_ray(const AABB &aabb, const Ray &ray, float &t0, float &t1)
{
    std::array<Plane, 6> sides({
                                   Plane(aabb.min, Vector3f(-1, 0, 0)),
                                   Plane(aabb.max, Vector3f(1, 0, 0)),
                                   Plane(aabb.min, Vector3f(0, -1, 0)),
                                   Plane(aabb.max, Vector3f(0, 1, 0)),
                                   Plane(aabb.min, Vector3f(0, 0, -1)),
                                   Plane(aabb.max, Vector3f(0, 0, 1))
                               });

    float test_min, test_max;
    float tmin = std::numeric_limits<float>::min();
    float tmax = std::numeric_limits<float>::max();

    PlaneSide hit;
    std::tie(test_min, hit) = isect_plane_ray(sides[0], ray);
    if (hit == PlaneSide::POSITIVE_NORMAL) {
        return false;
    }
    std::tie(test_max, hit) = isect_plane_ray(sides[1], ray);
    if (hit == PlaneSide::POSITIVE_NORMAL) {
        return false;
    }
    if (hit == PlaneSide::BOTH && test_min != test_max) {
        tmin = std::max(std::min(test_min, test_max), tmin);
        tmax = std::min(std::max(test_max, test_min), tmax);
    }

    std::tie(test_min, hit) = isect_plane_ray(sides[2], ray);
    if (hit == PlaneSide::POSITIVE_NORMAL) {
        return false;
    }
    std::tie(test_max, hit) = isect_plane_ray(sides[3], ray);
    if (hit == PlaneSide::POSITIVE_NORMAL) {
        return false;
    }
    if (hit == PlaneSide::BOTH && test_min != test_max) {
        tmin = std::max(std::min(test_min, test_max), tmin);
        tmax = std::min(std::max(test_max, test_min), tmax);
    }

    std::tie(test_min, hit) = isect_plane_ray(sides[4], ray);
    if (hit == PlaneSide::POSITIVE_NORMAL) {
        return false;
    }
    std::tie(test_max, hit) = isect_plane_ray(sides[5], ray);
    if (hit == PlaneSide::POSITIVE_NORMAL) {
        return false;
    }
    if (hit == PlaneSide::BOTH && test_min != test_max) {
        tmin = std::max(std::min(test_min, test_max), tmin);
        tmax = std::min(std::max(test_max, test_min), tmax);
    }

    if (tmin > tmax) {
        return false;
    }

    t0 = tmin;
    t1 = tmax;

    return true;
}
