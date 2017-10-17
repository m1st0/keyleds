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
#include "plugins/lua/LuaEffect.h"

#include <lua.hpp>
#include <cassert>
#include <cstring>
#include <sstream>
#include "plugins/lua/lua_keyleds.h"
#include "plugins/lua/types.h"

using keyleds::plugin::lua::LuaEffect;

/****************************************************************************/

#ifndef NDEBUG
#define SAVE_TOP(lua)   int saved_top_ = lua_gettop(lua)
#define CHECK_TOP(lua, depth)   assert(lua_gettop(lua) == saved_top_ + depth)
#else
#define SAVE_TOP(lua)
#define CHECK_TOP(lua, depth)
#endif

/****************************************************************************/
// Constants defining LUA environment

// LUA libraries to load
static constexpr std::array<lua_CFunction, 5> loadModules = {{
    luaopen_base, luaopen_math, luaopen_string, luaopen_table,
    keyleds::plugin::lua::open_keyleds
}};
static_assert(loadModules.back() == keyleds::plugin::lua::open_keyleds,
              "unexpected last element, is size correct?");

// Symbols not in this list get removed once libraries are loaded
static constexpr std::array<const char *, 26> globalWhitelist = {{
    // Libraries
    "coroutine", "keyleds", "math", "string",
    "table",
    // Functions
    "assert", "error", "getmetatable", "ipairs",
    "next", "pairs", "pcall", "print",
    "rawequal", "rawget", "rawset", "select",
    "setmetatable", "tocolor", "tonumber", "tostring",
    "type", "unpack", "xpcall",
    // Values
    "_G", "_VERSION"
}};

/****************************************************************************/
// Helper functions

// Ensure unique_ptr works on lua_State
namespace std {
    void default_delete<lua_State>::operator()(lua_State *p) const { lua_close(p); }
}

/// Convert a lua panic into abort - gives better messages than letting lua exit().
static int luaPanicHandler(lua_State *) { abort(); }

/// Builds the error message for script errors
static int errorHandler(lua_State * lua)
{
    std::ostringstream buffer;
    lua_Debug   data;

    buffer <<"Error: " <<lua_tostring(lua, -1);
    for (int level = 0; lua_getstack(lua, level, &data) != 0; ++level)
    {
        lua_getinfo(lua, "Snl", &data);
        buffer <<"\n    ";
        if (std::strcmp(data.what, "Lua") == 0) {
            if (data.name) {
                buffer <<"In " <<data.namewhat <<" '" <<data.name <<"'";
            } else {
                buffer <<"In anonymous " <<data.namewhat;
            }
        } else if (std::strcmp(data.what, "C") == 0) {
            buffer <<"In call";
            if (data.name) { buffer <<" '" <<data.name <<"'"; }
        } else if (std::strcmp(data.what, "main") == 0) {
            buffer <<"In global scope";
        } else if (std::strcmp(data.what, "tail") == 0) {
            buffer <<"In <tail call>";
        }
        if (data.currentline >= 0) {
            buffer <<", line " <<data.currentline;
        }
    }
    std::string result = buffer.str();
    lua_pushlstring(lua, result.c_str(), result.size());
    return 1;
}

/****************************************************************************/
// Lifecycle management

LuaEffect::LuaEffect(std::string name, EffectService & service, state_ptr state)
 : m_name(std::move(name)),
   m_service(service),
   m_state(std::move(state)),
   m_enabled(true)
{}

LuaEffect::~LuaEffect() {}

std::unique_ptr<LuaEffect> LuaEffect::create(const std::string & name, EffectService & service,
                                             const std::string & code)
{
    // Create a LUA state
    state_ptr state(luaL_newstate());
    auto * lua = state.get();
    lua_atpanic(lua, luaPanicHandler);

    SAVE_TOP(lua);

    // Load script
    if (luaL_loadbuffer(lua, code.data(), code.size(), name.c_str()) != 0) {
        service.log(2, lua_tostring(lua, -1));
        lua_pop(lua, 1);                    // pop error message
        CHECK_TOP(lua, 0);
        return nullptr;
    }                                       // ^push (script)

    setupState(lua, service);

    // Run script to let it build its environment
    lua_pushcfunction(lua, errorHandler);   // push (errhandler)
    lua_insert(lua, -2);                    // swap (script, errhandler) => (errhandler, script)
    bool ok = handleError(lua, service, lua_pcall(lua, 0, 0, -2)); // pop (errhandler, script)

    CHECK_TOP(lua, 0);
    return ok ? std::make_unique<LuaEffect>(name, service, std::move(state)) : nullptr;
}

void LuaEffect::setupState(lua_State * lua, EffectService & service)
{
    SAVE_TOP(lua);

    // Load libraries in default environment
    for (const auto & module : loadModules) {
        lua_pushcfunction(lua, module);
        lua_call(lua, 0, 0);
    }

    // Remove global symbols not in whitelist
    lua_pushnil(lua);
    while (lua_next(lua, LUA_GLOBALSINDEX) != 0) {
        lua_pop(lua, 1);
        if (lua_isstring(lua, -1)) {
            const char * key = lua_tostring(lua, -1);
            auto it = std::find_if(globalWhitelist.begin(), globalWhitelist.end(),
                                   [key](const auto * item) { return std::strcmp(key, item) == 0; });
            if (it == globalWhitelist.end()) {
                lua_pushnil(lua);
                lua_setglobal(lua, key);
            }
        }
    }

    // Add debug module if configuration requests it
    if (service.getConfig("debug") == "yes") {
        lua_pushcfunction(lua, luaopen_debug);
        lua_call(lua, 0, 0);
    }

    // Save service pointer
    lua_pushlightuserdata(lua, const_cast<void *>(serviceToken));
    lua_pushlightuserdata(lua, &service);
    lua_rawset(lua, LUA_GLOBALSINDEX);

    // Set keyleds members
    lua_getglobal(lua, "keyleds");
    {
        lua_pushlstring(lua, service.deviceName().data(), service.deviceName().size());
        lua_setfield(lua, -2, "deviceName");
        lua_pushlstring(lua, service.deviceModel().data(), service.deviceModel().size());
        lua_setfield(lua, -2, "deviceModel");
        lua_pushlstring(lua, service.deviceSerial().data(), service.deviceSerial().size());
        lua_setfield(lua, -2, "deviceSerial");

        auto & config = service.configuration();
        lua_createtable(lua, 0, config.size());
        for (const auto & item : config) {
            lua_pushlstring(lua, item.first.data(), item.first.size());
            lua_pushlstring(lua, item.second.data(), item.second.size());
            lua_rawset(lua, -3);
        }
        lua_setfield(lua, -2, "config");

        auto & groups = service.keyGroups();
        lua_createtable(lua, 0, groups.size());
        for (const auto & group : groups) {
            lua_pushlstring(lua, group.name().data(), group.name().size());
            lua_push(lua, &group);
            lua_rawset(lua, -3);
        }
        lua_setfield(lua, -2, "groups");

        lua_push(lua, &service.keyDB());
        lua_setfield(lua, -2, "db");
    }
    lua_pop(lua, 1);        // pop(keyleds)

    CHECK_TOP(lua, 0);
}

/****************************************************************************/
// Hooks

void LuaEffect::render(unsigned long ms, RenderTarget & target)
{
    if (!m_enabled) { return; }
    auto lua = m_state.get();
    SAVE_TOP(lua);
    lua_push(lua, &target);                         // push(rendertarget)

    lua_pushcfunction(lua, errorHandler);           // push(errhandler)
    if (pushHook(lua, "render")) {                  // push(render)
        lua_pushinteger(lua, ms);                   // push(arg1)
        lua_pushvalue(lua, -4);                     // push(arg2)
        if (!handleError(lua, m_service,
                         lua_pcall(lua, 2, 0, -4))) {// pop(errhandler, render, arg1, arg2)
            m_enabled = false;
        }
    } else {
        lua_pop(lua, 1);                            // pop(errhandler)
    }

    lua_to<RenderTarget *>(lua, -1) = nullptr;      // mark target as gone
    lua_pop(lua, 1);
    CHECK_TOP(lua, 0);
}

void LuaEffect::handleContextChange(const string_map & data)
{
    if (!m_enabled) { return; }
    auto lua = m_state.get();
    SAVE_TOP(lua);
    lua_pushcfunction(lua, errorHandler);           // push(errhandler)
    if (pushHook(lua, "onContextChange")) {         // push(hook)
        lua_createtable(lua, 0, data.size());       // push table
        for (const auto & item : data) {
            lua_pushlstring(lua, item.first.c_str(), item.first.size());
            lua_pushlstring(lua, item.second.c_str(), item.second.size());
            lua_rawset(lua, -3);
        }
        if (!handleError(lua, m_service, lua_pcall(lua, 1, 0, -3))) { // pop(errhandler, hook, table)
            m_enabled = false;
        }
    } else {
        lua_pop(lua, 1);                            // pop(errhandler)
    }
    CHECK_TOP(lua, 0);
}

void LuaEffect::handleGenericEvent(const string_map & data)
{
    if (!m_enabled) { return; }
    auto lua = m_state.get();
    SAVE_TOP(lua);
    lua_pushcfunction(lua, errorHandler);           // push(errhandler)
    if (pushHook(lua, "onGenericEvent")) {          // push(hook)
        lua_createtable(lua, 0, data.size());       // push table
        for (const auto & item : data) {
            lua_pushlstring(lua, item.first.c_str(), item.first.size());
            lua_pushlstring(lua, item.second.c_str(), item.second.size());
            lua_rawset(lua, -3);
        }
        if (!handleError(lua, m_service, lua_pcall(lua, 1, 0, -3))) { // pop(errhandler, hook, table)
            m_enabled = false;
        }
    } else {
        lua_pop(lua, 1);                            // pop(errhandler)
    }
    CHECK_TOP(lua, 0);
}

void LuaEffect::handleKeyEvent(const KeyDatabase::Key & key, bool press)
{
    if (!m_enabled) { return; }
    auto lua = m_state.get();
    SAVE_TOP(lua);
    lua_pushcfunction(lua, errorHandler);           // push(errhandler)
    if (pushHook(lua, "onKeyEvent")) {              // push(hook)
        lua_push(lua, &key);                        // push(arg1)
        lua_pushboolean(lua, press);                // push(arg2)
        if (!handleError(lua, m_service,
                         lua_pcall(lua, 2, 0, -4))) {// pop(errhandler, hook, arg1, arg2)
            m_enabled = false;
        }
    } else {
        lua_pop(lua, 1);                            // pop(errhandler)
    }
    CHECK_TOP(lua, 0);
}

/****************************************************************************/
// Static Helper methods

bool LuaEffect::pushHook(lua_State * lua, const char * name)
{
    SAVE_TOP(lua);
    lua_getglobal(lua, name);               // push(hook)
    if (!lua_isfunction(lua, -1)) {
        lua_pop(lua, 1);                    // pop(hook)
        CHECK_TOP(lua, 0);
        return false;
    }
    CHECK_TOP(lua, +1);
    return true;
}

bool LuaEffect::handleError(lua_State * lua, EffectService & service, int code)
{
    bool ok = false;
    switch(code)
    {
    case 0:
        ok = true;
        break;  // no error
    case LUA_ERRRUN:
        service.log(1, lua_tostring(lua, -1));
        lua_pop(lua, 1);    // pop error message
        break;
    case LUA_ERRMEM:
        service.log(1, "out of memory");
        break;
    case LUA_ERRERR:
    default:
        service.log(0, "unexpected error");
    }
    lua_pop(lua, 1);        // pop error handler
    return ok;
}
