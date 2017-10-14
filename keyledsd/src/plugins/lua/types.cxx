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
#include "plugins/lua/types.h"

#include <lua.hpp>
#include <cassert>

namespace keyleds { namespace plugin { namespace lua {

/****************************************************************************/

#ifndef NDEBUG
#define SAVE_TOP(lua)           int saved_top_ = lua_gettop(lua)
#define CHECK_TOP(lua, depth)   assert(lua_gettop(lua) == saved_top_ + depth)
#else
#define SAVE_TOP(lua)
#define CHECK_TOP(lua, depth)
#endif

/****************************************************************************/

void registerType(lua_State * lua, const char * name, const luaL_reg * methods)
{
    assert(lua);
    assert(name);
    assert(methods);

    SAVE_TOP(lua);
    luaL_newmetatable(lua, name);
    luaL_register(lua, nullptr, methods);
    lua_pushnil(lua);
    lua_setfield(lua, -2, "__metatable");
    lua_pop(lua, 1);
    CHECK_TOP(lua, 0);
}

bool isType(lua_State * lua, int index, const char * name)
{
    assert(lua);
    assert(name);

    SAVE_TOP(lua);
    if (lua_getmetatable(lua, index) == 0) { return false; }
    luaL_getmetatable(lua, name);
    bool result = lua_rawequal(lua, -2, -1);
    lua_pop(lua, 2);
    CHECK_TOP(lua, 0);

    return result;
}

} } } // namespace keyleds::plugin::lua
