//
// sh1106device.h
//
// SH1106 compatibility driver for MiniDexed sh1106-extended-ui.
// Based on the SH1106 MiniDexed driver by Sgw32 / PHOL-LABS and
// Circle display code by R. Stange.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
#ifndef _display_sh1106device_h
#define _display_sh1106device_h

#include <circle/device.h>
#include <circle/i2cmaster.h>
#include <circle/spinlock.h>
#include <circle/types.h>
#include <display/chardevice.h>

// 16 characters across 128 pixels: 8 pixels per character.
// DrawChar() keeps the original 6-pixel glyph centered in an 8-pixel cell.
#define SH1106_COLUMNS 16

// IMPORTANT: The class intentionally keeps the CSSD1306Device name.
// This makes it a drop-in replacement for MiniDexed's existing SSD1306
// path, so no config.cpp/userinterface.cpp changes are required yet.
class CSSD1306Device : public CCharDevice
{
public:
	CSSD1306Device (unsigned nWidth, unsigned nHeight,
		       CI2CMaster *pI2CMaster, u8 nAddress,
		       bool rotated=false, bool mirrored=false);
	~CSSD1306Device (void);

	boolean Initialize (void);

	// Draw rows 3 and 4 directly, optionally inverting one character range.
	// nInvertLine: -1 = none, 0 = row 3, 1 = row 4.
	void DrawLowerLines (const char *pLine3, const char *pLine4,
			     int nInvertLine = -1, unsigned nInvertStart = 0,
			     unsigned nInvertLength = 0, bool bImmediate = true);

private:
	void DevClearCursor (void) override;
	void DevSetCursor (unsigned nCursorX, unsigned nCursorY) override;
	void DevSetCursorMode (boolean bVisible) override;
	void DevSetChar (unsigned nPosX, unsigned nPosY, char chChar) override;
	void DevUpdateDisplay (void) override;

	void Clear (bool bImmediate = false);
	void Print (const char *pText, u8 nCursorX, u8 nCursorY,
		    bool bClearLine = false, bool bImmediate = false);

	void DrawChar (char chChar, u8 nCursorX, u8 nCursorY,
		       bool bInverted = false, bool bDoubleWidth = false);
	void Flip (void);
	void SetBacklightState (bool bEnabled);

private:
	unsigned m_nWidth;
	unsigned m_nHeight;
	CI2CMaster *m_pI2CMaster;
	u8 m_nAddress;
	bool m_bBacklightEnabled;
	bool m_bRotated;
	bool m_bMirrored;

	struct TFrameBufferUpdatePacket
	{
		u8 DataControlByte;
		u8 FrameBuffer[128 * 64 / 8];
	}
	PACKED;

	void WriteCommand (u8 nCommand) const;
	void WriteFrameBuffer (bool bForceFullUpdate = false) const;
	void SwapFrameBuffers (void);

	TFrameBufferUpdatePacket m_FrameBuffers[2];
	u8 m_nCurrentFrameBuffer;
};

#endif
