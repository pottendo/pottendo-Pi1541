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
#include "Options.h"
#include <fcntl.h>
#include <unistd.h>
extern Options options;

extern "C" 
{
    int petlp_main(int argc, char **argv);
    int pi1541_printline(BYTE *line, int len);
    int pi1541_close(void);
    int vicepr_main(int argc, char **argv);
}

size_t bufferlen = 1024*64;
unsigned const char *prBuffer4vicedrv;

Printer::Printer(u8 ID, bool activate) 
: deviceID(ID)
, iec_bus(ID)
, run(activate)
, spoolnr(0)
, last_secaddr(0)
{
    FRESULT res;
    FILINFO fi;
    string fp;
    iecCommands = new IEC_Commands(&iec_bus);
    fp = def_prefix + options.GetPrPath();
    DEBUG_LOG("%s: emulator, deviceID=%d, spooler dir = '%s'", __FUNCTION__, deviceID, fp.c_str());
    printerBuffer.reserve(128 * 1024);
    res = f_stat(fp.c_str(), &fi);
    if (res == FR_INVALID_NAME)
        DEBUG_LOG("%s: invalid path '%s'", __FUNCTION__, fp.c_str());
    else
    {
        res = f_mkdir(fp.c_str());
        if (res == FR_OK)
            DEBUG_LOG("%s: printer spool directory '%s' created", __FUNCTION__, fp.c_str());
        else if (res != FR_EXIST)
            DEBUG_LOG("%s: couldn't create printer spool directory: '%s'", __FUNCTION__, fp.c_str());
    }
}

Printer::~Printer()
{
    delete iecCommands;
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

void Printer::get_prspool(string &p, string ext, bool basename)
{ 
    p = p + '-' + to_string(spoolnr) + ext; 
    if (!basename)
        p = def_prefix + string(options.GetPrPath()) + '/' + p;
}

void Printer::add2buffer(u8 byte, u8 secondary_addr)
{
    last_secaddr = secondary_addr;
    printerBuffer.push_back(byte);
}

void Printer::flush(bool petscii, string &res)
{
    if (!printerBuffer.empty())
    {
        int line = 1;
        int lidx = 0;
        char lbuf[81];
        // For now, we just log the data for demonstration purposes.
        DEBUG_LOG("%s: Flushing %u bytes to printer, deviceID=%d", __FUNCTION__,
            static_cast<unsigned>(printerBuffer.size()), deviceID);
        for (size_t i = 0; i < printerBuffer.size(); ++i)
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
            DEBUG_LOG("L%03d:%s\n%u chars printed.", line, lbuf,
                static_cast<unsigned>(printerBuffer.size()));
            printerBuffer.clear();
        }
    }
}

void Printer::flush_petlp(void)
{
    int ret;
    char *argv[] = { "pipetlpdrv", "-o", "", "-lower", "-lang", "commodore", "-F", "PS", "-pi1541", nullptr };
    if (!printerBuffer.empty())
    {
        string p = "petlp";
        bufferlen = printerBuffer.size();
        get_prspool(p, string(".ps"));
        argv[2] = (char *)p.c_str();
        argv[3] = (char *)((last_secaddr == 7) ? "-lower" : "-upper");
        DEBUG_LOG("%s: Flushing %u bytes to pi1541 petlp printer file %s", __FUNCTION__,
            static_cast<unsigned>(printerBuffer.size()), p.c_str());
        ret = petlp_main(10, argv);
        DEBUG_LOG("%s: petlp_main allocated %dbytes", __FUNCTION__, ret);
        ret = pi1541_printline(printerBuffer.data(), static_cast<int>(printerBuffer.size()));
        if (ret < 0)
        {
            DEBUG_LOG("%s: pi1541_printline failed with error code %d", __FUNCTION__, ret);
        }
        pi1541_close();
        DEBUG_LOG("%s: Flushed %u bytes to pi1541 printer '%s", __FUNCTION__,
            static_cast<unsigned>(printerBuffer.size()), p.c_str());
    }
}

void Printer::flush_vice(void)
{
    int ret;
    char *argv[] = { "vice-drv", "--gfx", "", "--drv", "mps803", nullptr };
    if (!printerBuffer.empty())
    {
        string p = "pri1541";
        bufferlen = printerBuffer.size();
        prBuffer4vicedrv = printerBuffer.data();
        get_prspool(p, string(".png"));
        argv[2] = (char *)p.c_str();
        DEBUG_LOG("%s: Flushing %u bytes to pi1541 petlp printer file %s", __FUNCTION__,
            static_cast<unsigned>(printerBuffer.size()), p.c_str());
        ret = vicepr_main(5, argv);
        if (ret < 0)
        {
            DEBUG_LOG("%s: vicepr_main() failed with error code %d", __FUNCTION__, ret);
        }
        DEBUG_LOG("%s: Flushed %u bytes to pi1541 printer '%s", __FUNCTION__,
            static_cast<unsigned>(printerBuffer.size()), p.c_str());
    }
}

bool Printer::flush_raw(void)
{
    if (printerBuffer.empty())
        return true;
    string p = "pri1541";
    get_prspool(p, ".raw");

    FIL fp;
    FRESULT res = f_open(&fp, p.c_str(), FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK)
    {
        DEBUG_LOG("%s: couldn't open '%s' for writing (%d)", __FUNCTION__, p.c_str(), res);
        return false;
    }

    size_t offset = 0;
    while (offset < printerBuffer.size())
    {
        UINT bytesWritten = 0;
        UINT bytesToWrite = static_cast<UINT>(
            printerBuffer.size() - offset > 64 * 1024 ? 64 * 1024 : printerBuffer.size() - offset);
        res = f_write(&fp, printerBuffer.data() + offset, bytesToWrite, &bytesWritten);
        if (res != FR_OK || bytesWritten != bytesToWrite)
        {
            DEBUG_LOG("%s: couldn't write '%s' (%d)", __FUNCTION__, p.c_str(), res);
            f_close(&fp);
            return false;
        }
        offset += bytesWritten;
    }

    res = f_close(&fp);
    if (res != FR_OK)
    {
        DEBUG_LOG("%s: couldn't close '%s' (%d)", __FUNCTION__, p.c_str(), res);
        return false;
    }

    DEBUG_LOG("%s: dumped %u bytes to '%s'", __FUNCTION__,
        static_cast<unsigned>(printerBuffer.size()), p.c_str());
    return true;
}