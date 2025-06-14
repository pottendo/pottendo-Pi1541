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

#ifndef __CIRCLE_HMI_H__
#define __CIRCLE_HMI_H__
#include <circle/i2cmaster.h>
#include <display/ssd1306display.h>
#include <circle/terminal.h>
#include "types.h"

#define SSD1306_128x64_BYTES ((128 * 64) / 8)

class circle_hmi
{
public:
	circle_hmi(int BSCMaster = 1, u8 address = 0x3C, unsigned width = 128, unsigned height = 64, int flip = 0, LCD_MODEL type=LCD_UNKNOWN);
	~circle_hmi() = default;

	void PlotCharacter(bool useCBMFont, bool petscii, int x, int y, char ascii, bool inverse);
	void PlotText(bool useCBMFont, bool petscii, int x, int y, char* str, bool inverse);

	void InitHardware();
	void DisplayOn();
	void DisplayOff();
	void SetContrast(u8 value);
	u8 GetContrast() { return contrast; }
	void SetVCOMDeselect(u8 value);

	void ClearScreen();
	void RefreshScreen();
	void RefreshPage(u32 page);
	void RefreshTextRows(u32 start, u32 amountOfRows);
	void SetDisplayWindow(u8 x1, u8 y1, u8 x2, u8 y2);
	void PlotPixel(int x, int y, int c);
	void PlotImage(const unsigned char * source);
private:
	CI2CMaster i2c_master;
	CSSD1306Display i2c_display;
	CTerminalDevice i2c_term;

	int BSCMaster;
	u8 address;
	int type;
	int flip;
	int contrast;
	unsigned width;
	unsigned height;
};
#endif // __CIRCLE_HMI_H__

