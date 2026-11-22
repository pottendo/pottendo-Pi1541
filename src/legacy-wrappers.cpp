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

#include <circle/startup.h>
#include "circle-kernel.h"
#include <string>
#include "rpi-gpio.h"
#include "debug.h"
u32 _ctb;

/* wrappers */
void RPiConsole_put_pixel(uint32_t x, uint32_t y, uint16_t c) {	Kernel.set_pixel(x, y, c); }
void SetACTLed(int v) { Kernel.SetACTLed(v); }
void reboot_now(void) { reboot(); }
void i2c_init(int BSCMaster, int fast) { Kernel.i2c_init(BSCMaster, fast); }
void i2c_setclock(int BSCMaster, int clock_freq) { Kernel.i2c_setclock(BSCMaster, clock_freq); }
int i2c_read(int BSCMaster, unsigned char slaveAddress, void* buffer, unsigned count) { return Kernel.i2c_read(BSCMaster, slaveAddress, buffer, count); }
int i2c_write(int BSCMaster, unsigned char slaveAddress, void* buffer, unsigned count) { return Kernel.i2c_write(BSCMaster, slaveAddress, buffer, count); }
int i2c_scan(int BSCMaster, unsigned char slaveAddress) { return Kernel.i2c_scan(BSCMaster, slaveAddress); }
void USPiInitialize(void) 
{
	if (Kernel.usb_updatepnp() == false) 
	{
		Kernel.log("usb update failed");
	}
}
int USPiKeyboardAvailable(void) 
{ 
    int n = Kernel.usb_keyboard_available(); 
    Kernel.log("%s: %d keyboard(s) found", __FUNCTION__, n);
    return n;
}
void USPiKeyboardRegisterKeyStatusHandlerRaw(TKeyStatusHandlerRaw *handler) { Kernel.usb_reghandler(handler); }
TKernelTimerHandle TimerStartKernelTimer(unsigned long nDelay, TKernelTimerHandler *pHandler, void* pParam, void* pContext)
{
	return Kernel.timer_start(nDelay, pHandler, pParam, pContext);
}
void TimerCancelKernelTimer(TKernelTimerHandle hTimer) { Kernel.timer_cancel(hTimer); }
int GetTemperature(unsigned &value) { unsigned ret = CPUThrottle.GetTemperature(); if (ret) value = ret * 1000; return ret; }
int USPiMassStorageDeviceAvailable(void) { return Kernel.usb_massstorage_available(); }

void PlaySoundDMA(int play) { if (play == 0) Kernel.playsound(); }

extern "C" void klog(const char *fmt, ...)
{
	char t[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(t, 2048, fmt, args);
	Kernel.log(t);
	va_end(args);
}

void hexdump(const char *buf, int len)
{
    int i;
    int idx = 0;
    int lines = 0;
    char linestr[256] = {0};
	char ta[12];
	const char *tp = buf;
    while (len > 0) {
#if (AARCH == 64)		// avoid warning
		snprintf(ta, 12, "%04lx: ", ((buf + idx) - tp));
#else
		snprintf(ta, 12, "%04x: ", ((buf + idx) - tp));
#endif
		strcat(linestr, ta);
        for (i = 0; i < 16; i++) {
            if (i < len) {
                char t[4];
                snprintf(t, 4, "%02x ", (uint8_t) buf[idx + i]);
                strcat(linestr, t);
            } else {
                strcat(linestr, "   ");
            }
        }
        strcat(linestr, "|");
        for (i = 0; i < 16; i++) {
            if (i < len) {
                char t[2];
                char c;
                if (((unsigned char)buf[idx + i] > 31) && /* avoid tabs and other printable ctrl chars) */
                    isprint((unsigned char)buf[idx + i])) {
                    c= buf[idx + i];
                } else {
                    c = '.';
                }
                snprintf(t, 2, "%c", c);
                strcat(linestr, t);
            } else {
                strcat(linestr, " ");
            }
        }
        strcat(linestr, "|");
        DEBUG_LOG("%s", linestr);
		linestr[0] = '\0';
        idx += 16;
        len -= 16;
    }
}

char *fbinary (char *p, char *e, const char *s, size_t len) 
{
    char *fin = e - len;
    char *t = p;
    int match = -1;
    while ((t < fin) && ((match = memcmp(t, s, len)) != 0))
        t++;
    return (match == 0) ? t : 0;
}

save_CD::save_CD(const char *path, std::string &def_prefix)
: drive("")
{
    FRESULT res;
    f_getcwd(scwd, 255);
    if (!path)
        return;
    std::string cwd(scwd);
    if (cwd.find(def_prefix, 0) == std::string::npos)
    {
        DEBUG_LOG("%s: cwd %s differs from def_prefix %s", __FUNCTION__, cwd.c_str(), def_prefix.c_str());
        // prefix and drive may be different, preserve drive
        size_t colon = def_prefix.find(':');
        drive = def_prefix.substr(0, colon);
        if ((res = f_chdrive("SD")) != FR_OK)
            DEBUG_LOG("%s: failed chdrive %d", __FUNCTION__, res);
    }
    DEBUG_LOG("%s: preserve %s %s, CD to %s", __FUNCTION__, def_prefix.c_str(), scwd, path);
    if ((res = f_chdir(path)) != FR_OK)
        DEBUG_LOG("%s: failed chdir %d", __FUNCTION__, res);
}

save_CD::~save_CD()
{
    FRESULT res;
    DEBUG_LOG("%s: restoring %s: %s", __FUNCTION__, drive.c_str(), scwd);
    if (drive != "")
    {
        if ((res = f_chdrive(drive.c_str())) != FR_OK)
            DEBUG_LOG("%s: failed chdrive %d", __FUNCTION__, res);
    }
    if ((res = f_chdir(scwd)) != FR_OK)
        DEBUG_LOG("%s: failed chdir %d", __FUNCTION__, res);
}
