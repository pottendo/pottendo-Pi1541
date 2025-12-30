//
// emulator.cpp
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
#include "emulator.h"
#include "iec_bus.h"
#include "ROMs.h"
#include "InputMappings.h"
#include "Screen.h"
#include "ScreenLCD.h"
#include "options.h"

extern ROMs roms;
extern InputMappings* inputMappings;
extern SpinLock core0RefreshingScreen;
extern Screen *screen;
extern Screen *screen_hdmi; 
extern ScreenLCD* screenLCD;
extern Options options;
extern int playsound;
extern int headSoundFreq;
extern int headSoundCounterDuration;
extern u8 read6502(u16 address);
extern u8 read6502ExtraRAM(u16 address);
extern void write6502(u16 address, const u8 value);
extern void write6502ExtraRAM(u16 address, const u8 value);
extern u8 read6502_1581(u16 address);
extern void write6502_1581(u16 address, const u8 value);

extern u8 read6502_dr9(u16 address);
extern u8 read6502ExtraRAM_dr9(u16 address);
extern void write6502_dr9(u16 address, const u8 value);
extern void write6502ExtraRAM_dr9(u16 address, const u8 value);
extern u8 read6502_1581_dr9(u16 address);
extern void write6502_1581_dr9(u16 address, const u8 value);
extern bool usb_mass_update;

emulator_t *emulator_instance = nullptr;
emulator_t *emulator_instance_dr9 = nullptr;
IEC_Bus *iec_bus_instance = nullptr;

SpinLock emuSpinLock;
//SpinLock drive0, drive1;
volatile int dual_drive = -1;

/* fixme's - declared twice, etc. */
#define FAST_BOOT_CYCLES 1003061
#define SNOOP_CD_CBM 0xEA2D
#define SNOOP_CD_CBM1581 0xAEB7
#define SNOOP_CD_CBM1581_JIFFY 0xE281
#define SNOOP_CD_JIFFY_BOTH 0xFC07
#define SNOOP_CD_JIFFY_BOTH_V6_00 0xFC12
#define SNOOP_CD_JIFFY_DRIVEONLY 0xEA16
static const u8 snoopBackCommand[] = {
	'C', 'D', ':', '_', '?'	//0x43, 0x44, 0x3a, 0x5f, 3f
};
static int snoopIndex = 0;
static int snoopPC = 0;
extern bool Snoop(u8 a, int max);
extern void CheckAutoMountImage(EXIT_TYPE reset_reason , FileBrowser* fileBrowser);

void xPortB_OnPortOut(void* pUserData, unsigned char status)
{
	emulator_t *emu = static_cast<emulator_t *>(pUserData);
	//DEBUG_LOG("%s: iec_bus = %p, status = %02x", __FUNCTION__, iecBus, status);
	emu->get_iec_bus()->PortB_OnPortOut(nullptr, status);
}

emulator_t::emulator_t(u8 driveNumber)
    : selectedViaIECCommands(false), 
    deviceID(driveNumber),
	diskCaddy(driveNumber),
	iec_bus(driveNumber)
{
    DEBUG_LOG("%s: emulator for drive = %d\n", __FUNCTION__, driveNumber);
	_m_IEC_Commands = new IEC_Commands(&iec_bus);

	_m_IEC_Commands->SetStarFileName(options.GetStarFileName());
	GlobalSetDeviceID(deviceID);

	pi1541.drive.SetVIA(&pi1541.VIA[1]);
	pi1541.VIA[0].GetPortB()->SetPortOut(this, xPortB_OnPortOut);

	iec_bus_instance = &iec_bus;
}

emulator_t::~emulator_t()
{
    // Destructor implementation
	DEBUG_LOG("%s: destructor called for drive = %d\n", __FUNCTION__, deviceID);
    delete _m_IEC_Commands;
}

EmulatingMode emulator_t::BeginEmulating(FileBrowser* fileBrowser, const char* filenameForIcon)
{
	DiskImage* diskImage = diskCaddy.SelectFirstImage();
	DEBUG_LOG("%s: device = %d, name = %s, IconName='%s', screenLCD = %p", __FUNCTION__, 
				get_deviceID(), diskImage->GetName(), filenameForIcon, screenLCD);
	if (diskImage)
	{
#if defined(PI1581SUPPORT)
		if (diskImage->IsD81())
		{
			pi1581.Insert(diskImage);
			fileBrowser->DisplayDiskInfo(diskImage, filenameForIcon);
			fileBrowser->ShowDeviceAndROM( roms.ROMName1581 );
			return EMULATING_1581;
		}
		else
#endif
		{
			pi1541.drive.Insert(diskImage);
			fileBrowser->DisplayDiskInfo(diskImage, filenameForIcon);
			fileBrowser->ShowDeviceAndROM();
			return EMULATING_1541;
		}
	}
	inputMappings->WaitForClearButtons();
	return IEC_COMMANDS;
}

void emulator_t::GlobalSetDeviceID(u8 id)
{
	DEBUG_LOG("%s: setting device ID to %d", __FUNCTION__, id);
	deviceID = id;
	_m_IEC_Commands->SetDeviceId(id);
	pi1541.SetDeviceID(id);
#if defined(PI1581SUPPORT)
	pi1581.SetDeviceID(id);
#endif
}

EXIT_TYPE __not_in_flash_func(emulator_t::Emulate1541) (FileBrowser* fileBrowser)
{
	EXIT_TYPE exitReason = EXIT_UNKNOWN;
	bool oldLED = false;
#if defined(ESP32)
	uint64_t ctBefore = 0, ctAfter = 0;
#else
	unsigned ctBefore = 0;
	unsigned ctAfter = 0;
#endif	
	int cycleCount = 0;
	unsigned caddyIndex;
	int headSoundCounter = 0;
	int headSoundFreqCounter = 0;
	unsigned char oldHeadDir = 0;
	int resetCount = 0;
	bool refreshOutsAfterCPUStep = true;
	unsigned numberOfImages = diskCaddy.GetNumberOfImages();
	unsigned numberOfImagesMax = numberOfImages;
	int exitCyclesRemaining = 0;

	if (numberOfImagesMax > 10)
		numberOfImagesMax = 10;

#if not defined(EXPERIMENTALZERO)
	core0RefreshingScreen.Acquire();
#endif
	diskCaddy.Display();
#if not defined(EXPERIMENTALZERO)
	core0RefreshingScreen.Release();
#endif

	inputMappings->directDiskSwapRequest = 0;
	// Force an update on all the buttons now before we start emulation mode. 
	iec_bus.ReadBrowseMode();

	bool extraRAM = options.GetExtraRAM();
	if (get_deviceID() == 9)
	{
		emulator_instance_dr9 = this;
		DataBusReadFn dataBusRead = extraRAM ? read6502ExtraRAM_dr9 : read6502_dr9;
		DataBusWriteFn dataBusWrite = extraRAM ? write6502ExtraRAM_dr9 : write6502_dr9;
		pi1541.m6502.SetBusFunctions(dataBusRead, dataBusWrite);
		//drive0.Acquire();
	}
	else
	{
		emulator_instance = this;
		DataBusReadFn dataBusRead = extraRAM ? read6502ExtraRAM : read6502;
		DataBusWriteFn dataBusWrite = extraRAM ? write6502ExtraRAM : write6502;
		pi1541.m6502.SetBusFunctions(dataBusRead, dataBusWrite);
		//drive1.Acquire();
	}

	iec_bus.VIA = &pi1541.VIA[0];
	iec_bus.port = pi1541.VIA[0].GetPortB();
	pi1541.Reset();	// will call iec_bus.Reset();

	iec_bus.LetSRQBePulledHigh();
	DEBUG_LOG("%s: Running Emulator for device %d", __FUNCTION__, get_deviceID());
	//resetWhileEmulating = false;
	selectedViaIECCommands = false;

	u32 hash = pi1541.drive.GetDiskImage()->GetHash();
	// 0x42c02586 = maniac_mansion_s1[lucasfilm_1989](ntsc).g64
	// 0x18651422 = aliens[electric_dreams_1987].g64
	// 0x2a7f4b77 = zak_mckracken_boot[activision_1988](manual)(!).g64, 0x778fecda, 0x6ab92e00 (german version)
	// 0x97732c3e = maniac_mansion_s1[activision_1987](!).g64
	// 0x63f809d2 = 4x4_offroad_racing_s1[epyx_1988](ntsc)(!).g64
	if (hash == 0x42c02586 || hash == 0x18651422 || hash == 0x2a7f4b77 || hash == 0x97732c3e || 
		hash == 0x63f809d2 || hash == 0x778fecda || hash == 0x6ab92e00 || hash == 0x3adb56b7)
	{
		refreshOutsAfterCPUStep = false;
		DEBUG_LOG("%s: .g64 hash = %x, refreshOutsAfterCPUStep = false", __FUNCTION__, hash);
	}

	// Quickly get through 1541's self test code.
	// This will make the emulated 1541 responsive to commands asap.
	// During this time we don't need to set outputs.
	while (cycleCount < FAST_BOOT_CYCLES)
	{
		iec_bus.ReadEmulationMode1541();

		pi1541.m6502.SYNC();

		pi1541.m6502.Step();

		pi1541.Update();

		cycleCount++;
	}
#if defined(__PICO2__)	
	overclock(312000);
#endif
	//DEBUG_LOG("%s: Fast booted 1541 in %d cycles", __FUNCTION__, cycleCount);

	// Self test code done. Begin realtime emulation.
	emuSpinLock.Acquire();
	dual_drive++;
	IEC_Bus::dual_drive = dual_drive;
	//dual_drive = 0;
	emuSpinLock.Release();
	while (exitReason == EXIT_UNKNOWN)
	{
#if defined(RPI2)
	asm volatile ("mrc p15,0,%0,c9,c13,0" : "=r" (ctBefore));
#else
#if defined(__CIRCLE__)
	ctBefore = Kernel.get_clock_ticks();
#elif defined(__PICO2__)
	ctBefore = time_us_32();
#elif defined(ESP32)
	ctBefore = get_ticks();
#else
	ctBefore = read32(ARM_SYSTIMER_CLO);
#endif	
#endif

#if 0 // won't work sync with this method causes delay of >5ms		
		/* sync code of 2 emulators */
		if (dual_drive && get_deviceID() == 9)
		{
			drive1.Acquire(); // wait until drive 0 has reached a later point to be able to continue
			drive0.Release(); // let drive 0 run
		}
#endif
		if (refreshOutsAfterCPUStep)
			iec_bus.ReadEmulationMode1541();
		if (pi1541.m6502.SYNC())	// About to start a new instruction.
		{
			pc = pi1541.m6502.GetPC();
			// See if the emulated cpu is executing CD:_ (ie back out of emulated image)
			if (pc == SNOOP_CD_CBM || pc == SNOOP_CD_JIFFY_BOTH || pc == SNOOP_CD_JIFFY_BOTH_V6_00 || (pc == SNOOP_CD_JIFFY_DRIVEONLY && (roms.GetHash() != 0x9eef0d97))) snoopPC = pc;

			if (pc == snoopPC)
			{

				if (Snoop(pi1541.m6502.GetA(), sizeof(snoopBackCommand)))
					exitCyclesRemaining = 40000;
			}
		}

		if (exitCyclesRemaining > 0)
		{
			exitCyclesRemaining--;
			if (exitCyclesRemaining == 0)
			{
				emulating = IEC_COMMANDS;
				exitReason = EXIT_CD;
			}
		}
		pi1541.m6502.Step();	// If the CPU reads or writes to the VIA then clk and data can change

		//To artificialy delay the outputs later into the phi2's cycle (do this on future Pis that will be faster and perhaps too fast)
		//read32(ARM_SYSTIMER_CLO);	//Each one of these is > 100ns
		//read32(ARM_SYSTIMER_CLO);
		//read32(ARM_SYSTIMER_CLO);

//		iec_bus.ReadEmulationMode1541();

		if (refreshOutsAfterCPUStep)
			iec_bus.RefreshOuts1541();	// Now output all outputs.
		iec_bus.OutputLED = pi1541.drive.IsLEDOn();

		if (iec_bus.OutputLED ^ oldLED)
		{
			SetACTLed(iec_bus.OutputLED);
			oldLED = iec_bus.OutputLED;
			iec_bus.RefreshOutLED(); /*FCP*/
		}


		// Do head moving sound
		unsigned char headDir = pi1541.drive.GetLastHeadDirection();
		if (headDir != oldHeadDir)	// Need to start a new sound?
		{
			oldHeadDir = headDir;
			if (playsound > 0)
			{
				headSoundCounter = headSoundCounterDuration;
				headSoundFreqCounter = headSoundFreq;
			}
			else
			{
#if not defined(EXPERIMENTALZERO)
				PlaySoundDMA(playsound);
#endif
			}
		}

		iec_bus.ReadGPIOUserInput(true);
#if 0 // won't work sync with this method causes delay of >5ms		
		/* sync code of 2 emulators */
		if (dual_drive && get_deviceID() == 8)
		{
			drive1.Release(); // let drive 1 run
			drive0.Acquire(); // wait until drive 0 has reached a later point to be able to continue
		}
#endif

		// Other core will check the uart (as it is slow) (could enable uart irqs - will they execute on this core?)
#if not defined(EXPERIMENTALZERO)
		inputMappings->CheckKeyboardEmulationMode(numberOfImages, numberOfImagesMax);
#endif
		inputMappings->CheckButtonsEmulationMode();

		bool exitEmulation = inputMappings->Exit();
		bool exitDoAutoLoad = inputMappings->AutoLoad();

		// We have now output so HERE is where the next phi2 cycle starts.
		pi1541.Update();


		bool reset = iec_bus.IsReset();
		if (reset)
			resetCount++;
		else
			resetCount = 0;
#if defined(__CIRCLE__)
extern bool webserver_upload;
		if (webserver_upload)
		{
			DEBUG_LOG("%s: webserver upload done.", __FUNCTION__);
			webserver_upload = false;
			exitDoAutoLoad = true;
		}
extern char mount_img[256];
extern int mount_new;
		if (mount_new == deviceID)
		{
			DEBUG_LOG("%s: mount_img = '%s'", __FUNCTION__, mount_img);
			emulating = IEC_COMMANDS;
			exitEmulation = true;
		}
#endif		
		if ((emulating == IEC_COMMANDS) || (resetCount > 10) || exitEmulation || exitDoAutoLoad)
		{
			if (reset)
				exitReason = EXIT_RESET;
			if (exitEmulation)
				exitReason = EXIT_KEYBOARD;
			if (exitDoAutoLoad)
				exitReason = EXIT_AUTOLOAD;
		}
#if defined(RPI2)
		do  // Sync to the 1MHz clock
		{
			asm volatile ("mrc p15,0,%0,c9,c13,0" : "=r" (ctAfter));
		} while ((ctAfter - ctBefore) < clockCycles1MHz);
#else
		do	// Sync to the 1MHz clock
		{
#if defined (__CIRCLE__)
			ctAfter = Kernel.get_clock_ticks();
#elif defined(__PICO2__)
			ctAfter = time_us_32();
#elif defined(ESP32)			
			ctAfter = get_ticks();
#else
			ctAfter = read32(ARM_SYSTIMER_CLO);
#endif	
			unsigned ct = ctAfter - ctBefore;
			if (ct > 1)
			{
				// If this ever occurs then we have taken too long (ie >1us) and lost a cycle.
				// Cycle accuracy is now in jeopardy. If this occurs during critical communication loops then emulation can fail!
//#if defined(__PICO2__) || defined(ESP32)
				DEBUG_LOG("Drive %d lost cycles: cycle time = %dus", get_deviceID(), ct);
//#endif				
				//delay(1000 * 10);
				//sprintf(tempBuffer, "-%d-", ct);
				//DisplayMessage(0, 20, true, tempBuffer, RGBA(255, 255, 255, 255), RGBA(0,0,0,0));
			}
		} while (ctAfter == ctBefore);
#endif
		ctBefore = ctAfter;
		
		if (!refreshOutsAfterCPUStep)
		{
			iec_bus.ReadEmulationMode1541();
			iec_bus.RefreshOuts1541();	// Now output all outputs.
		}

		if ((playsound > 0) && headSoundCounter > 0)
		{
			headSoundFreqCounter--;		// Continue updating a GPIO non DMA sound.
			if (headSoundFreqCounter <= 0)
			{
				headSoundFreqCounter = headSoundFreq;
				headSoundCounter -= headSoundFreq * 8;
				if (headSoundCounter <= 0)
					iec_bus.OutputSound = 0;
				else
					iec_bus.OutputSound = !iec_bus.OutputSound;
				iec_bus.RefreshOutSound(); /*FCP*/
			}
		}

		if (numberOfImages > 1)
		{
			bool nextDisk = inputMappings->NextDisk();
			bool prevDisk = inputMappings->PrevDisk();
			if (nextDisk)
			{
				pi1541.drive.Insert(diskCaddy.PrevDisk());
#if defined(EXPERIMENTALZERO)
				diskCaddy.Update();
#endif
#if defined(__CIRCLE__)
				if (options.GetHeadLess())
					diskCaddy.Update();
#endif
			}
			else if (prevDisk)
			{
				pi1541.drive.Insert(diskCaddy.NextDisk());
#if defined(EXPERIMENTALZERO)
				diskCaddy.Update();
#endif
#if defined(__CIRCLE__)
				if (options.GetHeadLess())
					diskCaddy.Update();
#endif
			}
#if not defined(EXPERIMENTALZERO)
			else if (inputMappings->directDiskSwapRequest != 0)
			{
				for (caddyIndex = 0; caddyIndex < numberOfImagesMax; ++caddyIndex)
				{
					if (inputMappings->directDiskSwapRequest & (1 << caddyIndex))
					{
						DiskImage* diskImage = diskCaddy.SelectImage(caddyIndex);
						if (diskImage && diskImage != pi1541.drive.GetDiskImage())
						{
							pi1541.drive.Insert(diskImage);
							break;
						}
					}
				}
				inputMappings->directDiskSwapRequest = 0;
			}
#endif
		}
	}
	//if (get_deviceID() == 8) drive1.Release();
	//if (get_deviceID() == 9) drive0.Release();
	emuSpinLock.Acquire();
	dual_drive--;
	IEC_Bus::dual_drive = dual_drive;
	emuSpinLock.Release();
	return exitReason;
}

#if defined(PI1581SUPPORT)
EXIT_TYPE emulator_t::Emulate1581(FileBrowser* fileBrowser)
{
	EXIT_TYPE exitReason = EXIT_UNKNOWN;
	bool oldLED = false;
	unsigned ctBefore = 0;
	unsigned ctAfter = 0;
	int cycleCount = 0;
	unsigned caddyIndex;
	int headSoundCounter = 0;
	int headSoundFreqCounter = 0;
	unsigned int oldTrack = 0;
	int resetCount = 0;
	int exitCyclesRemaining = 0;

	unsigned numberOfImages = diskCaddy.GetNumberOfImages();
	unsigned numberOfImagesMax = numberOfImages;
	if (numberOfImagesMax > 10)
		numberOfImagesMax = 10;

#if not defined(EXPERIMENTALZERO)
	core0RefreshingScreen.Acquire();
#endif
	diskCaddy.Display();
#if not defined(EXPERIMENTALZERO)
	core0RefreshingScreen.Release();
#endif

	inputMappings->directDiskSwapRequest = 0;
	// Force an update on all the buttons now before we start emulation mode. 
	iec_bus.ReadBrowseMode();

	DataBusReadFn dataBusRead = read6502_1581;
	DataBusWriteFn dataBusWrite = write6502_1581;
	pi1581.m6502.SetBusFunctions(dataBusRead, dataBusWrite);

	iec_bus.CIA = &pi1581.CIA;
	iec_bus.port = pi1581.CIA.GetPortB();
	pi1581.Reset();	// will call iec_bus.Reset();

#if defined(RPI2)
	asm volatile ("mrc p15,0,%0,c9,c13,0" : "=r" (ctBefore));
#else
#if defined (__CIRCLE__)
	ctBefore = Kernel.get_clock_ticks();
#else
	ctBefore = read32(ARM_SYSTIMER_CLO);
#endif	
#endif

	//resetWhileEmulating = false;
	selectedViaIECCommands = false;

	oldTrack = pi1581.wd177x.GetCurrentTrack();

	while (exitReason == EXIT_UNKNOWN)
	{
		iec_bus.ReadEmulationMode1581();

		for (int cycle2MHz = 0; cycle2MHz < 2; ++cycle2MHz)
		{
			if (pi1581.m6502.SYNC())	// About to start a new instruction.
			{
				pc = pi1581.m6502.GetPC();
				// See if the emulated cpu is executing CD:_ (ie back out of emulated image)
				if (pc == SNOOP_CD_CBM1581 || pc == SNOOP_CD_CBM1581_JIFFY) snoopPC = pc;

				if (pc == snoopPC)
				{
					if (Snoop(pi1581.m6502.GetA(), sizeof(snoopBackCommand) - 1))
						exitCyclesRemaining = 40000;
				}
			}

			if (exitCyclesRemaining > 0)
			{
				exitCyclesRemaining--;
				if (exitCyclesRemaining == 0)
				{
					emulating = IEC_COMMANDS;
					exitReason = EXIT_CD;
				}
			}

			pi1581.m6502.Step();
			pi1581.Update();
		}

		iec_bus.RefreshOuts1581();	// Now output all outputs.

		iec_bus.OutputLED = pi1581.IsLEDOn();

		if (iec_bus.OutputLED ^ oldLED)
		{
			SetACTLed(iec_bus.OutputLED);
			oldLED = iec_bus.OutputLED;
			iec_bus.RefreshOutLED(); /*FCP*/
		}


		// Do head moving sound
		unsigned int track = pi1581.wd177x.GetCurrentTrack();
		if (track != oldTrack)	// Need to start a new sound?
		{
			oldTrack = track;
			if (playsound > 0)
			{
				headSoundCounter = headSoundCounterDuration;
				headSoundFreqCounter = headSoundFreq;
			}
			else
			{
#if not defined(EXPERIMENTALZERO)
				PlaySoundDMA(playsound);
#endif
			}
		}

		iec_bus.ReadGPIOUserInput(true);

		// Other core will check the uart (as it is slow) (could enable uart irqs - will they execute on this core?)
#if not defined(EXPERIMENTALZERO)
		inputMappings->CheckKeyboardEmulationMode(numberOfImages, numberOfImagesMax);
#endif
		inputMappings->CheckButtonsEmulationMode();

		bool exitEmulation = inputMappings->Exit();
		bool exitDoAutoLoad = inputMappings->AutoLoad();


		bool reset = iec_bus.IsReset();
		if (reset)
			resetCount++;
		else
			resetCount = 0;
#if defined(__CIRCLE__)
		extern bool webserver_upload;
		if (webserver_upload)
		{
			DEBUG_LOG("%s: webserver upload done.", __FUNCTION__);
			webserver_upload = false;
			exitDoAutoLoad = true;
		}
		extern char mount_img[256];
		extern int mount_new;
		if (mount_new == deviceID)
		{
			DEBUG_LOG("%s: mount_img = '%s'", __FUNCTION__, mount_img);
			emulating = IEC_COMMANDS;
			exitEmulation = true;
		}
#endif
		if ((emulating == IEC_COMMANDS) || (resetCount > 10) || exitEmulation || exitDoAutoLoad)
		{
			if (reset)
				exitReason = EXIT_RESET;
			if (exitEmulation)
				exitReason = EXIT_KEYBOARD;
			if (exitDoAutoLoad)
				exitReason = EXIT_AUTOLOAD;

		}

#if defined(RPI2)
		do  // Sync to the 1MHz clock
		{
			asm volatile ("mrc p15,0,%0,c9,c13,0" : "=r" (ctAfter));
		} while ((ctAfter - ctBefore) < clockCycles1MHz);
#else
		do	// Sync to the 1MHz clock
		{
#if defined (__CIRCLE__)
			ctAfter = Kernel.get_clock_ticks();
#else
			ctAfter = read32(ARM_SYSTIMER_CLO);
#endif	
			unsigned ct = ctAfter - ctBefore;
			if (ct > 1)
			{
				// If this ever occurs then we have taken too long (ie >1us) and lost a cycle.
				// Cycle accuracy is now in jeopardy. If this occurs during critical communication loops then emulation can fail!
				//DEBUG_LOG("!");
			}
		} while (ctAfter == ctBefore);
#endif
		ctBefore = ctAfter;

		if ((playsound > 0) && headSoundCounter > 0)
		{
			headSoundFreqCounter--;		// Continue updating a GPIO non DMA sound.
			if (headSoundFreqCounter <= 0)
			{
				headSoundFreqCounter = headSoundFreq;
				headSoundCounter -= headSoundFreq * 8;
				if (headSoundCounter <= 0)
					iec_bus.OutputSound = 0;
				else
					iec_bus.OutputSound = !iec_bus.OutputSound;
				iec_bus.RefreshOutSound(); /*FCP*/
			}
		}

		if (numberOfImages > 1)
		{
			bool nextDisk = inputMappings->NextDisk();
			bool prevDisk = inputMappings->PrevDisk();
			if (nextDisk)
			{
				pi1581.Insert(diskCaddy.PrevDisk());
#if defined(EXPERIMENTALZERO)
				diskCaddy.Update();
#endif
#if defined(__CIRCLE__)
				if (options.GetHeadLess())
					diskCaddy.Update();
#endif
			}
			else if (prevDisk)
			{
				pi1581.Insert(diskCaddy.NextDisk());
#if defined(EXPERIMENTALZERO)
				diskCaddy.Update();
#endif
#if defined(__CIRCLE__)
				if (options.GetHeadLess())
					diskCaddy.Update();
#endif
			}
#if not defined(EXPERIMENTALZERO)
			else if (inputMappings->directDiskSwapRequest != 0)
			{
				for (caddyIndex = 0; caddyIndex < numberOfImagesMax; ++caddyIndex)
				{
					if (inputMappings->directDiskSwapRequest & (1 << caddyIndex))
					{
						DiskImage* diskImage = diskCaddy.SelectImage(caddyIndex);
						if (diskImage && diskImage != pi1581.GetDiskImage())
						{
							pi1581.Insert(diskImage);
							break;
						}
					}
				}
				inputMappings->directDiskSwapRequest = 0;
			}
#endif
		}

	}
	return exitReason;
}
#endif

void __not_in_flash_func(emulator_t::run_emulator)(void)
{
#if not defined(EXPERIMENTALZERO)
	Keyboard* keyboard = Keyboard::Instance();
#endif
	EXIT_TYPE exitReason = EXIT_UNKNOWN;
	bool shutdown = false;
	roms.lastManualSelectedROMIndex = 0;
	diskCaddy.SetScreen(screen, screenLCD, &roms);
	fileBrowser = new FileBrowser(inputMappings, &diskCaddy, &roms, &deviceID, options.DisplayPNGIcons(), screen, screenLCD, options.ScrollHighlightRate());
	pi1541.Initialise();

	_m_IEC_Commands->SetAutoBootFB128(options.AutoBootFB128());
	_m_IEC_Commands->Set128BootSectorName(options.Get128BootSectorName());
	_m_IEC_Commands->SetLowercaseBrowseModeFilenames(options.LowercaseBrowseModeFilenames());
	_m_IEC_Commands->SetNewDiskType(options.GetNewDiskType());
	_m_IEC_Commands->SetCDSlashSlashToRoot(options.CDSlashSlashToRoot() != 0);

	emulating = IEC_COMMANDS;
	DEBUG_LOG("%s: Drive %d, enter main emulation loop", __FUNCTION__, deviceID);
	while (!shutdown)
	{
		if (emulating == IEC_COMMANDS)
		{
			iec_bus.VIA = 0;
			iec_bus.CIA = 0;
			iec_bus.port = 0;

			iec_bus.Reset();

			iec_bus.LetSRQBePulledHigh();
#if not defined(EXPERIMENTALZERO)
			core0RefreshingScreen.Acquire();
#endif
			iec_bus.WaitMicroSeconds(100);

			roms.ResetCurrentROMIndex();
			fileBrowser->ClearScreen();

			fileBrowserSelectedName = 0;
			fileBrowser->ClearSelections();

			fileBrowser->RefeshDisplay(); // Just redisplay the current folder.
#if not defined(EXPERIMENTALZERO)
			core0RefreshingScreen.Release();
#endif
			selectedViaIECCommands = false;

			inputMappings->Reset();
#if not defined(EXPERIMENTALZERO)
			inputMappings->SetKeyboardBrowseLCDScreen(screenLCD && options.KeyboardBrowseLCDScreen());
#endif
			fileBrowser->ShowDeviceAndROM();
#if defined (__CIRCLE__)
			if (usb_mass_update)
			{
				fileBrowser->PopFolder();
				usb_mass_update = false;
			}
#endif
			if (!options.GetDisableSD2IECCommands())
			{
				_m_IEC_Commands->SimulateIECBegin();

				CheckAutoMountImage(exitReason, fileBrowser);

				while (emulating == IEC_COMMANDS)
				{
					IEC_Commands::UpdateAction updateAction = _m_IEC_Commands->SimulateIECUpdate();

					switch (updateAction)
					{
						case IEC_Commands::RESET:
							if (options.GetOnResetChangeToStartingFolder())
								fileBrowser->DisplayRoot();
							iec_bus.Reset();
							_m_IEC_Commands->SimulateIECBegin();
							CheckAutoMountImage(EXIT_UNKNOWN, fileBrowser);
							break;
						case IEC_Commands::NONE:
							fileBrowser->Update();
							// Check selections made via FileBrowser
							if (fileBrowser->SelectionsMade())
								emulating = BeginEmulating(fileBrowser, fileBrowser->LastSelectionName());
							break;
						case IEC_Commands::IMAGE_SELECTED:
							// Check selections made via IEC commands (like fb64)

							fileBrowserSelectedName = _m_IEC_Commands->GetNameOfImageSelected();

							if (DiskImage::IsLSTExtention(fileBrowserSelectedName))
							{
								if (fileBrowser->SelectLST(fileBrowserSelectedName))
								{
									emulating = BeginEmulating(fileBrowser, fileBrowserSelectedName);
								}
								else
								{
									_m_IEC_Commands->Reset();
									fileBrowserSelectedName = 0;
								}
							}
							else if (DiskImage::IsDiskImageExtention(fileBrowserSelectedName))
							{
								const FILINFO* filInfoSelected = _m_IEC_Commands->GetImageSelected();
								DEBUG_LOG("IEC mounting %s\r\n", filInfoSelected->fname);
								bool readOnly = (filInfoSelected->fattrib & AM_RDO) != 0;

								if (diskCaddy.Insert(filInfoSelected, readOnly))
								{
									emulating = BeginEmulating(fileBrowser, filInfoSelected->fname);
								}
								else
								{
									DEBUG_LOG("no mount\r\n");
									_m_IEC_Commands->MountFailed();
									fileBrowserSelectedName = 0;
								}
							}
							else
							{
								fileBrowserSelectedName = 0;
							}

							if (fileBrowserSelectedName == 0)
								_m_IEC_Commands->Reset();

							selectedViaIECCommands = true;
							break;
						case IEC_Commands::DIR_PUSHED:
							fileBrowser->FolderChanged();
							break;
						case IEC_Commands::POP_DIR:
							fileBrowser->PopFolder();
							break;
						case IEC_Commands::POP_TO_ROOT:
							fileBrowser->DisplayRoot();
							break;
						case IEC_Commands::REFRESH:
							fileBrowser->FolderChanged();
							break;
						case IEC_Commands::DEVICEID_CHANGED:
							GlobalSetDeviceID( _m_IEC_Commands->GetDeviceId() );
							fileBrowser->ShowDeviceAndROM();
							break;
						case IEC_Commands::DEVICE_SWITCHED:
							DEBUG_LOG("DECIVE_SWITCHED\r\n");
							fileBrowser->DeviceSwitched();
							break;
						default:
							break;
					}
#if defined(__CIRCLE__)
extern char mount_img[256];
extern char mount_path[256];
extern int mount_new;
extern int drive_ctrl;
					FILINFO fi;
					if (mount_new)
					{
						DEBUG_LOG("%s: webserver requests to mount in dir '%s' the img '%s'", __FUNCTION__, mount_path, mount_img);
						if (f_chdir(mount_path) != FR_OK)
							DEBUG_LOG("%s: chdir to '%s' failed", __FUNCTION__, mount_path);
						else if (drive_ctrl == 1)
						{

							fileBrowser->FolderChanged();
							strncpy(fi.fname, mount_img, 255);
							diskCaddy.Insert(&fi, false);
							fileBrowser->Update();
							emulating = BeginEmulating(fileBrowser, mount_img);
						} 
						else if (drive_ctrl == 2)/* .LST - XXX FIXME not yet clean for dual drive */
						{
							fileBrowser->FolderChanged();
							if (fileBrowser->SelectLST(mount_img))
								fileBrowser->SetSelectionsMade(true);
						}
						else if (drive_ctrl == 3)/* shut down emulator */
						{
							DEBUG_LOG("%s: webserver requests to shut down emulator for drive %d", __FUNCTION__, deviceID);
							emulating = EMULATION_SHUTDOWN;
						}
						mount_new = 0;
						drive_ctrl = 0;				
					}
#endif
					usDelay(1);
				}
			}
			else
			{
				while (emulating == IEC_COMMANDS)
				{
#if defined(__CIRCLE__)
extern char mount_img[256];
extern char mount_path[256];
extern int mount_new;
extern int drive_ctrl;
					FILINFO fi;
					if (mount_new)
					{
						DEBUG_LOG("%s: webserver requests to mount in dir '%s' the img '%s'", __FUNCTION__, mount_path, mount_img);
						if (f_chdir(mount_path) != FR_OK)
							DEBUG_LOG("%s: chdir to '%s' failed", __FUNCTION__, mount_path);
						else if (drive_ctrl == 1)
						{

							fileBrowser->FolderChanged();
							strncpy(fi.fname, mount_img, 255);
							diskCaddy.Insert(&fi, false);
							fileBrowser->Update();
							emulating = BeginEmulating(fileBrowser, mount_img);
						}
						else if (drive_ctrl == 2)/* .LST */
						{
							fileBrowser->FolderChanged();
							fileBrowser->SelectLST(mount_img);
							if (fileBrowser->SelectLST(mount_img))
								fileBrowser->SetSelectionsMade(true);
						}
						else if (drive_ctrl == 3)/* exit emulation */
						{
							DEBUG_LOG("%s: webserver requests to exit emulation for drive %d", __FUNCTION__, deviceID);
							emulating = EMULATION_SHUTDOWN;
						}
						mount_new = 0;
						drive_ctrl = 0;				
					}
#endif
					fileBrowser->Update();
					if (fileBrowser->SelectionsMade())
						emulating = BeginEmulating(fileBrowser, fileBrowser->LastSelectionName());
					usDelay(1);
				}
			}
		}
		else
		{
			if (emulating == EMULATING_1541)
				exitReason = Emulate1541(fileBrowser);
#if defined(PI1581SUPPORT)
			else if (emulating == EMULATING_1581)
				exitReason = Emulate1581(fileBrowser);
#endif
			else
			{
				DEBUG_LOG("%s: Drive %d shutting down emulator", __FUNCTION__, deviceID);
				shutdown = true;
			}

			DEBUG_LOG("Drive %d exited emulation %d\r\n", deviceID, exitReason);

			// Clearing the caddy now
			//	- will write back all changed/dirty/written to disk images now
#if not defined(EXPERIMENTALZERO)
			core0RefreshingScreen.Acquire();
#endif
			if (diskCaddy.Empty())
				iec_bus.WaitMicroSeconds(2 * 1000000);
			iec_bus.WaitUntilReset();
			emulating = IEC_COMMANDS;
	
			if ((exitReason == EXIT_RESET) && (options.GetOnResetChangeToStartingFolder() || selectedViaIECCommands))
				fileBrowser->DisplayRoot(); // TO CHECK

			inputMappings->WaitForClearButtons();

#if not defined(EXPERIMENTALZERO)
			core0RefreshingScreen.Release();
#endif
		}
	}
	delete fileBrowser;
}

