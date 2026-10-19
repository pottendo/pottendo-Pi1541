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

extern "C" 
{
    int petlp_main(int argc, char **argv);
    int pi1541_printline(BYTE *line, int len);
    int pi1541_close(void);
}

Printer::Printer(u8 ID, bool activate) 
: deviceID(ID)
, iec_bus(ID)
, printerBufferIndex(0)
, run(activate)
{
    iecCommands = new IEC_Commands(&iec_bus);
    DEBUG_LOG("%s: emulator, deviceID=%d", __FUNCTION__, deviceID);
    printerBuffer = new u8[printerBufferSize];
}

Printer::~Printer()
{
    delete iecCommands;
    delete[] printerBuffer;
    DEBUG_LOG("%s: %d deleted", __FUNCTION__, deviceID);
}

void Printer::emulate(void)
{
    DEBUG_LOG("%s: printer emulation started for device %d", __FUNCTION__, deviceID);
    iecCommands->SetDeviceId(deviceID);
    iec_bus.Reset();
    iec_bus.LetSRQBePulledHigh();
    iec_bus.WaitMicroSeconds(100);
    iecCommands->SimulateIECBegin();

    do
    {
        IEC_Commands::UpdateAction action = iecCommands->SimulateIECUpdate();
        if (action == IEC_Commands::UpdateAction::RESET)
        {
            MsDelay(500);
            iecCommands->Reset();
            iec_bus.Reset();
            iecCommands->SimulateIECBegin();
        }
    } while (run);
    DEBUG_LOG("%s: printer emulation exited", __FUNCTION__);
}

void Printer::add2buffer(u8 byte)
{
    if (printerBufferIndex < printerBufferSize)
    {
        printerBuffer[printerBufferIndex++] = byte;
        /*
        DEBUG_LOG("%s: added byte 0x%02x (%c) to buffer, deviceID=%d, bufferIndex=%d", 
            __FUNCTION__, byte, isprint(byte) ? byte : '.', deviceID, printerBufferIndex);
        */
    }
    else
    {
        DEBUG_LOG("%s: Printer buffer overflow, deviceID=%d", __FUNCTION__, deviceID);
        printerBufferIndex = 0; // Reset buffer index to avoid overflow
    }
}

void Printer::flush(bool petscii, string &res)
{
    if (printerBufferIndex > 0)
    {
        int line = 1;
        int lidx = 0;
        char lbuf[81];
        // For now, we just log the data for demonstration purposes.
        DEBUG_LOG("%s: Flushing %d bytes to printer, deviceID=%d", __FUNCTION__, printerBufferIndex, deviceID);
        for (uint32_t i = 0; i < printerBufferIndex; ++i)
        {
            if (printerBuffer[i] == 0x0a) // Handle line feed
                continue;
            if (petscii)
                lbuf[lidx] = printerBuffer[i]; // Keep the original byte for petscii output
            else
                lbuf[lidx] = isprint(printerBuffer[i]) ? printerBuffer[i] : '.';
            if (printerBuffer[i] == 0x0d)
            {
                lbuf[lidx] = '\0'; // Null-terminate the string
                if (petscii)
                    res += string(lbuf) + "<br />"; // Convert carriage return to newline for better readability
                else
                    DEBUG_LOG("L%03d: %s", line++, lbuf);
                lidx = 0; 
                continue;
            }
            if (lidx >= 79)
            {
                lbuf[lidx + 1] = '\0'; // Null-terminate the string
                if (petscii)
                    res += string(lbuf) + "<br />"; // Convert carriage return to newline for better readability
                else
                    DEBUG_LOG("L%03d: %s", line, lbuf);
                lidx = 0; 
            }
            lidx++;
        }
        lbuf[lidx] = '\0'; // Null-terminate the string
        if (petscii)
            res += string(lbuf) + "<br />"; // Convert carriage return to newline for better readability
        else
        {
            DEBUG_LOG("L%03d:%s\n%d chars printed.", line, lbuf, printerBufferIndex);
            // Reset the buffer index after flushing
            printerBufferIndex = 0;
        }
    }
}

void Printer::flush_petlp(void)
{
    int ret;
    char *argv[] = { "pipetlpdrv", "-F", "PS", "-o", "petlp.ps", "-pi1541", nullptr };
    if (printerBufferIndex > 0)
    {
        DEBUG_LOG("%s: Flushing %d bytes to pi1541 petlp printer", __FUNCTION__, printerBufferIndex);
        ret = petlp_main(7, argv);
        DEBUG_LOG("%s: petlp_main returned %d", __FUNCTION__, ret);
        ret = pi1541_printline(printerBuffer, printerBufferIndex);
        if (ret < 0)
        {
            DEBUG_LOG("%s: pi1541_printline failed with error code %d", __FUNCTION__, ret);
        }
        pi1541_close();
        DEBUG_LOG("%s: Flushed %d bytes to pi1541 printer", __FUNCTION__, printerBufferIndex);
    }
}