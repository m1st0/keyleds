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
#include "plugins/lua/lua_Animation.h"

#include "plugins/lua/lua_common.h"
#include "plugins/lua/lua_keyleds.h"

namespace keyleds { namespace lua {

/****************************************************************************/

static int pause(lua_State * lua)
{
    lua_check<Animation>(lua, 1).running = false;
    return 0;
}

static int resume(lua_State * lua)
{
    lua_check<Animation>(lua, 1).running = true;
    return 0;
}

static int stop(lua_State * lua)
{
    Environment(lua).controller()->stopAnimation(lua, lua_check<Animation>(lua, 1));
    return 0;
}

static const luaL_Reg methods[] = {
    { "pause",      pause },
    { "resume",     resume },
    { "stop",       stop },
    { nullptr,      nullptr }
};

static int index(lua_State * lua)
{
    if (lua_handleMethodIndex(lua, 2, methods)) { return 1; }
    return lua_keyError(lua, 2);
}

/****************************************************************************/

int lua_pushNewAnimation(lua_State * lua)
{
    if (!lua_isfunction(lua, 1)) { return luaL_argerror(lua, 1, badTypeErrorMessage); }

    auto * controller = Environment(lua).controller();
    if (!controller) { return luaL_error(lua, noEffectTokenErrorMessage); }

    auto * thread = controller->createAnimation(lua);

    lua_insert(lua, 1);
    int nargs = lua_gettop(lua) - 2;
    lua_xmove(lua, thread, nargs + 1);

    controller->runAnimation(lua_to<Animation>(lua, 1), thread, nargs);
    return 1;
}

/****************************************************************************/

const char * metatable<Animation>::name = "LAnimation";
const struct luaL_Reg metatable<Animation>::methods[] = {
    { "__index",        index },
    { nullptr,          nullptr}
};

} } // namespace keyleds::lua
