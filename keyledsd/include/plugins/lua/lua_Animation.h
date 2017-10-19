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
#ifndef KEYLEDS_PLUGINS_LUA_LUA_ANIMATION_H_761E3512
#define KEYLEDS_PLUGINS_LUA_LUA_ANIMATION_H_761E3512

#include "plugins/lua/types.h"

namespace keyleds { namespace lua {

/****************************************************************************/

struct Animation
{
    int         id;
    bool        running;
    unsigned    sleepTime;
};

template <> struct metatable<Animation>
    { static const char * name; static const struct luaL_reg methods[]; struct weak_table : std::false_type{}; };

int lua_pushNewAnimation(lua_State * lua);

/****************************************************************************/

} } // namespace keyleds::lua

#endif
