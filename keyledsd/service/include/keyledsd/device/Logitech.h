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
#ifndef KEYLEDSD_KEYBOARD_H_F57B19AC
#define KEYLEDSD_KEYBOARD_H_F57B19AC

#include <QObject>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "keyledsd/colors.h"
#include "keyledsd/Device.h"
#include "tools/DeviceWatcher.h"

struct keyleds_device;
struct keyleds_key_color;

namespace std {
    template <> struct default_delete<struct keyleds_device> {
        void operator()(struct keyleds_device *) const;
    };
};

namespace keyleds { namespace device {

/****************************************************************************/

/** Physical device interface
 *
 * Handles communication with the underlying device. This class is built as a
 * wrapper around libkeyleds, with additional checks and caching. It also
 * converts library errors into exceptions.
 */
class Logitech final : public Device
{
public:
    // Exceptions
    class error : public Device::error
    {
        using keyleds_error_t = unsigned int;
    public:
                        error(std::string what, keyleds_error_t code, int oserror=0);
        bool            expected() const override;
        bool            recoverable() const override;
        keyleds_error_t code() const { return m_code; }
        int             oserror() const { return m_oserror; }
    private:
        keyleds_error_t m_code;
        int             m_oserror;
    };

private:
                    Logitech(std::unique_ptr<struct keyleds_device>,
                             std::string path, Type type, std::string name, std::string model,
                             std::string serial, std::string firmware, int layout, block_list);
public:
                    Logitech(const Logitech &) = delete;
                    ~Logitech() override;
    Logitech &      operator=(const Logitech &) = delete;
    Logitech &      operator=(Logitech &&) = default;

    // Factory method
    static std::unique_ptr<Device> open(const std::string & path);

    // Virtual method implementation
    bool            hasLayout() const override;
    std::string     resolveKey(key_block_id_type, key_id_type) const override;
    int             decodeKeyId(key_block_id_type, key_id_type) const override;

    // Manipulate
    void            setTimeout(unsigned us) override;
    void            flush() override;
    bool            resync() noexcept override;
    void            fillColor(const KeyBlock & block, const RGBColor) override;
    void            setColors(const KeyBlock & block, const ColorDirective[], size_t size) override;
    void            getColors(const KeyBlock & block, ColorDirective[]) override;
    void            commitColors() override;

private:
    static Type         getType(struct keyleds_device *);
    static std::string  getName(struct keyleds_device *);
    static block_list   getBlocks(struct keyleds_device *);
    static void         parseVersion(struct keyleds_device *, std::string * model,
                                     std::string * serial, std::string * firmware);

private:
    std::unique_ptr<struct keyleds_device> m_device;    ///< Underlying libkeyleds opaque handle
};

/****************************************************************************/

/** Keyleds-specific device watcher
 *
 * A device::FilteredDeviceWatcher that further filters detected devices to only
 * let Logitech devices through.
 */
class LogitechWatcher : public ::device::FilteredDeviceWatcher
{
    Q_OBJECT
public:
            LogitechWatcher(struct udev * udev = nullptr, QObject *parent = nullptr);
    bool    isVisible(const ::device::Description & dev) const override;
};

/****************************************************************************/

} } // namespace keyleds::device

#endif
