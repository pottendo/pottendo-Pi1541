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

#ifndef SCREEN_HEADLESS_H
#define SCREEN_HEADLESS_H

#include "Screen.h"
#include <string.h>

class ScreenHeadLess : virtual public Screen
{

public:
	ScreenHeadLess() : Screen()	{}
	~ScreenHeadLess() = default;

	void Open(u32 width, u32 height, u32 colourDepth) {}

	void DrawRectangle(u32 x1, u32 y1, u32 x2, u32 y2, RGBA colour) {};
	void Clear(RGBA colour) {};

	void ScrollArea(u32 x1, u32 y1, u32 x2, u32 y2) {};

	void WriteChar(bool petscii, u32 x, u32 y, unsigned char c, RGBA colour) {};
	u32 PrintText(bool petscii, u32 xPos, u32 yPos, char *ptr, RGBA TxtColour = RGBA(0xff, 0xff, 0xff, 0xff), RGBA BkColour = RGBA(0, 0, 0, 0xFF), bool measureOnly = false, u32* width = 0, u32* height = 0)
		{ return strlen(ptr); }
	u32 MeasureText(bool petscii, char *ptr, u32* width = 0, u32* height = 0)
		{ return strlen(ptr); }

	void DrawLine(u32 x1, u32 y1, u32 x2, u32 y2, RGBA colour) {};
	void DrawLineV(u32 x, u32 y1, u32 y2, RGBA colour) {};
	void PlotPixel(u32 x, u32 y, RGBA colour) {};

	void PlotImage(u32* image, int x, int y, int w, int h) {};

	float GetScaleX() const { return 1.0; }
	float GetScaleY() const { return 1.0; }

	u32 ScaleX(u32 x) { return (u32)((float)x * 1.0); }
	u32 ScaleY(u32 y) { return (u32)((float)y * 1.0); }

	u32 GetFontHeight() { return 8; };
	u32 GetFontHeightDirectoryDisplay() { return 8; };

	void SwapBuffers() {}
};

#endif
