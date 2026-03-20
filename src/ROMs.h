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

#ifndef ROMs_H
#define ROMs_H

#include "types.h"

class ROMs
{
public:
	ROMs() :
		currentROMIndex(0)
	{
	}

	void SelectROM(const char* ROMName);

	inline u8 Read(u16 address)
	{
		return ROMImages[currentROMIndex][address & ROMMask[currentROMIndex]];
	}
	void ResetCurrentROMIndex();
#if defined (ESP32)
	static const int ROM_SIZE = 16384; // allow only 176kB ROMs on ESP32
#else
	static const int ROM_SIZE = 16384*2; // allow 32kB ROMs
#endif
#if	defined(PI1581SUPPORT)
	inline u8 ReadMPS802(u16 address)
	{
		return ROMImageMPS802[address & 0x1fff];
	}	
	static const int ROM_MPS802_SIZE = 8192;
	inline u8 Read1581(u16 address)
	{
		return ROMImage1581[address & 0x7fff];
	}

	static const int ROM1581_SIZE = 16384 * 2;
	unsigned char ROMImage1581[ROM1581_SIZE];
	char ROMName1581[256];
	static const int MAX_ROMS = 7;
	unsigned char ROMImageMPS802[ROM_MPS802_SIZE];
#else
	static const int MAX_ROMS = 1;
#endif

	unsigned char ROMImages[MAX_ROMS][ROM_SIZE];
	char ROMNames[MAX_ROMS][256];
	bool ROMValid[MAX_ROMS];
	unsigned ROMHash[MAX_ROMS];
	unsigned ROMMask[MAX_ROMS];
	
	unsigned currentROMIndex;
	unsigned lastManualSelectedROMIndex;

	unsigned GetLongestRomNameLen() { return longestRomNameLen; }
	unsigned UpdateLongestRomNameLen(unsigned maybeLongest);

	const char* GetSelectedROMName() const
	{
		return ROMNames[currentROMIndex];
	}

	unsigned GetHash() const
	{
		return ROMHash[currentROMIndex]; 
	}
protected:
	unsigned longestRomNameLen;
};

#endif
