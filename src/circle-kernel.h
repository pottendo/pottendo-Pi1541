//
// kernel.h
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
#ifndef _circle_kernel_h
#define _circle_kernel_h

#include <circle/multicore.h>
#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/types.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/sched/scheduler.h>
#include <SDCard/emmc.h>
#include <fatfs/ff.h>
#include <wlan/bcm4343.h>
#include <wlan/hostap/wpa_supplicant/wpasupplicant.h>
#include <circle/net/netsubsystem.h>
#include <stdio.h>

enum TShutdownMode
{
	ShutdownNone,
	ShutdownHalt,
	ShutdownReboot
};

class Pi1541Cores : public CMultiCoreSupport {
public:
	Pi1541Cores(CMemorySystem *pMemorySystem) : CMultiCoreSupport(pMemorySystem) {}
	virtual ~Pi1541Cores(void) = default;

	virtual void Run(unsigned int); 
};

class CKernel
{
public:
	CKernel (void);
	boolean Initialize (void);
	TShutdownMode Run (void);
	int Cleanup (void) { return 0; }
	inline void set_pixel(unsigned int x, unsigned int y, TScreenColor c) { mScreen.SetPixel(x, y, c); }

	CInterruptSystem *get_isys(void) { return &mInterrupt; }
	CTimer *get_timer(void) { return &mTimer; }
	CLogger *get_logger(void) { return &mLogger; }
	CScreenDevice *get_scrdevice(void) { return &mScreen; }
    CEMMCDevice &get_emmc(void) { return m_EMMC; }

	void blink(int n) { m_ActLED.Blink(n); }
	void tlog(int i) { char x[1024]; sprintf(x, "0x%08x", i); mLogger.Write("tlog:", LogNotice, x); }
	void log(const char *fmt, ...);
	void SetACTLed(int v) { if (v) m_ActLED.On(); else m_ActLED.Off(); }
	boolean init_screen(u32 widthDesired, u32 heightDesired, u32 colourDepth, 
						u32 &width, u32 &height, u32 &bpp, u32 &pitch, u8** framebuffer);
	void launch_cores(void) { m_MCores.Initialize(); }
	void yield(void) { mScheduler.Yield(); }
	void run_wifi(void);
	void run_webserver(void);

private:
	CActLED				m_ActLED;
	CKernelOptions		mOptions;
	CDeviceNameService	mDeviceNameService;
	CScreenDevice		mScreen;
	CSerialDevice		mSerial;
	CExceptionHandler	mExceptionHandler;
	CInterruptSystem	mInterrupt;
	CTimer				mTimer;
	CLogger				mLogger;
	CScheduler			mScheduler;
	CUSBHCIDevice		m_USBHCI;
	CEMMCDevice			m_EMMC;
	FATFS				m_FileSystem;
	CBcm4343Device		m_WLAN;
	CSynchronizationEvent	mEvent;
	CNetSubSystem		m_Net;
	CWPASupplicant		m_WPASupplicant;
	Pi1541Cores		 	m_MCores;
};

extern CKernel Kernel;

void reboot_now(void);
static inline void delay_us(u32 usec) { Kernel.get_timer()->usDelay(usec); }
static inline void usDelay(u32 usec) { Kernel.get_timer()->usDelay(usec); }
static inline void MsDelay(u32 msec) { Kernel.get_timer()->usDelay(1000 * msec); }

void USPiInitialize(void);
void TimerSystemInitialize(void);
void InterruptSystemInitialize(void);
int USPiMassStorageDeviceAvailable(void);
int USPiKeyboardAvailable(void);
void USPiKeyboardRegisterKeyStatusHandlerRaw(void *fn);
void TimerCancelKernelTimer(unsigned hTimer);
unsigned TimerStartKernelTimer(
		unsigned nDelay,		// in HZ units
		void* pHandler,
		void* pParam,
		void* pContext);
int GetTemperature(unsigned &value);
void SetACTLed(int v);
void _enable_unaligned_access(void);
void enable_MMU_and_IDCaches(void);
void InitialiseLCD();
void UpdateLCD(const char* track, unsigned temperature);
void DisplayI2CScan(int y_pos);
void emulator(void);

extern "C" void kernel_main(unsigned int r0, unsigned int r1, unsigned int atags);
#endif

