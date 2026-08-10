//
// userinterface.h
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
#ifndef _userinterface_h
#define _userinterface_h
#include "config.h"
#include "uimenu.h"
#include "uibuttons.h"
#include <sensor/ky040.h>
#include <display/hd44780device.h>
// SH1106 compatibility driver for this branch. It exposes the same
// CSSD1306Device interface expected by the existing MiniDexed UI code.
#include "sh1106device.h"
#include <display/st7789device.h>
#include <circle/gpiomanager.h>
#include <circle/writebuffer.h>
#include <circle/i2cmaster.h>
#include <circle/spimaster.h>
#include <circle/timer.h>
#include <circle/spinlock.h>

class CMiniDexed;
class CUserInterface
{
public:
	CUserInterface (CMiniDexed *pMiniDexed, CGPIOManager *pGPIOManager, CI2CMaster *pI2CMaster, CSPIMaster *pSPIMaster, CConfig *pConfig);
	~CUserInterface (void);

	bool Initialize (void);

	void Process (void);

	void ParameterChanged (void);
	void DisplayChanged (void);
	// Show the existing voice-edit menu item for a raw DX7 voice data element.
	void ShowVoiceDataElement (unsigned nTG, unsigned nVoiceDataElement, unsigned nValue);
	void ShowAltPotController (unsigned nTG, const char *pParameterName, int nValue);

	// Write to display in this format:
	// +----------------+
	// |PARAM       MENU|
	// |[<]VALUE     [>]|
	// +----------------+
	void DisplayWrite (const char *pMenu, const char *pParam, const char *pValue,
			   bool bArrowDown, bool bArrowUp);

	// Write only rows 3 and 4 of the 128x64 SH1106.
	// Rows 1 and 2 remain controlled by the original MiniDexed UI.
	void DisplayWriteLower (const char *pLine3, const char *pLine4);
	// To be called from the MIDI device on reception of a MIDI CC/message.
	void UIMIDICmdHandler (unsigned nMidiCh, unsigned nMidiType, unsigned nMidiData1, unsigned nMidiData2);

	// Diagnostic MIDI monitor used by the last item of the TG menu.
	// Recording is lightweight and never writes the OLED from MIDI/IRQ context.
	void MIDIMonitorRecord (bool bRX, const u8 *pMessage, size_t nLength, unsigned nCable = 0);
	void MIDIMonitorEnter (void);
	void MIDIMonitorExit (void);
	void MIDIMonitorStep (int nDirection);
	void MIDIMonitorDisplay (void);

private:
	void LCDWrite (const char *pString);		// Print to optional HD44780 display
	void DisplayWriteTopRaw (const char *pLine1, const char *pLine2);
	const char *MIDIMonitorMatchButton (unsigned nType, unsigned nNumber) const;
	unsigned MIDIMonitorConfigValue (unsigned nPage, const char **ppName) const;
	void EncoderEventHandler (CKY040::TEvent Event);
	static void EncoderEventStub (CKY040::TEvent Event, void *pParam);

	// Encoder 2 controls only the lower/extended Performance mixer.
	void Encoder2EventHandler (CKY040::TEvent Event);
	static void Encoder2EventStub (CKY040::TEvent Event, void *pParam);
	static void ExtendedBlinkTimerHandler (TKernelTimerHandle hTimer, void *pParam, void *pContext);
	void ArmExtendedBlinkTimer (void);
	void DisplayExtendedMixer (void);
	void AdjustExtendedMixerValue (int nDirection);
	void SelectExtendedMixerTG (int nDirection);
	void SelectExtendedMixerParameter (int nDirection);
	void ProcessEncoder2Events (void);
	bool SyncExtendedFromUIButtonEvent (CUIButton::BtnEvent Event);
	void UIButtonsEventHandler (CUIButton::BtnEvent Event);
	static void UIButtonsEventStub (CUIButton::BtnEvent Event, void *pParam);
	void UISetMIDIButtonChannel (unsigned uCh);

private:
	CMiniDexed *m_pMiniDexed;
	CGPIOManager *m_pGPIOManager;
	CI2CMaster *m_pI2CMaster;
	CSPIMaster *m_pSPIMaster;
	CConfig *m_pConfig;
	CCharDevice    *m_pLCD;
	CHD44780Device *m_pHD44780;
	CSSD1306Device *m_pSSD1306;
	CST7789Display *m_pST7789Display;
	CST7789Device  *m_pST7789;
	CWriteBufferDevice *m_pLCDBuffered;

	CUIButtons *m_pUIButtons;

	unsigned m_nMIDIButtonCh;

	CKY040 *m_pRotaryEncoder;
	bool m_bSwitchPressed;

	CKY040 *m_pRotaryEncoder2;
	volatile bool m_bSwitchPressed2;
	volatile bool m_bEncoder2LongPressHandled;
	volatile int m_nEncoder2StepPending;
	volatile bool m_bEncoder2ClickPending;
	volatile bool m_bEncoder2HoldPending;
	bool m_bExtendedParameterSelect;
	bool m_bExtendedBlinkOn;
	bool m_bExtendedRedrawPending;
	unsigned m_nExtendedMixerTG;
	unsigned m_nExtendedMixerParameter;
	unsigned m_nExtendedLastPerformanceID;

	// MIDI monitor state. Only a small prefix of SysEx is retained; length is exact.
	CSpinLock m_MIDIMonitorLock;
	bool m_bMIDIMonitorActive;
	volatile bool m_bMIDIMonitorRedrawPending;
	unsigned m_nMIDIMonitorPage;
	bool m_bMIDIMonitorRX;
	unsigned m_nMIDIMonitorCable;
	size_t m_nMIDIMonitorLength;
	u8 m_MIDIMonitorData[8];

	CUIMenu m_Menu;
};
#endif
