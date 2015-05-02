/**********************************************************************
File name: fluid.cpp
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
#include "engine/sim/fluid.hpp"

#include <GL/glew.h>

#include <iostream>

#include "engine/gl/util.hpp"
#include "engine/io/log.hpp"


#define TIMELOG_FLUIDSIM

#ifdef TIMELOG_FLUIDSIM
#include <chrono>
typedef std::chrono::steady_clock timelog_clock;

#define TIMELOG_ms(x) std::chrono::duration_cast<std::chrono::duration<float, std::ratio<1, 1000> > >(x).count()
#endif


namespace sim {

static io::Logger &logger = io::logging().get_logger("sim.fluid");


const float Fluid::flow_damping = 0.991f;
const float Fluid::flow_friction = 0.3f;
const unsigned int Fluid::block_size = 60;


FluidCellMeta::FluidCellMeta():
    terrain_height(0.f)
{

}


FluidCell::FluidCell():
    fluid_height(0.f),
    fluid_flow{0.f, 0.f}
{

}


FluidBlocks::FluidBlocks(const unsigned int block_count_per_axis,
                         const unsigned int block_size):
    m_block_size(block_size),
    m_blocks_per_axis(block_count_per_axis),
    m_cells_per_axis(m_block_size*m_blocks_per_axis),
    m_block_meta(m_blocks_per_axis*m_blocks_per_axis),
    m_meta_cells(m_cells_per_axis*m_cells_per_axis),
    m_front_cells(m_cells_per_axis*m_cells_per_axis),
    m_back_cells(m_cells_per_axis*m_cells_per_axis)
{

}


Fluid::Fluid(const Terrain &terrain):
    m_terrain(terrain),
    m_block_count((m_terrain.size()-1) / block_size),
    m_blocks(m_block_count, block_size),
    m_terrain_update_conn(m_terrain.terrain_updated().connect(
                              sigc::mem_fun(*this, &Fluid::terrain_updated))),
    m_worker_terminate(false),
    // initializing the m_block_ctr to a value >= m_block_count**2 will make
    // all workers go to sleep immediately
    m_block_ctr(m_block_count*m_block_count),
    m_terminated(false),
    m_coordinator_thread(std::bind(&Fluid::coordinator_impl, this)),
    m_run(false),
    m_done(false)
{
    if (m_block_count * block_size != m_terrain.size()-1) {
        throw std::runtime_error("Terrain size minus one must be a multiple of"
                                 " fluid block size, which is "+
                                 std::to_string(block_size));
    }

    if (!std::atomic_is_lock_free(&m_block_ctr)) {
        logger.logf(io::LOG_WARNING, "fluid sim counter is not lock-free.");
    } else {
        logger.logf(io::LOG_INFO, "fluid sim counter is lock-free.");
    }

    unsigned int thread_count = std::thread::hardware_concurrency();
    if (thread_count == 0) {
        thread_count = 2;
        logger.logf(io::LOG_ERROR,
                    "failed to determine hardware concurrency. "
                    "giving it a try with %u",
                    thread_count);
    }
    m_worker_threads.reserve(thread_count);
    for (unsigned int i = 0; i < thread_count; i++) {
        m_worker_threads.emplace_back(std::bind(&Fluid::worker_impl, this));
    }
}

Fluid::~Fluid()
{
    m_terrain_update_conn.disconnect();
    m_terminated = true;
    m_control_wakeup.notify_all();
    m_coordinator_thread.join();
    for (auto &thread: m_worker_threads) {
        thread.join();
    }
}

void Fluid::coordinator_impl()
{
    logger.logf(io::LOG_INFO, "fluidsim: %u cells in %u blocks",
                m_blocks.m_cells_per_axis*m_blocks.m_cells_per_axis,
                m_blocks.m_blocks_per_axis*m_blocks.m_blocks_per_axis);
    while (!m_terminated) {
        {
            std::unique_lock<std::mutex> control_lock(m_control_mutex);
            while (!m_run && !m_terminated) {
                m_control_wakeup.wait(control_lock);
            }
            if (m_terminated) {
                control_lock.unlock();
                std::unique_lock<std::mutex> done_lock(m_done_mutex);
                m_done = true;
                break;
            }
            m_run = false;
        }

#ifdef TIMELOG_FLUIDSIM
        const timelog_clock::time_point t0 = timelog_clock::now();
        timelog_clock::time_point t_sync, t_prepare, t_sim;
#endif
        // sync terrain
        TerrainRect updated_rect;
        {
            std::unique_lock<std::mutex> lock(m_terrain_update_mutex);
            updated_rect = m_terrain_update;
            m_terrain_update = NotARect;
        }
        if (!updated_rect.empty()) {
            logger.logf(io::LOG_INFO, "terrain to sync (%u vertices)",
                        updated_rect.area());
            sync_terrain(updated_rect);
        }


#ifdef TIMELOG_FLUIDSIM
        t_sync = timelog_clock::now();
#endif

        m_block_ctr = 0;
        {
            std::unique_lock<std::mutex> worker_lock(m_worker_wakeup_mutex);
            m_worker_job = JobType::PREPARE;
        }
        m_worker_wakeup.notify_all();

        {
            std::unique_lock<std::mutex> done_lock(m_worker_done_mutex);
            while (m_block_ctr < m_block_count*m_block_count) {
                m_worker_done.wait(done_lock);
            }
        }

#ifdef TIMELOG_FLUIDSIM
        t_prepare = timelog_clock::now();
#endif

        m_block_ctr = 0;
        {
            std::unique_lock<std::mutex> worker_lock(m_worker_wakeup_mutex);
            m_worker_job = JobType::UPDATE;
        }
        m_worker_wakeup.notify_all();

        {
            std::unique_lock<std::mutex> done_lock(m_worker_done_mutex);
            while (m_block_ctr < m_block_count*m_block_count) {
                m_worker_done.wait(done_lock);
            }
        }

        {
            std::unique_lock<std::mutex> done_lock(m_done_mutex);
            assert(!m_done);
            m_done = true;
        }
        m_done_wakeup.notify_all();

#ifdef TIMELOG_FLUIDSIM
        t_sim = timelog_clock::now();
        logger.logf(io::LOG_DEBUG, "fluid: sync time: %.2f ms",
                    TIMELOG_ms(t_sync - t0));
        logger.logf(io::LOG_DEBUG, "fluid: prep time: %.2f ms",
                    TIMELOG_ms(t_prepare - t_sync));
        logger.logf(io::LOG_DEBUG, "fluid: sim time: %.2f ms",
                    TIMELOG_ms(t_sim - t_prepare));
#endif
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    m_worker_terminate = true;
    m_worker_wakeup.notify_all();
}

void Fluid::prepare_block(const unsigned int x, const unsigned int y)
{
    // copy the frontbuffer to the backbuffer
    const unsigned int cy0 = y*m_blocks.m_block_size;
    const unsigned int cy1 = (y+1)*m_blocks.m_block_size;
    const unsigned int cx0 = x*m_blocks.m_block_size;
    const unsigned int cx1 = (x+1)*m_blocks.m_block_size;
    for (unsigned int cy = cy0; cy < cy1; cy++)
    {
        FluidCell *front = m_blocks.cell_front(cx0, cy);
        FluidCell *back = m_blocks.cell_front(cx0, cy);
        for (unsigned int cx = cx0; cx < cx1; cx++)
        {
            *back = *front;
            ++front;
            ++back;
        }
    }
}

void Fluid::sync_terrain(TerrainRect rect)
{
    if (rect.x1() == m_terrain.size()) {
        rect.set_x1(m_terrain.size()-1);
    }
    if (rect.y1() == m_terrain.size()) {
        rect.set_y1(m_terrain.size()-1);
    }

    const unsigned int terrain_size = m_terrain.size();
    const Terrain::HeightField *field = nullptr;
    auto lock = m_terrain.readonly_field(field);
    for (unsigned int y = rect.y0(); y < rect.y1(); y++) {
        FluidCellMeta *meta_ptr = m_blocks.cell_meta(rect.x0(), y);
        for (unsigned int x = rect.x0(); x < rect.x1(); x++) {
            const Terrain::height_t hsum =
                    (*field)[y*terrain_size+x]+
                    (*field)[y*terrain_size+x+1]+
                    (*field)[(y+1)*terrain_size+x]+
                    (*field)[(y+1)*terrain_size+x+1];
            meta_ptr->terrain_height = hsum / 4.f;
            ++meta_ptr;
        }
    }
}

void Fluid::terrain_updated(TerrainRect r)
{
    std::unique_lock<std::mutex> lock(m_terrain_update_mutex);
    m_terrain_update = bounds(r, m_terrain_update);
}

void Fluid::update_block(const unsigned int x, const unsigned int y)
{
}

void Fluid::worker_impl()
{
    const unsigned int out_of_tasks_limit = m_block_count*m_block_count;

    std::unique_lock<std::mutex> wakeup_lock(m_worker_wakeup_mutex);
    while (!m_worker_terminate)
    {
        while (m_block_ctr >= out_of_tasks_limit && !m_worker_terminate) {
            /*logger.logf(io::LOG_DEBUG, "fluid: %p waiting for tasks...", this);*/
            m_worker_wakeup.wait(wakeup_lock);
            /*logger.logf(io::LOG_DEBUG, "fluid: %p woke up", this);*/
        }
        if (m_worker_terminate) {
            return;
        }
        JobType my_job = m_worker_job;
        /*logger.logf(io::LOG_DEBUG, "fluid: %p starts processing", this);*/
        wakeup_lock.unlock();

        while (1) {
            const unsigned int my_block = m_block_ctr.fetch_add(1);
            /*logger.logf(io::LOG_DEBUG, "fluid: %p got %u", this, my_block);*/
            if (my_block >= out_of_tasks_limit) {
                break;
            }

            const unsigned int x = my_block % m_blocks.m_blocks_per_axis;
            const unsigned int y = my_block / m_blocks.m_blocks_per_axis;
            switch (my_job) {
            case JobType::PREPARE:
                prepare_block(x, y);
                break;
            case JobType::UPDATE:
                update_block(x, y);
                break;
            }
        }

        m_worker_done.notify_one();

        wakeup_lock.lock();
    }
}

void Fluid::start()
{
    m_blocks.swap_buffers();
    {
        std::unique_lock<std::mutex> lock(m_control_mutex);
        assert(!m_run);
        m_run = true;
    }
    m_control_wakeup.notify_all();
}

void Fluid::to_gl_texture() const
{
    const unsigned int total_cells = m_blocks.m_cells_per_axis*m_blocks.m_cells_per_axis;

    // terrain_height, fluid_height, flowx, flowy
    std::vector<Vector4f> buffer(total_cells);
    std::shared_lock<std::shared_timed_mutex> lock(m_blocks.m_frontbuffer_mutex);
    const FluidCellMeta *meta = m_blocks.cell_meta(0, 0);
    const FluidCell *cell = m_blocks.cell_front(0, 0);
    Vector4f *dest = &buffer.front();
    for (unsigned int i = 0; i < total_cells; i++)
    {
        *dest = Vector4f(meta->terrain_height, cell->fluid_height,
                         cell->fluid_flow[0], cell->fluid_flow[1]);

        ++cell;
        ++meta;
        ++dest;
    }

    glTexSubImage2D(GL_TEXTURE_2D, 0,
                    0, 0,
                    m_blocks.m_cells_per_axis,
                    m_blocks.m_cells_per_axis,
                    GL_RGBA,
                    GL_FLOAT,
                    buffer.data());
    engine::raise_last_gl_error();
}

void Fluid::wait_for()
{
    {
        std::unique_lock<std::mutex> lock(m_done_mutex);
        while (!m_done) {
            m_done_wakeup.wait(lock);
        }
        m_done = false;
    }
}


}
