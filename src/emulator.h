//
// emulator.h
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// written by pottendo
//
#include <string>
#include "iec_commands.h"
#include "FileBrowser.h"
#include "Pi1541.h"
#include "Pi1581.h"
#include "iec_bus.h"

class emulator_t {
    IEC_Commands *_m_IEC_Commands;
    FileBrowser *fileBrowser;
    bool selectedViaIECCommands;
    const char* fileBrowserSelectedName;
    u8 deviceID = 8;
    const uint32_t driveID = 0;
    DiskCaddy diskCaddy;
    Pi1541 pi1541;
    #if defined(PI1581SUPPORT)
    Pi1581 pi1581;
    #endif
    EmulatingMode emulating;
    u16 pc;
    IEC_Bus iec_bus;
    bool shared_IEC;
public:
    emulator_t(u8 deviceID, const uint32_t drive);
    ~emulator_t();

    EmulatingMode BeginEmulating(FileBrowser* fileBrowser, const char* filenameForIcon);
    void GlobalSetDeviceID(u8 id);
    EXIT_TYPE Emulate1581(FileBrowser* fileBrowser);
    EXIT_TYPE Emulate1541(FileBrowser* fileBrowser);
    void run_emulator(void);
    inline Pi1541 *get_pi1541() { return &pi1541; }
    inline Pi1581 *get_pi1581() { return &pi1581; }
    inline IEC_Bus *get_iec_bus() { return &iec_bus; }
    inline u8 get_deviceID() const { return deviceID; }
    inline uint32_t get_driveID() const { return driveID; }
    inline DiskCaddy *get_diskCaddy() { return &diskCaddy; }
    inline EmulatingMode get_emulatingMode() const { return emulating; }
    std::string get_emulationModeString(void) const;
    void refresh_display(void);
    void share_IECBus(void);
    std::string select_drive(bool switch_drive = false);
};
