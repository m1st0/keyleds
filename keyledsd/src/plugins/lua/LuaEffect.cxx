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
#include "plugins/lua/lua_Animation.h"
#include "plugins/lua/lua_Key.h"
#include "plugins/lua/lua_KeyDatabase.h"
#include "plugins/lua/lua_KeyGroup.h"
#include "plugins/lua/lua_RenderTarget.h"
#include "plugins/lua/lua_common.h"     //FIXME should not be needed
#include "plugins/lua/lua_keyleds.h"
#include "plugins/lua/types.h"

using keyleds::plugin::lua::LuaEffect;
using namespace keyleds::lua;

/****************************************************************************/
// Constants defining LUA environment

// LUA libraries to load
static constexpr std::array<lua_CFunction, 4> loadModules = {{
    luaopen_base, luaopen_math, luaopen_string, luaopen_table,
}};
static_assert(loadModules.back() == luaopen_table,
              "unexpected last element, is size correct?");

// Symbols not in this list get removed once libraries are loaded
static constexpr std::array<const char *, 27> globalWhitelist = {{
    // Libraries
    "coroutine", "keyleds", "math", "string",
    "table",
    // Functions
    "assert", "error", "getmetatable", "ipairs",
    "next", "pairs", "pcall", "print",
    "rawequal", "rawget", "rawset", "select",
    "setmetatable", "tocolor", "tonumber", "tostring",
    "type", "unpack", "wait", "xpcall",
    // Values
    "_G", "_VERSION"
}};

static const void * const animationToken = &animationToken;

/****************************************************************************/
// Helper functions

static int luaPanicHandler(lua_State *);
static int luaErrorHandler(lua_State *);

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

    auto effect = std::make_unique<LuaEffect>(name, service, std::move(state));

    effect->setupState();

    // Run script to let it build its environment
    lua_pushcfunction(lua, luaErrorHandler);// push (errhandler)
    lua_insert(lua, -2);                    // swap (script, errhandler) => (errhandler, script)
    bool ok = handleError(lua, service, lua_pcall(lua, 0, 0, -2)); // pop (errhandler, script)

    CHECK_TOP(lua, 0);
    if (!ok) { return nullptr; }

    // If script defined an init function, call it now
    if (pushHook(lua, "init")) {                    // push(init)
        lua_pushcfunction(lua, luaErrorHandler);    // push(errhandler)
        lua_insert(lua, -2);                        // swap(init, errhandler) => (errhandler, init)
        if (!handleError(lua, service,
                         lua_pcall(lua, 0, 0, -2))) {// pop(errhandler, render)
            CHECK_TOP(lua, 0);
            return nullptr;
        }
    }

    CHECK_TOP(lua, 0);
    return effect;
}

void LuaEffect::setupState()
{
    auto lua = m_state.get();
    SAVE_TOP(lua);

    // Load libraries in default environment
    for (const auto & module : loadModules) {
        lua_pushcfunction(lua, module);
        lua_call(lua, 0, 0);
    }

    // Load keyleds library, passing a pointer to ourselves
    Environment(lua).openKeyleds(this);

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
    if (m_service.getConfig("debug") == "yes") {
        lua_pushcfunction(lua, luaopen_debug);
        lua_call(lua, 0, 0);
    }

    // Insert animation list
    lua_pushlightuserdata(lua, const_cast<void *>(animationToken));
    lua_newtable(lua);
    lua_rawset(lua, LUA_REGISTRYINDEX);

    // Set keyleds members
    lua_getglobal(lua, "keyleds");
    {
        lua_pushlstring(lua, m_service.deviceName().data(), m_service.deviceName().size());
        lua_setfield(lua, -2, "deviceName");
        lua_pushlstring(lua, m_service.deviceModel().data(), m_service.deviceModel().size());
        lua_setfield(lua, -2, "deviceModel");
        lua_pushlstring(lua, m_service.deviceSerial().data(), m_service.deviceSerial().size());
        lua_setfield(lua, -2, "deviceSerial");

        auto & config = m_service.configuration();
        lua_createtable(lua, 0, config.size());
        for (const auto & item : config) {
            lua_pushlstring(lua, item.first.data(), item.first.size());
            lua_pushlstring(lua, item.second.data(), item.second.size());
            lua_rawset(lua, -3);
        }
        lua_setfield(lua, -2, "config");

        auto & groups = m_service.keyGroups();
        lua_createtable(lua, 0, groups.size());
        for (const auto & group : groups) {
            lua_pushlstring(lua, group.name().data(), group.name().size());
            lua_push(lua, &group);
            lua_rawset(lua, -3);
        }
        lua_setfield(lua, -2, "groups");

        lua_push(lua, &m_service.keyDB());
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

    stepAnimations(ms);

    SAVE_TOP(lua);
    lua_push(lua, &target);                         // push(rendertarget)

    lua_pushcfunction(lua, luaErrorHandler);        // push(errhandler)
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
    lua_pushcfunction(lua, luaErrorHandler);        // push(errhandler)
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
    lua_pushcfunction(lua, luaErrorHandler);        // push(errhandler)
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
    lua_pushcfunction(lua, luaErrorHandler);        // push(errhandler)
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
// Lua interface

void LuaEffect::print(const std::string & msg) const
{
    m_service.log(4, msg.c_str());
}

const keyleds::device::KeyDatabase & LuaEffect::keyDB() const
{
    return m_service.keyDB();
}

bool LuaEffect::parseColor(const std::string & str, RGBAColor * color) const
{
    return m_service.parseColor(str, color);
}

keyleds::device::RenderTarget * LuaEffect::createRenderTarget()
{
    return m_service.createRenderTarget();
}

void LuaEffect::destroyRenderTarget(RenderTarget * target)
{
    m_service.destroyRenderTarget(target);
}

/// pushes the animation onto the stack
lua_State * LuaEffect::createAnimation(lua_State * lua)
{
    lua_push(lua, Animation{0, true, 0});           // push(animation)

    lua_createtable(lua, 0, 1);                     // push(fenv)
    auto * thread = lua_newthread(m_state.get());   // push(thread)
    lua_setfield(lua, -2, "thread");                // pop(thread)
    lua_setfenv(lua, -2);                           // pop(fenv)

    lua_pushlightuserdata(lua, const_cast<void *>(animationToken)); // push(token)
    lua_rawget(lua, LUA_REGISTRYINDEX);             // pop(token) push(animlist)
    lua_pushvalue(lua, -2);                         // push(anim)
    auto id = luaL_ref(lua, -2);                    // pop(anim)
    lua_to<Animation>(lua, -2).id = id;
    lua_pop(lua, 1);                                // pop(animlist)

    return thread;
}

void LuaEffect::stopAnimation(lua_State * lua, Animation & animation)
{
    lua_pushlightuserdata(lua, const_cast<void *>(animationToken));
    lua_rawget(lua, LUA_REGISTRYINDEX);

    luaL_unref(lua, -1, animation.id);
    lua_pop(lua, 1);
}

void LuaEffect::stepAnimations(unsigned ms)
{
    auto * lua = m_state.get();
    lua_pushlightuserdata(lua, const_cast<void *>(animationToken));
    lua_rawget(lua, LUA_REGISTRYINDEX);

    size_t size = lua_objlen(lua, -1);
    for (size_t index = 1; index <= size; ++index) {
        lua_rawgeti(lua, -1, index);                    // push(animation)
        if (!lua_is<Animation>(lua, -1)) {
            lua_pop(lua, 1);                            // (pop(animation))
            continue;
        }

        auto & animation = lua_to<Animation>(lua, -1);

        if (animation.running) {
            if (animation.sleepTime <= ms) {
                lua_getfenv(lua, -1);                   // push(fenv)
                lua_getfield(lua, -1, "thread");        // push(thread)

                auto * thread = static_cast<lua_State *>(const_cast<void *>(lua_topointer(lua, -1)));
                runAnimation(animation, thread, 0);

                lua_pop(lua, 2);                        // pop(fenv, thread)
            }
            if (animation.sleepTime > ms) {
                animation.sleepTime -= ms;
            } else {
                animation.sleepTime = 0;
            }
        }
        lua_pop(lua, 1);                                // pop(animation)
    }
    lua_pop(lua, 1);                                    // pop(animlist)
}

void LuaEffect::runAnimation(Animation & animation, lua_State * thread, int nargs)
{
    auto * lua = m_state.get();

    bool terminate = true;
    switch (lua_resume(thread, nargs)) {
        case 0:
            break;
        case LUA_YIELD:
            if (lua_topointer(thread, 1) != const_cast<void *>(waitToken)) {
                luaL_traceback(lua, thread, "invalid yield", 0);
                m_service.log(1, lua_tostring(lua, -1));
                lua_pop(lua, 1);
                break;
            }
            animation.sleepTime += int(1000.0 * lua_tonumber(thread, 2));
            terminate = false;
            break;
        case LUA_ERRRUN:
            luaL_traceback(lua, thread, lua_tostring(thread, -1), 0);
            m_service.log(1, lua_tostring(lua, -1));
            lua_pop(lua, 1);
            break;
        case LUA_ERRMEM:
            m_service.log(1, "out of memory");
            break;
        default:
            m_service.log(1, "unexpected error");
    }
    if (terminate) {
        stopAnimation(m_state.get(), animation);
    }
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
        service.log(0, "unexpected error");
        break;
    }
    lua_pop(lua, 1);        // pop error handler
    return ok;
}

// Ensure unique_ptr works on lua_State
namespace std {
    void default_delete<lua_State>::operator()(lua_State *p) const { lua_close(p); }
}

/****************************************************************************/

/// Convert a lua panic into abort - gives better messages than letting lua exit().
static int luaPanicHandler(lua_State *) { abort(); }

/// Builds the error message for script errors
static int luaErrorHandler(lua_State * lua)
{
    luaL_traceback(lua, lua, lua_tostring(lua, -1), 1);
    return 1;
}
