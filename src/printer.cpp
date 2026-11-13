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
#include "ippclient.h"
#include <fcntl.h>
#include <unistd.h>
extern Options options;

extern "C" 
{
    int petlp_main(int argc, char **argv);
    int pi1541_printline(BYTE *line, size_t len, int secondary);
    int pi1541_close(void);
    int vicepr_main(int argc, char **argv);
    int vicepr_finish(void);
}

size_t bufferlen = 1024*64;
unsigned const char *prBuffer4vicedrv;
unsigned out_pr1 = 0;
uint16_t Printer::next_snr = 1;

Printer::Printer(u8 ID, bool activate) 
: deviceID(ID)
, iec_bus(ID)
, last_secaddr(255)
, spooled(0)
, run(activate)
, shoff(false)
, spoolfn(string("PrI1541"))
, spoolnr(0)
, spoolfilenr(0)
, dirty(false)
, lext(".pdf")
, fp(def_prefix + options.GetPrPath())
, dprint(false)
, ipp_client(nullptr)
{
    FRESULT res;
    FILINFO fi;
    iecCommands = new IEC_Commands(&iec_bus);
    
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
    driver = options.GetPrDriver();
    if (deviceID == 6)
        driver = "mps1520";
    if (driver == "mps1520")
    {
        deviceID = 6;
        iec_bus.SetDeviceID(deviceID);
    }
    else if ((deviceID == 4) || (deviceID == 5))
    {
        if (driver != "mps801" && driver != "mps802" && driver != "mps803" && 
            driver != "nl10" && driver != "mps1520" && driver != "petlp")
        {
            DEBUG_LOG("%s: unsupported printer driver: %s, defaulting to 'mps803'", __FUNCTION__, driver.c_str());
        }
    }
    else 
    {
        DEBUG_LOG("%s: invalid printer device %d, disabling printer.", __FUNCTION__, deviceID);
        shoff = true;
    }
    if (!shoff && (strcmp(options.GetPrHost(), "") != 0) && options.NetworkOn())
        dprint = true;
    next_spoolnr(next_snr); // initialize next_snr with first free entry found
    DEBUG_LOG("%s: emulator, deviceID=%d, driver = '%s', spooler dir = '%s', direct print: %s %s", 
        __FUNCTION__, deviceID, driver.c_str(), fp.c_str(), (dprint ? "enabled" : "no"), (dprint ? options.GetPrHost() : ""));
}

Printer::~Printer()
{
    delete iecCommands;
    for (auto channel : spoolChannels)
        delete channel;
    if (ipp_client)
        delete ipp_client;
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
    out_pr1 = 0;
}

void Printer::get_prspool(string &p, string ext, bool basename)
{ 
    char buf[8];
    if (ext == "")
        ext = lext;
    snprintf(buf, 7, "%03d", spoolnr);
    p = spoolfn + '-' + string(buf) + ext; 
    if (!basename)
        p = def_prefix + string(options.GetPrPath()) + '/' + p;
}

void Printer::add2buffer(u8 byte, u8 sec_addr)
{
    if (sec_addr != last_secaddr)
    {
        //DEBUG_LOG("%s: secondary address changed from %d to %d", __FUNCTION__, last_secaddr, sec_addr);
        last_secaddr = sec_addr;
        curr_channel = new SpoolChannel(sec_addr);
        spoolChannels.push_back(curr_channel);
    }
    curr_channel->printerBuffer.push_back(byte);
    spooled++;
    if (options.GetPrSound())
        Kernel.playsound(1);
    if (!dirty)
    {
        dirty = true;
        spoolnr = next_snr;
    }
}

void Printer::next_spoolnr(uint16_t spnr)
{
    char buf[256];
    FILINFO fi;
    int i;
    for (i = spnr; i < 4096; i++)
    {
        snprintf(buf, 255, "%s%s/%s-%03d.pdf", 
            def_prefix.c_str(), options.GetPrPath(), spoolfn.c_str(), i);
        if (f_stat(buf, &fi) != FR_OK)
            break;
    }
    next_snr = i;
    DEBUG_LOG("%s: next spool nr = %d", __FUNCTION__, next_snr);
}

// normally called from web-server
void Printer::flush(string drv, string &res)
{
    if (!dirty)
        return;
    flush_raw(res);
    flush_vice(driver, res, true);
    flush_vice(driver, res, false);
    dirty = false;
    spooled = 0;
    for (auto channel : spoolChannels)
    {
        channel->printerBuffer.clear();
        delete channel;
    }
    spoolChannels.clear();
    last_secaddr = 255;
    next_spoolnr(spoolnr);
}

static void open2print(IPPClient &ipp_client, string &f, string &msg)
{
    FIL fp;
    FRESULT res;
    FILINFO fi;
    unsigned int nread = 0, br;

    if ((res = f_stat(f.c_str(), &fi)) != FR_OK)
    {
        DEBUG_LOG("%s: can't stat '%s'(%d)", __FUNCTION__, f.c_str(), res);
        msg += "f_stat failed for " + f;
        return;
    }
    char *buf = new char[fi.fsize];
    if (!buf)
    {
        DEBUG_LOG("%s: can't allocate memory for printing of '%s'(%d)", __FUNCTION__, f.c_str(), res);
        msg += "Can't allocate memory for " + f;
        return;
    }
    if ((res = f_open(&fp, f.c_str(), FA_READ)) != FR_OK)
    {
        DEBUG_LOG("%s: open failed for '%s'(%d)", __FUNCTION__, f.c_str(), res);
        msg += "open failed for " + f;
        goto out;
    }

    while (((res = f_read(&fp, &buf[nread], fi.fsize - nread, &br)) == FR_OK) && ((nread + br) < fi.fsize))
        nread += br;
    DEBUG_LOG("%s: successfully read %dbytes from %s", __FUNCTION__, fi.fsize, f.c_str());
    if (ipp_client.Print(buf, fi.fsize))
    {
        msg += "<i>" + f + "</i> sent to printer";
        DEBUG_LOG("%s: %s sent to printer", __FUNCTION__, f.c_str());
    }
    else
    {
        msg += "failed to send <i>" + f + "</i> to printer";
        DEBUG_LOG("%s: failed to send %s to printer", __FUNCTION__, f.c_str());
    }
out:
    delete[] buf;    
    f_close(&fp);
}

void Printer::flush_vice(string drv, string &res, bool gfx)
{
    int ret;
    string output_path;
    string d = drv;
    uint32_t channel_count[16] = {0};
    if ((drv == "petlp") && gfx)
        d = "mps803";  // we don't have an gxf driver in petlp for Epson, use mps803 for preview

    if (spooled == 0)
        return;
    bool petlp_active = false;
    if (d == "petlp")
    {
        lext = ".ps";
        get_prspool(output_path, ".ps");
        bufferlen = 1024 * 64;
        char *argv[] = {
            const_cast<char *>("pipetlpdrv"),
            const_cast<char *>("-o"),
            const_cast<char *>(output_path.c_str()),
            const_cast<char *>("-upper"),
            const_cast<char *>("-F"),
            const_cast<char *>("PS"),
            const_cast<char *>("-pi1541"),
            nullptr};
        ret = petlp_main(8, argv);
        if (ret < 0)
        {
            DEBUG_LOG("%s: petlp initialization failed with error code %d",
                      __FUNCTION__, ret);
            res += string("petlp failed to initialize, error ")
                + to_string(ret) + "<br />";
        }
        else
        {
            petlp_active = true;
        }
        ret = 0;
    }
    for (auto channel : spoolChannels)
    {
        bufferlen = channel->printerBuffer.size();
        prBuffer4vicedrv = channel->printerBuffer.data();
        channel_count[channel->secondaryAddress] +=
            static_cast<uint32_t>(channel->printerBuffer.size());
        if (d == "petlp")
        {
            if (petlp_active)
            {
                ret = pi1541_printline(channel->printerBuffer.data(),
                                       channel->printerBuffer.size(),
                                       channel->secondaryAddress);
                if (ret < 0)
                {
                    DEBUG_LOG("%s: petlp failed with error code %d",
                              __FUNCTION__, ret);
                    res += string("petlp failed, error ")
                        + to_string(ret) + "<br />";
                }
            }
        }
        else
        {
            if (!gfx)
                lext = ".pdf";
            get_prspool(output_path, gfx ? ".png" : ".pdf");
            string secaddr = to_string(channel->secondaryAddress);
            if (gfx)
            {
                char *argv[] = {
                    const_cast<char *>("vice-drv"),
                    const_cast<char *>("--gfx"),
                    const_cast<char *>(output_path.c_str()),
                    const_cast<char *>("--drv"),
                    const_cast<char *>(d.c_str()),
                    const_cast<char *>("--secaddr"),
                    const_cast<char *>(secaddr.c_str()),
                    nullptr};

                ret = vicepr_main(7, argv);
            }
            else
            {
                string pwg_output_path;
                get_prspool(pwg_output_path, ".pwg");
                char *argv[] = {
                    const_cast<char *>("vice-drv"),
                    const_cast<char *>("--pdf"),
                    const_cast<char *>(output_path.c_str()),
                    const_cast<char *>("--drv"),
                    const_cast<char *>(d.c_str()),
                    const_cast<char *>("--secaddr"),
                    const_cast<char *>(secaddr.c_str()),
                    const_cast<char *>("--pwg"),
                    const_cast<char *>(pwg_output_path.c_str()),
                    nullptr};

                ret = vicepr_main(9, argv);
            }
        }
        if (ret != 0)
        {
            DEBUG_LOG("%s: driver failed with error code %d",
                      __FUNCTION__, ret);
            res += string("printer driver failed, error ") + to_string(ret) + "<br />";
        }
    }
    if (petlp_active)
    {
        ret = pi1541_close();
        if (ret < 0)
        {
            DEBUG_LOG("%s: petlp failed to finish the final page, error %d",
                      __FUNCTION__, ret);
            res += string("petlp failed to finish the final page, error ")
                + to_string(ret) + "<br />";
        }
    }
    if (d != "petlp")
    {
        ret = vicepr_finish();
        if (ret != 0)
        {
            DEBUG_LOG("%s: failed to finish the final printer page, error %d",
                      __FUNCTION__, ret);
            res += string("printer driver failed to finish final page, error ")
                + to_string(ret);
            res += "<br />";
        }
    }
    for (auto i = 0; i < 16; i++)
    {
        if (channel_count[i] > 0)
        {
            DEBUG_LOG("%s: flushed %u bytes, sec_addr=%d to pi1541 printer file '%s'",
                      __FUNCTION__, channel_count[i], i, output_path.c_str());
            res += string("flushed ") + to_string(channel_count[i]) +
                   " bytes, sec_addr=" + to_string(i) + " to " + output_path + "<br />";
        }
    }
    if (!gfx && dprint)
    {
        // direct print
        IPPClient ipp_client(Kernel.get_mTLSSimple(), options.GetPrHost());
        if (!ipp_client.Connect())
            res += "Direct print: connect failed.";
        else
        {
            size_t ext = output_path.rfind(".pdf");
            output_path.replace(ext, 4, ".pwg");
            res += "Direct print: printing... <i>" + output_path + "</i><br />";
            open2print(ipp_client, output_path, res);
            res += "<br />";
        }
    }
}

bool Printer::flush_raw(string &r)
{
    if (spooled == 0)
        return true;
    string p = "";
    get_prspool(p, ".raw");
#ifdef DEBUG_PRINTERDRIVER
    string map_path = "";
    get_prspool(map_path, ".raw.map");
    string channel_map = "offset,length,secondary_address\r\n";
    size_t raw_offset = 0;
    for (auto channel : spoolChannels)
    {
        size_t channel_size = channel->printerBuffer.size();
        channel_map += to_string(raw_offset) + "," + to_string(channel_size) + "," +
                       to_string(channel->secondaryAddress) + "\r\n";
        raw_offset += channel_size;
    }
#endif

    FIL fp;
    FRESULT res = f_open(&fp, p.c_str(), FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK)
    {
        DEBUG_LOG("%s: couldn't open '%s' for writing (%d)", __FUNCTION__, p.c_str(), res);
        r += string("couldn't open ") + p + "(" + r + ")";
        return false;
    }

    size_t offset = 0;
    for (auto channel : spoolChannels)
    {
        while (offset < channel->printerBuffer.size())
        {
            UINT bytesWritten = 0;
            UINT bytesToWrite = static_cast<UINT>(
                channel->printerBuffer.size() - offset > 64 * 1024 ? 64 * 1024 : channel->printerBuffer.size() - offset);
            res = f_write(&fp, channel->printerBuffer.data() + offset, bytesToWrite, &bytesWritten);
            if (res != FR_OK || bytesWritten != bytesToWrite)
            {
                DEBUG_LOG("%s: couldn't write '%s' (%d)", __FUNCTION__, p.c_str(), res);
                f_close(&fp);
                r += string("couldn't write ") + p + "(" + r + ")";
                return false;
            }
            offset += bytesWritten;
        }
        offset = 0; // Reset offset for the next channel
    }
    
    res = f_close(&fp);
    if (res != FR_OK)
    {
        DEBUG_LOG("%s: couldn't close '%s' (%d)", __FUNCTION__, p.c_str(), res);
        return false;
    }

#ifdef DEBUG_PRINTERDRIVER
    res = f_open(&fp, map_path.c_str(), FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK)
    {
        DEBUG_LOG("%s: couldn't open '%s' for writing (%d)",
                  __FUNCTION__, map_path.c_str(), res);
        r += string("couldn't open ") + map_path + "(" + r + ")";
        return false;
    }

    size_t map_offset = 0;
    while (map_offset < channel_map.size())
    {
        UINT map_bytes_written = 0;
        UINT map_bytes_to_write = static_cast<UINT>(
            channel_map.size() - map_offset > 64 * 1024
                ? 64 * 1024
                : channel_map.size() - map_offset);
        res = f_write(&fp, channel_map.data() + map_offset,
                      map_bytes_to_write, &map_bytes_written);
        if (res != FR_OK || map_bytes_written != map_bytes_to_write)
        {
            DEBUG_LOG("%s: couldn't write '%s' (%d)",
                      __FUNCTION__, map_path.c_str(), res);
            f_close(&fp);
            r += string("couldn't write ") + map_path + "(" + r + ")";
            return false;
        }
        map_offset += map_bytes_written;
    }

    res = f_close(&fp);
    if (res != FR_OK)
    {
        DEBUG_LOG("%s: couldn't close '%s' (%d)",
                  __FUNCTION__, map_path.c_str(), res);
        return false;
    }
#endif

    DEBUG_LOG("%s: dumped %u bytes to '%s'", __FUNCTION__,
        static_cast<unsigned>(spooled), p.c_str());
    r += string("flushed ") + to_string(spooled) + " bytes to " + p;
#ifdef DEBUG_PRINTERDRIVER
    r += string(" with channel map ") + map_path;
#endif
    r += "<br />";
    return true;
}