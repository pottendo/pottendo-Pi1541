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

#include "defs.h"
#if defined (PI1581SUPPORT)
#include "Pi1581.h"
#include "iec_bus.h"
#include "Options.h"
#include "ROMs.h"
#include "debug.h"

extern Pi1581 pi1581;
extern u8 s_u8Memory[0xc000];
extern ROMs roms;
extern IEC_Bus* iec_bus;

#include <stdio.h>

// Fast serial diagnostics (not for merging). A ring of the last events,
// formatted by F81Format and written to the SD card when 1581 emulation is
// left, to see where a C128 fast serial transfer stops.
// Types: 1 SDR write (send), 7 SDR write while still shifting, 8 SDR write
// in input mode, 2 SDR read, 3 byte received, 4 serial port mode (1 out),
// 5 FAST SER DIR (1 out), 6 ATN (1 asserted), 9 CLK in (1 asserted),
// 10 DATA in (1 asserted, only while the port is in input direction).
struct F81Entry { u32 t; u16 pc; u8 type, v; };
static const u32 F81_N = 8192;	// power of two
static F81Entry f81Log[F81_N];
static u32 f81Pos;
static u32 f81Cycles;
static u32 f81Counts[11];
static bool f81LastAtn, f81LastClk, f81LastData, f81LastDir;
static bool f81DrivingSRQ;

void F81Log(unsigned char type, unsigned char value)
{
	F81Entry& e = f81Log[f81Pos & (F81_N - 1)];
	e.t = f81Cycles;
	e.pc = pi1581.m6502.GetPC();
	e.type = type;
	e.v = value;
	f81Pos++;
	if (type < 11)
		f81Counts[type]++;
}

void F81Reset()
{
	f81Pos = 0;
	f81Cycles = 0;
	for (int i = 0; i < 11; ++i)
		f81Counts[i] = 0;
	f81LastAtn = f81LastClk = f81LastData = f81LastDir = false;
	f81DrivingSRQ = false;
}

int F81Format(char* text, int size)
{
	int n = 0;
	#define F81APPEND(...) \
		do { if (n < size) n += snprintf(text + n, size - n, __VA_ARGS__); } while (0)
	F81APPEND("Pi1581 fast serial diagnostics\r\n\r\n");
	F81APPEND("Counts: send %u, send-while-busy %u, SDR write in input mode %u, SDR read %u, received %u,\r\n"
		"        mode switches %u, direction changes %u, ATN changes %u, CLK changes %u\r\n",
		(unsigned)f81Counts[1], (unsigned)f81Counts[7], (unsigned)f81Counts[8], (unsigned)f81Counts[2],
		(unsigned)f81Counts[3], (unsigned)f81Counts[4], (unsigned)f81Counts[5], (unsigned)f81Counts[6],
		(unsigned)f81Counts[9]);
	char line[256];
	pi1581.CIA.DiagDescribe(line, sizeof(line));
	F81APPEND("CIA at exit: %s\r\n", line);
	u16 PC; u8 SP, A, X, Y, P;
	pi1581.m6502.GetRegs(PC, SP, A, X, Y, P);
	F81APPEND("CPU at exit: PC=%04x A=%02x X=%02x Y=%02x SP=%02x P=%02x\r\n",
		(unsigned)PC, (unsigned)A, (unsigned)X, (unsigned)Y, (unsigned)SP, (unsigned)P);
	F81APPEND("Direction %s, bus ATN %d CLK %d DATA %d SRQ %d (1 = low)\r\n",
		pi1581.fastSerialDirection == 1 /* FAST_SERIAL_DIR_OUT, declared below */ ? "OUT" : "IN",
		(int)iec_bus->GetPI_Atn(), (int)iec_bus->GetPI_Clock(), (int)iec_bus->GetPI_Data(),
		(int)iec_bus->GetPI_SRQ());
	F81APPEND("\r\nEvents, oldest first (t in 1581 cycles, 2 per us):\r\n");
	u32 count = f81Pos < F81_N ? f81Pos : F81_N;
	u32 start = f81Pos - count;
	static const char* names[11] = { "?", "SEND", "RECV-RD", "RECV", "MODE", "DIR", "ATN",
		"SEND-BUSY", "SDR-W-IN", "CLK", "DATA" };
	for (u32 i = 0; i < count; ++i)
	{
		const F81Entry& e = f81Log[(start + i) & (F81_N - 1)];
		const char* nm = e.type < 11 ? names[e.type] : "?";
		char c = (e.v >= 0x20 && e.v < 0x7f) ? (char)e.v : '.';
		F81APPEND("%10u %04x %-9s %02x %c\r\n", (unsigned)e.t, (unsigned)e.pc, nm, (unsigned)e.v, c);
	}
	return n;
}

// PA0 SIDE0
// PA1 !RDY
// PA2 !MOTOR
// PA3 ID 1
// PA4 ID 2
// PA5 POWER LED
// PA6 ACT LED
// PA7 !DISK_CHNG
// PB0 DATA IN
// PB1 DATA OUT
// PB2 CLK IN
// PB3 CLK OUT
// PB4 ATNA
// PB5 FAST SER DIR
// PB6 /WPAT
// PB7 ATN IN

// FAST SER DIR sets the direction of U13 (74ls241)
// When 1
//	- SP is sent to DATA
//	- Fast Clock (SRQ) is sent to CNT
// When 0
//	- DATA is sent to SP
//	- CNT is sent to Fast Clock (SRQ)

enum PortPins
{
	// PORT A
	PORTA_PINS_SIDE0 = 0x01,	//pa0
	PORTA_PINS_RDY = 0x20,		//pa1
	PORTA_PINS_MOTOR = 0x04,	//pa2

	PORTA_PINS_DEVSEL0 = 0x08,	//pa3
	PORTA_PINS_DEVSEL1 = 0x10,	//pa4

	PORTA_PINS_ACT_LED = 0x40,	//pa6
	PORTA_PINS_DISKCHNG = 0x80,	//pa7

	// PORT B
	PORTB_PINS_FAST_SER_DIR = 0x20, //pb5
	PORTB_PINS_WPAT = 0x40, //pb6
};

enum
{
	FAST_SERIAL_DIR_IN,
	FAST_SERIAL_DIR_OUT
};

extern u16 pc;

// CS
// 8520
//	$4000
//
// 1770
//	$6000
//
// ROM
//	$8000
//
// RAM
// 0-$1fff

u8 read6502_1581(u16 address)
{
	u8 value = 0;
#if defined(PI1581SUPPORT)
	if (address & 0x8000)
	{
		value = roms.Read1581(address);
	}
	else if (address >= 0x6000)
	{
		value = pi1581.wd177x.Read(address);
		//DEBUG_LOG("177x r %04x %02x %04x\r\n", address, value, pc);
	}
	else if (address >= 0x4000)
	{
		value = pi1581.CIA.Read(address);
		//DEBUG_LOG("CIA r %04x %02x %04x\r\n", address, value, pc);
	}
	else if (address < 0x2000)
	{
		value = s_u8Memory[address & 0x1fff];
	}
	else
	{
		value = address >> 8;	// Empty address bus
	}
#endif
	return value;
}

// Use for debugging (Reads VIA registers without the regular VIA read side effects)
u8 peek6502_1581(u16 address)
{
	u8 value = 0;
	return value;
}

void write6502_1581(u16 address, const u8 value)
{
#if defined(PI1581SUPPORT)
	if (address & 0x8000)
	{
		return;
	}
	else if (address >= 0x6000)
	{
		//DEBUG_LOG("177x w %04x %02x %04x\r\n", address, value, pc);
		pi1581.wd177x.Write(address, value);
	}
	else if (address >= 0x4000)
	{
		//DEBUG_LOG("CIA w %04x %02x %04x\r\n", address, value, pc);
		pi1581.CIA.Write(address, value);
	}
	else if (address < 0x2000)
	{
		s_u8Memory[address & 0x1fff] = value;
	}
#endif
}

static void CIAPortA_OnPortOut(void* pUserData, unsigned char status)
{
	Pi1581* pi1581 = (Pi1581*)pUserData;

	pi1581->wd177x.SetSide(status & PORTA_PINS_SIDE0);

	bool motorAsserted = (status & PORTA_PINS_MOTOR) == 0;

	if (motorAsserted)
	{
		if (!pi1581->wd177x.IsExternalMotorAsserted())
		{
			pi1581->CIA.GetPortA()->SetInput(PORTA_PINS_RDY, true);

			pi1581->RDYDelayCount = 250000;
			pi1581->wd177x.AssertExternalMotor(motorAsserted); // !MOTOR
		}
	}
	else
	{
		pi1581->RDYDelayCount = 0;
		pi1581->CIA.GetPortA()->SetInput(PORTA_PINS_RDY, true);
		if (pi1581->wd177x.IsExternalMotorAsserted())
		{
			//DEBUG_LOG("pc=%04x\r\n", pc);
			pi1581->wd177x.AssertExternalMotor(motorAsserted); // !MOTOR
		}
	}

	pi1581->SetLED((status & PORTA_PINS_ACT_LED) != 0);
}

static void CIAPortB_OnPortOut(void* pUserData, unsigned char status)
{
	Pi1581* pi1581 = (Pi1581*)pUserData;

	pi1581->wd177x.SetWPRTPin(status & PORTB_PINS_WPAT); // !WPAT

	if (status & PORTB_PINS_FAST_SER_DIR)
		pi1581->fastSerialDirection = FAST_SERIAL_DIR_OUT;
	else
		pi1581->fastSerialDirection = FAST_SERIAL_DIR_IN;


	iec_bus->PortB_OnPortOut(0, status);
}

Pi1581::Pi1581()
{
	Initialise();
}

void Pi1581::Initialise()
{
	LED = false;

	CIA.ConnectIRQ(&m6502.IRQ);
	// IRQ is not connected on a 1581
	//wd177x.ConnectIRQ(&m6502.IRQ);

	CIA.GetPortA()->SetPortOut(this, CIAPortA_OnPortOut);
	CIA.GetPortB()->SetPortOut(this, CIAPortB_OnPortOut);

	// For now disk is writable
	CIA.GetPortB()->SetInput(PORTB_PINS_WPAT, true);

	CIA.GetPortA()->SetInput(PORTA_PINS_DISKCHNG, false);
	CIA.GetPortA()->SetInput(PORTA_PINS_RDY, true);

	RDYDelayCount = 0;
}

void Pi1581::Update()
{
	//CIA.GetPortA()->SetInput(PORTA_PINS_DISKCHNG, 1);
	//CIA.GetPortA()->SetInput(PORTA_PINS_RDY, false);

	if (RDYDelayCount)
	{
		RDYDelayCount--;
		if (RDYDelayCount == 0)
		{
			CIA.GetPortA()->SetInput(PORTA_PINS_RDY, false);
		}
	}

	CIA.Execute();

	// SRQ is pulled high by the c128

	// When U13 (74LS241) is set to fast serial IN
	//	- R20 pulls SP high
	//	- R19 pulls CNT high
	//	- R26 pulls Fast Clock (SRQ) in high
	// When U13 (74LS241) is set to fast serial OUT 
	//	- R25 pulls DATA high
	//	- R28 pulls Fast Clock (SRQ) out high

	f81Cycles++;
	{
		bool atn = iec_bus->GetPI_Atn();
		bool clk = iec_bus->GetPI_Clock();
		bool dir = fastSerialDirection == FAST_SERIAL_DIR_OUT;
		if (atn != f81LastAtn) { F81Log(6, atn); f81LastAtn = atn; }
		if (clk != f81LastClk) { F81Log(9, clk); f81LastClk = clk; }
		if (dir != f81LastDir) { F81Log(5, dir); f81LastDir = dir; }
	}

	if (fastSerialDirection == FAST_SERIAL_DIR_OUT)
	{
		// When 1
		//	- SP is sent to DATA
		//	- Fast Clock (SRQ) is sent to CNT
		iec_bus->SetFastSerialData(!CIA.GetPinSP());	// Communication on fast serial is done after the inverter.
		// SRQSetToOut means "pull the line low", like DataSetToOut above,
		// so CNT low pulls SRQ low. This used to be CNT itself, which held
		// SRQ low whenever CNT idled high. It never showed because
		// RefreshOuts1581 held SRQ low regardless; now that it does not,
		// the inverted clock made a C128 read nothing (empty directory).
		iec_bus->SetFastSerialSRQ(!CIA.GetPinCNT());
		f81DrivingSRQ = !CIA.GetPinCNT();
		//trace lines and see it this needs to set other lines
	}
	else
	{
		// When 0
		//	- DATA is sent to SP
		//	- CNT is sent to Fast Clock (SRQ)
		CIA.SetPinSP(!iec_bus->GetPI_Data());	// Communication on fast serial is done before the inverter.
		// Let go of SRQ (the 74LS241 no longer drives it) and follow the
		// line: GetPI_SRQ() is true while SRQ is low, CNT follows the level.
		iec_bus->SetFastSerialSRQ(false);
		// If we were still pulling SRQ low on the previous cycle, the line
		// as read this cycle is our own drive (the read forces it low while
		// we drive it), not the host. Skip it, or CNT sees a false low and
		// then a rising edge: a spurious shift that misaligns every byte.
		if (f81DrivingSRQ)
			f81DrivingSRQ = false;
		else
			CIA.SetPinCNT(!iec_bus->GetPI_SRQ());
		{
			bool data = iec_bus->GetPI_Data();
			if (data != f81LastData) { F81Log(10, data); f81LastData = data; }
		}
	}

	for (int i = 0; i < 4; ++i)
	{
		wd177x.Execute();
	}
}

void Pi1581::Reset()
{
	IOPort* CIABPortB;

	fastSerialDirection = FAST_SERIAL_DIR_IN;
	CIA.Reset();
	wd177x.Reset();
	iec_bus->Reset();
	// On a real drive the outputs look like they are being pulled high (when set to inputs) (Taking an input from the front end of an inverter)
	CIABPortB = CIA.GetPortB();
	CIABPortB->SetInput(VIAPORTPINS_DATAOUT, true);
	CIABPortB->SetInput(VIAPORTPINS_CLOCKOUT, true);
	CIABPortB->SetInput(VIAPORTPINS_ATNAOUT, true);
}

void Pi1581::SetDeviceID(u8 id)
{
	CIA.GetPortA()->SetInput(PORTA_PINS_DEVSEL0, id & 1);
	CIA.GetPortA()->SetInput(PORTA_PINS_DEVSEL1, id & 2);
}

void Pi1581::Insert(DiskImage* diskImage)
{
	if (!diskImage || (!diskImage->IsD81() && !diskImage->IsD71()))
	{
		DEBUG_LOG("%s: 1581: Unsupported disk image type inserted: %s", __FUNCTION__, diskImage->GetName());
		return;
	}
//	CIA.GetPortB()->SetInput(PORTB_PINS_WPAT, !diskImage->GetReadOnly());
	CIA.GetPortA()->SetInput(PORTA_PINS_DISKCHNG, true);
	wd177x.Insert(diskImage);
	this->diskImage = diskImage;
}

#endif /* PI1581 SUPPORT */