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

#include "circle-kernel.h"
#include "debug.h"
#include "circle-hmi.h"
#include <circle/display.h>
#include <circle/font.h>

circle_hmi::circle_hmi(int BSCMaster, u8 address, unsigned width, unsigned height, int flip, LCD_MODEL type)
	: i2c_master(BSCMaster, TRUE),
	  i2c_display(&i2c_master, width, height, address),
	  i2c_term(&i2c_display, 0, Font8x8),
	  address(address),
	  width(width), height(height)
{
	i2c_display.SetRotation(180 * flip);
}

void circle_hmi::PlotCharacter(bool useCBMFont, bool petscii, int x, int y, char ascii, bool inverse)
{}

void circle_hmi::PlotText(bool useCBMFont, bool petscii, int x, int y, char* str, bool inverse)
{
	char cstr[128];
	char *del = "\E[K";
	if (strcmp(str, "                ") == 0)
		str="";
	if (strlen(str) > 16)
	{
		str[16] = '\0';
		del="";
	}
	const char *inv = (inverse ? "\E[7m" : "");
	snprintf(cstr, 127, "\E[%d;%dH\E[?25l%s%s\E[0m%s", (y + 1), x + 1, inv, str, del);
	//DEBUG_LOG("%s: text %d, %d,%d: '%s'", __FUNCTION__, inverse, x, y, cstr);
	i2c_term.Write(cstr, strlen(cstr));
}

void circle_hmi::InitHardware()
{
	if (!i2c_display.Initialize() || !i2c_term.Initialize())
		DEBUG_LOG("%s: display initialize failed", __FUNCTION__);
	i2c_display.Clear();
	for (int i = 0; i< 128; i++)
		i2c_display.SetPixel(i,i, CDisplay::White);
}

void circle_hmi::DisplayOn()
{
	i2c_display.On();
}

void circle_hmi::DisplayOff()
{
	i2c_display.Off();
}

void circle_hmi::SetContrast(u8 value)
{
	contrast = value;
	DEBUG_LOG("%s: val = %d (%d)", __FUNCTION__, value, ((value >> 8) & 7) << 4 );
	u8 buffer[8] = { 0x00, 0x81, 0x00, value, 0x00, 0xDB, 0x00, ((value >> 8) & 7) << 4 };
	if (i2c_master.Write(address, buffer, 4) < 0)
		DEBUG_LOG("%s: failed to set contrast", __FUNCTION__);
}

void circle_hmi::SetVCOMDeselect(u8 value)
{}

void circle_hmi::ClearScreen()
{
	i2c_display.Clear();
}

void circle_hmi::RefreshScreen()
{
	i2c_term.Update();
}

void circle_hmi::RefreshPage(u32 page)
{
	i2c_term.Update();
}

void circle_hmi::RefreshTextRows(u32 start, u32 amountOfRows)
{
	i2c_term.Update();
}

void circle_hmi::SetDisplayWindow(u8 x1, u8 y1, u8 x2, u8 y2)
{}

void circle_hmi::PlotPixel(int x, int y, int c)
{
	switch (c) {
	case 1:
		i2c_term.SetPixel(x, y, CDisplay::White);
		break;
	default:
		i2c_term.SetPixel(x, y, CDisplay::Black);
		break;
	}
}

void circle_hmi::PlotImage(const unsigned char * source)
{
	return;
	CDisplay::TArea a = {0, i2c_display.GetWidth()-1, 0, i2c_display.GetHeight()-1};
	DEBUG_LOG("%s: plotting image %dx%d", __FUNCTION__, a.x2, a.y2);
	i2c_display.SetArea(a, source);
	MsDelay(2000);
}
