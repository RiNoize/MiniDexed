//
// sh1106device.cpp
//
// Much of this driver is based on code from:
//    mt32-pi - A baremetal MIDI synthesizer for Raspberry Pi
//    Copyright (C) 2020-2022 Dale Whinham <daleyo@gmail.com>
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2018-2022  R. Stange <rsta2@o2online.de>
//
// SH1106 integration adapted for MiniDexed sh1106-extended-ui.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
#include "sh1106device.h"
#include <circle/timer.h>
#include <circle/types.h>
#include <circle/util.h>
#include <assert.h>
#include <display/font6x8.h>

namespace
{
	constexpr u8 kColumnOffset = 2;
}

enum TSH1106Command : u8
{
	SetColumnAddressLow        = 0x00,
	SetColumnAddressHigh       = 0x10,
	SetStartLine               = 0x40,
	SetContrast                = 0x81,
	SetChargePump              = 0x8D,
	EntireDisplayOnResume      = 0xA4,
	SetNormalDisplay           = 0xA6,
	SetMultiplexRatio          = 0xA8,
	SetDisplayOff              = 0xAE,
	SetDisplayOn               = 0xAF,
	SetDisplayOffset           = 0xD3,
	SetDisplayClockDivideRatio = 0xD5,
	SetPrechargePeriod         = 0xD9,
	SetCOMPins                 = 0xDA,
	SetVCOMHDeselectLevel      = 0xDB,
	SetPageAddress             = 0xB0,
};

// Compile-time font/graphics conversion functions.
// The SH1106 stores pixel data in columns, while Font6x8 is row based.
namespace
{
	using CharData = u8[8];

	static constexpr u8 SingleColumn (const CharData& CharData, u8 nColumn)
	{
		u8 bit = 5 - nColumn;
		u8 column = 0;
		for (u8 i = 0; i < 8; ++i)
		{
			column |= (CharData[i] >> bit & 1) << i;
		}
		return column;
	}

	static constexpr u16 DoubleColumn (const CharData& CharData, u8 nColumn)
	{
		u8 singleColumn = SingleColumn (CharData, nColumn);
		u16 column = 0;

		for (u8 i = 0; i < 8; ++i)
		{
			bool bit = singleColumn >> i & 1;
			column |= bit << i * 2 | bit << (i * 2 + 1);
		}

		return column;
	}

	template<size_t N, class F>
	class Font
	{
	public:
		using Column = u16;
		using ColumnData = Column[6];

		constexpr Font (const CharData (&CharData)[N], F Function)
		: mCharData{{0}}
		{
			for (size_t i = 0; i < N; ++i)
			{
				for (u8 j = 0; j < 6; ++j)
				{
					mCharData[i][j] = Function (CharData[i], j);
				}
			}
		}

		const ColumnData& operator[] (size_t nIndex) const
		{
			return mCharData[nIndex];
		}

	private:
		ColumnData mCharData[N];
	};
}

constexpr auto FontDouble = Font<FONT_SIZE, decltype(DoubleColumn)> (Font6x8, DoubleColumn);

CSSD1306Device::CSSD1306Device (unsigned nWidth, unsigned nHeight,
				CI2CMaster *pI2CMaster, u8 nAddress,
				bool rotated, bool mirrored)
: CCharDevice (SH1106_COLUMNS, nHeight / 16),
  m_nWidth (nWidth),
  m_nHeight (nHeight),
  m_pI2CMaster (pI2CMaster),
  m_nAddress (nAddress),
  m_bBacklightEnabled (true),
  m_bRotated (rotated),
  m_bMirrored (mirrored),
  m_FrameBuffers{{0x40, {0}}, {0x40, {0}}},
  m_nCurrentFrameBuffer (0)
{
}

CSSD1306Device::~CSSD1306Device (void)
{
}

boolean CSSD1306Device::Initialize (void)
{
	assert (m_pI2CMaster != nullptr);

	// This branch supports the common 128x32 and 128x64 SH1106 modules.
	if (!(m_nHeight == 32 || m_nHeight == 64) || m_nWidth != 128)
	{
		return false;
	}

	const u8 nMultiplexRatio = m_nHeight - 1;
	const u8 nCOMPins = m_nHeight == 32 ? 0x02 : 0x12;

	const u8 nSegRemap =
		(m_bRotated && !m_bMirrored) || (!m_bRotated && m_bMirrored)
		? 0xA0 : 0xA1;
	const u8 nCOMScanDir = m_bRotated ? 0xC0 : 0xC8;

	const u8 InitSequence[] =
	{
		SetDisplayOff,
		SetDisplayClockDivideRatio, 0x80,
		SetMultiplexRatio, nMultiplexRatio,
		SetDisplayOffset, 0x00,
		SetStartLine | 0x00,
		SetChargePump, 0x14,
		nSegRemap,
		nCOMScanDir,
		SetCOMPins, nCOMPins,
		SetContrast, 0x7F,
		SetPrechargePeriod, 0x22,
		SetVCOMHDeselectLevel, 0x20,
		EntireDisplayOnResume,
		SetNormalDisplay,
		SetDisplayOn,
	};

	for (u8 nCommand : InitSequence)
	{
		WriteCommand (nCommand);
	}

	return CCharDevice::Initialize ();
}

void CSSD1306Device::DevClearCursor (void)
{
	Clear (TRUE);
}

void CSSD1306Device::DevSetCursorMode (boolean bVisible)
{
}

void CSSD1306Device::DevSetChar (unsigned nPosX, unsigned nPosY, char chChar)
{
	DrawChar (chChar, nPosX, nPosY, FALSE, FALSE);
}

void CSSD1306Device::DevSetCursor (unsigned nCursorX, unsigned nCursorY)
{
}

void CSSD1306Device::DevUpdateDisplay (void)
{
	WriteFrameBuffer (true);
}

void CSSD1306Device::WriteCommand (u8 nCommand) const
{
	const u8 Buffer[] = {0x80, nCommand};
	m_pI2CMaster->Write (m_nAddress, Buffer, sizeof Buffer);
}

void CSSD1306Device::WriteFrameBuffer (bool bForceFullUpdate) const
{
	WriteCommand (SetStartLine | 0x00);

	const size_t nFrameBufferSize = m_nWidth * m_nHeight / 8;
	const bool bNeedsUpdate = bForceFullUpdate
		|| memcmp (m_FrameBuffers[0].FrameBuffer,
			   m_FrameBuffers[1].FrameBuffer,
			   nFrameBufferSize) != 0;

	if (!bNeedsUpdate)
	{
		return;
	}

	assert (m_nWidth <= 128);
	const unsigned nPages = m_nHeight / 8;
	const u8 *pFrameBuffer = m_FrameBuffers[m_nCurrentFrameBuffer].FrameBuffer;

	for (unsigned nPage = 0; nPage < nPages; ++nPage)
	{
		const u8 nColumnStart = kColumnOffset;
		const u8 Cmds[] =
		{
			0x80, static_cast<u8> (SetPageAddress | nPage),
			0x80, static_cast<u8> (SetColumnAddressLow | (nColumnStart & 0x0F)),
			0x80, static_cast<u8> (SetColumnAddressHigh | ((nColumnStart >> 4) & 0x0F)),
		};
		m_pI2CMaster->Write (m_nAddress, Cmds, sizeof Cmds);

		u8 Buffer[1 + 128];
		Buffer[0] = 0x40;
		memcpy (Buffer + 1, pFrameBuffer + nPage * m_nWidth, m_nWidth);
		m_pI2CMaster->Write (m_nAddress, Buffer, 1 + m_nWidth);
	}
}

void CSSD1306Device::SwapFrameBuffers (void)
{
	m_nCurrentFrameBuffer = (m_nCurrentFrameBuffer + 1) % 2;
}

void CSSD1306Device::DrawChar (char chChar, u8 nCursorX, u8 nCursorY,
			       bool bInverted, bool bDoubleWidth)
{
	const size_t nRowOffset = nCursorY * m_nWidth * 2;
	// 16 columns on 128 pixels = 8 pixels per character cell.
	// Keep the original 6-pixel glyph intact and center it in that cell:
	// 1 blank pixel + 6 glyph pixels + 1 blank pixel.
	const size_t nCellWidth = bDoubleWidth ? 16 : 8;
	const size_t nColumnOffset = nCursorX * nCellWidth;
	u8 *pFrameBuffer = m_FrameBuffers[m_nCurrentFrameBuffer].FrameBuffer;

	if (chChar == '\xFF')
	{
		chChar = '\x80';
	}
	else if (chChar < ' ')
	{
		chChar = ' ';
	}

	// Clear the complete cell first so the left/right margins stay clean.
	for (size_t i = 0; i < nCellWidth; ++i)
	{
		const size_t nOffset = nRowOffset + nColumnOffset + i;
		pFrameBuffer[nOffset] = 0;
		pFrameBuffer[nOffset + m_nWidth] = 0;
	}

	for (u8 i = 0; i < 6; ++i)
	{
		u16 nFontColumn = FontDouble[static_cast<u8> (chChar - ' ')][i];

		if (i > 0 && bInverted)
		{
			nFontColumn ^= 0x3FFF;
		}

		nFontColumn <<= 2;

		const size_t nOffset = nRowOffset + nColumnOffset
			+ (bDoubleWidth ? 2 + i * 2 : 1 + i);
		pFrameBuffer[nOffset] = nFontColumn & 0xFF;
		pFrameBuffer[nOffset + m_nWidth] = (nFontColumn >> 8) & 0xFF;

		if (bDoubleWidth)
		{
			pFrameBuffer[nOffset + 1] = pFrameBuffer[nOffset];
			pFrameBuffer[nOffset + m_nWidth + 1]
				= pFrameBuffer[nOffset + m_nWidth];
		}
	}
}

void CSSD1306Device::Flip (void)
{
	WriteFrameBuffer ();
	SwapFrameBuffers ();
}

void CSSD1306Device::Print (const char *pText, u8 nCursorX, u8 nCursorY,
			    bool bClearLine, bool bImmediate)
{
	if (bClearLine)
	{
		for (u8 nChar = 0; nChar < nCursorX; ++nChar)
		{
			DrawChar (' ', nChar, nCursorY);
		}
	}

	while (*pText && nCursorX < SH1106_COLUMNS)
	{
		DrawChar (*pText++, nCursorX, nCursorY);
		++nCursorX;
	}

	if (bClearLine)
	{
		while (nCursorX < SH1106_COLUMNS)
		{
			DrawChar (' ', nCursorX++, nCursorY);
		}
	}

	if (bImmediate)
	{
		WriteFrameBuffer (true);
	}
}

void CSSD1306Device::Clear (bool bImmediate)
{
	u8 *pFrameBuffer = m_FrameBuffers[m_nCurrentFrameBuffer].FrameBuffer;
	memset (pFrameBuffer, 0, m_nWidth * m_nHeight / 8);

	if (bImmediate)
	{
		WriteFrameBuffer (true);
	}
}

void CSSD1306Device::SetBacklightState (bool bEnabled)
{
	m_bBacklightEnabled = bEnabled;
	WriteCommand (bEnabled ? SetDisplayOn : SetDisplayOff);
}
