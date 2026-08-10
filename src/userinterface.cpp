//
// userinterface.cpp
//
// MiniDexed - Dexed FM synthesizer for bare metal Raspberry Pi
// Copyright (C) 2022  The MiniDexed Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include "userinterface.h"
#include "minidexed.h"
#include <circle/logger.h>
#include <circle/string.h>
#include <circle/startup.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
LOGMODULE ("ui");

namespace
{
	// Second encoder: physical pins 11/13/15 on Raspberry Pi.
	// BCM GPIO numbering: CLK=17, DT=27, SW=22.
	static const unsigned Encoder2PinClock  = 17;
	static const unsigned Encoder2PinData   = 27;
	static const unsigned Encoder2PinSwitch = 22;
	static const unsigned ExtendedBlinkMS   = 550;

	enum TExtendedMixerParameter
	{
		ExtendedMixerVoice = 0,
		ExtendedMixerVolume,
		ExtendedMixerPan,
		ExtendedMixerReverbSend,
		ExtendedMixerCutoff,
		ExtendedMixerResonance,
		ExtendedMixerPortamento,
		ExtendedMixerParameterCount
	};

	static int ClampInt (int nValue, int nMinimum, int nMaximum)
	{
		if (nValue < nMinimum) return nMinimum;
		if (nValue > nMaximum) return nMaximum;
		return nValue;
	}

	static bool LooksLikeLegacyPerformanceGridLine (const char *pText)
	{
		if (!pText)
		{
			return false;
		}

		// The old 1602 Performance overview uses four fields separated by one
		// space.  Depending on the parameter each field is 2 or 3 characters:
		// PAN can be "-7 +7 -2 +2" (11 chars), while Voice/Volume use
		// "AAA BBB CCC DDD" (15 chars).  Accept every 2/3-character
		// combination so none of the legacy page-2 variants reaches rows 1-2.
		const size_t nLen = strlen (pText);
		if (nLen < 11 || nLen > 15)
		{
			return false;
		}

		for (unsigned w0 = 2; w0 <= 3; ++w0)
		for (unsigned w1 = 2; w1 <= 3; ++w1)
		for (unsigned w2 = 2; w2 <= 3; ++w2)
		for (unsigned w3 = 2; w3 <= 3; ++w3)
		{
			const size_t p1 = w0;
			const size_t p2 = p1 + 1 + w1;
			const size_t p3 = p2 + 1 + w2;
			const size_t nExpected = p3 + 1 + w3;

			if (nExpected == nLen
			 && pText[p1] == ' '
			 && pText[p2] == ' '
			 && pText[p3] == ' ')
			{
				return true;
			}
		}

		return false;
	}

	static void ShortVoice3 (const std::string &Name, char Result[4])
	{
		unsigned nOut = 0;
		for (unsigned i = 0; i < Name.length () && nOut < 3; ++i)
		{
			char c = Name[i];
			if (c == ' ' || c == '\t') continue;
			if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
			Result[nOut++] = c;
		}
		while (nOut < 3) Result[nOut++] = '-';
		Result[3] = '\0';
	}
}

CUserInterface::CUserInterface (CMiniDexed *pMiniDexed, CGPIOManager *pGPIOManager, CI2CMaster *pI2CMaster, CSPIMaster *pSPIMaster, CConfig *pConfig)
:	m_pMiniDexed (pMiniDexed),
	m_pGPIOManager (pGPIOManager),
	m_pI2CMaster (pI2CMaster),
	m_pSPIMaster (pSPIMaster),
	m_pConfig (pConfig),
	m_pLCD (0),
	m_pHD44780 (0),
	m_pSSD1306 (0),
	m_pST7789Display (0),
	m_pST7789 (0),
	m_pLCDBuffered (0),
	m_pUIButtons (0),
	m_pRotaryEncoder (0),
	m_bSwitchPressed (false),
	m_pRotaryEncoder2 (0),
	m_bSwitchPressed2 (false),
	m_bEncoder2LongPressHandled (false),
	m_nEncoder2StepPending (0),
	m_bEncoder2ClickPending (false),
	m_bEncoder2HoldPending (false),
	m_bExtendedParameterSelect (false),
	m_bExtendedBlinkOn (true),
	m_bExtendedRedrawPending (false),
	m_nExtendedMixerTG (0),
	m_nExtendedMixerParameter (ExtendedMixerVoice),
	m_nExtendedLastPerformanceID (0xFFFFFFFFU),
	m_Menu (this, pMiniDexed, pConfig)
{
}
CUserInterface::~CUserInterface (void)
{
	delete m_pRotaryEncoder2;
	delete m_pRotaryEncoder;
	delete m_pUIButtons;
	delete m_pLCDBuffered;
	delete m_pLCD;
}

bool CUserInterface::Initialize (void)
{
	assert (m_pConfig);
	if (m_pConfig->GetLCDEnabled ())
	{
		unsigned i2caddr = m_pConfig->GetLCDI2CAddress ();
		unsigned ssd1306addr = m_pConfig->GetSSD1306LCDI2CAddress ();
		bool st7789 = m_pConfig->GetST7789Enabled ();
		if (ssd1306addr != 0) {
			m_pSSD1306 = new CSSD1306Device (m_pConfig->GetSSD1306LCDWidth (), m_pConfig->GetSSD1306LCDHeight (),
											 m_pI2CMaster, ssd1306addr,
											 m_pConfig->GetSSD1306LCDRotate (), m_pConfig->GetSSD1306LCDMirror ());
			if (!m_pSSD1306->Initialize ())
			{
				LOGDBG("LCD: SSD1306 initialization failed");
				return false;
			}
			LOGDBG ("LCD: SSD1306");
			m_pLCD = m_pSSD1306;
		}
		else if (st7789)
		{
			if (m_pSPIMaster == nullptr)
			{
				LOGDBG("LCD: ST7789 Enabled but SPI Initialisation Failed");
				return false;
			}
			unsigned long nSPIClock = 1000 * m_pConfig->GetSPIClockKHz();
			unsigned nSPIMode = m_pConfig->GetSPIMode();
			unsigned nCPHA = (nSPIMode & 1) ? 1 : 0;
			unsigned nCPOL = (nSPIMode & 2) ? 1 : 0;
			LOGDBG("SPI: CPOL=%u; CPHA=%u; CLK=%u",nCPOL,nCPHA,nSPIClock);
			m_pST7789Display = new CST7789Display (m_pSPIMaster,
							m_pConfig->GetST7789Data(),
							m_pConfig->GetST7789Reset(),
							m_pConfig->GetST7789Backlight(),
							m_pConfig->GetST7789Width(),
							m_pConfig->GetST7789Height(),
							nCPOL, nCPHA, nSPIClock,
							m_pConfig->GetST7789Select());
			if (m_pST7789Display->Initialize())
			{
				m_pST7789Display->SetRotation (m_pConfig->GetST7789Rotation());
				bool bLargeFont = !(m_pConfig->GetST7789SmallFont());
				m_pST7789 = new CST7789Device (m_pSPIMaster, m_pST7789Display, m_pConfig->GetLCDColumns (), m_pConfig->GetLCDRows (), Font8x16, bLargeFont, bLargeFont);
				if (m_pST7789->Initialize())
				{
					LOGDBG ("LCD: ST7789");
					m_pLCD = m_pST7789;
				}
				else
				{
					LOGDBG ("LCD: Failed to initalize ST7789 character device");
					delete (m_pST7789);
					delete (m_pST7789Display);
					m_pST7789 = nullptr;
					m_pST7789Display = nullptr;
					return false;
				}
			}
			else
			{
				LOGDBG ("LCD: Failed to initialize ST7789 display");
				delete (m_pST7789Display);
				m_pST7789Display = nullptr;
				return false;
			}
		}
		else if (i2caddr == 0)
		{
			m_pHD44780 = new CHD44780Device (m_pConfig->GetLCDColumns (), m_pConfig->GetLCDRows (),
								 m_pConfig->GetLCDPinData4 (),
								 m_pConfig->GetLCDPinData5 (),
								 m_pConfig->GetLCDPinData6 (),
								 m_pConfig->GetLCDPinData7 (),
								 m_pConfig->GetLCDPinEnable (),
								 m_pConfig->GetLCDPinRegisterSelect (),
								 m_pConfig->GetLCDPinReadWrite ());
			if (!m_pHD44780->Initialize ())
			{
				LOGDBG("LCD: HD44780 initialization failed");
				return false;
			}
			LOGDBG ("LCD: HD44780");
			m_pLCD = m_pHD44780;
		}
		else
		{
			m_pHD44780 = new CHD44780Device (m_pI2CMaster, i2caddr,
							m_pConfig->GetLCDColumns (), m_pConfig->GetLCDRows ());
			if (!m_pHD44780->Initialize ())
			{
				LOGDBG("LCD: HD44780 (I2C) initialization failed");
				return false;
			}
			LOGDBG ("LCD: HD44780 I2C");
			m_pLCD = m_pHD44780;
		}
		assert (m_pLCD);
		m_pLCDBuffered = new CWriteBufferDevice (m_pLCD);
		assert (m_pLCDBuffered);
		// clear sceen and go to top left corner
		LCDWrite ("\x1B[H\x1B[J");		// cursor home and clear screen
		LCDWrite ("\x1B[?25l\x1B""d+");		// cursor off, autopage mode
		LCDWrite ("MiniDexed\nLoading...");
		m_pLCDBuffered->Update ();

		LOGDBG ("LCD initialized");
	}

	m_pUIButtons = new CUIButtons (	m_pConfig );
	assert (m_pUIButtons);

	if (!m_pUIButtons->Initialize ())
	{
		return false;
	}
	m_pUIButtons->RegisterEventHandler (UIButtonsEventStub, this);
	UISetMIDIButtonChannel (m_pConfig->GetMIDIButtonCh ());

	LOGDBG ("Button User Interface initialized");

	if (m_pConfig->GetEncoderEnabled ())
	{
		m_pRotaryEncoder = new CKY040 (m_pConfig->GetEncoderPinClock (),
					       m_pConfig->GetEncoderPinData (),
					       m_pConfig->GetButtonPinShortcut (),
					       m_pGPIOManager,
					       m_pConfig->GetEncoderDetents ());
		assert (m_pRotaryEncoder);
		if (!m_pRotaryEncoder->Initialize ())
		{
			return false;
		}

		m_pRotaryEncoder->RegisterEventHandler (EncoderEventStub, this);

		LOGDBG ("Rotary encoder initialized");

		// Encoder 2 is intentionally independent from CUIMenu.  It drives only
		// the lower SH1106 extended mixer and therefore cannot move the original
		// MiniDexed menu controlled by Encoder 1.
		m_pRotaryEncoder2 = new CKY040 (Encoder2PinClock,
						 Encoder2PinData,
						 Encoder2PinSwitch,
						 m_pGPIOManager,
						 m_pConfig->GetEncoderDetents ());
		assert (m_pRotaryEncoder2);
		if (!m_pRotaryEncoder2->Initialize ())
		{
			LOGDBG ("Extended rotary encoder initialization failed");
			return false;
		}
		m_pRotaryEncoder2->RegisterEventHandler (Encoder2EventStub, this);
		LOGDBG ("Extended rotary encoder initialized: GPIO17/27/22");
	}

	m_Menu.EventHandler (CUIMenu::MenuEventUpdate);

	// Rows 1-2 stay with the original MiniDexed UI.
	// Rows 3-4 start directly in the Performance mixer.
	DisplayExtendedMixer ();
	ArmExtendedBlinkTimer ();

	return true;
}

void CUserInterface::Process (void)
{
	// Encoder 2 callbacks are interrupt/timer callbacks. Keep them tiny and
	// perform synth changes + OLED I2C writes here in normal UI context.
	ProcessEncoder2Events ();

	if (m_pLCDBuffered)
	{
		m_pLCDBuffered->Update ();
	}
	if (m_pUIButtons)
	{
		m_pUIButtons->Update();
	}
	if (m_bExtendedRedrawPending)
	{
		m_bExtendedRedrawPending = false;
		DisplayExtendedMixer ();
	}
}

void CUserInterface::ParameterChanged (void)
{
	m_Menu.EventHandler (CUIMenu::MenuEventUpdateParameter);

	// Do NOT redraw the OLED synchronously here. Loading a Performance calls
	// ParameterChanged() many times (for the parameters of all TGs). A full
	// I2C redraw on every call makes Performance loading take several seconds.
	// Coalesce all those notifications into one redraw in Process().
	m_bExtendedRedrawPending = true;
}
void CUserInterface::DisplayChanged (void)
{
	m_Menu.EventHandler (CUIMenu::MenuEventUpdate);

	// Same coalescing rule as ParameterChanged().
	m_bExtendedRedrawPending = true;
}

void CUserInterface::ShowVoiceDataElement (unsigned nTG, unsigned nVoiceDataElement, unsigned nValue)
{
	m_Menu.ShowVoiceDataElement (nTG, nVoiceDataElement, nValue);
}

void CUserInterface::ShowAltPotController (unsigned nTG, const char *pParameterName, int nValue)
{
	m_Menu.ShowAltPotController (nTG, pParameterName, nValue);
}
void CUserInterface::DisplayWrite (const char *pMenu, const char *pParam, const char *pValue,
				   bool bArrowDown, bool bArrowUp)
{
	assert (pMenu);
	assert (pParam);
	assert (pValue);

	// The old 1602 Performance page-2 timer still exists in CUIMenu.
	// On the SH1106 extended UI, suppress only its distinctive 4x3-token
	// grid so rows 1-2 never alternate away from the original menu page.
	if (m_pConfig->GetSSD1306LCDHeight () == 64
	 && pMenu[0] == '\0'
	 && LooksLikeLegacyPerformanceGridLine (pParam)
	 && LooksLikeLegacyPerformanceGridLine (pValue))
	{
		return;
	}

	CString Msg ("\x1B[H\E[?25l");		// cursor home and off

	// first line
	Msg.Append (pParam);

	size_t nLen = strlen (pParam) + strlen (pMenu);
	if (nLen < m_pConfig->GetLCDColumns ())
	{
		for (unsigned i = m_pConfig->GetLCDColumns ()-nLen; i > 0; i--)
		{
			Msg.Append (" ");
		}
	}

	Msg.Append (pMenu);
	// second line
	CString Value (" ");
	if (bArrowDown)
	{
		Value = "<";			// arrow left character
	}

	Value.Append (pValue);

	if (bArrowUp)
	{
		if (Value.GetLength () < m_pConfig->GetLCDColumns ()-1)
		{
			for (unsigned i = m_pConfig->GetLCDColumns ()-Value.GetLength ()-1; i > 0; i--)
			{
				Value.Append (" ");
			}
		}

		Value.Append (">");		// arrow right character
	}

	Msg.Append (Value);
	if (Value.GetLength () < m_pConfig->GetLCDColumns ())
	{
		Msg.Append ("\x1B[K");		// clear end of line
	}

	LCDWrite (Msg);
}

void CUserInterface::DisplayWriteLower (const char *pLine3, const char *pLine4)
{
	assert (pLine3);
	assert (pLine4);

	// Non-styled fallback.  The extended mixer itself uses DrawLowerLines()
	// so it can invert the selected TG without spending display characters.
	CString Msg ("\x1B[3;1H\x1B[?25l");
	Msg.Append (pLine3);
	Msg.Append ("\x1B[K\x1B[4;1H");
	Msg.Append (pLine4);
	Msg.Append ("\x1B[K");
	LCDWrite (Msg);
}

void CUserInterface::ArmExtendedBlinkTimer (void)
{
	CTimer::Get ()->StartKernelTimer (MSEC2HZ (ExtendedBlinkMS),
					 ExtendedBlinkTimerHandler, 0, this);
}

void CUserInterface::ExtendedBlinkTimerHandler (TKernelTimerHandle hTimer,
						 void *pParam, void *pContext)
{
	(void) hTimer;
	(void) pParam;
	CUserInterface *pThis = static_cast<CUserInterface *> (pContext);
	assert (pThis);

	pThis->m_bExtendedBlinkOn = !pThis->m_bExtendedBlinkOn;
	pThis->m_bExtendedRedrawPending = true;
	pThis->ArmExtendedBlinkTimer ();
}

void CUserInterface::DisplayExtendedMixer (void)
{
	// This extension is only valid for our 128x64 SH1106 compatibility path.
	if (m_pConfig->GetSSD1306LCDI2CAddress () == 0
	 || m_pConfig->GetSSD1306LCDHeight () != 64
	 || !m_pMiniDexed || !m_pSSD1306)
	{
		return;
	}

	// Loading another Performance always returns the extension to its
	// default VOICE overview.
	const unsigned nPerformanceID = m_pMiniDexed->GetActualPerformanceID ();
	if (nPerformanceID != m_nExtendedLastPerformanceID)
	{
		m_nExtendedLastPerformanceID = nPerformanceID;
		m_nExtendedMixerParameter = ExtendedMixerVoice;
		m_nExtendedMixerTG = 0;
		m_bExtendedParameterSelect = false;
		m_bExtendedBlinkOn = true;
	}

	char Line3[17];
	char Line4[17];
	memset (Line3, ' ', 16);
	memset (Line4, ' ', 16);
	Line3[16] = '\0';
	Line4[16] = '\0';

	int nInvertLine = -1;
	unsigned nInvertStart = 0;
	unsigned nInvertLength = 0;

	if (m_bExtendedParameterSelect)
	{
		const char *pShort = "VOL";
		const char *pLong = "VOLUME";
		switch (m_nExtendedMixerParameter)
		{
		case ExtendedMixerVoice:        pShort = "VOI"; pLong = "VOICE"; break;
		case ExtendedMixerPan:          pShort = "PAN"; pLong = "PAN"; break;
		case ExtendedMixerReverbSend:   pShort = "REV"; pLong = "REVERB SEND"; break;
		case ExtendedMixerCutoff:       pShort = "CUT"; pLong = "CUTOFF"; break;
		case ExtendedMixerResonance:    pShort = "RES"; pLong = "RESONANCE"; break;
		case ExtendedMixerPortamento:   pShort = "POR"; pLong = "PORTAMENTO"; break;
		case ExtendedMixerVolume:
		default:                        pShort = "VOL"; pLong = "VOLUME"; break;
		}

		const char *pTitle = "SELECT PARAMETER"; // exactly 16 chars
		memcpy (Line3, pTitle, strlen (pTitle));
		const unsigned nLongLen = strlen (pLong);
		const unsigned nStart = nLongLen < 16 ? (16 - nLongLen) / 2 : 0;
		memcpy (Line4 + nStart, pLong, nLongLen < 16 ? nLongLen : 16);

		if (m_bExtendedBlinkOn)
		{
			// Pulse only the short identifier embedded at the center of the name.
			// For long names invert the whole visible name; it is clearer on OLED.
			nInvertLine = 1;
			nInvertStart = nStart;
			nInvertLength = nLongLen < 16 ? nLongLen : 16;
		}
		(void) pShort;
		m_pSSD1306->DrawLowerLines (Line3, Line4, nInvertLine,
					     nInvertStart, nInvertLength, true);
		return;
	}

	unsigned nTGs = m_pConfig->GetToneGenerators ();
	if (nTGs > 8) nTGs = 8;

	if (m_nExtendedMixerParameter == ExtendedMixerVoice)
	{
		for (unsigned nTG = 0; nTG < 8; ++nTG)
		{
			char Voice[4] = {'-', '-', '-', '\0'};
			if (nTG < nTGs)
			{
				ShortVoice3 (m_pMiniDexed->GetVoiceName (nTG), Voice);
			}

			char *pLine = nTG < 4 ? Line3 : Line4;
			unsigned nSlot = nTG & 3U;
			unsigned nOffset = nSlot * 4;
			pLine[nOffset] = Voice[0];
			pLine[nOffset + 1] = Voice[1];
			pLine[nOffset + 2] = Voice[2];
		}

		if (m_bExtendedBlinkOn && m_nExtendedMixerTG < nTGs)
		{
			nInvertLine = m_nExtendedMixerTG < 4 ? 0 : 1;
			nInvertStart = (m_nExtendedMixerTG & 3U) * 4;
			nInvertLength = 3;
		}
	}
	else
	{
		const char *pLabel = "VOL";
		CMiniDexed::TTGParameter Param = CMiniDexed::TGParameterVolume;
		switch (m_nExtendedMixerParameter)
		{
		case ExtendedMixerPan:
			pLabel = "PAN"; Param = CMiniDexed::TGParameterPan; break;
		case ExtendedMixerReverbSend:
			pLabel = "REV"; Param = CMiniDexed::TGParameterReverbSend; break;
		case ExtendedMixerCutoff:
			pLabel = "CUT"; Param = CMiniDexed::TGParameterCutoff; break;
		case ExtendedMixerResonance:
			pLabel = "RES"; Param = CMiniDexed::TGParameterResonance; break;
		case ExtendedMixerPortamento:
			pLabel = "POR"; Param = CMiniDexed::TGParameterPortamentoTime; break;
		case ExtendedMixerVolume:
		default:
			break;
		}

		Line3[0] = pLabel[0];
		Line3[1] = pLabel[1];
		Line3[2] = pLabel[2];

		for (unsigned nTG = 0; nTG < 8; ++nTG)
		{
			char Value[3] = {'-', '-', '\0'};
			if (nTG < nTGs)
			{
				int nRaw = m_pMiniDexed->GetTGParameter (Param, nTG);
				if (m_nExtendedMixerParameter == ExtendedMixerVolume
				 || m_nExtendedMixerParameter == ExtendedMixerReverbSend
				 || m_nExtendedMixerParameter == ExtendedMixerPortamento)
				{
					int nShown = ClampInt ((nRaw * 99 + 63) / 127, 0, 99);
					snprintf (Value, sizeof Value, "%02d", nShown);
				}
				else if (m_nExtendedMixerParameter == ExtendedMixerPan)
				{
					int nPan;
					if (nRaw < 64)
						nPan = -((64 - nRaw) * 9 + 32) / 64;
					else if (nRaw > 64)
						nPan = ((nRaw - 64) * 9 + 31) / 63;
					else
						nPan = 0;

					if (nPan > 0)
					{
						Value[0] = '+';
						Value[1] = (char) ('0' + ClampInt (nPan, 1, 9));
					}
					else if (nPan < 0)
					{
						Value[0] = '-';
						Value[1] = (char) ('0' + ClampInt (-nPan, 1, 9));
					}
					else
					{
						Value[0] = ' ';
						Value[1] = '0';
					}
				}
				else
				{
					int nShown = ClampInt (nRaw, 0, 99);
					snprintf (Value, sizeof Value, "%02d", nShown);
				}
			}

			char *pLine = nTG < 4 ? Line3 : Line4;
			const unsigned nSlot = nTG & 3U;
			const unsigned nOffset = 4 + nSlot * 3;
			pLine[nOffset] = Value[0];
			pLine[nOffset + 1] = Value[1];
		}

		if (m_bExtendedBlinkOn && m_nExtendedMixerTG < nTGs)
		{
			nInvertLine = m_nExtendedMixerTG < 4 ? 0 : 1;
			nInvertStart = 4 + (m_nExtendedMixerTG & 3U) * 3;
			nInvertLength = 2;
		}
	}

	m_pSSD1306->DrawLowerLines (Line3, Line4, nInvertLine,
				     nInvertStart, nInvertLength, true);
}

void CUserInterface::SelectExtendedMixerTG (int nDirection)
{
	unsigned nTGs = m_pConfig->GetToneGenerators ();
	if (nTGs == 0) return;
	if (nTGs > 8) nTGs = 8;

	if (nDirection > 0)
	{
		m_nExtendedMixerTG = (m_nExtendedMixerTG + 1) % nTGs;
	}
	else if (m_nExtendedMixerTG == 0)
	{
		m_nExtendedMixerTG = nTGs - 1;
	}
	else
	{
		--m_nExtendedMixerTG;
	}

	m_bExtendedBlinkOn = true;
	m_bExtendedRedrawPending = true;
}

void CUserInterface::SelectExtendedMixerParameter (int nDirection)
{
	if (nDirection > 0)
	{
		m_nExtendedMixerParameter =
			(m_nExtendedMixerParameter + 1) % ExtendedMixerParameterCount;
	}
	else if (m_nExtendedMixerParameter == 0)
	{
		m_nExtendedMixerParameter = ExtendedMixerParameterCount - 1;
	}
	else
	{
		--m_nExtendedMixerParameter;
	}
	m_bExtendedBlinkOn = true;
	m_bExtendedRedrawPending = true;
}

void CUserInterface::AdjustExtendedMixerValue (int nDirection)
{
	if (!m_pMiniDexed || nDirection == 0)
	{
		return;
	}

	// VOICE is the default overview, but it is also editable. Turning Encoder 2
	// changes the program of the selected TG inside its current voice bank.
	if (m_nExtendedMixerParameter == ExtendedMixerVoice)
	{
		int nProgram = m_pMiniDexed->GetTGParameter (CMiniDexed::TGParameterProgram,
						       m_nExtendedMixerTG);
		nProgram += nDirection;
		if (nProgram < 0) nProgram = 31;
		else if (nProgram > 31) nProgram = 0;

		m_pMiniDexed->SetTGParameter (CMiniDexed::TGParameterProgram,
					       nProgram, m_nExtendedMixerTG);
		m_bExtendedBlinkOn = true;
		m_bExtendedRedrawPending = true;
		return;
	}

	CMiniDexed::TTGParameter Param = CMiniDexed::TGParameterVolume;
	int nRaw = 0;
	int nNewRaw = 0;

	switch (m_nExtendedMixerParameter)
	{
	case ExtendedMixerPan:
		Param = CMiniDexed::TGParameterPan;
		nRaw = m_pMiniDexed->GetTGParameter (Param, m_nExtendedMixerTG);
		{
			int nPan;
			if (nRaw < 64)
				nPan = -((64 - nRaw) * 9 + 32) / 64;
			else if (nRaw > 64)
				nPan = ((nRaw - 64) * 9 + 31) / 63;
			else
				nPan = 0;
			nPan = ClampInt (nPan + nDirection, -9, 9);
			if (nPan < 0)
				nNewRaw = 64 - ((-nPan * 64 + 4) / 9);
			else if (nPan > 0)
				nNewRaw = 64 + ((nPan * 63 + 4) / 9);
			else
				nNewRaw = 64;
		}
		break;

	case ExtendedMixerReverbSend:
		Param = CMiniDexed::TGParameterReverbSend;
		nRaw = m_pMiniDexed->GetTGParameter (Param, m_nExtendedMixerTG);
		{
			int nShown = ClampInt ((nRaw * 99 + 63) / 127, 0, 99);
			nShown = ClampInt (nShown + nDirection, 0, 99);
			nNewRaw = (nShown * 127 + 49) / 99;
		}
		break;

	case ExtendedMixerCutoff:
		Param = CMiniDexed::TGParameterCutoff;
		nRaw = m_pMiniDexed->GetTGParameter (Param, m_nExtendedMixerTG);
		nNewRaw = ClampInt (nRaw + nDirection, 0, 99);
		break;

	case ExtendedMixerResonance:
		Param = CMiniDexed::TGParameterResonance;
		nRaw = m_pMiniDexed->GetTGParameter (Param, m_nExtendedMixerTG);
		nNewRaw = ClampInt (nRaw + nDirection, 0, 99);
		break;

	case ExtendedMixerPortamento:
		Param = CMiniDexed::TGParameterPortamentoTime;
		nRaw = m_pMiniDexed->GetTGParameter (Param, m_nExtendedMixerTG);
		{
			int nShown = ClampInt ((nRaw * 99 + 63) / 127, 0, 99);
			nShown = ClampInt (nShown + nDirection, 0, 99);
			nNewRaw = (nShown * 127 + 49) / 99;
		}
		break;

	case ExtendedMixerVolume:
	default:
		Param = CMiniDexed::TGParameterVolume;
		nRaw = m_pMiniDexed->GetTGParameter (Param, m_nExtendedMixerTG);
		{
			int nShown = ClampInt ((nRaw * 99 + 63) / 127, 0, 99);
			nShown = ClampInt (nShown + nDirection, 0, 99);
			nNewRaw = (nShown * 127 + 49) / 99;
		}
		break;
	}

	m_pMiniDexed->SetTGParameter (Param, nNewRaw, m_nExtendedMixerTG);
	m_bExtendedBlinkOn = true;
	m_bExtendedRedrawPending = true;
}

void CUserInterface::ProcessEncoder2Events (void)
{
	// Handle one detent per UI pass. This avoids bursts of synchronous OLED
	// transfers and keeps audio/MIDI work responsive even if the encoder spins fast.
	int nDirection = 0;
	if (m_nEncoder2StepPending > 0)
	{
		--m_nEncoder2StepPending;
		nDirection = +1;
	}
	else if (m_nEncoder2StepPending < 0)
	{
		++m_nEncoder2StepPending;
		nDirection = -1;
	}

	if (m_bEncoder2HoldPending)
	{
		m_bEncoder2HoldPending = false;
		if (m_bExtendedParameterSelect)
		{
			m_bExtendedParameterSelect = false;
			m_nExtendedMixerParameter = ExtendedMixerVoice;
		}
		else
		{
			m_bExtendedParameterSelect = true;
			m_nExtendedMixerParameter = ExtendedMixerVolume;
		}
		m_bExtendedBlinkOn = true;
		m_bExtendedRedrawPending = true;
	}

	if (m_bEncoder2ClickPending)
	{
		m_bEncoder2ClickPending = false;
		if (m_bExtendedParameterSelect)
		{
			m_bExtendedParameterSelect = false;
			m_bExtendedBlinkOn = true;
			m_bExtendedRedrawPending = true;
		}
		else
		{
			SelectExtendedMixerTG (+1);
		}
	}

	if (nDirection != 0)
	{
		if (m_bExtendedParameterSelect)
		{
			SelectExtendedMixerParameter (nDirection);
		}
		else
		{
			AdjustExtendedMixerValue (nDirection);
		}
	}
}

void CUserInterface::Encoder2EventHandler (CKY040::TEvent Event)
{
	// CKY040 rotation events are emitted directly from GPIO interrupt context
	// (and switch click/hold from kernel timer context). Never touch I2C or the
	// synth here; only record pending work for Process().
	switch (Event)
	{
	case CKY040::EventSwitchDown:
		m_bSwitchPressed2 = true;
		m_bEncoder2LongPressHandled = false;
		break;

	case CKY040::EventSwitchUp:
		m_bSwitchPressed2 = false;
		m_bEncoder2LongPressHandled = false;
		break;

	case CKY040::EventSwitchClick:
		m_bEncoder2ClickPending = true;
		break;

	case CKY040::EventClockwise:
		if (!m_bSwitchPressed2 && m_nEncoder2StepPending < 16)
		{
			++m_nEncoder2StepPending;
		}
		break;

	case CKY040::EventCounterclockwise:
		if (!m_bSwitchPressed2 && m_nEncoder2StepPending > -16)
		{
			--m_nEncoder2StepPending;
		}
		break;

	case CKY040::EventSwitchHold:
		if (m_bSwitchPressed2 && !m_bEncoder2LongPressHandled)
		{
			m_bEncoder2LongPressHandled = true;
			m_bEncoder2HoldPending = true;
		}
		break;

	default:
		break;
	}
}

void CUserInterface::Encoder2EventStub (CKY040::TEvent Event, void *pParam)
{
	CUserInterface *pThis = static_cast<CUserInterface *> (pParam);
	assert (pThis != 0);
	pThis->Encoder2EventHandler (Event);
}

void CUserInterface::LCDWrite (const char *pString)
{
	if (m_pLCDBuffered)
	{
		m_pLCDBuffered->Write (pString, strlen (pString));
	}
}

void CUserInterface::EncoderEventHandler (CKY040::TEvent Event)
{
	switch (Event)
	{
	case CKY040::EventSwitchDown:
		m_bSwitchPressed = true;
		break;

	case CKY040::EventSwitchUp:
		m_bSwitchPressed = false;
		break;
	case CKY040::EventClockwise:
		if (m_bSwitchPressed) {
			// We must reset the encoder switch button to prevent events from being
			// triggered after the encoder is rotated
			m_pUIButtons->ResetButton(m_pConfig->GetButtonPinShortcut());
			m_Menu.EventHandler(CUIMenu::MenuEventPressAndStepUp);

		}
		else {
			m_Menu.EventHandler(CUIMenu::MenuEventStepUp);
		}
		break;
	case CKY040::EventCounterclockwise:
		if (m_bSwitchPressed) {
			m_pUIButtons->ResetButton(m_pConfig->GetButtonPinShortcut());
			m_Menu.EventHandler(CUIMenu::MenuEventPressAndStepDown);
		}
		else {
			m_Menu.EventHandler(CUIMenu::MenuEventStepDown);
		}
		break;

	case CKY040::EventSwitchHold:
		if (m_pRotaryEncoder->GetHoldSeconds () >= 120)
		{
			delete m_pLCD;		// reset LCD

			reboot ();
		}
		break;

	default:
		break;
	}
}
void CUserInterface::EncoderEventStub (CKY040::TEvent Event, void *pParam)
{
	CUserInterface *pThis = static_cast<CUserInterface *> (pParam);
	assert (pThis != 0);

	pThis->EncoderEventHandler (Event);
}

void CUserInterface::SyncExtendedFromUIButtonEvent (CUIButton::BtnEvent Event)
{
	// MIDI buttons use the same CUIButton events as physical buttons. Mirror
	// their TG/parameter choice into the lower Performance mixer while leaving
	// the original upper-screen menu/overlay behavior untouched.
	switch (Event)
	{
	case CUIButton::BtnEventTG1: m_nExtendedMixerTG = 0; break;
	case CUIButton::BtnEventTG2: m_nExtendedMixerTG = 1; break;
	case CUIButton::BtnEventTG3: m_nExtendedMixerTG = 2; break;
	case CUIButton::BtnEventTG4: m_nExtendedMixerTG = 3; break;
	case CUIButton::BtnEventTG5: m_nExtendedMixerTG = 4; break;
	case CUIButton::BtnEventTG6: m_nExtendedMixerTG = 5; break;
	case CUIButton::BtnEventTG7: m_nExtendedMixerTG = 6; break;
	case CUIButton::BtnEventTG8: m_nExtendedMixerTG = 7; break;

	case CUIButton::BtnEventTGVoice:
		m_nExtendedMixerParameter = ExtendedMixerVoice;
		m_bExtendedParameterSelect = false;
		break;
	case CUIButton::BtnEventTGVolume:
		m_nExtendedMixerParameter = ExtendedMixerVolume;
		m_bExtendedParameterSelect = false;
		break;
	case CUIButton::BtnEventTGPan:
		m_nExtendedMixerParameter = ExtendedMixerPan;
		m_bExtendedParameterSelect = false;
		break;
	case CUIButton::BtnEventTGReverbSend:
		m_nExtendedMixerParameter = ExtendedMixerReverbSend;
		m_bExtendedParameterSelect = false;
		break;
	case CUIButton::BtnEventTGCutoff:
		m_nExtendedMixerParameter = ExtendedMixerCutoff;
		m_bExtendedParameterSelect = false;
		break;
	case CUIButton::BtnEventTGResonance:
		m_nExtendedMixerParameter = ExtendedMixerResonance;
		m_bExtendedParameterSelect = false;
		break;
	case CUIButton::BtnEventTGPortamento:
		m_nExtendedMixerParameter = ExtendedMixerPortamento;
		m_bExtendedParameterSelect = false;
		break;
	default:
		return;
	}

	unsigned nTGs = m_pConfig->GetToneGenerators ();
	if (nTGs > 8) nTGs = 8;
	if (nTGs && m_nExtendedMixerTG >= nTGs)
	{
		m_nExtendedMixerTG = nTGs - 1;
	}
	m_bExtendedBlinkOn = true;
	m_bExtendedRedrawPending = true;
}

void CUserInterface::UIButtonsEventHandler (CUIButton::BtnEvent Event)
{
	SyncExtendedFromUIButtonEvent (Event);

	switch (Event)
	{
	case CUIButton::BtnEventPrev:
		m_Menu.EventHandler (CUIMenu::MenuEventStepDown);
		break;

	case CUIButton::BtnEventNext:
		m_Menu.EventHandler (CUIMenu::MenuEventStepUp);
		break;
	case CUIButton::BtnEventBack:
		m_Menu.EventHandler (CUIMenu::MenuEventBack);
		break;

	case CUIButton::BtnEventSelect:
		m_Menu.EventHandler (CUIMenu::MenuEventSelect);
		break;

	case CUIButton::BtnEventHome:
		m_Menu.EventHandler (CUIMenu::MenuEventHome);
		break;

	case CUIButton::BtnEventPgmUp:
		m_Menu.EventHandler (CUIMenu::MenuEventPgmUp);
		break;

	case CUIButton::BtnEventPgmDown:
		m_Menu.EventHandler (CUIMenu::MenuEventPgmDown);
		break;
	case CUIButton::BtnEventBankUp:
		m_Menu.EventHandler (CUIMenu::MenuEventBankUp);
		break;

	case CUIButton::BtnEventBankDown:
		m_Menu.EventHandler (CUIMenu::MenuEventBankDown);
		break;

	case CUIButton::BtnEventTGUp:
		m_Menu.EventHandler (CUIMenu::MenuEventTGUp);
		break;

	case CUIButton::BtnEventTGDown:
		m_Menu.EventHandler (CUIMenu::MenuEventTGDown);
		break;

	case CUIButton::BtnEventTG1:
		m_Menu.EventHandler (CUIMenu::MenuEventTG1);
		break;
	case CUIButton::BtnEventTG2:
		m_Menu.EventHandler (CUIMenu::MenuEventTG2);
		break;

	case CUIButton::BtnEventTG3:
		m_Menu.EventHandler (CUIMenu::MenuEventTG3);
		break;

	case CUIButton::BtnEventTG4:
		m_Menu.EventHandler (CUIMenu::MenuEventTG4);
		break;

	case CUIButton::BtnEventTG5:
		m_Menu.EventHandler (CUIMenu::MenuEventTG5);
		break;

	case CUIButton::BtnEventTG6:
		m_Menu.EventHandler (CUIMenu::MenuEventTG6);
		break;
	case CUIButton::BtnEventTG7:
		m_Menu.EventHandler (CUIMenu::MenuEventTG7);
		break;

	case CUIButton::BtnEventTG8:
		m_Menu.EventHandler (CUIMenu::MenuEventTG8);
		break;

	case CUIButton::BtnEventEffects:
		m_Menu.EventHandler (CUIMenu::MenuEventEffects);
		break;

	case CUIButton::BtnEventMasterVolume:
		m_Menu.EventHandler (CUIMenu::MenuEventMasterVolume);
		break;

	case CUIButton::BtnEventPerformance:
		m_Menu.EventHandler (CUIMenu::MenuEventPerformance);
		break;
	case CUIButton::BtnEventTGVoice:
		m_Menu.EventHandler (CUIMenu::MenuEventTGVoice);
		break;

	case CUIButton::BtnEventTGBank:
		m_Menu.EventHandler (CUIMenu::MenuEventTGBank);
		break;

	case CUIButton::BtnEventTGVolume:
		m_Menu.EventHandler (CUIMenu::MenuEventTGVolume);
		break;

	case CUIButton::BtnEventTGPan:
		m_Menu.EventHandler (CUIMenu::MenuEventTGPan);
		break;

	case CUIButton::BtnEventTGReverbSend:
		m_Menu.EventHandler (CUIMenu::MenuEventTGReverbSend);
		break;
	case CUIButton::BtnEventTGDetune:
		m_Menu.EventHandler (CUIMenu::MenuEventTGDetune);
		break;

	case CUIButton::BtnEventTGOctave:
		m_Menu.EventHandler (CUIMenu::MenuEventTGOctave);
		break;

	case CUIButton::BtnEventTGCutoff:
		m_Menu.EventHandler (CUIMenu::MenuEventTGCutoff);
		break;

	case CUIButton::BtnEventTGResonance:
		m_Menu.EventHandler (CUIMenu::MenuEventTGResonance);
		break;

	case CUIButton::BtnEventTGPitchBend:
		m_Menu.EventHandler (CUIMenu::MenuEventTGPitchBend);
		break;
	case CUIButton::BtnEventTGPortamento:
		m_Menu.EventHandler (CUIMenu::MenuEventTGPortamento);
		break;

	case CUIButton::BtnEventTGPolyMono:
		m_Menu.EventHandler (CUIMenu::MenuEventTGPolyMono);
		break;

	case CUIButton::BtnEventTGModulation:
		m_Menu.EventHandler (CUIMenu::MenuEventTGModulation);
		break;

	case CUIButton::BtnEventTGChannel:
		m_Menu.EventHandler (CUIMenu::MenuEventTGChannel);
		break;
	case CUIButton::BtnEventTGEditVoice:
		m_Menu.EventHandler (CUIMenu::MenuEventTGEditVoice);
		break;

	case CUIButton::BtnEventTGSolo:
		m_Menu.EventHandler (CUIMenu::MenuEventTGSolo);
		break;

	case CUIButton::BtnEventAltPot:
		m_Menu.EventHandler (CUIMenu::MenuEventAltPot);
		break;
	case CUIButton::BtnEventAltPotPrev:
		m_Menu.EventHandler (CUIMenu::MenuEventAltPotPrev);
		break;

	case CUIButton::BtnEventAltPotNext:
		m_Menu.EventHandler (CUIMenu::MenuEventAltPotNext);
		break;
	case CUIButton::BtnEventAltPotMode:
		m_Menu.EventHandler (CUIMenu::MenuEventAltPotMode);
		break;

	default:
		break;
	}
}

void CUserInterface::UIButtonsEventStub (CUIButton::BtnEvent Event, void *pParam)
{
	CUserInterface *pThis = static_cast<CUserInterface *> (pParam);
	assert (pThis != 0);

	pThis->UIButtonsEventHandler (Event);
}
void CUserInterface::UIMIDICmdHandler (unsigned nMidiCh, unsigned nMidiType, unsigned nMidiData1, unsigned nMidiData2)
{
	if (m_nMIDIButtonCh == CMIDIDevice::Disabled)
	{
		// MIDI buttons are not enabled
		return;
	}
	if ((m_nMIDIButtonCh != nMidiCh) && (m_nMIDIButtonCh != CMIDIDevice::OmniMode))
	{
		// Message not on the MIDI Button channel and MIDI buttons not in OMNI mode
		return;
	}

	if (m_pUIButtons)
	{
		m_pUIButtons->BtnMIDICmdHandler (nMidiType, nMidiData1, nMidiData2);
	}
}
void CUserInterface::UISetMIDIButtonChannel (unsigned uCh)
{
	// Mirrors the logic in Performance Config for handling MIDI channel configuration
	if (uCh == 0)
	{
		m_nMIDIButtonCh = CMIDIDevice::Disabled;
		LOGNOTE("MIDI Button channel not set");
	}
	else if (uCh <= CMIDIDevice::Channels)
	{
		m_nMIDIButtonCh = uCh - 1;
		LOGNOTE("MIDI Button channel set to: %d", m_nMIDIButtonCh+1);
	}
	else
	{
		m_nMIDIButtonCh = CMIDIDevice::OmniMode;
		LOGNOTE("MIDI Button channel set to: OMNI");
	}
}
