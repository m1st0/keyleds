/* Keyleds -- Gaming keyboard tool
 * Copyright (C) 2017 Julien Hartmann, juli1.hartmann@gmail.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "plugins/lua/State.h"

#include <lua.hpp>
#include <cassert>
#include "plugins/lua/lua_keyleds.h"

using keyleds::plugin::lua::State;

namespace std {
    void default_delete<lua_State>::operator()(lua_State *p) const { lua_close(p); }
}

/****************************************************************************/
// Constants

// Inner environemnt
static constexpr const char registryToken[] = "_keyleds_";
static constexpr const char containerRegToken[] = "containers";

#ifndef NDEBUG
#define SAVE_TOP(lua)           int saved_top_ = lua_gettop(lua)
#define CHECK_TOP(lua, depth)   assert(lua_gettop(lua) == saved_top_ + depth)
#else
#define SAVE_TOP(lua)
#define CHECK_TOP(lua, depth)
#endif

/****************************************************************************/

/// Convert a lua panic into abort - gives better messages than letting lua exit().
static int luaPanicHandler(lua_State *) { abort(); }

/****************************************************************************/
// State

State::State()
 : m_lua(luaL_newstate())
{
    auto * lua = m_lua.get();
    lua_atpanic(lua, luaPanicHandler);
    SAVE_TOP(lua);

    // Create local registries
    lua_newtable(lua);                          // push(registry)
    lua_setfield(lua, LUA_REGISTRYINDEX, registryToken); // pop(registry)

    lua_newtable(lua);                          // push(containers)
    luaSetRegistry(lua, containerRegToken);     // pop(containers)

    CHECK_TOP(lua, 0);
}

State::~State() {}

State::Container State::createContainer()
{
    auto * lua = m_lua.get();
    SAVE_TOP(lua);

    luaGetRegistry(lua, containerRegToken);     // push(containers)
    auto * container = lua_newthread(lua);      // push(thread)

    {
        SAVE_TOP(container);
        lua_newtable(container);
        lua_replace(container, LUA_GLOBALSINDEX);

        // Create special alias _G
        lua_pushvalue(container, LUA_GLOBALSINDEX);         // push(container)
        lua_setfield(container, LUA_GLOBALSINDEX, "_G");    // pop(container)

        CHECK_TOP(container, 0);
    }

    // At this point, stack has (containers, container)
    int id = luaL_ref(lua, -2);                 // pop(thread)
    lua_pop(lua, 1);                            // pop(containers)

    CHECK_TOP(lua, 0);
    return Container(*this, id, container);
}

void State::luaGetRegistry(lua_State * lua, const char * key)
{
    SAVE_TOP(lua);
    lua_getfield(lua, LUA_REGISTRYINDEX, registryToken);
    lua_getfield(lua, -1, key);
    lua_replace(lua, -2);
    CHECK_TOP(lua, +1);
}

void State::luaSetRegistry(lua_State * lua, const char * key)
{
    SAVE_TOP(lua);
    lua_getfield(lua, LUA_REGISTRYINDEX, registryToken);    // push (registry)
    lua_insert(lua, -2);                        // swap (value, registry) => (registry, value)
    lua_setfield(lua, -2, key);                 // pop(value)
    lua_pop(lua, 1);                            // pop(registry)
    CHECK_TOP(lua, -1);
}


/****************************************************************************/

State::Container::Container(State & state, ref_id id, lua_State * lua)
 : m_state(state), m_id(id), m_lua(lua)
{}

State::Container::~Container()
{
    if (m_lua) {
        SAVE_TOP(m_lua);
        State::luaGetRegistry(m_lua, containerRegToken);// push(environments)
        luaL_unref(m_lua, -1, m_id);                    // remove ref to thread so it can be GC'd
        lua_pop(m_lua, 1);                              // pop(environments)
        CHECK_TOP(m_lua, 0);
        lua_gc(m_lua, LUA_GCCOLLECT, 0);                // force destruction of objects
    }
}

void State::Container::push(lua_State * lua)
{
    SAVE_TOP(lua);
    State::luaGetRegistry(lua, containerRegToken);      // push(environments)
    lua_rawgeti(lua, -1, m_id);                         // push(env)
    lua_replace(lua, -2);                               // swap(environments, env) => (env)
    CHECK_TOP(lua, +1);
}
