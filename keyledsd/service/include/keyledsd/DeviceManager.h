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
#ifndef KEYLEDSD_DEVICEMANAGER_H_0517383B
#define KEYLEDSD_DEVICEMANAGER_H_0517383B

#include <QObject>
#include "keyledsd/Configuration.h"
#include "keyledsd/Device.h"
#include "keyledsd/EffectManager.h"
#include "keyledsd/KeyDatabase.h"
#include "keyledsd/RenderLoop.h"
#include "tools/FileWatcher.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace device { class Description; }

namespace keyleds {

class LayoutDescription;

/****************************************************************************/

/** Main device manager
 *
 * Centralizes all operations and information for a specific device.
 * It is given a device instance to manage and a reference to current
 * configuration at creation time, and coordinates feature detection,
 * layout management, and related objects' life cycle.
 */
class DeviceManager final : public QObject
{
    Q_OBJECT
    using Effect = effect::interface::Effect;
    using FileWatcher = tools::FileWatcher;
    using string_map = std::vector<std::pair<std::string, std::string>>;
private:
    /** An effect group, fully loaded with effects
     *
     * Holds a list of loaded effects to include while rendering device status
     * and the matching effect is enabled.
     */
    class EffectGroup final
    {
        using effect_list = std::vector<EffectManager::effect_ptr>;
    public:
                            EffectGroup(std::string name, effect_list && effects);
                            EffectGroup(EffectGroup &&) noexcept = default;
                            ~EffectGroup();
        EffectGroup &       operator=(EffectGroup &&) = default;

        const std::string & name() const noexcept { return m_name; }
        const effect_list & effects() const { return m_effects; }
    private:
        std::string         m_name;
        effect_list         m_effects;
    };
    using effect_group_list = std::vector<EffectGroup>;

public:
    using dev_list = std::vector<std::string>;
public:
                            DeviceManager(EffectManager &, FileWatcher &,
                                          const ::device::Description &,
                                          std::unique_ptr<Device>,
                                          const Configuration *,
                                          QObject *parent = nullptr);
                            ~DeviceManager() override;

    const std::string &     sysPath() const noexcept { return m_sysPath; }
    const std::string &     serial() const noexcept { return m_serial; }
    const std::string &     name() const noexcept { return m_name; }
    const dev_list &        eventDevices() const { return m_eventDevices; }
    const Device &          device() const { return *m_device; }
    const KeyDatabase &     keyDB() const { return m_keyDB; }

    auto                    getRenderTarget() const { return RenderLoop::renderTargetFor(*m_device); }

          bool              paused() const { return m_renderLoop.paused(); }

public:
    void                    setConfiguration(const Configuration *);
    void                    setContext(const string_map &);
    void                    handleFileEvent(FileWatcher::event, uint32_t, std::string);
    void                    handleGenericEvent(const string_map &);
    void                    handleKeyEvent(int, bool);
    void                    setPaused(bool);

private:
    // Static loaders, invoked once at manager creation to set it up
    static std::string      getSerial(const ::device::Description &);
    static std::string      getName(const Configuration &, const std::string & serial);
    static dev_list         findEventDevices(const ::device::Description &);
    static KeyDatabase      setupKeyDatabase(Device &);
    static KeyDatabase      buildKeyDatabase(const Device &, const LayoutDescription &);

    /// Loads the list of effects to activate for the given context
    std::vector<Effect *>   loadEffects(const string_map & context);

    /// Instanciates an effect, combining its configuration with this device's info
    EffectGroup &           getEffectGroup(const Configuration::EffectGroup &);

private:
    EffectManager &         m_effectManager;    ///< Manages the lifecycle of effects
    const Configuration *   m_configuration;    ///< Reference to service configuration

    const std::string       m_sysPath;          ///< Device path on sys filesystem
    const std::string       m_serial;           ///< Device serial number
          std::string       m_name;             ///< User-given name
    const dev_list          m_eventDevices;     ///< List of event device paths that the
                                                ///  physical device can communicate on.
    std::unique_ptr<Device> m_device;           ///< The device handled by this manager
    FileWatcher::subscription m_fileWatcherSub; ///< Ensures we get notifications for devnode events
    const KeyDatabase       m_keyDB;            ///< Fully loaded key descriptions

    effect_group_list       m_effectGroups;     ///< Loaded effect group instances
    RenderLoop              m_renderLoop;       ///< The RenderLoop in charge of the device
    std::vector<Effect *>   m_activeEffects;    ///< Effects currently active on m_renderLoop
};

/****************************************************************************/

} // namespace keyleds

#endif
