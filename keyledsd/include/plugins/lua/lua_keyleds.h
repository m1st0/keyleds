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
#ifndef KEYLEDS_PLUGINS_LUA_TYPES_H_3EAF7EA0
#define KEYLEDS_PLUGINS_LUA_TYPES_H_3EAF7EA0

#include "lua.hpp"
#include "plugins/lua/types.h"
#include "keyledsd/device/KeyDatabase.h"

namespace keyleds { namespace device { class RenderTarget; } }

namespace keyleds { namespace plugin { namespace lua {

/****************************************************************************/

template <> struct metatable<const device::KeyDatabase *>
    { static const char * name; static const struct luaL_reg methods[]; };
template <> struct metatable<const device::KeyDatabase::KeyGroup *>
    { static const char * name; static const struct luaL_reg methods[]; };
template <> struct metatable<const device::KeyDatabase::Key *>
    { static const char * name; static const struct luaL_reg methods[]; };
template <> struct metatable<device::RenderTarget *>
    { static const char * name; static const struct luaL_reg methods[]; };

extern const void * const serviceToken; ///< lightuserdata indexing EffectService in globals

int open_keyleds(lua_State *);

/****************************************************************************/

} } } // namespace keyleds::plugin::lua

#endif