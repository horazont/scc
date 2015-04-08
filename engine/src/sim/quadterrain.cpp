#include "engine/sim/quadterrain.hpp"

#include <climits>
#include <cstring>
#include <memory>
#include <iostream>


namespace sim {

terrain_coord_t check_size(const terrain_coord_t size)
{
    terrain_coord_t value = size;
    if (size != 0) {
        while (!(value & 1)) {
            value >>= 1;
        }
        value >>= 1;
        if (value == 0) {
            return size;
        }
    }

    throw std::invalid_argument("size (" + std::to_string(size) + ") "
                                "is not a power of two");
}


QuadNode::QuadNode(QuadNode *parent,
                   Type type,
                   const terrain_coord_t x0,
                   const terrain_coord_t y0,
                   const terrain_coord_t size,
                   const terrain_height_t height):
    m_rect(x0, y0, x0+size, y0+size),
    m_size(size),
    m_parent(parent),
    m_type(type),
    m_height(height),
    m_dirty(false),
    m_changed(false),
    m_child_changed(false)
{
    init_data();
}

QuadNode::~QuadNode()
{
    free_data();
}

void QuadNode::free_data()
{
    switch (m_type)
    {
    case Type::NORMAL:
    {
        for (unsigned int i = 0; i < 4; i++) {
            delete m_data.children[i];
        }
        break;
    }
    case Type::LEAF:
    {
        break;
    }
    case Type::HEIGHTMAP:
    {
        delete m_data.heightmap;
    }
    }
}

QuadNode *QuadNode::get_root()
{
    if (!m_parent) {
        return this;
    }
    return m_parent->get_root();
}

void QuadNode::heightmap_recalculate_height()
{
    // zero point five
    intermediate_terrain_height_t total_height = m_size*m_size / 2;
    const Heightmap::size_type size = m_data.heightmap->size();
    const terrain_height_t *ptr = m_data.heightmap->data();
    for (unsigned int i = 0; i < size; i++) {
        total_height += *ptr++;
    }
    total_height /= (m_size*m_size);
    m_height = total_height;
}

void QuadNode::init_data()
{
    switch (m_type)
    {
    case Type::NORMAL:
    {
        if (m_size & 1) {
            throw std::runtime_error("Invalid size for parent node");
        }

        const terrain_coord_t child_size = m_size / 2;
        m_data.children[0] = new QuadNode(
                    this,
                    Type::LEAF,
                    m_rect.x0(),
                    m_rect.y0(),
                    child_size,
                    m_height);
        m_data.children[1] = new QuadNode(
                    this,
                    Type::LEAF,
                    m_rect.x0()+child_size,
                    m_rect.y0(),
                    child_size,
                    m_height);
        m_data.children[2] = new QuadNode(
                    this,
                    Type::LEAF,
                    m_rect.x0(),
                    m_rect.y0()+child_size,
                    child_size,
                    m_height);
        m_data.children[3] = new QuadNode(
                    this,
                    Type::LEAF,
                    m_rect.x0()+child_size,
                    m_rect.y0()+child_size,
                    child_size,
                    m_height);
        break;
    }
    case Type::LEAF:
    {
        break;
    }
    case Type::HEIGHTMAP:
    {
        m_data.heightmap = new Heightmap(
                    m_size*m_size,
                    m_height);
        break;
    }
    }
}

void QuadNode::normal_recalculate_height()
{
    intermediate_terrain_height_t total_height = 2; // zero point five
    for (unsigned int i = 0; i < 4; i++) {
        total_height += m_data.children[i]->height();
    }
    const terrain_height_t new_height = total_height / 4;
    if (new_height != m_height) {
        m_height = new_height;
        m_dirty = true;
    }
}

QuadNode *QuadNode::find_node_at(terrain_coord_t x,
                                 terrain_coord_t y,
                                 const terrain_coord_t lod)
{
    if (x >= m_size || y >= m_size)
    {
        return nullptr;
    }

    // TODO: if the lookups become a problem, we can read the indicies for the
    // "children" array directly from the coordinates. We only need a fast
    // way to reverse the bit order in x and y
    if (m_size <= lod) {
        return this;
    }

    switch (m_type)
    {
    case Type::NORMAL:
    {
        unsigned int child_index = 0;
        if (x >= m_size/2) {
            x -= m_size/2;
            child_index |= 1;
        }
        if (y >= m_size/2) {
            y -= m_size/2;
            child_index |= 2;
        }

        return m_data.children[child_index]->find_node_at(x, y, lod);
    }
    case Type::HEIGHTMAP:
    case Type::LEAF:
    {
        return this;
    }
    }

    assert(false);
    return nullptr;
}

void QuadNode::from_heightmap(Heightmap &src,
                              const terrain_coord_t x0,
                              const terrain_coord_t y0,
                              const terrain_coord_t src_size)
{
    assert(x0 <= m_rect.x0());
    assert(y0 <= m_rect.y0());
    const terrain_coord_t xbase = m_rect.x0() - x0;
    const terrain_coord_t ybase = m_rect.y0() - y0;

    switch (m_type)
    {
    case Type::HEIGHTMAP:
    {
        free_data();
        m_type = Type::LEAF;

        // intended fallthrough
    }
    case Type::LEAF:
    {
        if (m_size == 1) {
            m_height = src.data()[ybase*src_size+xbase];
            m_dirty = true;
            return;
        }

        subdivide();

        // intended fallthrough
    }
    case Type::NORMAL:
    {
        bool mergable = true;
        terrain_height_t first_height = 0;
        for (unsigned int i = 0; i < 4; i++) {
            QuadNode *const child = m_data.children[i];
            child->from_heightmap(src, x0, y0, src_size);
            if (i == 0) {
                first_height = child->height();
            }
            mergable = (mergable
                        && child->type() == Type::LEAF
                        && child->height() == first_height);
        }

        if (mergable) {
            merge();
            m_height = first_height;
            m_dirty = true;
        }
        break;
    }
    }
}

void QuadNode::to_heightmap(Heightmap &dest,
                            const terrain_coord_t x0,
                            const terrain_coord_t y0,
                            const terrain_coord_t dest_size)
{
    assert(x0 <= m_rect.x0());
    assert(y0 <= m_rect.y0());
    switch (m_type)
    {
    case Type::LEAF:
    {
        const unsigned int basex = m_rect.x0() - x0;
        const unsigned int basey = m_rect.y0() - y0;
        terrain_height_t *dest_ptr = &dest.front();
        for (unsigned int y = 0; y < m_size; y++) {
            for (unsigned int x = 0; x < m_size; x++) {
                dest_ptr[(basey+y)*dest_size + basex + x] = m_height;
            }
        }
        break;
    }
    case Type::NORMAL:
    {
        for (unsigned int i = 0; i < 4; i++) {
            m_data.children[i]->to_heightmap(dest, x0, y0, dest_size);
        }
        break;
    }
    case Type::HEIGHTMAP:
    {
        const unsigned int basex = x0 - m_rect.x0();
        const unsigned int basey = y0 - m_rect.y0();
        terrain_height_t *dest_ptr = &dest.front();
        for (unsigned int y = 0; y < m_size; y++) {
            memcpy(&dest_ptr[(basey+y)*dest_size+basex],
                   &m_data.heightmap->data()[y*m_size],
                   sizeof(terrain_height_t)*dest_size);
        }
        break;
    }
    }
}

void QuadNode::cleanup()
{
    switch (m_type)
    {
    case Type::HEIGHTMAP:
    {
        if (m_dirty) {
            heightmap_recalculate_height();
        }
        break;
    }
    case Type::LEAF:
    {
        break;
    }
    case Type::NORMAL:
    {
        bool any_changed = false;
        for (unsigned int i = 0; i < 4; i++) {
            QuadNode *const child = m_data.children[i];
            child->cleanup();
            any_changed |= child->subtree_changed();
        }
        m_child_changed = any_changed;
        if (m_child_changed) {
            normal_recalculate_height();
        }
        break;
    }
    }
    m_changed = m_dirty;
    m_dirty = false;
}

QuadNode *QuadNode::find_node_at(const TerrainRect::point_t &p,
                                 const terrain_coord_t lod)
{
    /* std::cout << p << " " << m_rect << " ... " << std::endl; */
    QuadNode *result = find_node_at(p[eX], p[eY], lod);
    /* std::cout << " ... " << result << std::endl; */
    return result;
}

QuadNode *QuadNode::neighbour(const Direction dir)
{
    TerrainRect::point_t p(x0(), y0());
    switch (dir)
    {
    case NORTH:
    case NORTHWEST:
    case NORTHEAST:
    {
        if (p[eY] == 0) {
            return nullptr;
        }
        p[eY] -= 1;
        break;
    }
    case SOUTH:
    case SOUTHWEST:
    case SOUTHEAST:
    {
        p[eY] += m_size;
        break;
    }
    default:;
    }
    switch (dir)
    {
    case NORTHWEST:
    case WEST:
    case SOUTHWEST:
    {
        if (p[eX] == 0) {
            return nullptr;
        }
        p[eX] -= 1;
        break;
    }
    case NORTHEAST:
    case EAST:
    case SOUTHEAST:
    {
        p[eX] += m_size;
        break;
    }
    default:;
    }

    return get_root()->find_node_at(p, m_size);
}

terrain_height_t QuadNode::sample_int(const terrain_coord_t x,
                                      const terrain_coord_t y)
{
    QuadNode *const node = find_node_at(x, y);
    if (!node) {
        return 0;
    }

    return node->sample_local_int(x - node->m_rect.x0(),
                                  y - node->m_rect.y0());
}

terrain_height_t QuadNode::sample_local_int(const terrain_coord_t x,
                                            const terrain_coord_t y)
{
    switch (m_type)
    {
    case Type::LEAF:
    case Type::NORMAL:
    {
        return m_height;
    }
    case Type::HEIGHTMAP:
    {
        return (*m_data.heightmap)[y*m_size+x];
    }
    }
    return 0;
}

void QuadNode::sample_line(std::vector<TerrainVector> &dest,
                           const terrain_coord_t x0,
                           const terrain_coord_t y0,
                           const SampleDirection dir,
                           terrain_coord_t n)
{
    switch (dir)
    {
    case QuadNode::SAMPLE_SOUTH:
    {
        for (unsigned int i = 0; i <= n; i++) {
            const terrain_coord_t x = x0;
            const terrain_coord_t y = y0 + i;
            QuadNode *node = find_node_at(TerrainRect::point_t(x, y));
            if (!node) {
                break;
            }
            dest.emplace_back(x, node->y0(), node->height());
            if (node->size() > 1 && i < n) {
                dest.emplace_back(x, node->y0()+node->size()-1, node->height());
            }
            i += node->size()-1;
        }
        break;
    }
    case QuadNode::SAMPLE_EAST:
    {
        for (unsigned int i = 0; i <= n; i++) {
            const terrain_coord_t x = x0 + i;
            const terrain_coord_t y = y0;
            QuadNode *node = find_node_at(TerrainRect::point_t(x, y));
            if (!node) {
                break;
            }
            dest.emplace_back(node->x0(), y, node->height());
            if (node->size() > 1 && i < n) {
                dest.emplace_back(node->x0()+node->size()-1, y, node->height());
            }
            i += node->size()-1;
        }
        break;
    }
    }


}

void QuadNode::set_height_rect(const TerrainRect &rect,
                               const terrain_height_t new_height)
{
    switch (m_type)
    {
    case Type::LEAF:
    {
        if (rect == m_rect) {
            if (m_height != new_height) {
                m_height = new_height;
                m_dirty = true;
            }
            return;
        }

        subdivide();
        // intended fallthrough
    }
    case Type::NORMAL:
    {
        // split among children
        TerrainRect child_rect;
        for (unsigned int i = 0; i < 4; i++)
        {
            child_rect = rect;
            child_rect &= m_data.children[i]->m_rect;
            if (!child_rect.empty()) {
                m_data.children[i]->set_height_rect(child_rect, new_height);
            }
        }
        break;
    }
    case Type::HEIGHTMAP:
    {
        const TerrainRect local_rect(
                    rect.x0() - m_rect.x0(),
                    rect.y0() - m_rect.y0(),
                    rect.x1() - m_rect.x0(),
                    rect.y1() - m_rect.y0());

        terrain_height_t *const ptr = &m_data.heightmap->front();
        for (unsigned int y = local_rect.y0(); y < local_rect.y1(); y++) {
            terrain_height_t *row_ptr = &ptr[y*m_size+local_rect.x0()];
            for (unsigned int x = local_rect.x0(); x < local_rect.x1(); x++) {
                *row_ptr++ = new_height;
            }
        }

        m_dirty = true;
    }
    }
}

void QuadNode::heightmapify()
{
    assert(m_type != Type::HEIGHTMAP);
    std::unique_ptr<Heightmap> new_heightmap(new Heightmap(m_size*m_size, m_height));
    if (m_type == Type::NORMAL) {
        to_heightmap(*new_heightmap, m_rect.x0(), m_rect.y0(), m_size);
    }
    free_data();
    m_type = Type::HEIGHTMAP;
    m_data.heightmap = new_heightmap.release();
}

void QuadNode::subdivide()
{
    assert(m_type == Type::LEAF);
    m_type = Type::NORMAL;
    init_data();
}

void QuadNode::merge()
{
    assert(m_type == Type::NORMAL);
    free_data();
    m_type = Type::LEAF;
}

void QuadNode::mark_heightmap_dirty()
{
    m_dirty = true;
}

void QuadNode::quadtreeify()
{
    std::unique_ptr<Heightmap> heightmap(m_data.heightmap);
    m_type = Type::LEAF;
    init_data();
    from_heightmap(*heightmap, m_rect.x0(), m_rect.y0(), m_size);
}

/* engine::QuadTerrain */

QuadTerrain::QuadTerrain(terrain_coord_t max_subdivisions,
                         terrain_height_t initial_height):
    m_max_subdivisions(max_subdivisions),
    m_size(1 << max_subdivisions),
    m_root(nullptr, QuadNode::Type::LEAF, 0, 0, m_size, initial_height)
{

}

}