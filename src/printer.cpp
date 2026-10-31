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
, shoff(false)
, spoolfn(string("PrI1541"))
, spoolnr(0)
, last_secaddr(0)
, spoolfilenr(0)
, dirty(false)
, lext(".pdf")
{
    FRESULT res;
    FILINFO fi;
    string fp;
    iecCommands = new IEC_Commands(&iec_bus);
    fp = def_prefix + options.GetPrPath();
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
    if (deviceID == 6)
        driver = "1520";
    else if ((deviceID == 4) || (deviceID == 5))
    {
        driver = options.GetPrDriver();
        if (driver != "mps801" && driver != "mps802" && driver != "mps803" && driver != "nl10" && driver != "1520" && driver != "petlp")
        {
            DEBUG_LOG("%s: unsupported printer driver: %s, defaulting to 'mps803'", __FUNCTION__, driver.c_str());
        }
    }
    else 
    {
        DEBUG_LOG("%s: invalid printer device %d, disabling printer.", __FUNCTION__, deviceID);
        shoff = true;
    }
    DEBUG_LOG("%s: emulator, deviceID=%d, driver = '%s', spooler dir = '%s'", __FUNCTION__, deviceID, driver.c_str(), fp.c_str());
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
    if (ext == "")
        ext = lext;
    p = spoolfn + '-' + to_string(spoolnr) + ext; 
    if (!basename)
        p = def_prefix + string(options.GetPrPath()) + '/' + p;
}

void Printer::add2buffer(u8 byte, u8 secondary_addr)
{
    last_secaddr = secondary_addr;
    printerBuffer.push_back(byte);
    if (!dirty)
    {
        dirty = true;
        spoolnr++;
    }
}

void Printer::flush(string drv, string &res)
{
    if (!dirty)
        return;
    flush_raw(res);
    res += "<br />";
    flush_vice(driver, res, true);
    res += "<br />";
    flush_vice(driver, res, false);
    res += "<br/ >";
    dirty = false;
    printerBuffer.clear();
}

void Printer::flush_vice(string drv, string &res, bool gfx)
{
    int ret;
    string output_path;
    string d = drv;
    if ((drv == "petlp") && gfx)
        d = "mps803";  // we don't have an gxf driver in petlp for Epson, use mps803 for preview

    if (printerBuffer.empty())
        return;
    bufferlen = printerBuffer.size();
    prBuffer4vicedrv = printerBuffer.data();
    if (d == "petlp")
    {
        lext = ".ps";
        get_prspool(output_path, ".ps");
        char *argv[] = { 
            const_cast<char *>("pipetlpdrv"), 
            const_cast<char *>("-o"), 
            const_cast<char *>(output_path.c_str()), 
            const_cast<char *>((last_secaddr == 7) ? "-lower" : "-upper"), 
            const_cast<char *>("-lang"), 
            const_cast<char *>("commodore"), 
            const_cast<char *>("-F"), 
            const_cast<char *>("PS"),
            const_cast<char *>("-pi1541"),
            nullptr 
        };
        ret = petlp_main(10, argv);
        //DEBUG_LOG("%s: petlp_main allocated %dbytes", __FUNCTION__, ret);
        ret = pi1541_printline(printerBuffer.data(), bufferlen);
        if (ret < 0)
            DEBUG_LOG("%s: petlp failed with error code %d", __FUNCTION__, ret);
        else
            DEBUG_LOG("%s: Flushed %u bytes to pi1541 printer '%s", __FUNCTION__,
                bufferlen, output_path.c_str());
        pi1541_close();
    }
    else
    {
        if (!gfx)
            lext = ".pdf";
        get_prspool(output_path, gfx ? ".png" : ".pdf");
        char *argv[] = {
            const_cast<char *>("vice-drv"),
            const_cast<char *>(gfx ? "--gfx" : "--pdf"),
            const_cast<char *>(output_path.c_str()),
            const_cast<char *>("--drv"),
            const_cast<char *>(d.c_str()),
            nullptr};

        ret = vicepr_main(5, argv);
    }
    if (ret != 0)
    {
        DEBUG_LOG("%s: driver failed with error code %d",
            __FUNCTION__, ret);
        res += string("printer driver failed, error ") + to_string(ret);
    }
    else
    {
        DEBUG_LOG("%s: flushed %u bytes to pi1541 printer file '%s'",
            __FUNCTION__, bufferlen, output_path.c_str());
        res += string("flushed ") + to_string(printerBuffer.size())
            + " bytes to " + output_path;
    }
}

bool Printer::flush_raw(string &r)
{
    if (printerBuffer.empty())
        return true;
    string p = "";
    get_prspool(p, ".raw");

    FIL fp;
    FRESULT res = f_open(&fp, p.c_str(), FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK)
    {
        DEBUG_LOG("%s: couldn't open '%s' for writing (%d)", __FUNCTION__, p.c_str(), res);
        r += string("couldn't open ") + p + "(" + r + ")";
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
            r += string("couldn't write ") + p + "(" + r + ")";
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
    r += string("flushed ") + to_string(printerBuffer.size()) + " bytes to " + p;
    return true;
}