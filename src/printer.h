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
#include <vector>
#include "iec_commands.h"
#include "iec_bus.h"
using namespace std;

class Printer
{
    const string def_prefix = "SD:/1541/";
    u8 deviceID;
    IEC_Commands* iecCommands;
    IEC_Bus iec_bus;
    vector<u8> printerBuffer;
    bool run;
    uint16_t spoolnr;
    u8 last_secaddr;
public:
    Printer(u8 deviceID, bool activate);
    ~Printer();
    IEC_Bus* GetIEC_Bus() { return &iec_bus; }
    void emulate(void);
    void add2buffer(u8 byte, u8 sec_addr);
    void get_prspool(string &p, bool basename = false);
    void flush(bool petscii, string &res);
    void flush_petlp(void);
    bool flush_raw(void);
    inline bool running(void) { return run; }
    inline void set_running(bool r) { run = r; }
};
extern Printer *printer;

#endif /* _printer_h_ */