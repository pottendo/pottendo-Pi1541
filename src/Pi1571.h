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

#ifndef PI1571_H
#define PI1571_H

#include "Drive.h"
#include "m6502.h"
#include "iec_bus.h"
#include "wd177x.h"
#include "m8520.h"

class Pi1571
{

public:
	Pi1571();

	void Initialise();

	void Update1Mhz();
	void Update();

	void Reset();

	void SetDeviceID(u8 id);

	void Insert(DiskImage* diskImage);

	inline const DiskImage* GetDiskImage() const { return diskImage; }

	inline bool IsLEDOn() const { return drive.IsLEDOn(); }
	inline bool IsMotorOn() const { return wd177x.IsExternalMotorAsserted(); }
	inline void SetLED(bool value) { LED = value; }

	inline int GetMhz() const { return (int)speed2Mhz + 1; }

	//static void VIA1PortB_OnPortOut(void* pUserData, unsigned char status);
	//static void VIA1PortA_OnPortOut(void* pUserData, unsigned char status);

	Drive drive;
	m6522 VIA[2];
	WD177x wd177x;
	m8520 CIA;

	M6502 m6502;

	unsigned fastSerialDirection;
	unsigned int RDYDelayCount;
	bool speed2Mhz;

private:
	DiskImage* diskImage;
	bool LED;

	u8 byteReadyCount;

	//u8 Memory[0xc000];

	//static u8 Read6502(u16 address, void* data);
	//static u8 Read6502ExtraRAM(u16 address, void* data);
	//static u8 Peek6502(u16 address, void* data);
	//static void Write6502(u16 address, const u8 value, void* data);
	//static void Write6502ExtraRAM(u16 address, const u8 value, void* data);
};

#endif