//
// printer.cpp
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

#include "printer.h"
#include "iec_bus.h"

Printer::Printer(u8 ID) 
: deviceID(ID)
, iec_bus(ID)
{
    iecCommands = new IEC_Commands(&iec_bus);
    DEBUG_LOG("%s: emulator, deviceID=%d", __FUNCTION__, deviceID);
}

Printer::~Printer()
{
    delete iecCommands;
}

void Printer::emulate(void)
{
    iecCommands->SetDeviceId(deviceID);
    iecCommands->SimulateIECBegin();
    while (true)
    {
        IEC_Commands::UpdateAction action = iecCommands->SimulateIECUpdate();
        if (action == IEC_Commands::UpdateAction::RESET)
        {
            iecCommands->Reset();
            iec_bus.Reset();
            iecCommands->SimulateIECBegin();
        }
        DEBUG_LOG("%s: action = %d", __FUNCTION__, action);
        MsDelay(1000);
    }
}