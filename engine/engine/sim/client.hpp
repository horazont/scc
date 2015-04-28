/**********************************************************************
File name: client.hpp
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
#ifndef SCC_SIM_CLIENT_H
#define SCC_SIM_CLIENT_H

#include <chrono>

#include "engine/sim/terrain.hpp"
#include "engine/sim/fluid.hpp"

namespace sim {


typedef std::chrono::steady_clock WorldClock;


enum WorldOperationResult
{
    WOPR_NO_ERROR = 0,
};


typedef uint32_t WorldOperationToken;


/**
 * A class holding the complete world state, including all simulation data.
 *
 * Most of the state is aggregated by composing different classes into this
 * uberclass.
 */
class WorldState
{
public:
    WorldState();

protected:
    Terrain m_terrain;
    Fluid m_fluid;

public:
    inline const Fluid &fluid() const
    {
        return m_fluid;
    }

    inline Fluid &fluid()
    {
        return m_fluid;
    }

    inline const Terrain &terrain() const
    {
        return m_terrain;
    }

    inline Terrain &terrain()
    {
        return m_terrain;
    }

};


/**
 * Actual implementation of all mutation operations which affect the world
 * state.
 *
 * This does not implement the IAsyncWorldMutator interface, which is made
 * for asynchronous operations. The methods of this class block until the
 * operation has completed and return a status code.
 */
class WorldMutator
{
public:
    WorldMutator(WorldState &world);

private:
    WorldState &m_state;

public:
    /**
     * Raise the terrain around \a xc, \a yc.
     *
     * This uses the given brush, determined by the \a brush_size and the
     * \a density_map, multiplied with \a brush_strength. \a brush_strength
     * may be negative to create a lowering effect.
     *
     * @param xc X center of the effect
     * @param yc Y center of the effect
     * @param brush_size Diameter of the brush
     * @param density_map Density values of the brush (must have \a brush_size
     * times \a brush_size entries).
     * @param brush_strength Strength factor for applying the brush, should be
     * in the range [-1.0, 1.0].
     */
    WorldOperationResult tf_raise(const float xc, const float yc,
                                  const unsigned int brush_size,
                                  const std::vector<float> &density_map,
                                  const float brush_strength);

    /**
     * Level the terrain around \a xc, \a yc to a specific reference height
     * \a ref_height.
     *
     * This uses the given brush, determined by the \a brush_size and the
     * \a density_map, multiplied with \a brush_strength. Using a negative
     * brush strength will have the same effect as using a negative
     * \a ref_height and will lead to clipping on the lower end of the terrain
     * height dynamic range.
     *
     * @param xc X center of the effect
     * @param yc Y center of the effect
     * @param brush_size Diameter of the brush
     * @param density_map Density values of the brush (must have \a brush_size
     * times \a brush_size entries).
     * @param brush_strength Strength factor for applying the brush, should be
     * in the range [0.0, 1.0].
     * @param ref_height Reference height to level the terrain to.
     */
    WorldOperationResult tf_level(const float xc, const float yc,
                                  const unsigned int brush_size,
                                  const std::vector<float> &density_map,
                                  const float brush_strength,
                                  const float ref_height);


    WorldOperationResult fluid_raise(const float xc, const float yc,
                                     const unsigned int brush_size,
                                     const std::vector<float> &density_map,
                                     const float brush_strength);
};


class WorldOperation
{
public:
    WorldOperation();

private:
    static std::atomic<WorldOperationToken> m_token_ctr;
    WorldOperationToken m_token;

public:
    virtual WorldOperationResult execute(WorldMutator &target) = 0;

    inline WorldOperationToken token() const
    {
        return m_token;
    }

};


class SubmittingWorldMutator
{
public:
    typedef std::function<void(std::unique_ptr<WorldOperation>&&)> submit_func_t;

public:
    template <typename func_t>
    SubmittingWorldMutator(func_t &&func):
        m_submit(std::forward<func_t>(func))
    {

    }

private:
    submit_func_t m_submit;

public:
    /**
     * Asynchronously raise or lower the terrain.
     *
     * @return A token which can be used to identify a result event related
     * to this command.
     *
     * @see WorldState.tf_raise for a full description of all arguments.
     */
    WorldOperationToken tf_raise(
            const float xc, const float yc,
            const unsigned int brush_size,
            const std::vector<float> &density_map,
            const float brush_strength);

    /**
     * Asynchronously level the terrain.
     *
     * @return A token which can be used to identify a result event related
     * to this command.
     *
     * @see WorldState.tf_level for a full description of all arguments.
     */
    WorldOperationToken tf_level(
            const float xc, const float yc,
            const unsigned int brush_size,
            const std::vector<float> &density_map,
            const float brush_strength,
            const float ref_height);

    /**
     * Asynchronously raise the fluid level.
     *
     * @return A token which can be used to identify a result event related
     * to this command.
     *
     * @see WorldState.fluid_raise for a full description of all arguments.
     */
    WorldOperationToken fluid_raise(
            const float xc, const float yc,
            const unsigned int brush_size,
            const std::vector<float> &density_map,
            const float brush_strength);
};

typedef SubmittingWorldMutator IAsyncWorldMutator;


class IClient
{
public:
    virtual IAsyncWorldMutator &async_mutator() = 0;

};


class Server
{
public:
    Server();
    ~Server();

private:
    WorldState m_state;
    WorldMutator m_mutator;

    /* guarded by m_op_queue_mutex */
    std::mutex m_op_queue_mutex;
    std::vector<std::unique_ptr<WorldOperation> > m_op_queue;

    std::atomic_bool m_terminated;
    std::thread m_game_thread;

    /* owned by m_game_thread */
    std::vector<std::unique_ptr<WorldOperation> > m_op_buffer;

protected:
    void game_frame();
    void game_thread();

public:
    WorldMutator &mutator();

    void enqueue_op(std::unique_ptr<WorldOperation> &&op);

};


class LocalServer: public Server, public IClient
{
public:
    LocalServer();

protected:
    SubmittingWorldMutator m_async_mutator;

public:
    IAsyncWorldMutator &async_mutator() override;

};


static_assert(std::ratio_less<WorldClock::period, std::milli>::value,
              "WorldClock (std::chrono::steady_clock) has not enough precision");

}

#endif
