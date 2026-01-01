// Pi1541 - A Commodore 1541 disk drive emulator
// Copyright(C) 2018 Stephen White
//
// This file is part of Pi1541.
// 
// Pi1541 is free software : you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// Pi1541 is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with Pi1541. If not, see <http://www.gnu.org/licenses/>.

#include "iec_bus.h"
#include "InputMappings.h"
#include "options.h"
#include "emulator.h"

extern Options options;
extern emulator_t *emu_selected;
//#define REAL_XOR 1

int IEC_Bus::buttonCount = sizeof(ButtonPinFlags) / sizeof(unsigned);

u32 IEC_Bus::PIGPIO_MASK_IN_ATN = 1 << PIGPIO_ATN;
u32 IEC_Bus::PIGPIO_MASK_IN_DATA = 1 << PIGPIO_DATA;
u32 IEC_Bus::PIGPIO_MASK_IN_CLOCK = 1 << PIGPIO_CLOCK;
u32 IEC_Bus::PIGPIO_MASK_IN_SRQ = 1 << PIGPIO_SRQ;
u32 IEC_Bus::PIGPIO_MASK_IN_RESET = 1 << PIGPIO_RESET;
u32 IEC_Bus::PIGPIO_MASK_OUT_LED = 1 << PIGPIO_OUT_LED;
u32 IEC_Bus::PIGPIO_MASK_OUT_SOUND = 1 << PIGPIO_OUT_SOUND;

bool IEC_Bus::splitIECLines = false;
bool IEC_Bus::invertIECInputs = false;
bool IEC_Bus::invertIECOutputs = false;
bool IEC_Bus::ignoreReset = false;

IEC_Bus::IEC_Bus(u8 deviceID, const uint32_t driveID) :
	device_id(deviceID),
	drive_id(driveID)
{
	PI_Atn = false;
	PI_Data = false;
	PI_Clock = false;
	PI_SRQ = false;
	PI_Reset = false;

	VIA_Atna = false;
	VIA_Data = false;
	VIA_Clock = false;

	DataSetToOut = false;
	AtnaDataSetToOut = false;
	ClockSetToOut = false;
	SRQSetToOut = false;

	VIA = 0;
	CIA = 0;
	port = 0;

	OutputLED = false;
	OutputSound = false;

	Resetting = false;

	myOutsGPFSEL1 = 0;
	myOutsGPFSEL0 = 0;
	InputButton[5] = {0};
	InputButtonPrev[5] = {0};
	validInputCount[5] = {0};
	//inputRepeatThreshold[5];
	inputRepeat[5] = {0};
	inputRepeatPrev[5] = {0};

	//emulationModeCheckButtonIndex = 0;
	emuSpinLock.Acquire();
	SetSplitIECLines(options.SplitIECLines());
	SetInvertIECInputs(options.InvertIECInputs());
	SetInvertIECOutputs(options.InvertIECOutputs());	
	SetIgnoreReset(options.IgnoreReset());
	emuSpinLock.Release();
	DEBUG_LOG("%s: splitIECLines=%d, invertIECInputs=%d, invertIECOutputs=%d, ignoreReset=%d", __FUNCTION__, splitIECLines, invertIECInputs, invertIECOutputs, ignoreReset);
	gplev0 = 0; // not needed as static is used for a working emulation
	Initialise();

	DEBUG_LOG("%s: IEC Bus initialized for device %d", __FUNCTION__, device_id);
}

//ROTARY: Added for rotary encoder support - 09/05/2019 by Geo...
RotaryEncoder IEC_Bus::rotaryEncoder;
bool IEC_Bus::rotaryEncoderEnable;
//ROTARY: Added for rotary encoder inversion (Issue#185) - 08/13/2020 by Geo...
bool IEC_Bus::rotaryEncoderInvert;

#if defined (__CIRCLE__)
CGPIOPin IEC_Bus::IO_ATN;
CGPIOPin IEC_Bus::IO_CLK;
CGPIOPin IEC_Bus::IO_DAT;
CGPIOPin IEC_Bus::IO_SRQ;
CGPIOPin IEC_Bus::IO_RST;
CGPIOPin IEC_Bus::IO_buttons[5];
CGPIOPin IEC_Bus::IO_led;
CGPIOPin IEC_Bus::IO_sound;

CGPIOPin IEC_Bus::IO_IN_BUTTON4;
CGPIOPin IEC_Bus::IO_IN_BUTTON5;
CGPIOPin IEC_Bus::IO_IN_RESET;
CGPIOPin IEC_Bus::IO_IN_SRQ;
CGPIOPin IEC_Bus::IO_IN_BUTTON2;
CGPIOPin IEC_Bus::IO_IN_BUTTON3;
CGPIOPin IEC_Bus::IO_IN_ATN;
CGPIOPin IEC_Bus::IO_IN_DATA;
CGPIOPin IEC_Bus::IO_IN_CLOCK;
CGPIOPin IEC_Bus::IO_IN_BUTTON1;

//CGPIOPin IO_OUT_RESET;
CGPIOPin IEC_Bus::IO_OUT_SPI0_RS;

CGPIOPin IEC_Bus::IO_OUT_ATN;
CGPIOPin IEC_Bus::IO_OUT_SOUND;
CGPIOPin IEC_Bus::IO_OUT_LED;
CGPIOPin IEC_Bus::IO_OUT_CLOCK;
CGPIOPin IEC_Bus::IO_OUT_DATA;
CGPIOPin IEC_Bus::IO_OUT_SRQ;
bool IEC_Bus::iec_initialized = false;
unsigned IEC_Bus::gplev0 = 0;
unsigned IEC_Bus::_mask = 0;
int IEC_Bus::dual_drive = -1;
#endif

void __not_in_flash_func(IEC_Bus::ReadGPIOUserInput)(bool minimalCheck)
{
#if !defined(__PICO2__)	&& !defined(ESP32)
 	int indexEnter = InputMappings::INPUT_BUTTON_ENTER;
 	int indexUp = InputMappings::INPUT_BUTTON_UP;
	int indexDown = InputMappings::INPUT_BUTTON_DOWN;	

	//ROTARY: Added for rotary encoder support - 09/05/2019 by Geo...
	if (IEC_Bus::rotaryEncoderEnable == true)
	{
		//Poll the rotary encoder
		//
		// Note: If the rotary encoder returns any value other than 'NoChange' an
		//       event has been detected.  We force the button state of the original
		//       input button registers to reflect the desired action, and allow the
		//       original processing logic to do it's work.
		//
		rotary_result_t rotaryResult = IEC_Bus::rotaryEncoder.Poll(gplev0);
		switch (rotaryResult)
		{

			case ButtonDown:
				SetButtonState(indexEnter, true);
				break;

			case RotateNegative:
				SetButtonState(indexUp, true);
				break;

			case RotatePositive:
				SetButtonState(indexDown, true);
				break;

			default:
				SetButtonState(indexEnter, false);
				SetButtonState(indexUp, false);
				SetButtonState(indexDown, false);
				break;

		}
	}
	else 
	{
 		UpdateButton(indexEnter);
 		UpdateButton(indexUp);
 		UpdateButton(indexDown);
 	}	
#endif /* !__PICO2__ && !ESP32*/
	if (!minimalCheck)
	{
 		int indexBack = InputMappings::INPUT_BUTTON_BACK;
 		int indexInsert = InputMappings::INPUT_BUTTON_INSERT;
		  
 		UpdateButton(indexBack);
 		UpdateButton(indexInsert);
	}
}

void IEC_Bus::UpdateButton(int index)
{
	bool inputcurrent = (gplev0 & ButtonPinFlags[index]) == 0;

	InputButtonPrev[index] = InputButton[index];
	inputRepeatPrev[index] = inputRepeat[index];

	if (inputcurrent)
	{
		validInputCount[index]++;
		if (validInputCount[index] == INPUT_BUTTON_DEBOUNCE_THRESHOLD)
		{
			InputButton[index] = true;
			inputRepeatThreshold[index] = INPUT_BUTTON_DEBOUNCE_THRESHOLD + INPUT_BUTTON_REPEAT_THRESHOLD;
			inputRepeat[index]++;
		}

		if (validInputCount[index] == inputRepeatThreshold[index])
		{
			inputRepeat[index]++;
			inputRepeatThreshold[index] += INPUT_BUTTON_REPEAT_THRESHOLD / inputRepeat[index];
		}
	}
	else
	{
		InputButton[index] = false;
		validInputCount[index] = 0;
		inputRepeatThreshold[index] = INPUT_BUTTON_REPEAT_THRESHOLD;
		inputRepeat[index] = 0;
		inputRepeatPrev[index] = 0;
	}
}
 
//ROTARY: Modified for rotary encoder support - 09/05/2019 by Geo...
void __not_in_flash_func(IEC_Bus::ReadBrowseMode)(void)
{
#if defined(__PICO2__)	
	gplev0 = gpio_get_all();
#elif defined (ESP32)
	gplev0 = 0;
	//if (!AtnaDataSetToOut)
		gplev0 |= (gpio_get_level((gpio_num_t)PIGPIO_ATN) << PIGPIO_ATN);
	if (!DataSetToOut)
		gplev0 |= (gpio_get_level((gpio_num_t)PIGPIO_DATA) << PIGPIO_DATA);
	if (!ClockSetToOut)
		gplev0 |= (gpio_get_level((gpio_num_t)PIGPIO_CLOCK) << PIGPIO_CLOCK);
	gplev0 |= (gpio_get_level((gpio_num_t)PIGPIO_RESET) << PIGPIO_RESET);
	//printf("%s: - gplev0 = %08x\n", __FUNCTION__, gplev0);
#else
#if !defined (CIRCLE_GPIO)
	gplev0 = read32(ARM_GPIO_GPLEV0);
#else
	gplev0 = CGPIOPin::ReadAll();
#endif	
#endif
	if (get_driveID() == emu_selected->get_driveID())
		ReadGPIOUserInput();

	bool ATNIn = (gplev0 & PIGPIO_MASK_IN_ATN) == (invertIECInputs ? PIGPIO_MASK_IN_ATN : 0);
	if (PI_Atn != ATNIn)
	{
		//DEBUG_LOG("ATN changed: %d\n", ATNIn);
		PI_Atn = ATNIn;
	}

	if (!AtnaDataSetToOut && !DataSetToOut)	// only sense if we have not brought the line low (because we can't as we have the pin set to output but we can simulate in software)
	{
		bool DATAIn = (gplev0 & PIGPIO_MASK_IN_DATA) == (invertIECInputs ? PIGPIO_MASK_IN_DATA : 0);
		if (PI_Data != DATAIn)
		{
			//DEBUG_LOG("DATAIn changed: %d\n", DATAIn);
			PI_Data = DATAIn;
		}
	}
	else
	{
		PI_Data = true;
	}

	if (!ClockSetToOut)	// only sense if we have not brought the line low (because we can't as we have the pin set to output but we can simulate in software)
	{
		bool CLOCKIn = (gplev0 & PIGPIO_MASK_IN_CLOCK) == (invertIECInputs ? PIGPIO_MASK_IN_CLOCK  : 0);
		if (PI_Clock != CLOCKIn)
		{
			//DEBUG_LOG("CLOCKIn changed: %d\n", CLOCKIn);
			PI_Clock = CLOCKIn;
		}
	}
	else
	{
		PI_Clock = true;
	}
	Resetting = !ignoreReset && ((gplev0 & PIGPIO_MASK_IN_RESET) == (invertIECInputs ? PIGPIO_MASK_IN_RESET : 0));
}

void __not_in_flash_func(IEC_Bus::ReadEmulationMode1541)(void)
{
	bool AtnaDataSetToOutOld = AtnaDataSetToOut;
	IOPort* portB = 0;
	//u32 cnt = 100;
	//time_fn_arm();
	//do {

#if defined(__PICO2__)	
	gplev0 = gpio_get_all();
#elif defined(ESP32)
	gplev0 = 0;
	gplev0 |= (gpio_get_level((gpio_num_t)PIGPIO_ATN) << PIGPIO_ATN);
	if (!DataSetToOut)
		gplev0 |= (gpio_get_level((gpio_num_t)PIGPIO_DATA) << PIGPIO_DATA);
	if (!ClockSetToOut)
		gplev0 |= (gpio_get_level((gpio_num_t)PIGPIO_CLOCK) << PIGPIO_CLOCK);
	gplev0 |= (gpio_get_level((gpio_num_t)PIGPIO_RESET) << PIGPIO_RESET);
	//printf("%s: - gplev0 = %04x\n", __FUNCTION__, gplev0);
#else
#if !defined (CIRCLE_GPIO)
	if (dual_drive == 0)
		gplev0 = read32(ARM_GPIO_GPLEV0);
	else if (get_driveID() == 0) 
		gplev0 = read32(ARM_GPIO_GPLEV0);
	//DEBUG_LOG("%s: gplev0 = %08x, device = %d", __FUNCTION__, gplev0, device_id);
#else	
	gplev0 = CGPIOPin::ReadAll();
#endif	
#endif
	//} while (--cnt);
	//time_fn_eval(1, "ReadAll()");
	portB = port;

#ifndef REAL_XOR
	bool ATNIn = (gplev0 & PIGPIO_MASK_IN_ATN) == (invertIECInputs ? PIGPIO_MASK_IN_ATN : 0);
	if (PI_Atn != ATNIn)
	{
		PI_Atn = ATNIn;

		//DEBUG_LOG("A%d\r\n", PI_Atn);
		//if (port)
		{
			if ((portB->GetDirection() & 0x10) != 0)
			{
				// Emulate the XOR gate UD3
				// We only need to do this when fully emulating, iec commands do this internally
				AtnaDataSetToOut = (VIA_Atna != PI_Atn);
			}

			portB->SetInput(VIAPORTPINS_ATNIN, ATNIn);	//is inverted and then connected to pb7 and ca1
			VIA->InputCA1(ATNIn);
		}
	}

	if (portB && (portB->GetDirection() & 0x10) == 0)
	{ /*FCP*/
		AtnaDataSetToOut = false; // If the ATNA PB4 gets set to an input then we can't be pulling data low. (Maniac Mansion does this)
		if(AtnaDataSetToOutOld) RefreshOuts1541(); /*FCP*/
	} /*FCP*/

	// moved from PortB_OnPortOut
	if (AtnaDataSetToOut)
		portB->SetInput(VIAPORTPINS_DATAIN, true);	// simulate the read in software

	if (!AtnaDataSetToOut && !DataSetToOut)	// only sense if we have not brought the line low (because we can't as we have the pin set to output but we can simulate in software)
	{
		bool DATAIn = (gplev0 & PIGPIO_MASK_IN_DATA) == (invertIECInputs ? PIGPIO_MASK_IN_DATA : 0);
		//if (PI_Data != DATAIn)
		{
			PI_Data = DATAIn;
			portB->SetInput(VIAPORTPINS_DATAIN, DATAIn);	// VIA DATAin pb0 output from inverted DIN 5 DATA
		}
	}
	else
	{
		PI_Data = true;
		portB->SetInput(VIAPORTPINS_DATAIN, true);	// simulate the read in software
	}
#else
	bool ATNIn = (gplev0 & PIGPIO_MASK_IN_ATN) == (invertIECInputs ? PIGPIO_MASK_IN_ATN : 0);
	if (PI_Atn != ATNIn)
	{
		PI_Atn = ATNIn;

		{
			portB->SetInput(VIAPORTPINS_ATNIN, ATNIn);	//is inverted and then connected to pb7 and ca1
			VIA->InputCA1(ATNIn);
		}
	}

	if (!DataSetToOut)	// only sense if we have not brought the line low (because we can't as we have the pin set to output but we can simulate in software)
	{
		bool DATAIn = (gplev0 & PIGPIO_MASK_IN_DATA) == (invertIECInputs ? PIGPIO_MASK_IN_DATA : 0);
		//if (PI_Data != DATAIn)
		{
			PI_Data = DATAIn;
			portB->SetInput(VIAPORTPINS_DATAIN, DATAIn);	// VIA DATAin pb0 output from inverted DIN 5 DATA
		}
	}
	else
	{
		PI_Data = true;
		portB->SetInput(VIAPORTPINS_DATAIN, true);	// simulate the read in software
	}
#endif
	if (!ClockSetToOut)	// only sense if we have not brought the line low (because we can't as we have the pin set to output but we can simulate in software)
	{
		bool CLOCKIn = (gplev0 & PIGPIO_MASK_IN_CLOCK) == (invertIECInputs ? PIGPIO_MASK_IN_CLOCK : 0);

		//if (PI_Clock != CLOCKIn)
		{
			PI_Clock = CLOCKIn;
			portB->SetInput(VIAPORTPINS_CLOCKIN, CLOCKIn); // VIA CLKin pb2 output from inverted DIN 4 CLK
		}
	}
	else
	{
		PI_Clock = true;
		portB->SetInput(VIAPORTPINS_CLOCKIN, true); // simulate the read in software
	}
	Resetting = !ignoreReset && ((gplev0 & PIGPIO_MASK_IN_RESET) == (invertIECInputs ? PIGPIO_MASK_IN_RESET : 0));
}

#if defined(PI1581SUPPORT)
void IEC_Bus::ReadEmulationMode1581(void)
{
	IOPort* portB = 0;
#if !defined (CIRCLE_GPIO)
	gplev0 = read32(ARM_GPIO_GPLEV0);
#else	
	gplev0 = CGPIOPin::ReadAll();
#endif	
	portB = port;

	bool ATNIn = (gplev0 & PIGPIO_MASK_IN_ATN) == (invertIECInputs ? PIGPIO_MASK_IN_ATN : 0);
	if (PI_Atn != ATNIn)
	{
		PI_Atn = ATNIn;

		//DEBUG_LOG("A%d\r\n", PI_Atn);
		//if (port)
		{
			if ((portB->GetDirection() & 0x10) != 0)
			{
				// Emulate the XOR gate UD3
				// We only need to do this when fully emulating, iec commands do this internally
				AtnaDataSetToOut = (VIA_Atna & PI_Atn);
			}

			portB->SetInput(VIAPORTPINS_ATNIN, ATNIn);	//is inverted and then connected to pb7 and ca1
			CIA->SetPinFLAG(!ATNIn);
		}
	}

	if (portB && (portB->GetDirection() & 0x10) == 0)
		AtnaDataSetToOut = false; // If the ATNA PB4 gets set to an input then we can't be pulling data low. (Maniac Mansion does this)

	if (!AtnaDataSetToOut && !DataSetToOut)	// only sense if we have not brought the line low (because we can't as we have the pin set to output but we can simulate in software)
	{
		bool DATAIn = (gplev0 & PIGPIO_MASK_IN_DATA) == (invertIECInputs ? PIGPIO_MASK_IN_DATA : 0);
		if (PI_Data != DATAIn)
		{
			PI_Data = DATAIn;
			portB->SetInput(VIAPORTPINS_DATAIN, DATAIn);	// VIA DATAin pb0 output from inverted DIN 5 DATA
		}
	}
	else
	{
		PI_Data = true;
		portB->SetInput(VIAPORTPINS_DATAIN, true);	// simulate the read in software
	}

	if (!ClockSetToOut)	// only sense if we have not brought the line low (because we can't as we have the pin set to output but we can simulate in software)
	{
		bool CLOCKIn = (gplev0 & PIGPIO_MASK_IN_CLOCK) == (invertIECInputs ? PIGPIO_MASK_IN_CLOCK : 0);
		if (PI_Clock != CLOCKIn)
		{
			PI_Clock = CLOCKIn;
			portB->SetInput(VIAPORTPINS_CLOCKIN, CLOCKIn); // VIA CLKin pb2 output from inverted DIN 4 CLK
		}
	}
	else
	{
		PI_Clock = true;
		portB->SetInput(VIAPORTPINS_CLOCKIN, true); // simulate the read in software
	}

	if (!SRQSetToOut)	// only sense if we have not brought the line low (because we can't as we have the pin set to output but we can simulate in software)
	{
		bool SRQIn = (gplev0 & PIGPIO_MASK_IN_SRQ) == (invertIECInputs ? PIGPIO_MASK_IN_SRQ : 0);
		if (PI_SRQ != SRQIn)
		{
			PI_SRQ = SRQIn;
		}
	}
	else
	{
		PI_SRQ = true;
	}

	Resetting = !ignoreReset && ((gplev0 & PIGPIO_MASK_IN_RESET) == (invertIECInputs ? PIGPIO_MASK_IN_RESET : 0));
}
#endif /* PI1581 SUPPORT */

void IEC_Bus::PortB_OnPortOut(void* pUserData, unsigned char status)
{
	bool oldDataSetToOut = DataSetToOut;
	bool oldClockSetToOut = ClockSetToOut;
	bool AtnaDataSetToOutOld = AtnaDataSetToOut;

	// These are the values the VIA is trying to set the outputs to
	VIA_Atna = (status & (unsigned char)VIAPORTPINS_ATNAOUT) != 0;
	VIA_Data = (status & (unsigned char)VIAPORTPINS_DATAOUT) != 0;		// VIA DATAout PB1 inverted and then connected to DIN DATA
	VIA_Clock = (status & (unsigned char)VIAPORTPINS_CLOCKOUT) != 0;	// VIA CLKout PB3 inverted and then connected to DIN CLK

#ifndef REAL_XOR
	if (VIA)
	{
		// Emulate the XOR gate UD3
		AtnaDataSetToOut = (VIA_Atna != PI_Atn);
	}
	else
	{
		AtnaDataSetToOut = (VIA_Atna & PI_Atn);
	}
#else
	AtnaDataSetToOut = VIA_Atna;
#endif

	//if (AtnaDataSetToOut)
	//{
	//	// if the output of the XOR gate is high (ie VIA_Atna != PI_Atn) then this is inverted and pulls DATA low (activating it)
	//	//PI_Data = true;
	//	if (port) port->SetInput(VIAPORTPINS_DATAIN, true);	// simulate the read in software
	//}

	if (VIA && port)
	{
		// If the VIA's data and clock outputs ever get set to inputs the real hardware reads these lines as asserted.
		bool PB1SetToInput = (port->GetDirection() & 2) == 0;
		bool PB3SetToInput = (port->GetDirection() & 8) == 0;
		if (PB1SetToInput) VIA_Data = true;
		if (PB3SetToInput) VIA_Clock = true;
	}

	ClockSetToOut = VIA_Clock;
	DataSetToOut = VIA_Data;

	//if (!oldDataSetToOut && DataSetToOut)
	//{
	//	//PI_Data = true;
	//	if (port) port->SetInput(VIAPORTPINS_DATAOUT, true); // simulate the read in software
	//}

	//if (!oldClockSetToOut && ClockSetToOut)
	//{
	//	//PI_Clock = true;
	//	if (port) port->SetInput(VIAPORTPINS_CLOCKIN, true); // simulate the read in software
	//}

	//if (AtnaDataSetToOutOld ^ AtnaDataSetToOut)
	//	RefreshOuts1541();
}

void IEC_Bus::Reset(void)
{
	WaitUntilReset();

	// VIA $1800
	//	CA2, CB1 and CB2 are not connected (reads as high)
	// VIA $1C00
	//	CB1 not connected (reads as high)

	VIA_Atna = false;
	VIA_Data = false;
	VIA_Clock = false;

	DataSetToOut = false;
	ClockSetToOut = false;
	SRQSetToOut = false;

	PI_Atn = false;
	PI_Data = false;
	PI_Clock = false;
	PI_SRQ = false;

#ifdef REAL_XOR
	AtnaDataSetToOut = VIA_Atna;
#else
	if (VIA)
		AtnaDataSetToOut = (VIA_Atna != PI_Atn);
	else
		AtnaDataSetToOut = (VIA_Atna & PI_Atn);

	if (AtnaDataSetToOut) PI_Data = true;
#endif

#if defined(PI1581SUPPORT)
	RefreshOuts1581();
#endif	
}
