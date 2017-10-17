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
#include "plugins/lua/lua_keyleds.h"

#include <lua.hpp>
#include <cstring>
#include <iomanip>
#include <sstream>
#include "keyledsd/device/RenderLoop.h"
#include "keyledsd/effect/interfaces.h"

using keyleds::device::KeyDatabase;
using keyleds::device::RenderTarget;
using keyleds::effect::interface::EffectService;
using keyleds::RGBAColor;

namespace keyleds { namespace plugin { namespace lua {

/****************************************************************************/

static const char badKeyErrorMessage[] = "bad key '%s'";
static const char badTypeErrorMessage[] = "bad type";
static const char badIndexErrorMessage[] = "index out of bounds '%d'";
static const char noLongerExistsErrorMessage[] = "object no longer exists";
static const char noServiceTokenErrorMessage[] = "no service token in environment";

const void * const serviceToken = &serviceToken;

static EffectService * getService(lua_State * lua)
{
    lua_pushlightuserdata(lua, const_cast<void *>(serviceToken));   // push(token)
    lua_rawget(lua, LUA_GLOBALSINDEX);                              // pop(token) push(value)
    auto * service = static_cast<EffectService *>(const_cast<void *>(lua_topointer(lua, -1)));
    lua_pop(lua, 1);
    return service;
}

/****************************************************************************/
// RGBAColor
//  - underlying type is a regular table with 4 array values
//  - we map color names to the matching array entries

static const char * rgbaColorMetatableName = "LRGBAColor";

class LuaRGBAColor final
{
    static constexpr std::array<const char *, 4> keys = {{
        "red", "green", "blue", "alpha"
    }};

    static int indexForKey(lua_State * lua, const char * key)
    {
        for (unsigned idx = 0; idx < keys.size(); ++idx) {
            if (std::strcmp(keys[idx], key) == 0) {
                return idx + 1;
            }
        }
        return luaL_error(lua, badKeyErrorMessage, key);
    }

public:
    static void push(lua_State * lua, std::array<lua_Number, 4> values)
    {
        lua_createtable(lua, 4, 0);
        luaL_getmetatable(lua, rgbaColorMetatableName);
        lua_setmetatable(lua, -2);
        for (int i = 0; i < 4; ++i) {
            lua_pushnumber(lua, values[i]);
            lua_rawseti(lua, -2, i + 1);
        }
    }

    static int equal(lua_State * lua)
    {
        for (int i = 1; i <= 4; ++i) {
            lua_rawgeti(lua, 1, i);
            lua_rawgeti(lua, 2, i);
            if (lua_tonumber(lua, -2) != lua_tonumber(lua, -1)) {
                lua_pushboolean(lua, false);
                return 1;
            }
            lua_pop(lua, 2);
        }
        lua_pushboolean(lua, true);
        return 1;
    }

    static int index(lua_State * lua)
    {
        lua_rawgeti(lua, 1, indexForKey(lua, luaL_checkstring(lua, 2)));
        return 1;
    }

    static int newIndex(lua_State * lua)
    {
        lua_rawseti(lua, 1, indexForKey(lua, luaL_checkstring(lua, 2)));
        return 1;
    }

    static int toString(lua_State * lua)
    {
        std::ostringstream buffer;
        buffer <<std::fixed <<std::setprecision(3);
        buffer <<"color(";
        lua_rawgeti(lua, 1, 1);
        buffer <<lua_tonumber(lua, -1) <<", ";
        lua_rawgeti(lua, 1, 2);
        buffer <<lua_tonumber(lua, -1) <<", ";
        lua_rawgeti(lua, 1, 3);
        buffer <<lua_tonumber(lua, -1) <<", ";
        lua_rawgeti(lua, 1, 4);
        buffer <<lua_tonumber(lua, -1) <<")";
        lua_pushstring(lua, buffer.str().c_str());
        return 1;
    }
};

constexpr std::array<const char *, 4> LuaRGBAColor::keys;
static const struct luaL_Reg rgbaColorMetatableMethods[] = {
    { "__eq",       LuaRGBAColor::equal },
    { "__index",    LuaRGBAColor::index },
    { "__newindex", LuaRGBAColor::newIndex },
    { "__tostring", LuaRGBAColor::toString },
    { nullptr,      nullptr}
};

/****************************************************************************/
// KeyDatabase

/** class KeyDatabase
 *      string  name
 *      Key operator[](int)
 *      Key findKeyCode(int)
 *      Key findName(string)
 */

class LuaKeyDatabase final
{
    static const std::array<struct luaL_Reg, 3> methods;

    static int findKeyCode(lua_State * lua)
    {
        const auto * db = lua_check<const KeyDatabase *>(lua, 1);

        auto it = db->findKeyCode(luaL_checkinteger(lua, 2));
        if (it != db->end()) {
            lua_push(lua, &*it);
        } else {
            lua_pushnil(lua);
        }
        return 1;
    }

    static int findName(lua_State * lua)
    {
        const auto * db = lua_check<const KeyDatabase *>(lua, 1);

        auto it = db->findName(luaL_checkstring(lua, 2));
        if (it != db->end()) {
            lua_push(lua, &*it);
        } else {
            lua_pushnil(lua);
        }
        return 1;
    }

public:
    static int index(lua_State * lua)
    {
        const auto * db = lua_to<const KeyDatabase *>(lua, 1);

        auto idx = lua_tointeger(lua, 2);
        if (idx != 0) {
            if (static_cast<size_t>(std::abs(idx)) > db->size()) {
                return luaL_error(lua, badIndexErrorMessage, idx);
            }
            idx = idx > 0 ? idx - 1 : db->size() + idx;
            lua_push(lua, &(*db)[idx]);
        } else {
            const char * field = lua_tostring(lua, 2);
            if (!field) { luaL_argerror(lua, 2, badTypeErrorMessage); }

            auto it = std::find_if(
                methods.begin(), methods.end(),
                [field](const auto & method) { return std::strcmp(method.name, field) == 0; }
            );
            if (it == methods.end()) {
                return luaL_error(lua, badKeyErrorMessage, field);
            }
            lua_pushcfunction(lua, it->func);
        }
        return 1;
    }

    static int len(lua_State * lua)
    {
        const auto * db = lua_to<const KeyDatabase *>(lua, 1);
        lua_pushinteger(lua, db->size());
        return 1;
    }
};

const std::array<struct luaL_Reg, 3> LuaKeyDatabase::methods = {{
    { "findKeyCode",    LuaKeyDatabase::findKeyCode },
    { "findName",       LuaKeyDatabase::findName },
}};

const char * metatable<const device::KeyDatabase *>::name = "LKeyDatabase";
const struct luaL_Reg metatable<const device::KeyDatabase *>::methods[] = {
    { "__index",        LuaKeyDatabase::index },
    { "__len",          LuaKeyDatabase::len },
    { nullptr,          nullptr}
};

/****************************************************************************/
// KeyDatabase::KeyGroup

/** class KeyGroup
 *      Key operator[](int)
 */

class LuaKeyGroup final
{
public:
    static int index(lua_State * lua)
    {
        const auto * group = lua_to<const KeyDatabase::KeyGroup *>(lua, 1);

        auto idx = luaL_checkinteger(lua, 2);
        if (static_cast<size_t>(std::abs(idx)) > group->size()) {
            return luaL_error(lua, badIndexErrorMessage, idx);
        }
        idx = idx > 0 ? idx - 1 : group->size() + idx;
        lua_push(lua, &(*group)[idx]);
        return 1;
    }

    static int len(lua_State * lua)
    {
        const auto * group = lua_to<const KeyDatabase::KeyGroup *>(lua, 1);
        lua_pushinteger(lua, group->size());
        return 1;
    }

    static int toString(lua_State * lua)
    {
        const auto * keyGroup = lua_to<const KeyDatabase::KeyGroup *>(lua, 1);

        bool isFirst = true;
        std::ostringstream buffer;
        buffer <<"[";
        for (const auto & key : *keyGroup) {
            if (isFirst) { isFirst = false; }
                    else { buffer <<", "; }
            buffer <<key.name;
        }
        buffer <<"]";
        lua_pushstring(lua, buffer.str().c_str());
        return 1;
    }
};

const char * metatable<const KeyDatabase::KeyGroup *>::name = "LKeyGroup";
const struct luaL_Reg metatable<const KeyDatabase::KeyGroup *>::methods[] = {
    { "__index",    LuaKeyGroup::index},
    { "__len",      LuaKeyGroup::len},
    { "__tostring", LuaKeyGroup::toString },
    { nullptr,      nullptr}
};

/****************************************************************************/
// KeyDatabase::Key

/** class Key
 *      number  index
 *      number  keyCode
 *      string  name
 *      number  x0, y0, x1, y1
 */

class LuaKey final
{
public:
    static int index(lua_State * lua)
    {
        const auto * key = lua_to<const KeyDatabase::Key *>(lua, 1);
        const char * field = luaL_checkstring(lua, 2);

        if (std::strcmp(field, "index") == 0) {
            lua_pushnumber(lua, key->index);
        } else if (std::strcmp(field, "keyCode") == 0 ) {
            lua_pushnumber(lua, key->keyCode);
        } else if (std::strcmp(field, "name") == 0) {
            lua_pushlstring(lua, key->name.c_str(), key->name.size());
        } else if (std::strcmp(field, "x0") == 0) {
            lua_pushnumber(lua, key->position.x0);
        } else if (std::strcmp(field, "y0") == 0) {
            lua_pushnumber(lua, key->position.y0);
        } else if (std::strcmp(field, "x1") == 0) {
            lua_pushnumber(lua, key->position.x1);
        } else if (std::strcmp(field, "y1") == 0) {
            lua_pushnumber(lua, key->position.y1);
        } else {
            return luaL_error(lua, badKeyErrorMessage, field);
        }
        return 1;
    }

    static int toString(lua_State * lua)
    {
        const auto * key = lua_to<const KeyDatabase::Key *>(lua, 1);
        lua_pushfstring(lua, "Key(%d, %d, %s)", key->index, key->keyCode, key->name.c_str());
        return 1;
    }
};

const char * metatable<const KeyDatabase::Key *>::name = "LKey";
const struct luaL_Reg metatable<const KeyDatabase::Key *>::methods[] = {
    { "__index",    LuaKey::index},
    { "__tostring", LuaKey::toString },
    { nullptr,      nullptr}
};

/****************************************************************************/
// RenderTarget

/** class RenderTarget
 *      void blend(const RenderTarget)
 *      color operator[](int)
 */

class LuaRenderTarget final
{
    static const std::array<struct luaL_Reg, 1> methods;

    static int toTargetIndex(lua_State * lua, int idx) // 0-based
    {
        if (lua_is<const KeyDatabase::Key *>(lua, idx)) {
            return lua_to<const KeyDatabase::Key *>(lua, idx)->index;
        }
        if (lua_isnumber(lua, idx)) {
            return lua_tointeger(lua, idx) - 1;
        }
        if (lua_isstring(lua, idx)) {
            size_t size;
            const char * keyName = lua_tolstring(lua, idx, &size);
            auto * service = getService(lua);
            if (!service) { return luaL_error(lua, noServiceTokenErrorMessage); }

            auto it = service->keyDB().findName(std::string(keyName, size));
            if (it != service->keyDB().end()) {
                return it->index;
            }
            return -1;
        }
        return luaL_argerror(lua, idx, badTypeErrorMessage);
    }

    static int blend(lua_State * lua)
    {
        using keyleds::device::blend;

        auto * to = lua_check<RenderTarget *>(lua, 1);
        if (!to) { return luaL_argerror(lua, 1, noLongerExistsErrorMessage); }
        auto * from = lua_check<RenderTarget *>(lua, 2);
        if (!from) { return luaL_argerror(lua, 2, noLongerExistsErrorMessage); }

        blend(*to, *from);
        return 0;
    }

public:
    static int create(lua_State * lua)
    {
        auto * service = getService(lua);
        if (!service) { return luaL_error(lua, noServiceTokenErrorMessage); }

        auto * target = service->createRenderTarget();
        for (auto & entry : *target) { entry = RGBAColor(0, 0, 0, 0); }

        lua_push(lua, target);

        // Attach service reference to render target
        lua_createtable(lua, 0, 1);
        lua_pushlightuserdata(lua, const_cast<void *>(serviceToken));
        lua_pushlightuserdata(lua, service);
        lua_rawset(lua, -3);
        lua_setfenv(lua, -2);
        return 1;
    }

    static int destroy(lua_State * lua)
    {
        auto * target = lua_to<RenderTarget *>(lua, 1);
        if (!target) { return 0; }                  // object marked as gone already

        lua_getfenv(lua, 1);
        if (!lua_istable(lua, 2)) { return 0; }     // no fenv => not owner

        // Extract service pointer from fenv - we do not run within thread context
        lua_pushlightuserdata(lua, const_cast<void *>(serviceToken));
        lua_rawget(lua, 2);
        auto * service = static_cast<EffectService *>(const_cast<void *>(lua_topointer(lua, 3)));
        if (!service) { return 0; }

        service->destroyRenderTarget(target);

        lua_to<RenderTarget *>(lua, 1) = nullptr;   // mark object as gone
        return 0;
    }

    static int index(lua_State * lua)
    {
        auto * target = lua_to<RenderTarget *>(lua, 1);
        if (!target) { return luaL_error(lua, noLongerExistsErrorMessage); }

        // Handle method retrieval
        if (lua_isstring(lua, 2)) {
            const char * field = lua_tostring(lua, 2);
            auto it = std::find_if(
                methods.begin(), methods.end(),
                [field](const auto & method) { return std::strcmp(method.name, field) == 0; }
            );
            if (it != methods.end()) {
                lua_pushcfunction(lua, it->func);
                return 1;
            }
        }

        // Handle table-like access
        int index = toTargetIndex(lua, 2);
        if (index < 0 || unsigned(index) >= target->size()) {
            lua_pushnil(lua);
            return 1;
        }

        // Extract color data
        auto & color = (*target)[index];

        LuaRGBAColor::push(lua, {{
            lua_Number(color.red) / 255.0,
            lua_Number(color.green) / 255.0,
            lua_Number(color.blue) / 255.0,
            lua_Number(color.alpha) / 255.0
        }});
        return 1;
    }

    static int len(lua_State * lua)
    {
        auto * target = lua_to<RenderTarget *>(lua, 1);
        if (!target) { return luaL_error(lua, noLongerExistsErrorMessage); }
        lua_pushinteger(lua, target->size());
        return 1;
    }

    static int newIndex(lua_State * lua)
    {
        auto * target = lua_to<RenderTarget *>(lua, 1);
        if (!target) { return luaL_error(lua, noLongerExistsErrorMessage); }

        int index = toTargetIndex(lua, 2);
        if (index < 0 || unsigned(index) >= target->size()) {
            return 0;   /// invalid indices and key names are silently ignored, to allow
                        /// generic code that works on different keyboards
        }

        if (!isType(lua, 3, rgbaColorMetatableName)) {
            return luaL_argerror(lua, 3, badTypeErrorMessage);
        }
        lua_rawgeti(lua, 3, 1);
        lua_rawgeti(lua, 3, 2);
        lua_rawgeti(lua, 3, 3);
        lua_rawgeti(lua, 3, 4);

        if (!lua_isnumber(lua, -4) || !lua_isnumber(lua, -3) ||
            !lua_isnumber(lua, -2) || !lua_isnumber(lua, -1)) {
            return luaL_argerror(lua, 3, badTypeErrorMessage);
        }

        (*target)[index] = RGBAColor(
            std::min(255, int(256.0 * lua_tonumber(lua, -4))),
            std::min(255, int(256.0 * lua_tonumber(lua, -3))),
            std::min(255, int(256.0 * lua_tonumber(lua, -2))),
            std::min(255, int(256.0 * lua_tonumber(lua, -1)))
        );
        return 0;
    }
};

const std::array<struct luaL_Reg, 1> LuaRenderTarget::methods = {{
    { "blend",      LuaRenderTarget::blend }
}};

const char * metatable<RenderTarget *>::name = "LRenderTarget";
const struct luaL_Reg metatable<RenderTarget *>::methods[] = {
    { "__gc",       LuaRenderTarget::destroy },
    { "__index",    LuaRenderTarget::index },
    { "__len",      LuaRenderTarget::len },
    { "__newindex", LuaRenderTarget::newIndex },
    { nullptr,      nullptr}
};

/****************************************************************************/

const struct luaL_Reg keyledsLibrary[] = {
    { "newRenderTarget",    LuaRenderTarget::create },
    { nullptr,              nullptr}
};

/****************************************************************************/
// Global scope

static int luaPrint(lua_State * lua)        // (...) => ()
{
    int nargs = lua_gettop(lua);
    std::ostringstream buffer;

    for (int idx = 1; idx <= nargs; ++idx) {
        lua_getglobal(lua, "tostring");
        lua_pushvalue(lua, idx);
        lua_pcall(lua, 1, 1, 0);
        buffer <<lua_tostring(lua, -1);
        lua_pop(lua, 1);
    }

    auto * service = getService(lua);
    if (!service) { return luaL_error(lua, noServiceTokenErrorMessage); }

    service->log(4, buffer.str().c_str());
    return 0;
}

static int luaToColor(lua_State * lua)      // (any) => (table)
{
    int nargs = lua_gettop(lua);
    if (nargs == 1) {
        // We are called as a conversion function
        if (lua_isstring(lua, 1)) {
            // On a string, parse it
            auto * service = getService(lua);
            if (!service) { return luaL_error(lua, noServiceTokenErrorMessage); }

            size_t size;
            const char * string = lua_tolstring(lua, 1, &size);
            RGBAColor color;
            if (service->parseColor(std::string(string, size), &color)) {
                LuaRGBAColor::push(lua, {{
                    lua_Number(color.red) / 255.0,
                    lua_Number(color.green) / 255.0,
                    lua_Number(color.blue) / 255.0,
                    lua_Number(color.alpha) / 255.0
                }});
                return 1;
            }
        }
    } else if (3 <= nargs && nargs <= 4) {
        if (nargs == 3) { lua_pushnumber(lua, 1.0); }
        if (lua_isnumber(lua, 1) && lua_isnumber(lua, 2) &&
            lua_isnumber(lua, 3) && lua_isnumber(lua, 4)) {
            lua_createtable(lua, 4, 0);
            luaL_getmetatable(lua, rgbaColorMetatableName);
            lua_setmetatable(lua, -2);
            lua_insert(lua, 1);
            lua_rawseti(lua, 1, 4);
            lua_rawseti(lua, 1, 3);
            lua_rawseti(lua, 1, 2);
            lua_rawseti(lua, 1, 1);
            return 1;
        }
    }
    lua_pushnil(lua);
    return 1;
}

static const struct luaL_reg keyledsGlobals[] = {
    { "print",      luaPrint    },
    { "tocolor",    luaToColor  },
    { nullptr, nullptr }
};

/****************************************************************************/

int open_keyleds(lua_State * lua)
{
    // Register types
    registerType<const KeyDatabase *>(lua);
    registerType<const KeyDatabase::KeyGroup *>(lua);
    registerType<const KeyDatabase::Key *>(lua);
    registerType<RenderTarget *>(lua);
    registerType(lua, rgbaColorMetatableName, rgbaColorMetatableMethods, false);

    // Register library itself
    luaL_register(lua, "keyleds", keyledsLibrary);
    lua_pop(lua, 1);

    // Register globals
    lua_pushvalue(lua, LUA_GLOBALSINDEX);
    luaL_register(lua, nullptr, keyledsGlobals);
    lua_pop(lua, 1);

    return 0;
}

/****************************************************************************/

} } } // namespace keyleds::plugin::lua
