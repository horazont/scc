/**********************************************************************
File name: world.hpp
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
#ifndef SCC_SIM_WORLD_H
#define SCC_SIM_WORLD_H

#include <chrono>

#include <google/protobuf/io/zero_copy_stream.h>

#include "engine/sim/terrain.hpp"
#include "engine/sim/fluid.hpp"
#include "engine/sim/objects.hpp"

#include "types.pb.h"

namespace sim {

namespace messages {

class WorldCommand;
class WorldCommandResponse;

}

typedef std::chrono::steady_clock WorldClock;


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
    ObjectManager m_objects;

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

    inline const ObjectManager &objects() const
    {
        return m_objects;
    }

    inline ObjectManager &objects()
    {
        return m_objects;
    }

};


/**
 * Abstract base class for operations modifying the game state using a
 * WorldMutator.
 */
class WorldOperation
{
public:
    virtual ~WorldOperation();

public:
    /**
     * Execute the world operation against the world which is mutated by the
     * given \a mutator.
     *
     * @param state World state to manipulate.
     * @return The result of the operation, as returned by the mutator.
     */
    virtual WorldOperationResult execute(WorldState &state) = 0;

public:
    /**
     * Use the given \a msg to recover a world command which can be applied
     * to a WorldMutator.
     *
     * If the message has more than one command payload, which one is chosen
     * is unspecified.
     *
     * @param msg A valid message containing a world command.
     * @return A new WorldOperation instance which executes the world command
     * described by the given message.
     */
    static std::unique_ptr<WorldOperation> from_message(
            const sim::messages::WorldCommand &msg);
};

typedef std::unique_ptr<WorldOperation> WorldOperationPtr;


class AbstractClient
{
public:
    typedef std::function<void(WorldOperationResult)> ResultCallback;

public:
    virtual ~AbstractClient();

private:
    static std::atomic<WorldOperationToken> m_token_ctr;
    std::mutex m_callbacks_mutex;
    std::unordered_map<WorldOperationToken, ResultCallback> m_callbacks;

protected:
    virtual void send_command_to_backend(
            const sim::messages::WorldCommand &cmd) = 0;

public:
    /**
     * Receive a response from the server. This triggers calling any callback
     * associated with the response.
     *
     * @param resp Response message to process.
     */
    void recv_response(const sim::messages::WorldCommandResponse &resp);

    /**
     * Send a world command wrapped in a protobuf message to the server.
     *
     * @param cmd Command to send. The client fills in the token field
     * appropriately and then forwards the message for sending.
     * @param callback Optionally, a callback to call when a response to the
     * command from the server is processed. The thread in which \a callback
     * is called is unspecified.
     */
    void send_command(
            sim::messages::WorldCommand &cmd,
            ResultCallback &&callback = nullptr);

};


class IServerClientInterface
{
public:
    virtual void send_message(const google::protobuf::Message &msg) = 0;
    virtual bool valid() const = 0;

};


class Server
{
public:
    typedef std::shared_lock<std::shared_timed_mutex> SyncSafeLock;

public:
    Server();
    ~Server();

private:
    WorldState m_state;

    /* guarded by m_clients_mutex */
    std::mutex m_clients_mutex;
    std::vector<std::unique_ptr<IServerClientInterface> > m_client_interfaces;

    /* guarded by m_op_queue_mutex */
    std::mutex m_op_queue_mutex;
    std::vector<std::unique_ptr<WorldOperation> > m_op_queue;

    std::atomic_bool m_terminated;
    std::thread m_game_thread;

    /* owned by m_game_thread */
    std::vector<std::unique_ptr<WorldOperation> > m_op_buffer;

    /**
     * This mutex is used to put the Server into a state which is safe for
     * syncing, that is, reading arbitrary data.
     */
    std::shared_timed_mutex m_interframe_mutex;

protected:
    void game_frame();
    void game_thread();

public:
    inline WorldState &state()
    {
        return m_state;
    }

    inline const WorldState &state() const
    {
        return m_state;
    }

    /**
     * Thread-safely enqueue a world operation for the next game frame.
     *
     * There won’t be any feedback on the operation. This is generally not
     * useful for actual client-server usage, but for local terraforming it
     * is quite handy.
     *
     * @param op Operation to execute in the next game frame.
     */
    void enqueue_op(std::unique_ptr<WorldOperation> &&op);

    /**
     * Return a lock object on the WorldState and ensure that simulations are
     * in a state where their front buffers / data can be read safely.
     *
     * This makes sure that the game loop stops until all locks are released
     * and that all simulations are in a state where they are readable.
     *
     * @return An opaque, movable object; it cannot be passed between threads.
     * As long as your thread holds such an object, it is safe to access the
     * world state for reading.
     */
    SyncSafeLock sync_safe_point();

};


static_assert(std::ratio_less<WorldClock::period, std::milli>::value,
              "WorldClock (std::chrono::steady_clock) has not enough precision");

}

#endif
