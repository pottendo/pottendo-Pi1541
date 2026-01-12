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
#include "circle-kernel.h"

#include <stdio.h>
#include <cstring>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <circle/timer.h>
#include <circle/startup.h>
#include <circle/cputhrottle.h>
#include <circle/memory.h>
#include <circle/net/ntpdaemon.h>
#include <circle/timer.h>
#include <iostream>
#include <sstream>
#include <circle/usb/usbmassdevice.h>
#include "options.h"
#include "webserver.h"
#include "version.h"
#include <list>

#define _DRIVE		"SD:"
#define _FIRMWARE_PATH	_DRIVE "/firmware/"		// firmware files must be provided here
#define _CONFIG_FILE	_DRIVE "/wpa_supplicant.conf"


CCPUThrottle CPUThrottle;
int reboot_req = 0;
extern CKernel Kernel;
extern Options options;

static u8 IPAddress[]      = {192, 168, 188, 31};
static u8 NetMask[]        = {255, 255, 255, 0};
static u8 DefaultGateway[] = {192, 168, 188, 1};
static u8 DNSServer[]      = {192, 168, 188, 1};
void parse_netaddr(const char *ip, u8 &a1, u8 &b1, u8 &c1, u8 &d1)
{
	std::stringstream s(ip);
	char ch; //to temporarily store the '.'
	int a, b, c, d;
	s >> a >> ch >> b >> ch >> c >> ch >> d;
	a1 = a; b1 = b; c1 = c; d1 = d;
}

void setIP(const char *ip)
{
	DEBUG_LOG("%s: static IP = %s", __FUNCTION__, ip);
	parse_netaddr(ip, IPAddress[0], IPAddress[1], IPAddress[2], IPAddress[3]);
}

void setNM(const char *nm)
{
	DEBUG_LOG("%s: static NetMask = %s", __FUNCTION__, nm);
	parse_netaddr(nm, NetMask[0], NetMask[1], NetMask[2], NetMask[3]);
}

void setGW(const char *gw)
{
	DEBUG_LOG("%s: static DefaultGateway = %s", __FUNCTION__, gw);
	parse_netaddr(gw, DefaultGateway[0], DefaultGateway[1], DefaultGateway[2], DefaultGateway[3]);
}

void setDNS(const char *dns)
{
	DEBUG_LOG("%s: static DNSServer = %s", __FUNCTION__, dns);
	parse_netaddr(dns, DNSServer[0], DNSServer[1], DNSServer[2], DNSServer[3]);
}

void mem_stat(const char *func, std::string &mem, bool verb)
{
	static size_t old = 0;
	CMemorySystem *ms;
	ms = CMemorySystem::Get();
	if (verb) 
	{
		static char tmp[256];
#if AARCH == 32		
		snprintf(tmp, 255, "Memory: %dkB/%dkB)", 
			ms->GetHeapFreeSpace(HEAP_ANY) / 1024, ms->GetMemSize() / 1024);
#else	
		snprintf(tmp, 255, "Memory: %ldkB/%ldkB", 
			ms->GetHeapFreeSpace(HEAP_ANY) / 1024, ms->GetMemSize() / 1024);
#endif	
		mem = std::string(tmp);
		return;
	}
	DEBUG_LOG("%s: Memory delta: %d, heap = %d", func, ms->GetHeapFreeSpace(HEAP_ANY) - old, ms->GetHeapFreeSpace(HEAP_ANY));
	old = ms->GetHeapFreeSpace(HEAP_ANY);
}

#ifdef HEAP_DEBUG	
void mem_heapinit(void)
{
	std::list<char *> ptrs;
	u32 s_nBucketSize[] = { HEAP_BLOCK_BUCKET_SIZES };
	unsigned nBuckets = sizeof s_nBucketSize / sizeof s_nBucketSize[0];
	u32 *pre_alloc = new u32[nBuckets];
	DEBUG_LOG("%s: #buckets = %d", __FUNCTION__, nBuckets);
	CMemorySystem::DumpStatus();
	for (unsigned i = 0; i < nBuckets; i++)
	{
		if (s_nBucketSize[i] < 100000) {
			pre_alloc[i] = 2000;
		} 
		else if (s_nBucketSize[i] < 1000000) {
			pre_alloc[i] = 50;
		}
		else if (s_nBucketSize[i] < 25000000) {
			pre_alloc[i] = 20;
		}
	}
	for (unsigned i = 0; i < nBuckets; i++)
	{
		DEBUG_LOG("%s: preallocating %d x %d", __FUNCTION__, pre_alloc[i], s_nBucketSize[i]);
		for (unsigned y = 0; y < pre_alloc[i]; y++)
		{
			char *dummy = (char *)malloc(s_nBucketSize[i]);
			if (!dummy)
			{
				DEBUG_LOG("%s: out of mem, adjust buckets: bucket=%d, pre-alloc=%d", __FUNCTION__, s_nBucketSize[i], pre_alloc[i]);
				goto out;
			}
			memset((void*)dummy, i, s_nBucketSize[i]);
			ptrs.push_back(dummy);
		}
	}
   	out:
	std::string du;
	DEBUG_LOG("%s: freeing %d ptrs", __FUNCTION__, ptrs.size());
	for (auto i: ptrs)
		free(i);
	mem_stat(__FUNCTION__, du, false);
	CMemorySystem::DumpStatus();
}
#endif	

void logHandler(void)
{
	Kernel.log_web("logHander: called");
}

CKernel::CKernel(void) :
	mScreen (mOptions.GetWidth (), mOptions.GetHeight ()),
	mTimer (&mInterrupt),
	mLogger (mOptions.GetLogLevel (), &mTimer), 
	mScheduler(),
	m_USBHCI (&mInterrupt, &mTimer, true),
	m_pKeyboard (0),
	m_EMMC (&mInterrupt, &mTimer, &m_ActLED),
	m_I2c (nullptr),
#if RASPPI <= 4
	m_PWMSoundDevice (nullptr),
#endif	
	m_WLAN (_FIRMWARE_PATH),
	m_Net(nullptr),
	m_WPASupplicant (_CONFIG_FILE),
	m_MCores(CMemorySystem::Get()),
	screen_failed(false),
	no_pwm(false),
	arch("unknown")
{
	boolean serialOK;
	
	serialOK = mSerial.Initialize (115200);
	screen_failed = !mScreen.Initialize ();
	CDevice *pTarget = m_DeviceNameService.GetDevice (mOptions.GetLogDevice (), FALSE);
	if (pTarget == 0)
	{
	    if (!screen_failed)
			pTarget = &mScreen;
	    else if (serialOK)
			pTarget = &mSerial;
	    else
			m_ActLED.Blink(5); // we're screwed for logging, tell the user by blinking
	}
	mLogger.Initialize (pTarget);
	mLogger.RegisterEventNotificationHandler(logHandler);
	if (screen_failed)
		log("screen initialization failed...  trying headless");
	strcpy(ip_address, "<n/a>");
	snprintf(pPi1541Version, 255, "pottendo-Pi1541 (%s)", PPI1541VERSION);
	version_extra[0] = '\0';
	DEBUG_LOG("%s: CKernel initialized, KERNEL_STACK_SIZE = 0x%08x, TASK_STACK_SIZE = 0x%08x", __FUNCTION__, KERNEL_STACK_SIZE, TASK_STACK_SIZE);
}

static void monitorhandler(TSystemThrottledState CurrentState, void *pParam)
{
	Kernel.log("%s - state = 0x%04x", __FUNCTION__, CurrentState);
	if (CurrentState & SystemStateUnderVoltageOccurred) {
		Kernel.log("%s: undervoltage occured...", __FUNCTION__);
	}
	if (CurrentState & SystemStateFrequencyCappingOccurred) {
		Kernel.log("%s: frequency capping occured...", __FUNCTION__);
	}
	if (CurrentState & SystemStateThrottlingOccurred) {
		Kernel.log("%s: throttling occured to %dMHz", __FUNCTION__, CPUThrottle.GetClockRate() / 1000000L);
	}
	if (CurrentState & SystemStateSoftTempLimitOccurred) {
		Kernel.log("%s: softtemplimit occured...", __FUNCTION__);
	}
}

boolean CKernel::Initialize (void) 
{
	boolean bOK;
	if (bOK = mInterrupt.Initialize ())
	mLogger.Write ("pottendo-kern", LogNotice, "Interrupt %s", bOK ? "ok" : "failed");
	if (bOK) bOK = mTimer.Initialize ();
	mLogger.Write ("pottendo-kern", LogNotice, "mTimer %s", bOK ? "ok" : "failed");
	if (bOK) bOK = m_USBHCI.Initialize ();
	mLogger.Write ("pottendo-kern", LogNotice, "USBHCI %s", bOK ? "ok" : "failed");
	bOK = true;	/* USB may not be needed */
	if (bOK) bOK = m_EMMC.Initialize ();
	mLogger.Write ("pottendo-kern", LogNotice, "EMMC %s", bOK ? "ok" : "failed");
	if (bOK) 
	{ 
		if (f_mount (&m_FileSystem, _DRIVE, 1) != FR_OK)
		{
			mLogger.Write ("pottendo-kern", LogError,
					"Cannot mount drive: %s", _DRIVE);
			bOK = FALSE;
		} else {
			mLogger.Write ("pottendo-kern", LogNotice, "mounted drive: " _DRIVE);
		}
	}
	return bOK;
}

#include "DiskCaddy.h"
#include "ScreenLCD.h"
extern ScreenLCD *screenLCD;
extern DiskCaddy diskCaddy;
extern CSpinLock core0RefreshingScreen;

TShutdownMode CKernel::Run(void)
{
	CMachineInfo *mi = CMachineInfo::Get();
	if (mi)
	{
		TMachineModel model = mi->GetMachineModel();
		switch (model) {
			case MachineModelZero2W:
				// disable PWM sound
				log("%s won't support PWM sound, disabling it", mi->GetMachineName());
				no_pwm = true;
				break;
			case MachineModel3APlus:
			case MachineModel3B:
			case MachineModel3BPlus:
			case MachineModel4B:
#if RASPPI <= 4			
				m_PWMSoundDevice = new CPWMSoundDevice(&mInterrupt);
				if (!m_PWMSoundDevice) no_pwm = true;
#endif				
				break;
			default:
				no_pwm = true;
				log ("model '%s' not tested, use at your onw risk...", mi->GetMachineName());
				MsDelay(2000);
				break;
		}
		extern unsigned versionMajor;
		extern unsigned versionMinor;
#if AARCH == 32		
		arch = "32bit";
#else
		arch = "64bit";
#endif		
		log(get_version());
	} else {
		log("GetMachinModel failed - halting system"); 
		return ShutdownHalt;
	}

	if (CPUThrottle.SetSpeed(CPUSpeedMaximum, true) != CPUSpeedUnknown)
	{	
		log("maxed freq to %dMHz", CPUThrottle.GetClockRate() / 1000000L);
	}
    unsigned tmask = SystemStateUnderVoltageOccurred | SystemStateFrequencyCappingOccurred |
					 SystemStateThrottlingOccurred | SystemStateSoftTempLimitOccurred;
	CPUThrottle.RegisterSystemThrottledHandler(tmask, monitorhandler, nullptr);
	CPUThrottle.Update();

	kernel_main(0, 0, 0);/* options will be initialized */
	new_ip = true;
	if (screen_failed)
		options.SetHeadLess(1);
	const char *pi1541HWOption = options.SplitIECLines() ? "Option B Hardware" : "Option A Hardware";
	Kernel.append2version(pi1541HWOption);
	DEBUG_LOG("%s: options selected %s", __FUNCTION__, pi1541HWOption);
	char dsp[64];
	snprintf(dsp, 63, "Display %s not found", options.I2CLcdModelName());
	if (screenLCD && i2c_scan(options.I2CBusMaster(), options.I2CLcdAddress()))
		snprintf(dsp, 63, "Display %s (I2C%d@0x%02x)", options.I2CLcdModelName(), options.I2CBusMaster(), options.I2CLcdAddress());
	append2version(dsp);

#ifdef HEAP_DEBUG
	// pre-alloc mem to track potential losses more easily
	mem_heapinit();
#endif

	// launch everything
	Kernel.launch_cores();
	logger.finished_booting("display core");
	if (options.GetHeadLess() == false)
	{
		UpdateScreen();
		DEBUG_LOG("%s: unexpected return of display thread, halting core %d", __FUNCTION__, 0);
	} 
	else 
	{
		DEBUG_LOG("%s: running headless, halting core %d...", __FUNCTION__, 0);
	}
	return ShutdownHalt;
}

logger_t logger(1000);
void CKernel::log(const char *fmt, ...)
{
    char t[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(t, 256, fmt, args);
	mLogger.Write("pi1541", LogNotice, t);
	va_end(args);
}

void CKernel::log_web(const char *t)
{
	if (options.GetNetEthernet() || options.GetNetWifi())
	{
		TLogSeverity sev;
		char src[LOG_MAX_SOURCE];
		char msg[LOG_MAX_MESSAGE];
		time_t tm;
		unsigned mtm;
		int tz;
		while (mLogger.ReadEvent(&sev, src, msg, &tm, &mtm, &tz) != FALSE)
		{
			char buf[80], buf2[84];
			struct tm *lt = localtime(&tm);
	        strftime(buf, sizeof(buf), "%b %e %X", lt);
			snprintf(buf2, sizeof(buf2), "%s.%02u", buf, mtm % 1000);
			logger.log((std::string(src) + ":" + std::string(msg)).c_str(), buf2);
		}
	}
}

boolean CKernel::init_screen(u32 widthDesired, u32 heightDesired, u32 colourDepth, u32 &width, u32 &height, u32 &bpp, u32 &pitch, u8** framebuffer)
{
	if (screen_failed) return false;
#if RASPPI < 5	
	Kernel.log("init_screen desired for %dx%dx%d", widthDesired, heightDesired, colourDepth);
	width = mScreen.GetWidth();
	height = mScreen.GetHeight();
	Kernel.log("HW screen reports %dx%d", width, height);
	if (mScreen.GetFrameBuffer() == nullptr)
		return false;
	*framebuffer = (u8 *)(mScreen.GetFrameBuffer());
	bpp = mScreen.GetFrameBuffer()->GetDepth();
	pitch = mScreen.GetFrameBuffer()->GetPitch();
	Kernel.log("bpp=%d, pitch=%d, fb=%p", bpp, pitch, *framebuffer);
	if (!pitch) return false;
#endif	
	return true;
}

bool CKernel::run_ethernet(void)
{
	bool bOK;
	int retry;
	if (m_Net) 
	{
		log("%s: cleaning up network stack...", __FUNCTION__);
		delete m_Net;
	}
	Kernel.log("Initializing ethernet network");
	if (!options.GetDHCP())
		m_Net = new CNetSubSystem(IPAddress, NetMask, DefaultGateway, DNSServer, DEFAULT_HOSTNAME, NetDeviceTypeEthernet);
	else
		m_Net = new CNetSubSystem(0, 0, 0, 0, DEFAULT_HOSTNAME, NetDeviceTypeEthernet);
	bOK = m_Net->Initialize(FALSE);
	if (!bOK) {
		log("couldn't start ethernet network...waiting 1s"); 
		mScheduler.MsSleep (1 * 1000);
		return bOK;
	}
	retry = 501;	// try for 500*100ms, same as Wifi
	while (--retry && !m_Net->IsRunning()) 
		mScheduler.MsSleep(100);
	return (retry != 0);
}

bool CKernel::run_wifi(void) 
{
	if (m_Net)
	{
		log("%s: cleaning up network stack...", __FUNCTION__);
		delete m_Net; m_Net = nullptr;
	}
	if (!options.GetDHCP())
		m_Net = new CNetSubSystem(IPAddress, NetMask, DefaultGateway, DNSServer, DEFAULT_HOSTNAME, NetDeviceTypeWLAN);
	else
		m_Net = new CNetSubSystem(0, 0, 0, 0, DEFAULT_HOSTNAME, NetDeviceTypeWLAN);
	if (!m_Net) return false;
	bool bOK = true;
	if (bOK) bOK = m_WLAN.Initialize();
	if (bOK) bOK = m_Net->Initialize(FALSE);
	if (bOK) bOK = m_WPASupplicant.Initialize();
	if (!bOK) {
		log("couldn't start wifi network...waiting 5s"); 
		mScheduler.MsSleep (5 * 1000);	
	}
	return bOK;
}

static void display_temp(void)
{
	unsigned temp = 0;
	RGBA BkColour = RGBA(0, 0, 0, 0xFF);
	RGBA TextColour = RGBA(0xff, 0xff, 0xff, 0xff);
	GetTemperature(temp);
#if 1
	char buf[128];
	sprintf(buf, " %dC", temp / 1000);
	core0RefreshingScreen.Acquire();
	screenLCD->PrintText(false, 8 * 12, 0, buf, TextColour, BkColour);
	screenLCD->SwapBuffers();
	core0RefreshingScreen.Release();
#endif	
	//DEBUG_LOG("%s: temp = %d", __FUNCTION__, temp / 1000);
}

void CKernel::run_webserver(bool isWifi) 
{
	CString IPString;
	while (!m_Net->IsRunning())
	{
		log("webserver waits for network... waiting 5s");
		mScheduler.MsSleep (5000);
	}
	m_Net->GetConfig()->GetIPAddress()->Format(&IPString);
	strcpy(ip_address, (const char *) IPString);
	log ("Open \"http://%s/\" in your web browser!", ip_address);
	new_ip = true;
	mScheduler.MsSleep (1000);/* wait a bit, LCD output */
	DisplayMessage(0, 24, true, (const char*) IPString, 0xffffff, 0x0);
	if (isWifi)
		DEBUG_LOG("%s: Wifi IsConnected = %d", __FUNCTION__, m_WPASupplicant.IsConnected());

	if (mTimer.SetTimeZone(options.GetTZ() * 60))
		DEBUG_LOG("%s: timezone set '%.0f'mins vs. UTC", __FUNCTION__, options.GetTZ() * 60.0);
	else
		DEBUG_LOG("%s: setting of timezone '%.0f'mins vs. UTC failed", __FUNCTION__, options.GetTZ() * 60.0);
	{
		CNTPDaemon CNTPDaemon("152.53.132.244", m_Net);
		unsigned max_cs = options.GetMaxContentSize();
		unsigned max_mps = options.GetMaxMultipartSize();
		if (max_cs < 256) { 
			max_cs = 256;
			DEBUG_LOG("%s: overruled maxContentSize to %d", __FUNCTION__, max_cs);
		}
		if (max_cs > 4000) {
			max_cs = 4000;
			DEBUG_LOG("%s: overruled maxContentSize to %d", __FUNCTION__, max_cs);
		}
		if (max_mps < 1000) { 
			max_mps = 1000;
			DEBUG_LOG("%s: overruled maxMultipartSize to %d", __FUNCTION__, max_mps);
		}
		if (max_mps > 20000) {
			max_mps = 20000;
			DEBUG_LOG("%s: overruled maxMultipartSize to %d", __FUNCTION__, max_mps);
		}
		DEBUG_LOG("%s: launching webserver with: maxContentSize = %dkb, maxMultipartSize = %dkb", __FUNCTION__, max_cs, max_mps);
		CWebServer CWebServer(m_Net, &m_ActLED, 0, max_cs * 1024, max_mps * 1024);
		int temp_period = 0;
		logger.finished_booting("network core");
		while (1)
		{
			mScheduler.MsSleep(100);
			if (options.DisplayTemperature() &&
				options.GetHeadLess() &&
				!(temp_period++ % 50)) // every 5 sec, display temp on LCD
			{
				CPUThrottle.Update();
				display_temp();
			}
			if (reboot_req && reboot_req++ > 4)
			{
				log("%s: rebooting now...", __FUNCTION__);
				DisplayMessage(0, 24, true, "Rebooting.......", 0xffffff, 0x0);
				mScheduler.MsSleep(20);
				reboot_now();
			}
			//if (!m_Net->IsRunning())
			//	break;
		}
		DEBUG_LOG("%s: network down, restarting...", __FUNCTION__);
	}
}

void CKernel::i2c_init(int BSCMaster, int fast)
{
	if (m_I2c)
		delete m_I2c;

	m_I2c = new CI2CMaster(BSCMaster, fast);
	if (!m_I2c->Initialize())
	{
		log("%s: failed", __FUNCTION__);
		return;
	}
	log("%s: Master %d successfully initialized", __FUNCTION__, BSCMaster);
}

void CKernel::i2c_setclock(int BSCMaster, int clock_freq)
{
	m_I2c->SetClock(clock_freq);
}

int CKernel::i2c_read(int BSCMaster, unsigned char slaveAddress, void* buffer, unsigned count)
{
	return m_I2c->Read(slaveAddress, buffer, count);
}

int CKernel::i2c_write(int BSCMaster, unsigned char slaveAddress, void* buffer, unsigned count)
{
	return m_I2c->Write(slaveAddress, buffer, count);
}

int CKernel::i2c_scan(int BSCMaster, unsigned char slaveAddress)
{
	int found = 0;
	u8 t[1];
	if (m_I2c->Read(slaveAddress, t, 1) >= 0) 
	{
		log("identified I2C slave on address 0x%02x", slaveAddress);
		found++;
	}
	return found;
}

void KeyboardRemovedHandler(CDevice *pDevice, void *pContext) 
{
	extern bool USBKeyboardDetected;
	USBKeyboardDetected = false;
	Kernel.get_kbd()->UnregisterKeyStatusHandlerRaw();
	Kernel.set_kbd(nullptr);
	Kernel.log("keyboard removed");
}

/* Ctrl-Alt-Del*/
void KeyboardShutdownHandler(void)
{
	Kernel.log("Ctrl-Alt-Del pressed, rebooting...");
	reboot();
}

int CKernel::usb_keyboard_available(void) 
{
	//if (m_pKeyboard) return 1;
	m_pKeyboard = (CUSBKeyboardDevice *) m_DeviceNameService.GetDevice ("ukbd1", FALSE);
	if (!m_pKeyboard)
		return 0;
	m_pKeyboard->RegisterRemovedHandler(KeyboardRemovedHandler);
	m_pKeyboard->RegisterShutdownHandler(KeyboardShutdownHandler);
	return 1;
}

int CKernel::usb_massstorage_available(void)
{
	pUMSD1 = m_DeviceNameService.GetDevice ("umsd1", TRUE);
	if (pUMSD1 == 0)
	{
		log("USB mass storage device not found");
		return 0;
	}
	return 1;
}

void CKernel::run_tempmonitor(bool run)
{
#if 0	
    unsigned tmask = SystemStateUnderVoltageOccurred | SystemStateFrequencyCappingOccurred |
					 SystemStateThrottlingOccurred | SystemStateSoftTempLimitOccurred;
	CPUThrottle.RegisterSystemThrottledHandler(tmask, monitorhandler, nullptr);
	CPUThrottle.Update();
#endif	
	do {
		if (CPUThrottle.SetOnTemperature() == false)
			log("temperature monitor failed...");
		MsDelay(5 * 1000);
		if (!CPUThrottle.Update())
			log("CPUThrottle Update failed");
		log("Temp %dC (max=%dC), dynamic adaption%spossible, current freq = %dMHz (max=%dMHz)", 
			CPUThrottle.GetTemperature(),
			CPUThrottle.GetMaxTemperature(), 
			CPUThrottle.IsDynamic() ? " " : " not ",
			CPUThrottle.GetClockRate() / 1000000L, 
			CPUThrottle.GetMaxClockRate() / 1000000L);
		CMemorySystem::Get()->DumpStatus();
	} while (run);
}

TKernelTimerHandle CKernel::timer_start(unsigned delay, TKernelTimerHandler *pHandler, void *pParam, void *pContext)
{
	return mTimer.StartKernelTimer(delay, pHandler, pParam, pContext);
}

#include "sample.h"
//extern const long int Sample_bin_size;
//extern const unsigned char *Sample_bin;
void CKernel::playsound(void)
{
	if (no_pwm) return;
#if RASPPI <= 4
	if (m_PWMSoundDevice->PlaybackActive())
		return;
	m_PWMSoundDevice->Playback ((void *)Sample_bin, Sample_bin_size, 1, 8);
#endif	
}

char *CKernel::get_version(void)
{
	extern unsigned int versionMajor, versionMinor;
	CMachineInfo *mi = CMachineInfo::Get();
	int rev = mi->GetModelRevision();
	snprintf(pPi1541Version, 255, "pottendo-Pi1541 (%s, %s), Pi1541 V%d.%02d on %s/Rev%d@%ldMHz%s", 
		PPI1541VERSION, arch, versionMajor, versionMinor, mi->GetMachineName(), rev, CPUThrottle.GetClockRate() / 1000000L, version_extra);
	return pPi1541Version;
}

void Pi1541Cores::Run(unsigned int core)			/* Virtual method */
{
	int i = 0;
	switch (core) {
		case 1:
		Kernel.log("%s: emulator on core %d", __FUNCTION__, core);
		logger.finished_booting("emulator core");
		emulator();
		break;
	case 2:
#if RASPPI >= 3
		if (!options.GetNetWifi() && !options.GetNetEthernet()) goto out;
		do
		{
			bool run_wifi;

			run_wifi = false;
			DEBUG_LOG("%s: DHCP %s", __FUNCTION__, options.GetDHCP() ? "enabled" : "disabled");
			if (options.GetNetEthernet()) // cable network has priority over Wifi
			{
				if (!Kernel.run_ethernet())
				{
					Kernel.log("setup ethernet failed");
					i = 0;
				}
				else
					i = 1;
			}
			if ((i == 0) && options.GetNetWifi())
			{
				i = 10;
				do
				{
					Kernel.log("attempt %d to launch WiFi on core %d", 11 - i, core);
				} while (i-- && !Kernel.run_wifi());
				run_wifi = true;
			}
			if (i == 0)
			{
				Kernel.log("network setup failed, retrying in 5s");
				MsDelay(5 * 1000);
			}
			else
			{
				Kernel.log("launching webserver on core %d", core);
				Kernel.run_webserver(run_wifi);
			}
		} while (true);
		out:
#endif	
		Kernel.log("disabling network support on core %d", core);
		logger.finished_booting("network core - disabled");
		if (options.DisplayTemperature())
		{	
			DEBUG_LOG("%s: displaying temperature on LCD", __FUNCTION__);
			while (1)
			{
				display_temp();
				MsDelay(5000);
			}
		}
		break;
	case 3:	/* health monitoring */
		logger.finished_booting("system monitor core");
		if (options.GetHealthMonitor() == 1)
			Kernel.log("disabling health monitoring on core %d", core);
		else
		{
			Kernel.log("launching system monitoring on core %d", core);
			Kernel.run_tempmonitor();
		}
		break;
	default:
		break;
	}
	Kernel.log("%s: halting core %d", __FUNCTION__, core);
	halt();	// whenever a core function returns, we halt the core.
}

