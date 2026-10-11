// printer.h
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

#ifndef _printer_h_
#define _printer_h_

#include "iec_commands.h"
#include "iec_bus.h"

class Printer
{
    u8 deviceID;
    IEC_Commands* iecCommands;
    IEC_Bus iec_bus;
public:
    Printer(u8 deviceID);
    ~Printer();
    void emulate(void);
};

#endif /* _printer_h_ */