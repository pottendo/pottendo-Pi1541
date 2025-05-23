//
// kernel.cpp
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

#ifndef __legacy_wrappers_h__
#define __legacy_wrappers_h__
#include <string>

void reboot_now(void);
void Reboot_Pi(void);
static inline void delay_us(u32 usec) { Kernel.get_timer()->usDelay(usec); }
static inline void usDelay(u32 usec) { Kernel.get_timer()->usDelay(usec); }
static inline void MsDelay(u32 msec) { Kernel.get_timer()->usDelay(msec * 1000L); }

void USPiInitialize(void);
void TimerSystemInitialize(void);
void InterruptSystemInitialize(void);
int USPiMassStorageDeviceAvailable(void);
int USPiKeyboardAvailable(void);
void USPiKeyboardRegisterKeyStatusHandlerRaw(TKeyStatusHandlerRaw *handler);
void TimerCancelKernelTimer(TKernelTimerHandle hTimer);
TKernelTimerHandle TimerStartKernelTimer(unsigned long nDelay, TKernelTimerHandler *pHandler, void* pParam,void* pContext);
int GetTemperature(unsigned &value);
void SetACTLed(int v);
void _enable_unaligned_access(void);
void enable_MMU_and_IDCaches(void);
void emulator(void);
void PlaySoundDMA(void);
void setIP(const char *ip);
void setNM(const char *nm);
void setGW(const char *gw);
void setDNS(const char *dns);
void mem_stat(const char *func, std::string &mem, bool verb = false);

extern "C" {
	void kernel_main(unsigned int r0, unsigned int r1, unsigned int atags);
	void UpdateScreen(void);
}
void DisplayMessage(int x, int y, bool LCD, const char* message, u32 textColour, u32 backgroundColour);

/* I2C provided by Circle */
#define RPI_I2CInit i2c_init
#define RPI_I2CSetClock i2c_setclock
#define RPI_I2CRead i2c_read
#define RPI_I2CWrite i2c_write
#define RPI_I2CScan i2c_scan
void i2c_init(int BSCMaster, int fast);
void i2c_setclock(int BSCMaster, int clock_freq);
int i2c_read(int BSCMaster, unsigned char slaveAddress, void* buffer, unsigned count);
int i2c_write(int BSCMaster, unsigned char slaveAddress, void* buffer, unsigned count);
int i2c_scan(int BSCMaster, unsigned char slaveAddress);

extern u32 _ctb;
inline void time_fn_arm(void) { DisableIRQs(); _ctb = read32(ARM_SYSTIMER_CLO); /* Kernel.get_clock_ticks(); */ }
inline void time_fn_eval(u32 ms_threshold, const char *t)
{
	unsigned delta, _cta;
	_cta = read32 (ARM_SYSTIMER_CLO); //Kernal.get_clock_ticks();
	EnableIRQs();
	if ((delta = (_cta - _ctb)) > ms_threshold)
		Kernel.log("%s: delta = %d", t, delta);
}
#endif /* __legacy_wrappers_h__ */
