#ifndef SCC_ENGINE_GL_HWELEMENTBUF_H
#define SCC_ENGINE_GL_HWELEMENTBUF_H

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <cassert>

#include <GL/glew.h>

#include "engine/gl/object.hpp"
#include "engine/gl/util.hpp"

#include "engine/io/log.hpp"

namespace engine {

extern io::Logger &gl_array_logger;

typedef unsigned int GLArrayRegionID;

class GLArrayRegion
{
public:
    GLArrayRegion(GLArrayRegionID id,
                  unsigned int start,
                  unsigned int count):
        m_id(id),
        m_start(start),
        m_count(count),
        m_in_use(false),
        m_dirty(false)
    {
    }

public:
    GLArrayRegionID m_id;
    unsigned int m_start;
    unsigned int m_count;
    bool m_in_use;
    bool m_dirty;

};


template <typename _buffer_t>
class GLArrayAllocation
{
public:
    typedef _buffer_t buffer_t;
    typedef typename buffer_t::element_t element_t;

public:
    GLArrayAllocation():
        m_region_id(0),
        m_buffer(0),
        m_elements_per_block(0),
        m_nblocks(0)
    {

    }

    GLArrayAllocation(
            buffer_t *const buffer,
            const unsigned int elements_per_block,
            const unsigned int nblocks,
            const GLArrayRegionID region_id):
        m_region_id(region_id),
        m_buffer(buffer),
        m_elements_per_block(elements_per_block),
        m_nblocks(nblocks)
    {

    }

    GLArrayAllocation(const GLArrayAllocation &ref) = delete;
    GLArrayAllocation(GLArrayAllocation &&src):
        m_region_id(src.m_region_id),
        m_buffer(src.m_buffer),
        m_elements_per_block(src.m_elements_per_block),
        m_nblocks(src.m_nblocks)
    {
        src.m_region_id = 0;
        src.m_buffer = nullptr;
        src.m_elements_per_block = 0;
        src.m_nblocks = 0;
    }

    ~GLArrayAllocation()
    {
        if (m_buffer) {
            m_buffer->region_release(m_region_id);
        }
    }

private:
    GLArrayRegionID m_region_id;
    buffer_t *m_buffer;
    unsigned int m_elements_per_block;
    unsigned int m_nblocks;

public:
    inline buffer_t *buffer() const
    {
        return m_buffer;
    }

    inline unsigned int elements_per_block() const
    {
        return m_elements_per_block;
    }

    inline std::size_t offset() const
    {
        return m_buffer->region_offset(m_region_id);
    }

    inline unsigned int base() const
    {
        return m_buffer->region_base(m_region_id);
    }

    inline unsigned int length() const
    {
        return m_nblocks;
    }

public:
    void mark_dirty() const
    {
        m_buffer->region_mark_dirty(m_region_id);
    }

    element_t *get() const
    {
        return m_buffer->region_get_ptr(m_region_id);
    }

    operator bool() const
    {
        return bool(m_buffer);
    }

};


template <typename _element_t,
          GLuint gl_target,
          GLuint gl_binding_type,
          typename buffer_t>
class GLArray: public GLObject<gl_binding_type>
{
public:
    typedef _element_t element_t;
    typedef GLArrayAllocation<buffer_t> allocation_t;
    typedef std::vector<std::unique_ptr<GLArrayRegion> > region_container;

public:
    GLArray():
        GLObject<gl_binding_type>(),
        m_usage(GL_DYNAMIC_DRAW),
        m_block_length(0),
        m_local_buffer(),
        m_regions(),
        m_region_map(),
        m_any_dirty(false),
        m_remote_size(0),
        m_region_id_ctr()
    {
        glGenBuffers(1, &this->m_glid);
        glBindBuffer(gl_target, this->m_glid);
        glBufferData(gl_target, 0, nullptr, GL_STATIC_DRAW);
        raise_last_gl_error();
        glBindBuffer(gl_target, 0);
    }

protected:
    GLuint m_usage;
    unsigned int m_block_length;

    std::basic_string<element_t> m_local_buffer;
    std::vector<std::unique_ptr<GLArrayRegion> > m_regions;
    std::unordered_map<GLArrayRegionID, GLArrayRegion*> m_region_map;
    bool m_any_dirty;

    unsigned int m_remote_size;
    GLArrayRegionID m_region_id_ctr;

protected:
    GLArrayRegion &append_region(unsigned int start,
                                 unsigned int count)
    {
        const GLArrayRegionID region_id = ++m_region_id_ctr;
        m_regions.emplace_back(new GLArrayRegion(
                                   region_id,
                                   start,
                                   count));
        GLArrayRegion &new_region = **(m_regions.end() - 1);
        m_region_map[region_id] = &new_region;
        return new_region;
    }

    inline unsigned int block_size() const
    {
        return m_block_length * sizeof(element_t);
    }

    region_container::iterator compact_regions(
            region_container::iterator iter,
            const unsigned int nregions)
    {
        unsigned int total = 0;
        unsigned int i = nregions;
        do {
            i -= 1;
            --iter;
            total += (*iter)->m_count;
            assert(!(*iter)->m_in_use);
            assert(false);
        } while (i > 0);

        (*iter)->m_count = total;
        return m_regions.erase(iter, iter+nregions);
    }

    region_container::iterator compact_or_expand(
            const unsigned int nblocks)
    {
        auto iterator = m_regions.begin();
        unsigned int aggregation_backlog = 0;
        auto best = m_regions.end();
        unsigned int best_metric = m_local_buffer.size() / m_block_length;

        for (; iterator != m_regions.end(); iterator++)
        {
            GLArrayRegion &region = **iterator;
            if (region.m_in_use)
            {
                if (aggregation_backlog > 1) {
                    iterator = compact_regions(
                                iterator,
                                aggregation_backlog);
                    GLArrayRegion &merged = **(iterator-1);
                    if (merged.m_count >= nblocks) {
                        unsigned int metric = merged.m_count - nblocks;
                        if (metric < best_metric)
                        {
                            best = iterator-1;
                            best_metric = metric;
                        }
                    }
                }
                aggregation_backlog = 0;
                continue;
            }

            aggregation_backlog += 1;
        }

        if (best != m_regions.end()) {
            return best;
        }

        unsigned int required_blocks = nblocks;
        gl_array_logger.log(io::LOG_DEBUG,
                            "out of luck, we have to reallocate");
        if (m_regions.size() > 0) {
            gl_array_logger.log(io::LOG_DEBUG, "but we have regions");
            GLArrayRegion &last_region = **m_regions.rbegin();
            if (!last_region.m_in_use) {
                gl_array_logger.log(io::LOG_DEBUG, "and the last one is not in use");
                assert(last_region.m_count < nblocks);
                required_blocks -= last_region.m_count;
            }
        }

        gl_array_logger.logf(io::LOG_DEBUG,
                             "requesting expansion by %d (out of %d) blocks",
                             required_blocks, nblocks);

        expand(required_blocks);
        return m_regions.end() - 1;
    }

    void delete_globject() override
    {
        glDeleteBuffers(1, &this->m_glid);
        m_remote_size = 0;
        GLObject<gl_binding_type>::delete_globject();
    }

    void expand(const unsigned int at_least_by_blocks)
    {
        const unsigned int required_blocks =
                m_local_buffer.size() / m_block_length + at_least_by_blocks;
        reserve(required_blocks);
    }

    void reserve(const unsigned int min_blocks)
    {
        const unsigned int required_size = min_blocks * m_block_length;
        const unsigned int old_size = m_local_buffer.size();
        unsigned int new_size = old_size;
        if (new_size == 0) {
            new_size = 1;
        }
        while (required_size > new_size) {
            new_size *= 2;
        }
        if (new_size <= old_size) {
            return;
        }

        m_local_buffer.resize(new_size);
        if (m_regions.size() > 0) {
            GLArrayRegion &last_region = **(m_regions.end() - 1);
            if (!last_region.m_in_use) {
                last_region.m_count += (new_size - old_size) / m_block_length;
            }
        } else {
            append_region(old_size / m_block_length,
                          (new_size - old_size) / m_block_length);
        }
    }

    bool reserve_remote()
    {
        if (m_remote_size >= m_local_buffer.size())
        {
            return false;
        }

        glBufferData(gl_target,
                     m_local_buffer.size() * sizeof(element_t),
                     m_local_buffer.data(),
                     m_usage);
        m_remote_size = m_local_buffer.size();
        return true;
    }

    GLArrayRegion &split_region(
            region_container::iterator iter,
            const unsigned int blocks_for_first)
    {
        GLArrayRegion *first_region = &**iter;

        const GLArrayRegionID new_region_id = ++m_region_id_ctr;
        auto new_region_iter = m_regions.emplace(
                    iter+1,
                    new GLArrayRegion(
                        new_region_id,
                        first_region->m_start + blocks_for_first,
                        first_region->m_count - blocks_for_first)
                    );
        GLArrayRegion &new_region = **new_region_iter;
        first_region = &**(new_region_iter-1);
        m_region_map[new_region_id] = &new_region;

        first_region->m_count = blocks_for_first;

        return *first_region;
    }

    void upload_dirty()
    {
        gl_array_logger.logf(io::LOG_DEBUG,
                             "upload dirty called on array (glid=%d, local_size=%d)",
                             this->m_glid,
                             m_local_buffer.size());

        if (reserve_remote())
        {
            gl_array_logger.log(
                        io::LOG_DEBUG,
                        "remote reallocation took place, no need to retransfer"
                        );
            // reallocation took place, this uploads all data
            for (auto &region: m_regions) {
                region->m_dirty = false;
            }
            m_any_dirty = false;
            return;
        }

        if (!m_any_dirty) {
            gl_array_logger.log(
                        io::LOG_DEBUG,
                        "not dirty, bailing out");
            // std::cout << "nothing to upload (m_any_dirty=false)" << std::endl;
            return;
        }

        unsigned int left_block = m_local_buffer.size();
        unsigned int right_block = 0;

        for (auto &region: m_regions)
        {
            if (!region->m_in_use || !region->m_dirty)
            {
                continue;
            }

            if (region->m_start < left_block)
            {
                left_block = region->m_start;
            }
            const unsigned int end = region->m_start + region->m_count;
            if (end > right_block)
            {
                right_block = end;
            }

            region->m_dirty = false;
        }

        if (right_block > 0) {
            const unsigned int offset = left_block * block_size();
            const unsigned int size = (right_block - left_block) * block_size();
            gl_array_logger.log(io::LOG_DEBUG)
                    << "uploading "
                    << size << " bytes at offset "
                    << offset
                    << " (glid=" << this->m_glid << ")" << io::submit;
            glBufferSubData(gl_target, offset, size, m_local_buffer.data() + offset*m_block_length);
        } else {
            // std::cout << "nothing to upload (right_block=0)" << std::endl;
        }

        m_any_dirty = false;
    }

public:
    allocation_t allocate(unsigned int nblocks)
    {
        gl_array_logger.logf(io::LOG_DEBUG,
                             "(glid=%d) trying to allocate %d blocks",
                             this->m_glid,
                             nblocks);

        auto iterator = m_regions.begin();
        for (; iterator != m_regions.end(); ++iterator)
        {
            GLArrayRegion &region = **iterator;
            gl_array_logger.logf(io::LOG_DEBUG,
                                 "region (%p) %d: start=%d, in_use=%d, count=%d",
                                 &region,
                                 region.m_id,
                                 region.m_start,
                                 region.m_in_use,
                                 region.m_count);
            if (region.m_in_use) {
                gl_array_logger.logf(io::LOG_DEBUG,
                                     "region %d in use, skipping",
                                     region.m_id);
                continue;
            }
            if (region.m_count < nblocks) {
                gl_array_logger.logf(io::LOG_DEBUG,
                                     "region %d too small (%d), skipping",
                                     region.m_id,
                                     region.m_count);
                continue;
            }
            gl_array_logger.logf(io::LOG_DEBUG,
                                 "region %d looks suitable",
                                 region.m_id);
            break;
        }

        if (iterator == m_regions.end())
        {
            gl_array_logger.log(io::LOG_DEBUG,
                                "out of buffer memory");
            // out of memory
            iterator = compact_or_expand(nblocks);
            gl_array_logger.logf(io::LOG_DEBUG,
                                 "compact_or_expand returned region %d (count=%d)",
                                 (*iterator)->m_id,
                                 (*iterator)->m_count);
        }

        GLArrayRegion *region_to_use = &**iterator;
        if (region_to_use->m_count > nblocks)
        {
            // split region
            gl_array_logger.logf(io::LOG_DEBUG,
                                 "region %d too large, splitting",
                                 region_to_use->m_id);
            region_to_use = &split_region(iterator, nblocks);
            gl_array_logger.logf(io::LOG_DEBUG,
                                 "now using region %d (start=%d, count=%d)",
                                 region_to_use->m_id,
                                 region_to_use->m_start,
                                 region_to_use->m_count);
        }

        region_to_use->m_in_use = true;
        region_to_use->m_dirty = false;

        gl_array_logger.logf(io::LOG_DEBUG,
                             "allocated %d blocks to region %d",
                             nblocks,
                             region_to_use->m_id);

        gl_array_logger.logf(io::LOG_DEBUG,
                             "region (%p) %d: start=%d, in_use=%d, count=%d",
                             region_to_use,
                             region_to_use->m_id,
                             region_to_use->m_start,
                             region_to_use->m_in_use,
                             region_to_use->m_count);

        return allocation_t((buffer_t*)this,
                            m_block_length,
                            nblocks,
                            region_to_use->m_id);
    }

    void dump_remote_raw()
    {
        if (m_remote_size == 0 || this->m_glid == 0) {
            std::cout << "no remote data" << std::endl;
            return;
        }

        std::cout << "BEGIN OF BUFFER DUMP (glid = " << this->m_glid << ")" << std::endl;

        std::basic_string<element_t> buf;
        buf.resize(m_remote_size);
        glGetBufferSubData(gl_target, 0, m_remote_size*sizeof(element_t), &buf.front());

        for (auto &item: buf) {
            std::cout << item << std::endl;
        }

        std::cout << "END OF BUFFER DUMP (glid = " << this->m_glid << ")" << std::endl;
    }

    element_t *region_get_ptr(const GLArrayRegionID region_id)
    {
        return (&m_local_buffer.front()) + m_region_map[region_id]->m_start*m_block_length;
    }

    void region_mark_dirty(const GLArrayRegionID region_id)
    {
        m_region_map[region_id]->m_dirty = true;
        m_any_dirty = true;
    }

    void region_release(const GLArrayRegionID region_id)
    {
        gl_array_logger.logf(io::LOG_DEBUG, "(glid=%d) region %d released",
                             this->m_glid,
                             region_id);
        m_region_map[region_id]->m_in_use = false;
    }

    std::size_t region_offset(const GLArrayRegionID region_id)
    {
        return m_region_map[region_id]->m_start*m_block_length*sizeof(element_t);
    }

    unsigned int region_base(const GLArrayRegionID region_id)
    {
        return m_region_map[region_id]->m_start;
    }

    void bind() override
    {
        glBindBuffer(gl_target, this->m_glid);
        this->bound();
    }

    void sync() override
    {
        bind();
        upload_dirty();
    }

    void unbind() override
    {
        glBindBuffer(gl_target, 0);
    }

};

}

#endif