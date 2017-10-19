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
#ifndef KEYLEDS_PLUGINS_LUA_LUA_KEYLEDS_H_3EAF7EA0
#define KEYLEDS_PLUGINS_LUA_LUA_KEYLEDS_H_3EAF7EA0

#include <string>

struct lua_State;
namespace keyleds { namespace device {
    class KeyDatabase;
    class RenderTarget;
} }
namespace keyleds { struct RGBAColor; }

namespace keyleds { namespace lua {

struct Animation;

/****************************************************************************/

/// Synctactical sugar for manipulating lua_State: Environment(lua).controller()
class Environment final
{
public:
    class Controller
    {
    protected:
        using Animation = keyleds::lua::Animation;
    public:
        virtual void            print(const std::string &) const = 0;
        virtual const device::KeyDatabase & keyDB() const = 0;
        virtual bool            parseColor(const std::string &, RGBAColor *) const = 0;

        virtual device::RenderTarget *  createRenderTarget() = 0;
        virtual void            destroyRenderTarget(device::RenderTarget *) = 0;

        virtual lua_State *     createAnimation(lua_State * lua) = 0;
        virtual void            runAnimation(Animation &, lua_State * thread, int nargs) = 0;
        virtual void            stopAnimation(lua_State * lua, Animation &) = 0;
    protected:
        ~Controller() {}
    };

public:
                    Environment(lua_State * lua) : m_lua(lua) {}

    void            openKeyleds(Controller *);
    Controller *    controller() const;
private:
    lua_State *     m_lua;
};

/****************************************************************************/

} } // namespace keyleds::lua

#endif