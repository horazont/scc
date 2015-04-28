/**********************************************************************
File name: client.cpp
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
#include "engine/sim/client.hpp"

namespace sim {

/* sim::Server */

Server::Server():
    m_state(),
    m_mutator(m_state),
    m_terminated(false),
    m_game_thread(std::bind(&Server::game_thread, this))
{

}

Server::~Server()
{
    m_terminated = true;
    m_game_thread.join();
}

void Server::game_frame()
{
    m_state.fluid().wait_for();

    {
        std::unique_lock<std::mutex> lock(m_op_queue_mutex);
        m_op_queue.swap(m_op_buffer);
    }

    for (auto &op: m_op_buffer)
    {
        op->execute(m_mutator);
    }

    m_op_buffer.clear();

    m_state.fluid().start();
}

void Server::game_thread()
{
    static const std::chrono::microseconds game_frame_duration(16000);
    static const std::chrono::microseconds busywait(100);

    // tnext_frame is always in the future when we are on time
    WorldClock::time_point tnext_frame = WorldClock::now();
    while (!m_terminated)
    {
        WorldClock::time_point tnow = WorldClock::now();
        {
            std::chrono::nanoseconds time_to_sleep = tnext_frame - tnow;
            if (time_to_sleep > busywait) {
                std::this_thread::sleep_for(time_to_sleep - busywait);
            }
            continue;
        }

        game_frame();

        tnext_frame += game_frame_duration;
    }
}


/* sim::LocalServer */

LocalServer::LocalServer():
    Server(),
    m_async_mutator(std::bind(&LocalServer::enqueue_op, this, std::placeholders::_1))
{

}

IAsyncWorldMutator &LocalServer::async_mutator()
{
    return m_async_mutator;
}

}
