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
#include <cassert>
#include <memory>
#include <fstream>
#include <vector>
#include "keyledsd/effect/PluginHelper.h"
#include "plugins/lua/LuaEffect.h"
#include "plugins/lua/State.h"

using namespace keyleds::plugin::lua;


class LuaPlugin final : public keyleds::effect::interface::Plugin
{
    using EffectService = keyleds::effect::interface::EffectService;

    struct StateInfo {
        std::string                             serial;
        std::unique_ptr<State>                  state;
        std::vector<std::unique_ptr<LuaEffect>> effects;
    };
    using state_list = std::vector<StateInfo>;

public:
    LuaPlugin(const char *) {}

    ~LuaPlugin() {}

    auto stateInfoFor(const std::string & serial)
    {
        auto it = std::find_if(
            m_states.begin(), m_states.end(),
            [&serial](const auto & info) { return info.serial == serial; }
        );
        if (it != m_states.end()) { return it; }

        m_states.emplace_back(StateInfo{serial, std::make_unique<State>(), {}});
        return m_states.end() - 1;
    }

    keyleds::effect::interface::Effect *
    createEffect(const std::string & name, EffectService & service) override
    {
        auto source = service.getFile("effects/" + name + ".lua");
        if (source.empty()) { return nullptr; }

        auto info = stateInfoFor(service.deviceSerial());

        std::unique_ptr<LuaEffect> effect;
        try {
            effect = LuaEffect::create(name, service, source, info->state->createContainer());
        } catch (...) {
            assert(false);      // TODO more sensible handling?
        }

        service.getFile({});    // let the service clear file data

        if (!effect) {
            if (info->effects.empty()) { m_states.erase(info); }
            return nullptr;
        }

        keyleds::effect::interface::Effect * ptr = effect.get();
        info->effects.push_back(std::move(effect));
        return ptr;
    }

    void destroyEffect(keyleds::effect::interface::Effect * ptr, EffectService & service) override
    {
        auto info = stateInfoFor(service.deviceSerial());

        auto it = std::find_if(info->effects.begin(), info->effects.end(),
                               [ptr](const auto & item) { return item.get() == ptr; });
        assert(it != info->effects.end());

        std::iter_swap(it, info->effects.end() - 1);
        info->effects.pop_back();

        if (info->effects.empty()) {
            std::iter_swap(info, m_states.end() - 1);
            m_states.pop_back();
        }
    }


private:
    state_list  m_states;
};


KEYLEDSD_EXPORT_PLUGIN("lua", LuaPlugin);
