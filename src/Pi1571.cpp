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

#include "Pi1571.h"
#include "iec_bus.h"
#include "options.h"
#include "ROMs.h"
#include "debug.h"

extern Pi1571 pi1571;
extern u8 s_u8Memory[0xc000];
extern ROMs roms;

// TODO
// - clean up common port pins in Drive.h
// - encaps pi1541.drive.IsLEDOn() and pi1541.drive.IsMotorOn();
// - encaps pi1541.drive.Insert()
//		- need to also do something like this
//			GetPortB()->SetInput(VIAPORTPINS_PINS_WRTP, !diskImage->GetReadOnly());
// - clean up OnPortOut (all drives)
//		- what VIAs? It is not clear.
// - handle all inputs to the VIAs in ::Update()?
//		- including IEC signals
// - move IEC::ReadEmulationMode* into Pi*
// - check CIA IOs (not the same as 1581)
// - check schematics for
//		- wd177x
//			- drive READY?
//			- act LED?
//			- is there a PORTA_PINS_DISKCHNG?
//			- Write protect connected to PB4
//			- STEP NC
//			- DIR (head direction) NC
// - write back
// - check 1571 snoop
// - chack what sets the via serial direction for the 15-8-1
// - 8520
//		- remove timerAReloaded/timerBReloaded (SCRATCH THAT they are used)
//		- private
//		- TOD
// - put sound playing (in main) inside a function that all drives can use


// $1800 VIA1
// PA 0 TRK0 SNS
// PA 1 SER DIR (for the 74ls241)
// PA 2 SIDE
// PA 3 NC
// PA 4 NC
// PA 5 !1/2 MHZ
// PA 6 ATN OUT
// PA 7 also is Byte ready (BYTE RDY)
// CA 2	WPS	write protect switch (schematics say its on this)
// CB 1	WPS	write protect switch? (1571 internals say its on this)

// PA0 I TRACK0 
//			0 = head on track 0
//			1 = head NOT on track 0
// PA1 O SER DIR
//			0 = 1570/71 bus is input
//			0 = 1570/71 bus is output
// PA2 O SIDE Active head
// PA3 NC
// PA4 NC
// PA5 O !1/2 MHZ Drive mode and processor clock
//		0 = 1541 mode with 1MHz clock frequency
//		1 = 1571 mode with 2MHz clock frequency
// PA6 ?
// PA7 I Byte ready signal

// $1800 VIA1
// PB0 DATA IN
// PB1 DATA OUT
// PB2 CLK IN
// PB3 CLK OUT
// PB4 ATNA
// PB5 ID 1
// PB6 ID 2
// PB7 ATN IN

// CA 1		Input for ATN signal of the serial bus
//			Creates interrupt on rising edge of ATN
// CA 2		?
// CB 1		Write protect signal
// CB 2		?

// $1C00 VIA2

// $1C00 VIA2
// PB 0,1	step motor
// PB 2		MTR dirve motor
// PB 3		ACT drive LED
// PB 4		WPS	write protect switch
// PB 5,6	bit rate
// PB 7		Sync

// CA 1		Byte ready
// CA 2		SOE set overflow enable 6502
// CB 2		read/write

enum PortPins
{
	// PORT A VIA1 $1800
	PORTA_PINS_TRK0_SNS = 0x01,		//pa0
	PORTA_PINS_FAST_SER_DIR = 0x02,	//pa1
	PORTA_PINS_SIDE0 = 0x04,		//pa2
	// NC = 0x08
	// NC = 0x10
	PORTA_PINS_MHZ = 0x20,			//pa5
	PORTA_PINS_RDY = 0x80			//pa7


	// PORT B VIA1 $1C00


	//PORTA_PINS_MOTOR = ?

	//PORTA_PINS_DEVSEL0 = 0x08,	//pa3
	//PORTA_PINS_DEVSEL1 = 0x10,	//pa4

	//PORTA_PINS_ACT_LED = 0x40,	//pa6
	//PORTA_PINS_DISKCHNG = 0x80,	//pa7

	//// PORT B
	//PORTB_PINS_FAST_SER_DIR = 0x20, //pb5
	//PORTB_PINS_WPAT = 0x40, //pb6

	//VIAPORTPINS_SYNC = 0x80,	//pb7
};

enum
{
	FAST_SERIAL_DIR_IN,
	FAST_SERIAL_DIR_OUT
};

extern u16 pc;

//        1571                C128DCR Drive (1571CR)
//0000 - 07FF  RAM           0000 - 07FF  RAM
//1800 - 180F  6522 VIA1     1800 - 180F  6522 VIA1
//1C00 - 1COF  6522 VIA2     1C00 - 1COF  6522 VIA2
//2000 - 2003  WD1770 FDC    2000 - 2005  5710 FDC
//4000 - 400F  6526 CIA      400C - 400E  5710 CIA Serial Port
//4010 - 4017  5710 FDC2
//6000 - 7FFF  RAM shadows
//8000 - FFFF  ROM           8000 - FFFF  ROM

// RAM
// 0-$7ff
//  6000-7FFF  RAM shadows
//
// 6522
//	$1800-$180f
//	$1c00-$1c0f
//
// 1770
//	$2000-$2005
//
// CS
// 6526
//	$4000-$400f
//
// ROM
//	$8000-$ffff

u8 read6502_1571(u16 address)
{
	u8 value = 0;
	if (address & 0x8000)
	{
		value = roms.Read1571(address);
	}
	else if (address >= 0x1800 && address < 0x1810)
	{
		value = pi1571.VIA[0].Read(address);
	}
	else if (address >= 0x1c00 && address < 0x1c10)
	{
		value = pi1571.VIA[1].Read(address);
	}
	else if (address >= 0x2000 && address < 0x2004)
	{
		value = pi1571.wd177x.Read(address);
	}
	else if (address >= 0x4000 && address < 0x4010)
	{
		value = pi1571.CIA.Read(address);
	}
	else if (address < 0x800 || (address >= 0x6000 && address < 0x8000))
	{
		value = s_u8Memory[address & 0x7ff];
	}
	else
	{
		value = address >> 8;	// Empty address bus
	}
	return value;
}

// Use for debugging (Reads VIA registers without the regular VIA read side effects)
u8 peek6502_1571(u16 address)
{
	u8 value = 0;
	return value;
}

void write6502_1571(u16 address, const u8 value)
{
	if (address & 0x8000)
	{
		return;
	}
	else if (address >= 0x1800 && address < 0x1810)
	{
		pi1571.VIA[0].Write(address, value);
	}
	else if (address >= 0x1c00 && address < 0x1c10)
	{
		pi1571.VIA[1].Write(address, value);
	}
	else if (address >= 0x2000 && address < 0x2004)
	{
		pi1571.wd177x.Write(address, value);
	}
	else if (address >= 0x4000 && address < 0x4010)
	{
		pi1571.CIA.Write(address, value);
	}
	else if (address < 0x800 || (address >= 0x6000 && address < 0x8000))
	{
		s_u8Memory[address & 0x7ff] = value;
	}
}

#if 0
static void CIAPortA_OnPortOut(void* pUserData, unsigned char status)
{
}

static void CIAPortB_OnPortOut(void* pUserData, unsigned char status)
{
	//Pi1571* pi1571 = (Pi1571*)pUserData;

	//pi1571->wd177x.SetWPRTPin(status & VIAPORTPINS_PINS_WRTP); // !WPAT


	//Pi1571::PortB_OnPortOut(0, status);
}
#endif 

static void VIA1PortB_OnPortOut(void* pUserData, unsigned char status)
{
	bool oldDataSetToOut = IEC_Bus::DataSetToOut;
	bool oldClockSetToOut = IEC_Bus::ClockSetToOut;
	bool AtnaDataSetToOutOld = IEC_Bus::AtnaDataSetToOut;

//	DEBUG_LOG("B %04x\r\n", pc);
	// These are the values the VIA is trying to set the outputs to
	IEC_Bus::VIA_Atna = (status & (unsigned char)VIAPORTPINS_ATNAOUT) != 0;
	IEC_Bus::VIA_Data = (status & (unsigned char)VIAPORTPINS_DATAOUT) != 0;		// VIA DATAout PB1 inverted and then connected to DIN DATA
	IEC_Bus::VIA_Clock = (status & (unsigned char)VIAPORTPINS_CLOCKOUT) != 0;	// VIA CLKout PB3 inverted and then connected to DIN CLK

#ifndef REAL_XOR
	// Emulate the XOR gate UD3
	IEC_Bus::AtnaDataSetToOut = (IEC_Bus::VIA_Atna != IEC_Bus::PI_Atn);
#else
	IEC_Bus::AtnaDataSetToOut = IEC_Bus::VIA_Atna;
#endif

	IOPort* portB = pi1571.VIA[0].GetPortB();

	// If the VIA's data and clock outputs ever get set to inputs the real hardware reads these lines as asserted.
	bool PB1SetToInput = (portB->GetDirection() & VIAPORTPINS_DATAOUT) == 0;
	bool PB3SetToInput = (portB->GetDirection() & VIAPORTPINS_CLOCKOUT) == 0;
	if (PB1SetToInput) IEC_Bus::VIA_Data = true;
	if (PB3SetToInput) IEC_Bus::VIA_Clock = true;

	IEC_Bus::ClockSetToOut = IEC_Bus::VIA_Clock;
	IEC_Bus::DataSetToOut = IEC_Bus::VIA_Data;
}

static void VIA1PortA_OnPortOut(void* pUserData, unsigned char status)
{
	Pi1571* pi1571 = (Pi1571*)pUserData;

	//DEBUG_LOG("A %04x\r\n", pc);

	if (status & PORTA_PINS_FAST_SER_DIR)
	{
		if (pi1571->fastSerialDirection != FAST_SERIAL_DIR_OUT)
		{
			pi1571->fastSerialDirection = FAST_SERIAL_DIR_OUT;
			//DEBUG_LOG("VSO %04x %d\r\n", pc, pi1571->fastSerialDirection);
		}
	}
	else
	{
		if (pi1571->fastSerialDirection != FAST_SERIAL_DIR_IN)
		{
			pi1571->fastSerialDirection = FAST_SERIAL_DIR_IN;
			//DEBUG_LOG("VSI %04x\r\n", pc);
		}
	}

	pi1571->speed2Mhz = (status & PORTA_PINS_MHZ) != 0;

	// PA7 Byte ready is set in the update

	int head = (status & PORTA_PINS_SIDE0) != 0;
	//DEBUG_LOG("SH %d %04x\r\n", head, pc);
	//pi1571->wd177x.SetSide(head);
	pi1571->drive.SetHead(head);
}

extern bool bLoggingCYCs;
int cnti = 0;

Pi1571::Pi1571()
{
	Initialise();
}

void Pi1571::Initialise()
{
	diskImage = 0;
	LED = false;

	VIA[0].ConnectIRQ(&m6502.IRQ);
	VIA[1].ConnectIRQ(&m6502.IRQ);

	CIA.ConnectIRQ(&m6502.IRQ);
	// IRQ is not connected on a 1571
	//wd177x.ConnectIRQ(&m6502.IRQ);

	// CIA ports are not used on a 1571
	//CIA.GetPortA()->SetPortOut(this, CIAPortA_OnPortOut);
	//CIA.GetPortB()->SetPortOut(this, CIAPortB_OnPortOut);

	// For now disk is writable
	//CIA.GetPortB()->SetInput(VIAPORTPINS_PINS_WRTP, true);

	//CIA.GetPortA()->SetInput(PORTA_PINS_DISKCHNG, false);
	//CIA.GetPortA()->SetInput(PORTA_PINS_RDY, true);

	VIA[0].GetPortB()->SetPortOut(this, VIA1PortB_OnPortOut);
	VIA[0].GetPortA()->SetPortOut(this, VIA1PortA_OnPortOut);

	RDYDelayCount = 0;
}

void Pi1571::Update1Mhz()
{
	IOPort* via1PortA = VIA[0].GetPortA();
	bool byteReady = drive.Update();

	if (byteReady)
	{
		//This pin sets the overflow flag on a negative transition from TTL one to TTL zero.
		// SO is sampled at the trailing edge of P1, the cpu V flag is updated at next P1.
		m6502.SO();
		byteReadyCount = 10;
	}

	// PA 7 also is Byte ready (BYTE RDY)
	//via1PortA->SetInput(PORTA_PINS_RDY, !byteReady);

	if (byteReadyCount)
	{
		byteReadyCount--;
		via1PortA->SetInput(PORTA_PINS_RDY, false);
	}
	else
	{
		via1PortA->SetInput(PORTA_PINS_RDY, true);
	}
}

void Pi1571::Update()
{
	IOPort* via1PortA = VIA[0].GetPortA();
	//bool byteReady = drive.Update(GetMhz());

	//if (byteReady)
	//{
	//	//This pin sets the overflow flag on a negative transition from TTL one to TTL zero.
	//	// SO is sampled at the trailing edge of P1, the cpu V flag is updated at next P1.
	//	m6502.SO();
	//}

	//// PA 7 also is Byte ready (BYTE RDY)
	//via1PortA->SetInput(PORTA_PINS_RDY, !byteReady);

	via1PortA->SetInput(PORTA_PINS_TRK0_SNS, drive.Track() != 0);

	VIA[1].Execute();
	VIA[0].Execute();

	//if (RDYDelayCount)
	//{
	//	RDYDelayCount--;
	//	if (RDYDelayCount == 0)
	//	{
	//		CIA.GetPortA()->SetInput(PORTA_PINS_RDY, false);
	//	}
	//}

	// SRQ is pulled high by the c128

	// When U13 (74LS241) is set to fast serial IN
	//	- R20 pulls SP high
	//	- R19 pulls CNT high
	//	- R26 pulls Fast Clock (SRQ) in high
	// When U13 (74LS241) is set to fast serial OUT 
	//	- R25 pulls DATA high
	//	- R28 pulls Fast Clock (SRQ) out high

	if (CIA.SerialInput() && (fastSerialDirection == FAST_SERIAL_DIR_IN))
	{
		CIA.SetPinSP(!IEC_Bus::GetPI_Data());	// Real hardware does not invert but we use hardware that inverts so to compensate we invert in software now
		CIA.SetPinCNT(IEC_Bus::GetPI_SRQ());	// Like the real hardware we have no inverter on the SRQ input 
	}

	CIA.Execute();

	if (CIA.SerialOutput() && (fastSerialDirection == FAST_SERIAL_DIR_OUT))
	{
		IEC_Bus::SetFastSerialData(!CIA.GetPinSP());	// Real hardware does not invert but we use hardware that inverts so to compensate we invert in software now
		IEC_Bus::SetFastSerialSRQ(!CIA.GetPinCNT());	// NEW: We have an inverter on our output hardware so to compensate we invert in software now
														// reasoning - if we have completed the shift CNT line goes high - to do this we output a low and this will be inverted and pulled high
		//DEBUG_LOG("cp=%d\r\n", CIA.GetPinCNT());
		//trace lines and see if this needs to set other lines
	}

	for (int i = 0; i < 4; ++i)
	{
		wd177x.Execute();
	}
}

void Pi1571::Reset()
{
	IOPort* CIABPortB;

	fastSerialDirection = FAST_SERIAL_DIR_IN;
	CIA.Reset();
	wd177x.Reset();
	IEC_Bus::Reset();
	// On a real drive the outputs look like they are being pulled high (when set to inputs) (Taking an input from the front end of an inverter)
	CIABPortB = CIA.GetPortB();
	CIABPortB->SetInput(VIAPORTPINS_DATAOUT, true);
	CIABPortB->SetInput(VIAPORTPINS_CLOCKOUT, true);
	CIABPortB->SetInput(VIAPORTPINS_ATNAOUT, true);

	IOPort* VIABortB;
	VIA[0].Reset();
	VIA[1].Reset();
	drive.Reset();
	VIABortB = VIA[0].GetPortB();
	VIABortB->SetInput(VIAPORTPINS_DATAOUT, true);
	VIABortB->SetInput(VIAPORTPINS_CLOCKOUT, true);
	VIABortB->SetInput(VIAPORTPINS_ATNAOUT, true);

	speed2Mhz = false;
}

void Pi1571::SetDeviceID(u8 id)
{
	VIA[0].GetPortB()->SetInput(VIAPORTPINS_DEVSEL0, id & 1);
	VIA[0].GetPortB()->SetInput(VIAPORTPINS_DEVSEL1, id & 2);

	CIA.GetPortA()->SetInput(VIAPORTPINS_DEVSEL0, id & 1);
	CIA.GetPortA()->SetInput(VIAPORTPINS_DEVSEL1, id & 2);
}

void Pi1571::Insert(DiskImage* diskImage)
{
	drive.Insert(diskImage);
//	CIA.GetPortB()->SetInput(VIAPORTPINS_PINS_WRTP, !diskImage->GetReadOnly());
//	CIA.GetPortA()->SetInput(PORTA_PINS_DISKCHNG, true);
	wd177x.Insert(diskImage);
	this->diskImage = diskImage;
}



