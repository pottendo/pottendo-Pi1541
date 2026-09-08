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

#ifndef OPTIONS_H
#define OPTIONS_H

#include "types.h"
#include "DiskImage.h"

class TextParser
{
public:
	TextParser(void)
		: data(0)
	{
	}

	void SetData(char* buffer) { data = buffer; }

	char* GetToken(bool includeSpace = false);

protected:
	char* data;
	bool ParseComment();
	void SkipWhiteSpace();
};

class Options : public TextParser
{
public:
	Options(void);

	void Process(char* buffer);

	inline unsigned int GetDeviceID() const { return deviceID; }
	inline unsigned int GetOnResetChangeToStartingFolder() const { return onResetChangeToStartingFolder; }
	inline const char* GetAutoMountImageName() const { return autoMountImageName; }
	inline const char* GetRomFontName() const { return ROMFontName; }
	inline const char* GetRomNameCMDHD() const { return ROMNameCMDHD; }
	inline unsigned int GetCMDHDDeviceID() const { return CMDHDDeviceID; }
	inline unsigned int GetCMDHDCacheMB() const { return CMDHDCacheMB; }
	// GPIO used to pull the IEC ATN line low (0 = the drive cannot drive ATN).
	// 24 on a Pi1541io: its ATN level shifter is bidirectional, so the pin that
	// reads ATN can drive it too.
	inline unsigned int GetCMDHDAtnOutGPIO() const { return CMDHDAtnOutGPIO; }
	inline unsigned int GetCMDHDLcdLamps() const { return CMDHDLcdLamps; }
	const char* GetRomName(int index) const;
	const char* GetRomName1581() const;
	inline const char* GetStarFileName() const { return starFileName; }
	inline unsigned int GetExtraRAM() const { return extraRAM; }
	inline unsigned int GetRAMBOard() const { return RAMBOard; }
	inline unsigned int GetDisableSD2IECCommands() const { return disableSD2IECCommands; }
	inline unsigned int GetDisableHDMI() const { return disableHDMI; }
	inline unsigned int GetSupportUARTInput() const { return supportUARTInput; }

	inline unsigned int HDMIGraphIEC() const { return hdmiGraphIEC; }
	inline unsigned int HDMIDisplayIECActivity() const { return hdmiDisplayIECActivity; }
	inline unsigned int DisplayTracks() const { return displayTracks; }
	inline unsigned int QuickBoot() const { return quickBoot; }
	inline unsigned int LogoDisplayDelay() const { return logoDisplayDelay; }
	inline unsigned int ShowOptions() const { return showOptions; }
	inline unsigned int DisplayPNGIcons() const { return displayPNGIcons; }
	inline int SoundOnGPIO() const { return soundOnGPIO; }
	inline unsigned int SoundOnGPIODuration() const { return soundOnGPIODuration; }
	inline unsigned int SoundOnGPIOFreq() const { return soundOnGPIOFreq; }
	inline unsigned int SplitIECLines() const { return splitIECLines; }
	inline unsigned int InvertIECInputs() const { return invertIECInputs; }
	inline unsigned int InvertIECOutputs() const { return invertIECOutputs; }
	inline unsigned int IgnoreReset() const { return ignoreReset; }

	inline unsigned int AutoBootFB128() const { return autoBootFB128; }
	inline const char* Get128BootSectorName() const { return C128BootSectorName; }

	inline unsigned int DisplayTemperature() const { return displayTemperature; }

	inline unsigned int LowercaseBrowseModeFilenames() const { return lowercaseBrowseModeFilenames; }

	inline unsigned int CDSlashSlashToRoot() const { return cdSlashSlashToRoot; }
	inline unsigned int StartInUSBDrive() const { return startInUSBDrive; }

	DiskImage::DiskType GetNewDiskType() const;

	inline unsigned int ScreenWidth() const { return screenWidth; }
	inline unsigned int ScreenHeight() const { return screenHeight; }

	inline unsigned int I2CBusMaster() const { return i2cBusMaster; }
	inline unsigned int I2CLcdAddress() const { return i2cLcdAddress; }
	inline unsigned int I2CScan() const { return i2cScan; }
	inline unsigned int I2CLcdFlip() const { return i2cLcdFlip; }
	inline unsigned int I2CLcdOnContrast() const { return i2cLcdOnContrast; }
	inline unsigned int I2CLcdDimContrast() const { return i2cLcdDimContrast; }
	inline unsigned int I2CLcdDimTime() const { return i2cLcdDimTime; }
	inline unsigned int I2cLcdUseCBMChar() const { return i2cLcdUseCBMChar; }
	inline LCD_MODEL I2CLcdModel() const { return i2cLcdModel; }
	inline const char *I2CLcdModelName() const { return i2cLcdModelName; }

	inline const char* GetLcdLogoName() const { return LcdLogoName; }

	inline float ScrollHighlightRate() const { return scrollHighlightRate; }

	// options.txt numbers the buttons 1-5; the arrays behind them are indexed
	// from 0, hence the -1. An out of range value used to sail straight
	// through: "buttonEnter = 0" gave 0u - 1 = 0xFFFFFFFF, truncated to 255
	// when stored in a u8, and inputmappings then read 250 elements past the
	// end of IEC_Bus's five element arrays.
	//
	// The two families need different handling. The CMD HD buttons are always
	// tested with "< 5" before use, so out of range can map to a sentinel that
	// disables the function - which is what options.txt already documents 0 to
	// mean. The browser buttons are stored as u8 and indexed unguarded, so
	// there is no disabled state available: they fall back to their default
	// instead, which at least leaves the browser usable.
	static const unsigned int BUTTON_DISABLED = 0xFFFFFFFF;

	static inline unsigned int ButtonIndex(unsigned int n)
	{
		return (n >= 1 && n <= 5) ? n - 1 : BUTTON_DISABLED;
	}

	static inline unsigned int BrowserButtonIndex(unsigned int n, unsigned int fallback)
	{
		return (n >= 1 && n <= 5) ? n - 1 : fallback - 1;
	}

	inline unsigned int GetButtonEnter() const { return buttonEnter - 1; }
	inline unsigned int GetButtonUp() const { return buttonUp - 1; }
	inline unsigned int GetButtonDown() const { return buttonDown - 1; }
	inline unsigned int GetButtonBack() const { return buttonBack - 1; }
	inline unsigned int GetButtonInsert() const { return buttonInsert - 1; }

	// CMD HD front panel buttons (1-5 in options.txt, 0 = function disabled)
	inline unsigned int GetCMDHDButtonSwap8() const { return ButtonIndex(CMDHDButtonSwap8); }
	inline unsigned int GetCMDHDButtonSwap9() const { return ButtonIndex(CMDHDButtonSwap9); }
	inline unsigned int GetCMDHDButtonWP() const { return ButtonIndex(CMDHDButtonWP); }
	inline unsigned int GetCMDHDButtonReset() const { return ButtonIndex(CMDHDButtonReset); }
	inline unsigned int GetCMDHDButtonExit() const { return ButtonIndex(CMDHDButtonExit); }

	//ROTARY: Added for rotary encoder support - 09/05/2019 by Geo...
	inline unsigned int RotaryEncoderEnable() const { return rotaryEncoderEnable; }
	//ROTARY: Added for rotary encoder inversion (Issue#185) - 08/13/2020 by Geo...
	inline unsigned int RotaryEncoderInvert() const { return rotaryEncoderInvert; }

	inline unsigned int GetHeadLess() const { return headLess; }
#if defined(__CIRCLE__)
	inline unsigned int GetNetWifi() const { return netWifi; }
	inline unsigned int GetNetEthernet() const { return netEthernet; }
	inline unsigned GetMaxContentSize() const { return maxContentSize; }
	inline unsigned GetMaxMultipartSize() const { return maxMultipartSize; }
	inline void SetHeadLess(unsigned int h) { headLess = h; }
	inline unsigned int GetHealthMonitor() const { return noHealthMonitor; }
	inline unsigned int GetDHCP() const { return useDHCP; }
	inline float GetTZ() const { return TZ; }
#endif	

	// Page up and down will jump a different amount based on the maximum number rows displayed.
	// Perhaps we should use some keyboard modifier to the the other screen?
	inline unsigned int KeyboardBrowseLCDScreen() const { return keyboardBrowseLCDScreen; }

	const char* GetLCDName() const { return LCDName; }

	const char* GetAutoBaseName() const { return autoBaseName; }

	static int GetDecimal(char* pString);
	static float GetFloat(char* pString);

private:
	unsigned int CMDHDDeviceID;
	unsigned int CMDHDCacheMB;
	unsigned int CMDHDAtnOutGPIO;
	unsigned int CMDHDLcdLamps;
	unsigned int deviceID;
	unsigned int onResetChangeToStartingFolder;
	unsigned int extraRAM;
	unsigned int RAMBOard;
	unsigned int disableSD2IECCommands;
	unsigned int disableHDMI;
	unsigned int supportUARTInput;
	unsigned int hdmiGraphIEC;
	unsigned int hdmiDisplayIECActivity;
	unsigned int displayTracks;
	unsigned int quickBoot;
	unsigned int logoDisplayDelay;
	unsigned int showOptions;
	unsigned int displayPNGIcons;
	int soundOnGPIO;
	unsigned int soundOnGPIODuration;
	unsigned int soundOnGPIOFreq;
	unsigned int invertIECInputs;
	unsigned int invertIECOutputs;
	unsigned int splitIECLines;
	unsigned int ignoreReset;
	unsigned int autoBootFB128;
	unsigned int displayTemperature;
	unsigned int lowercaseBrowseModeFilenames;

	unsigned int cdSlashSlashToRoot;
	unsigned int startInUSBDrive;

	unsigned int screenWidth;
	unsigned int screenHeight;
	unsigned int i2cBusMaster;
	unsigned int i2cLcdAddress;
	unsigned int i2cScan;
	unsigned int i2cLcdFlip;
	unsigned int i2cLcdOnContrast;
	unsigned int i2cLcdDimContrast;
	unsigned int i2cLcdDimTime;
	unsigned int i2cLcdUseCBMChar;
	LCD_MODEL i2cLcdModel = LCD_UNKNOWN;
	const char *i2cLcdModelName;

	float scrollHighlightRate;

	unsigned int keyboardBrowseLCDScreen;

        u8 buttonEnter;
        u8 buttonUp;
        u8 buttonDown;
        u8 buttonBack;
        u8 buttonInsert;
	u8 CMDHDButtonSwap8;
	u8 CMDHDButtonSwap9;
	u8 CMDHDButtonWP;
	u8 CMDHDButtonReset;
	u8 CMDHDButtonExit;

	char starFileName[256];
	char C128BootSectorName[256];
	char autoBaseName[256];
	char LCDName[256];
	char LcdLogoName[256];

	char autoMountImageName[256];
	char ROMFontName[256];
	char ROMNameCMDHD[256];
	char ROMName[256];
	char ROMNameSlot2[256];
	char ROMNameSlot3[256];
	char ROMNameSlot4[256];
	char ROMNameSlot5[256];
	char ROMNameSlot6[256];
	char ROMNameSlot7[256];
	char ROMNameSlot8[256];
	char ROMName1581[256];

	char newDiskType[32];

	//ROTARY: Added for rotary encoder support - 09/05/2019 by Geo...
	unsigned int rotaryEncoderEnable;
	//ROTARY: Added for rotary encoder inversion (Issue#185) - 08/13/2020 by Geo...
	unsigned int rotaryEncoderInvert;
	// headless
	unsigned int headLess;

#if defined (__CIRCLE__)
	// WiFi & Networking
	unsigned int netWifi;
	unsigned int netEthernet;
	// maximum size of webpage
	unsigned maxContentSize;
	//  and multipart elements
	unsigned maxMultipartSize; 

	// Healthmonitor Console, default is off
	unsigned int noHealthMonitor;

	// use DHCP
	unsigned int useDHCP;

	// Timezone, multiplied by 60, defaults to 2.0 (CEST)
	float TZ;

#endif	
};
#endif
