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

#include <string>
#include "iec_commands.h"
#include "iec_bus.h"
using namespace std;

class Printer
{
    u8 deviceID;
    IEC_Commands* iecCommands;
    IEC_Bus iec_bus;
    u8 *printerBuffer;
    const size_t printerBufferSize = 1024 * 64; // 64KB buffer for printer data
    uint32_t printerBufferIndex;
public:
    Printer(u8 deviceID);
    ~Printer();
    IEC_Bus* GetIEC_Bus() { return &iec_bus; }
    void emulate(void);
    void add2buffer(u8 byte);
    void flush(bool petscii, string &res);

};
extern Printer *printer;

#endif /* _printer_h_ */