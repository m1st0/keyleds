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
#ifndef KEYLEDS_PLUGINS_LUA_DEVICESTATE_H_BBB925ED
#define KEYLEDS_PLUGINS_LUA_DEVICESTATE_H_BBB925ED

#include <lua.hpp>
#include <memory>
#include <string>
#include <utility>

namespace std {
    template <> struct default_delete<lua_State> { void operator()(lua_State *) const; };
}

namespace keyleds { namespace plugin { namespace lua {

/****************************************************************************/

class State final
{
public:
    class Container;
public:
                                State();
                                State(State &&) noexcept = default;
    State &                     operator=(State &&) = default;
                                ~State();

    lua_State *                 lua() { return m_lua.get(); }

    Container                   createContainer();

    static void luaGetRegistry(lua_State *, const char * key);  ///< push onto stack
    static void luaSetRegistry(lua_State *, const char * key);  ///< pop from stack

private:
    std::unique_ptr<lua_State>  m_lua;
};

/****************************************************************************/

/// Lightweight wrapper retaining a reference to a lua thread
class State::Container final
{
    using ref_id = int;
public:
                        Container(State &, ref_id, lua_State *);
                        Container(const Container &) = delete;
                        Container(Container && other) noexcept
                         : m_state(other.m_state), m_id(LUA_NOREF), m_lua(nullptr)
                         { using std::swap; swap(m_id, other.m_id); swap(m_lua, other.m_lua); }
                        ~Container();

    State &             state() const { return m_state; }
    lua_State *         lua() { return m_lua; }

    void                push(lua_State *);  ///< push container table onto stack

private:
    State &             m_state;    ///< Global state the container belongs to
    ref_id              m_id;       ///< Reference preventing m_lua from being GC'ed within lua
    lua_State *         m_lua;      ///< Lua Container thread for this C++ container
};

/****************************************************************************/

} } } // namespace keyleds::plugin::lua

#endif
