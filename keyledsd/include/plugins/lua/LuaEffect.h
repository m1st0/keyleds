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
#ifndef KEYLEDS_PLUGINS_LUA_LUAEFFECT_H_F038C73D
#define KEYLEDS_PLUGINS_LUA_LUAEFFECT_H_F038C73D

#include <memory>
#include "keyledsd/effect/PluginHelper.h"
#include "plugins/lua/State.h"

namespace keyleds { namespace plugin { namespace lua {

/****************************************************************************/

class LuaEffect : public ::plugin::Effect
{
    using Container = State::Container;
public:
                    LuaEffect(std::string name, EffectService &, Container);
                    LuaEffect(const LuaEffect &) = delete;
                    ~LuaEffect();

    static std::unique_ptr<LuaEffect> create(const std::string & name, EffectService &,
                                             const std::string & code, Container);

    void            render(unsigned long ms, RenderTarget & target) override;
    void            handleContextChange(const string_map &) override;
    void            handleGenericEvent(const string_map &) override;
    void            handleKeyEvent(const KeyDatabase::Key &, bool) override;
private:
    static void     setupContainer(Container &, EffectService &);
    bool            pushHook(lua_State *, const char *);
    static bool     handleError(lua_State *, EffectService &, int code);
private:
    std::string     m_name;         ///< Name of the effect, from config file
    EffectService & m_service;      ///< For communicating with keyleds
    Container       m_container;    ///< Lua container this effect's scripts runs in
    bool            m_enabled;      ///< Should render/event handlers be run?
};

/****************************************************************************/

} } } // namespace keyleds::plugin::lua

#endif
