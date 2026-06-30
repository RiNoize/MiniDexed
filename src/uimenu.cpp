//
// uimenu.cpp
//
// MiniDexed - Dexed FM synthesizer for bare metal Raspberry Pi
// Copyright (C) 2022  The MiniDexed Team
//
// Original author of this class:
//	R. Stange <rsta2@o2online.de>
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
#include "uimenu.h"
#include "minidexed.h"
#include "mididevice.h"
#include "userinterface.h"
#include "sysexfileloader.h"
#include "config.h"
#include <math.h>
#include <circle/sysconfig.h>
#include <assert.h>
#include <cstddef>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

using namespace std;
LOGMODULE ("uimenu");

const CUIMenu::TMenuItem CUIMenu::s_MenuRoot[] =
{
	{"MiniDexed", MenuHandler, s_MainMenu},
	{0}
};

// inserting menu items before "TG1" affect TGShortcutHandler()
const CUIMenu::TMenuItem CUIMenu::s_MainMenu[] =
{
	{"TG1",		MenuHandler,	s_TGMenu, 0},
#ifdef ARM_ALLOW_MULTI_CORE
	{"TG2",		MenuHandler,	s_TGMenu, 1},
	{"TG3",		MenuHandler,	s_TGMenu, 2},
	{"TG4",		MenuHandler,	s_TGMenu, 3},
	{"TG5",		MenuHandler,	s_TGMenu, 4},
	{"TG6",		MenuHandler,	s_TGMenu, 5},
	{"TG7",		MenuHandler,	s_TGMenu, 6},
	{"TG8",		MenuHandler,	s_TGMenu, 7},
#if (RASPPI==4 || RASPPI==5)
	{"TG9",		MenuHandler,	s_TGMenu, 8},
	{"TG10",	MenuHandler,	s_TGMenu, 9},
	{"TG11",	MenuHandler,	s_TGMenu, 10},
	{"TG12",	MenuHandler,	s_TGMenu, 11},
	{"TG13",	MenuHandler,	s_TGMenu, 12},
	{"TG14",	MenuHandler,	s_TGMenu, 13},
	{"TG15",	MenuHandler,	s_TGMenu, 14},
	{"TG16",	MenuHandler,	s_TGMenu, 15},
#endif
#endif
	{"Effects",	MenuHandler,	s_EffectsMenu},
	{"Master Volume", EditMasterVolume, 0, 0},
	{"Performance",	MenuHandler, s_PerformanceMenu}, 
	{0}
};

const CUIMenu::TMenuItem CUIMenu::s_TGMenu[] =
{
	{"Voice",	EditProgramNumber},
	{"Bank",	EditVoiceBankNumber},
	{"Volume",	EditTGParameter,	0,	CMiniDexed::TGParameterVolume},
#ifdef ARM_ALLOW_MULTI_CORE
	{"Pan",		EditTGParameter,	0,	CMiniDexed::TGParameterPan},
	{"Reverb-Send",	EditTGParameter,	0,	CMiniDexed::TGParameterReverbSend},
#endif
	{"Detune",	EditTGParameter,	0,	CMiniDexed::TGParameterMasterTune},
	{"Octave",	EditTGParameter,	0,	CMiniDexed::TGParameterNoteShift},
	{"Key Low",	EditTGParameter,	0,	CMiniDexed::TGParameterNoteLimitLow},
	{"Key High",	EditTGParameter,	0,	CMiniDexed::TGParameterNoteLimitHigh},
	{"Cutoff",	EditTGParameter,	0,	CMiniDexed::TGParameterCutoff},
	{"Resonance",	EditTGParameter,	0,	CMiniDexed::TGParameterResonance},
	{"Filter Type",	EditTGParameter,	0,	CMiniDexed::TGParameterFilterType},
	{"Pitch Bend",	MenuHandler,		s_EditPitchBendMenu},
	{"Portamento",		MenuHandler,		s_EditPortamentoMenu},
	{"Poly/Mono",		EditTGParameter,	0,	CMiniDexed::TGParameterMonoMode}, 
	{"Modulation",		MenuHandler,		s_ModulationMenu},
	{"Channel",	EditTGParameter,	0,	CMiniDexed::TGParameterMIDIChannel},
	{"Edit Voice",	MenuHandler,		s_EditVoiceMenu},
	{0}
};

const CUIMenu::TMenuItem CUIMenu::s_EffectsMenu[] =
{
	{"Compress",	EditGlobalParameter,	0,	CMiniDexed::ParameterCompressorEnable},
#ifdef ARM_ALLOW_MULTI_CORE
	{"Reverb",	MenuHandler,		s_ReverbMenu},
#endif
	{0}
};

const CUIMenu::TMenuItem CUIMenu::s_EditPitchBendMenu[] =
{
	{"Bend Range",	EditTGParameter2,	0,	CMiniDexed::TGParameterPitchBendRange},
	{"Bend Step",		EditTGParameter2,	0,	CMiniDexed::TGParameterPitchBendStep},
	{0}
};

const CUIMenu::TMenuItem CUIMenu::s_EditPortamentoMenu[] =
{
	{"Mode",		EditTGParameter2,	0,	CMiniDexed::TGParameterPortamentoMode},
	{"Glissando",		EditTGParameter2,	0,	CMiniDexed::TGParameterPortamentoGlissando},
	{"Time",		EditTGParameter2,	0,	CMiniDexed::TGParameterPortamentoTime},
	{0}
};

const CUIMenu::TMenuItem CUIMenu::s_ModulationMenu[] =
{
	{"Mod. Wheel",		MenuHandler,	s_ModulationMenuParameters,	CMiniDexed::TGParameterMWRange},
	{"Foot Control",	MenuHandler,	s_ModulationMenuParameters,	CMiniDexed::TGParameterFCRange},
	{"Breath Control",	MenuHandler,	s_ModulationMenuParameters,	CMiniDexed::TGParameterBCRange},
	{"Aftertouch",	MenuHandler,	s_ModulationMenuParameters,	CMiniDexed::TGParameterATRange},
	{0}
};

const CUIMenu::TMenuItem CUIMenu::s_ModulationMenuParameters[] =
{
	{"Range",		EditTGParameterModulation,	0, 0},
	{"Pitch",		EditTGParameterModulation,	0, 1},
	{"Amplitude",	EditTGParameterModulation,	0, 2},
	{"EG Bias",		EditTGParameterModulation,	0, 3},
	{0}
};

#ifdef ARM_ALLOW_MULTI_CORE

const CUIMenu::TMenuItem CUIMenu::s_ReverbMenu[] =
{
	{"Enable",	EditGlobalParameter,	0,	CMiniDexed::ParameterReverbEnable},
	{"Size",	EditGlobalParameter,	0,	CMiniDexed::ParameterReverbSize},
	{"High damp",	EditGlobalParameter,	0,	CMiniDexed::ParameterReverbHighDamp},
	{"Low damp",	EditGlobalParameter,	0,	CMiniDexed::ParameterReverbLowDamp},
	{"Low pass",	EditGlobalParameter,	0,	CMiniDexed::ParameterReverbLowPass},
	{"Diffusion",	EditGlobalParameter,	0,	CMiniDexed::ParameterReverbDiffusion},
	{"Level",	EditGlobalParameter,	0,	CMiniDexed::ParameterReverbLevel},
	{0}
};

#endif

// inserting menu items before "OP1" affect OPShortcutHandler()
const CUIMenu::TMenuItem CUIMenu::s_EditVoiceMenu[] =
{
	{"OP1",		MenuHandler,		s_OperatorMenu, 0},
	{"OP2",		MenuHandler,		s_OperatorMenu, 1},
	{"OP3",		MenuHandler,		s_OperatorMenu, 2},
	{"OP4",		MenuHandler,		s_OperatorMenu, 3},
	{"OP5",		MenuHandler,		s_OperatorMenu, 4},
	{"OP6",		MenuHandler,		s_OperatorMenu, 5},
	{"Algorithm",	EditVoiceParameter,	0,		DEXED_ALGORITHM},
	{"Feedback",	EditVoiceParameter,	0,		DEXED_FEEDBACK},
	{"P EG Rate 1",	EditVoiceParameter,	0,		DEXED_PITCH_EG_R1},
	{"P EG Rate 2",	EditVoiceParameter,	0,		DEXED_PITCH_EG_R2},
	{"P EG Rate 3",	EditVoiceParameter,	0,		DEXED_PITCH_EG_R3},
	{"P EG Rate 4",	EditVoiceParameter,	0,		DEXED_PITCH_EG_R4},
	{"P EG Level 1",EditVoiceParameter,	0,		DEXED_PITCH_EG_L1},
	{"P EG Level 2",EditVoiceParameter,	0,		DEXED_PITCH_EG_L2},
	{"P EG Level 3",EditVoiceParameter,	0,		DEXED_PITCH_EG_L3},
	{"P EG Level 4",EditVoiceParameter,	0,		DEXED_PITCH_EG_L4},
	{"Osc Key Sync",EditVoiceParameter,	0,		DEXED_OSC_KEY_SYNC},
	{"LFO Speed",	EditVoiceParameter,	0,		DEXED_LFO_SPEED},
	{"LFO Delay",	EditVoiceParameter,	0,		DEXED_LFO_DELAY},
	{"LFO PMD",	EditVoiceParameter,	0,		DEXED_LFO_PITCH_MOD_DEP},
	{"LFO AMD",	EditVoiceParameter,	0,		DEXED_LFO_AMP_MOD_DEP},
	{"LFO Sync",	EditVoiceParameter,	0,		DEXED_LFO_SYNC},
	{"LFO Wave",	EditVoiceParameter,	0,		DEXED_LFO_WAVE},
	{"P Mod Sens.",	EditVoiceParameter,	0,		DEXED_LFO_PITCH_MOD_SENS},
	{"Transpose",	EditVoiceParameter,	0,		DEXED_TRANSPOSE},
	{"Name",	InputTxt,0 , 3}, 
	{0}
};

const CUIMenu::TMenuItem CUIMenu::s_OperatorMenu[] =
{
	{"Output Level",EditOPParameter,	0,	DEXED_OP_OUTPUT_LEV},
	{"Freq Coarse",	EditOPParameter,	0,	DEXED_OP_FREQ_COARSE},
	{"Freq Fine",	EditOPParameter,	0,	DEXED_OP_FREQ_FINE},
	{"Osc Detune",	EditOPParameter,	0,	DEXED_OP_OSC_DETUNE},
	{"Osc Mode",	EditOPParameter,	0,	DEXED_OP_OSC_MODE},
	{"EG Rate 1",	EditOPParameter,	0,	DEXED_OP_EG_R1},
	{"EG Rate 2",	EditOPParameter,	0,	DEXED_OP_EG_R2},
	{"EG Rate 3",	EditOPParameter,	0,	DEXED_OP_EG_R3},
	{"EG Rate 4",	EditOPParameter,	0,	DEXED_OP_EG_R4},
	{"EG Level 1",	EditOPParameter,	0,	DEXED_OP_EG_L1},
	{"EG Level 2",	EditOPParameter,	0,	DEXED_OP_EG_L2},
	{"EG Level 3",	EditOPParameter,	0,	DEXED_OP_EG_L3},
	{"EG Level 4",	EditOPParameter,	0,	DEXED_OP_EG_L4},
	{"Break Point",	EditOPParameter,	0,	DEXED_OP_LEV_SCL_BRK_PT},
	{"L Key Depth",	EditOPParameter,	0,	DEXED_OP_SCL_LEFT_DEPTH},
	{"R Key Depth",	EditOPParameter,	0,	DEXED_OP_SCL_RGHT_DEPTH},
	{"L Key Scale",	EditOPParameter,	0,	DEXED_OP_SCL_LEFT_CURVE},
	{"R Key Scale",	EditOPParameter,	0,	DEXED_OP_SCL_RGHT_CURVE},
	{"Rate Scaling",EditOPParameter,	0,	DEXED_OP_OSC_RATE_SCALE},
	{"A Mod Sens.",	EditOPParameter,	0,	DEXED_OP_AMP_MOD_SENS},
	{"K Vel. Sens.",EditOPParameter,	0,	DEXED_OP_KEY_VEL_SENS},
	{"Enable", EditOPParameter, 0, DEXED_OP_ENABLE},
	{0}
};

const CUIMenu::TMenuItem CUIMenu::s_SaveMenu[] =
{
	{"Overwrite",	SavePerformance, 0, 0}, 
	{"New",	InputTxt,0 , 1}, 
	{"Save as default",	SavePerformance, 0, 1}, 
	{0}
};

// must match CMiniDexed::TParameter
const CUIMenu::TParameter CUIMenu::s_GlobalParameter[CMiniDexed::ParameterUnknown] =
{
	{0,	1,	1,	ToOnOff},		// ParameterCompessorEnable
	{0,	1,	1,	ToOnOff},		// ParameterReverbEnable
	{0,	99,	1},				// ParameterReverbSize
	{0,	99,	1},				// ParameterReverbHighDamp
	{0,	99,	1},				// ParameterReverbLowDamp
	{0,	99,	1},				// ParameterReverbLowPass
	{0,	99,	1},				// ParameterReverbDiffusion
	{0,	99,	1},				// ParameterReverbLevel
	{0,	CMIDIDevice::ChannelUnknown-1,		1, ToMIDIChannel}, 	// ParameterPerformanceSelectChannel
	{0, NUM_PERFORMANCE_BANKS, 1}	// ParameterPerformanceBank
};

// must match CMiniDexed::TTGParameter
const CUIMenu::TParameter CUIMenu::s_TGParameter[CMiniDexed::TGParameterUnknown] =
{
	{0,	CSysExFileLoader::MaxVoiceBankID,	1},			// TGParameterVoiceBank
	{0, 0, 0},											// TGParameterVoiceBankMSB (not used in menus)
	{0, 0, 0},											// TGParameterVoiceBankLSB (not used in menus)
	{0,	CSysExFileLoader::VoicesPerBank-1,	1},			// TGParameterProgram
	{0,	127,					1, ToVolume},		// TGParameterVolume
	{0,	127,					8, ToPan},		// TGParameterPan
	{-99,	99,					1},			// TGParameterMasterTune
	{-24,	24,					12, ToOctave},		// TGParameterNoteShift
	{0,	127,					1},			// TGParameterNoteLimitLow
	{0,	127,					1},			// TGParameterNoteLimitHigh
	{0,	99,					1},			// TGParameterCutoff
	{0,	99,					1},			// TGParameterResonance
	{0,	CMiniDexed::FilterTypeUnknown-1,	1, ToFilterType},	// TGParameterFilterType
	{0,	CMIDIDevice::ChannelUnknown-1,		1, ToMIDIChannel}, 	// TGParameterMIDIChannel
	{0, 99, 1},								// TGParameterReverbSend
	{0,	12,					1},			// TGParameterPitchBendRange
	{0,	12,					1},			// TGParameterPitchBendStep
	{0,	1,					1, ToPortaMode},	// TGParameterPortamentoMode
	{0,	1,					1, ToPortaGlissando},	// TGParameterPortamentoGlissando
	{0,	99,					1},			// TGParameterPortamentoTime
	{0,	1,					1, ToPolyMono}, 		// TGParameterMonoMode 
	{0, 99, 1}, //MW Range
	{0, 1, 1, ToOnOff}, //MW Pitch
	{0, 1, 1, ToOnOff}, //MW Amp
	{0, 1, 1, ToOnOff}, //MW EGBias
	{0, 99, 1}, //FC Range
	{0, 1, 1, ToOnOff}, //FC Pitch
	{0, 1, 1, ToOnOff}, //FC Amp
	{0, 1, 1, ToOnOff}, //FC EGBias
	{0, 99, 1}, //BC Range
	{0, 1, 1, ToOnOff}, //BC Pitch
	{0, 1, 1, ToOnOff}, //BC Amp
	{0, 1, 1, ToOnOff}, //BC EGBias
	{0, 99, 1}, //AT Range
	{0, 1, 1, ToOnOff}, //AT Pitch
	{0, 1, 1, ToOnOff}, //AT Amp
	{0, 1, 1, ToOnOff} //AT EGBias	
};

// must match DexedVoiceParameters in Synth_Dexed
const CUIMenu::TParameter CUIMenu::s_VoiceParameter[] =
{
	{0,	99,	1},				// DEXED_PITCH_EG_R1
	{0,	99,	1},				// DEXED_PITCH_EG_R2
	{0,	99,	1},				// DEXED_PITCH_EG_R3
	{0,	99,	1},				// DEXED_PITCH_EG_R4
	{0,	99,	1},				// DEXED_PITCH_EG_L1
	{0,	99,	1},				// DEXED_PITCH_EG_L2
	{0,	99,	1},				// DEXED_PITCH_EG_L3
	{0,	99,	1},				// DEXED_PITCH_EG_L4
	{0,	31,	1,	ToAlgorithm},		// DEXED_ALGORITHM
	{0,	7,	1},				// DEXED_FEEDBACK
	{0,	1,	1,	ToOnOff},		// DEXED_OSC_KEY_SYNC
	{0,	99,	1},				// DEXED_LFO_SPEED
	{0,	99,	1},				// DEXED_LFO_DELAY
	{0,	99,	1},				// DEXED_LFO_PITCH_MOD_DEP
	{0,	99,	1},				// DEXED_LFO_AMP_MOD_DEP
	{0,	1,	1,	ToOnOff},		// DEXED_LFO_SYNC
	{0,	5,	1,	ToLFOWaveform},		// DEXED_LFO_WAVE
	{0,	7,	1},				// DEXED_LFO_PITCH_MOD_SENS
	{0,	48,	1,	ToTransposeNote},	// DEXED_TRANSPOSE
	{0,	1,	1}				// Voice Name - Dummy parameters for in case new item would be added in future 
};

// must match DexedVoiceOPParameters in Synth_Dexed
const CUIMenu::TParameter CUIMenu::s_OPParameter[] =
{
	{0,	99,	1},				// DEXED_OP_EG_R1
	{0,	99,	1},				// DEXED_OP_EG_R2
	{0,	99,	1},				// DEXED_OP_EG_R3
	{0,	99,	1},				// DEXED_OP_EG_R4
	{0,	99,	1},				// DEXED_OP_EG_L1
	{0,	99,	1},				// DEXED_OP_EG_L2
	{0,	99,	1},				// DEXED_OP_EG_L3
	{0,	99,	1},				// DEXED_OP_EG_L4
	{0,	99,	1,	ToBreakpointNote},	// DEXED_OP_LEV_SCL_BRK_PT
	{0,	99,	1},				// DEXED_OP_SCL_LEFT_DEPTH
	{0,	99,	1},				// DEXED_OP_SCL_RGHT_DEPTH
	{0,	3,	1,	ToKeyboardCurve},	// DEXED_OP_SCL_LEFT_CURVE
	{0,	3,	1,	ToKeyboardCurve},	// DEXED_OP_SCL_RGHT_CURVE
	{0,	7,	1},				// DEXED_OP_OSC_RATE_SCALE
	{0,	3,	1},				// DEXED_OP_AMP_MOD_SENS
	{0,	7,	1},				// DEXED_OP_KEY_VEL_SENS
	{0,	99,	1},				// DEXED_OP_OUTPUT_LEV
	{0,	1,	1,	ToOscillatorMode},	// DEXED_OP_OSC_MODE
	{0,	31,	1},				// DEXED_OP_FREQ_COARSE
	{0,	99,	1},				// DEXED_OP_FREQ_FINE
	{0,	14,	1,	ToOscillatorDetune},	// DEXED_OP_OSC_DETUNE
	{0, 1, 1, ToOnOff}		// DEXED_OP_ENABLE
};

const char CUIMenu::s_NoteName[100][5] =
{
"A-1", "A#-1", "B-1", "C0", "C#0", "D0", "D#0", "E0", "F0", "F#0", "G0", "G#0",
"A0", "A#0", "B0", "C1", "C#1", "D1", "D#1", "E1", "F1", "F#1", "G1", "G#1",
"A1", "A#1", "B1", "C2", "C#2", "D2", "D#2", "E2", "F2", "F#2", "G2", "G#2",
"A2", "A#2", "B2", "C3", "C#3", "D3", "D#3", "E3", "F3", "F#3", "G3", "G#3",
"A3", "A#3", "B3", "C4", "C#4", "D4", "D#4", "E4", "F4", "F#4", "G4", "G#4",
"A4", "A#4", "B4", "C5", "C#5", "D5", "D#5", "E5", "F5", "F#5", "G5", "G#5",
"A5", "A#5", "B5", "C6", "C#6", "D6", "D#6", "E6", "F6", "F#6", "G6", "G#6",
"A6", "A#6", "B6", "C7", "C#7", "D7", "D#7", "E7", "F7", "F#7", "G7", "G#7",
"A7", "A#7", "B7", "C8"
};

static const unsigned NoteC3 = 39;

const CUIMenu::TMenuItem CUIMenu::s_PerformanceMenu[] =
{
	{"Load",	PerformanceMenu, 0, 0}, 
	{"Save",	MenuHandler,	s_SaveMenu},
	{"Delete",	PerformanceMenu, 0, 1},
	{"Copy TG",	CopyTG, 0, 0},
	{"Bank",	EditPerformanceBankNumber, 0, 0},
	{"PCCH",	EditGlobalParameter,	0,	CMiniDexed::ParameterPerformanceSelectChannel},
	{0}
};


CUIMenu::CUIMenu (CUserInterface *pUI, CMiniDexed *pMiniDexed, CConfig *pConfig)
:	m_pUI (pUI),
	m_pMiniDexed (pMiniDexed),
	m_pConfig (pConfig),
	m_pParentMenu (s_MenuRoot),
	m_pCurrentMenu (s_MainMenu),
	m_nCurrentMenuItem (0),
	m_nCurrentSelection (0),
	m_nCurrentParameter (0),
	m_nCurrentMenuDepth (0)
{
	assert (m_pConfig);
	m_nToneGenerators = m_pConfig->GetToneGenerators();

	if (m_nToneGenerators == 1)
	{
		// If there is just one core, then there is only a single
		// tone generator so start on the TG1 menu...
		m_pParentMenu = s_MainMenu;
		m_pCurrentMenu = s_TGMenu;
		m_nCurrentMenuItem = 0;
		m_nCurrentSelection = 0;
		m_nCurrentParameter = 0;
		m_nCurrentMenuDepth = 1;

		// Place the "root" menu at the top of the stack
		m_MenuStackParent[0] = s_MenuRoot;
		m_MenuStackMenu[0] = s_MainMenu;
		m_nMenuStackItem[0]	= 0;
		m_nMenuStackSelection[0] = 0;
		m_nMenuStackParameter[0] = 0;
	}
}

void CUIMenu::EventHandler (TMenuEvent Event)
{
	// While the incoming SysEx feedback screen is active, suppress normal
	// menu refreshes. Otherwise the regular patch/menu display competes
	// with the SysEx display and the LCD flickers during fast potentiometer
	// movement. User actions still cancel the overlay and are handled normally.
	if (m_bSysExDisplayActive)
	{
		if (Event == MenuEventUpdate || Event == MenuEventUpdateParameter)
		{
			return;
		}

		m_bSysExDisplayActive = false;
	}

	if (Event == MenuEventUpdateParameter)
	{
		// External MIDI controllers (faders/knobs) arrive as parameter updates,
		// not as local encoder step events.  When the Performance page 2
		// parameter overview is active, keep it visible and restart the same
		// 4-second idle hold used by encoder edits, so fast MIDI controller
		// movements do not fall back to page 1 while the user is still editing.
		if (IsPerformanceMenuActive ()
			&& (m_bPerformanceOverviewShowTGParameter || m_bPerformanceOverviewAltPotGlobalLabels))
		{
			DisplayPerformanceTGOverview ();
			ArmPerformanceOverviewTimer (4000, false, true);
			return;
		}
	}

	if (Event != MenuEventUpdate && Event != MenuEventUpdateParameter)
	{
		// Any real user action cancels pending automatic performance-page flips.
		m_nPerformanceOverviewSequence++;

		// In the Performance screen, TG parameter shortcut buttons become
		// "overview selectors": Cutoff shows all TG cutoffs, Pan shows all TG pans,
		// Volume shows all TG volumes, etc. This keeps the 1602 as a performance
		// monitor instead of jumping into one TG menu.
		if (IsPerformanceMenuActive ())
		{
			if (HandlePerformanceOverviewShortcut (Event))
			{
				const char *pName = GetMIDIButtonFunctionName (Event);
				DisplayMIDIButtonOverlay ("MIDI Button", pName ? pName : "Parameter", 1000);
				return;
			}

			// When the compact Performance overview is showing a TG parameter
			// (Cutoff, Volume, Pan, etc.), direct TG buttons select which TG will be
			// edited by the encoder. They do not leave the Performance overview.
			if (HandlePerformanceOverviewTGSelect (Event))
			{
				DisplayPerformanceTGOverview ();
				ArmPerformanceOverviewTimer (4000, false);
				return;
			}

			// Encoder steps edit the selected TG/parameter while staying on the
			// compact overview page, so the changed value can be watched live.
			if (HandlePerformanceOverviewEditStep (Event))
			{
				DisplayPerformanceTGOverview ();
				ArmPerformanceOverviewTimer (4000, false);
				return;
			}
		}
	}

	switch (Event)
	{
	case MenuEventBack:				// pop menu
		if (m_nCurrentMenuDepth)
		{
			m_nCurrentMenuDepth--;

			m_pParentMenu = m_MenuStackParent[m_nCurrentMenuDepth];
			m_pCurrentMenu = m_MenuStackMenu[m_nCurrentMenuDepth];
			m_nCurrentMenuItem = m_nMenuStackItem[m_nCurrentMenuDepth];
			m_nCurrentSelection = m_nMenuStackSelection[m_nCurrentMenuDepth];
			m_nCurrentParameter = m_nMenuStackParameter[m_nCurrentMenuDepth];

			EventHandler (MenuEventUpdate);
		}
		break;

	case MenuEventHome:
		if (m_nToneGenerators == 1)
		{
			// "Home" is the TG0 menu if only one TG active
			m_pParentMenu = s_MainMenu;
			m_pCurrentMenu = s_TGMenu;
			m_nCurrentMenuItem = 0;
			m_nCurrentSelection = 0;
			m_nCurrentParameter = 0;
			m_nCurrentMenuDepth = 1;
			// Place the "root" menu at the top of the stack
			m_MenuStackParent[0] = s_MenuRoot;
			m_MenuStackMenu[0] = s_MainMenu;
			m_nMenuStackItem[0] = 0;
			m_nMenuStackSelection[0] = 0;
			m_nMenuStackParameter[0] = 0;
		}
		else
		{
			m_pParentMenu = s_MenuRoot;
			m_pCurrentMenu = s_MainMenu;
			m_nCurrentMenuItem = 0;
			m_nCurrentSelection = 0;
			m_nCurrentParameter = 0;
			m_nCurrentMenuDepth = 0;
		}
		EventHandler (MenuEventUpdate);
		break;

	case MenuEventPgmUp:
	case MenuEventPgmDown:
		PgmUpDownHandler(Event);
		break;

	case MenuEventBankUp:
	case MenuEventBankDown:
		BankUpDownHandler(Event);
		break;

	case MenuEventTGUp:
	case MenuEventTGDown:
		TGUpDownHandler(Event);
		break;

	case MenuEventTG1:
		TGSelectHandler (0);
		break;

	case MenuEventTG2:
		TGSelectHandler (1);
		break;

	case MenuEventTG3:
		TGSelectHandler (2);
		break;

	case MenuEventTG4:
		TGSelectHandler (3);
		break;

	case MenuEventTG5:
		TGSelectHandler (4);
		break;

	case MenuEventTG6:
		TGSelectHandler (5);
		break;

	case MenuEventTG7:
		TGSelectHandler (6);
		break;

	case MenuEventTG8:
		TGSelectHandler (7);
		break;

	case MenuEventEffects:
		MainMenuSelectHandler ("Effects");
		DisplayMIDIButtonOverlay ("Menu", "Effects", 1000);
		break;

	case MenuEventMasterVolume:
		MainMenuSelectHandler ("Master Volume");
		DisplayMIDIButtonOverlay ("Menu", "Master Volume", 1000);
		break;

	case MenuEventPerformance:
		MainMenuSelectHandler ("Performance");
		DisplayMIDIButtonOverlay ("Menu", "Performance", 1000);
		break;

	case MenuEventTGVoice:
		TGMenuSelectHandler ("Voice");
		DisplayMIDIButtonOverlay ("TG Function", "Voice", 1000);
		break;

	case MenuEventTGBank:
		TGMenuSelectHandler ("Bank");
		DisplayMIDIButtonOverlay ("TG Function", "Bank", 1000);
		break;

	case MenuEventTGVolume:
		TGMenuSelectHandler ("Volume");
		DisplayMIDIButtonOverlay ("TG Function", "Volume", 1000);
		break;

	case MenuEventTGPan:
		TGMenuSelectHandler ("Pan");
		DisplayMIDIButtonOverlay ("TG Function", "Pan", 1000);
		break;

	case MenuEventTGReverbSend:
		TGMenuSelectHandler ("Reverb-Send");
		DisplayMIDIButtonOverlay ("TG Function", "Reverb-Send", 1000);
		break;

	case MenuEventTGDetune:
		TGMenuSelectHandler ("Detune");
		DisplayMIDIButtonOverlay ("TG Function", "Detune", 1000);
		break;

	case MenuEventTGOctave:
		TGMenuSelectHandler ("Octave");
		DisplayMIDIButtonOverlay ("TG Function", "Octave", 1000);
		break;

	case MenuEventTGCutoff:
		TGMenuSelectHandler ("Cutoff");
		DisplayMIDIButtonOverlay ("TG Function", "Cutoff", 1000);
		break;

	case MenuEventTGResonance:
		TGMenuSelectHandler ("Resonance");
		DisplayMIDIButtonOverlay ("TG Function", "Resonance", 1000);
		break;

	case MenuEventTGPitchBend:
		TGMenuSelectHandler ("Pitch Bend");
		DisplayMIDIButtonOverlay ("TG Function", "Pitch Bend", 1000);
		break;

	case MenuEventTGPortamento:
		TGMenuSelectHandler ("Portamento");
		DisplayMIDIButtonOverlay ("TG Function", "Portamento", 1000);
		break;

	case MenuEventTGPolyMono:
		TGMenuSelectHandler ("Poly/Mono");
		DisplayMIDIButtonOverlay ("TG Function", "Poly/Mono", 1000);
		break;

	case MenuEventTGModulation:
		TGMenuSelectHandler ("Modulation");
		DisplayMIDIButtonOverlay ("TG Function", "Modulation", 1000);
		break;

	case MenuEventTGChannel:
		TGMenuSelectHandler ("Channel");
		DisplayMIDIButtonOverlay ("TG Function", "Channel", 1000);
		break;

	case MenuEventTGEditVoice:
		TGMenuSelectHandler ("Edit Voice");
		DisplayMIDIButtonOverlay ("TG Function", "Edit Voice", 1000);
		break;

	case MenuEventTGSolo:
		m_pMiniDexed->ToggleTGSolo ();
		if (m_pMiniDexed->IsTGSoloEnabled ())
		{
			// Solo Edit ON: jump into the selected TG menu so the user can
			// audition and edit Voice/Bank/Volume/Pan/etc. for that TG only.
			TGSelectHandler (m_pMiniDexed->GetTGSoloTG ());
			std::string Value = "TG" + std::to_string (m_pMiniDexed->GetTGSoloTG () + 1) + " ON";
			DisplayMIDIButtonOverlay ("Solo Edit", Value.c_str (), 1000);
		}
		else
		{
			// Solo Edit OFF: return visually to Performance -> Load, showing
			// the current performance name.  Do not reload the performance, so
			// unsaved edits made while soloing are preserved.
			m_bPerformanceOverviewShowTGParameter = false;
			m_bPerformanceOverviewAltPotGlobalLabels = false;
			m_bPerformanceOverviewNoteShiftFine = false;
			m_bPerformanceOverviewEditActive = false;
			m_nPerformanceOverviewHoldRemainingMS = 0;
			m_bSysExDisplayActive = false;
			m_nSysExDisplaySequence++;

			// First jump to the Performance menu, then enter its Load item.
			// This is equivalent to: Main -> Performance -> Load.
			// Do not send another Select after that, because that could reload
			// the performance and discard unsaved edits.
			MainMenuSelectHandler ("Performance");
			EventHandler (MenuEventSelect);
		}
		break;

	case MenuEventAltPot:
		if (IsPerformanceMenuActive ())
		{
			if (m_pMiniDexed->IsAltPotGlobalMode ())
			{
				m_bPerformanceOverviewShowTGParameter = false;
				m_bPerformanceOverviewNoteShiftFine = false;
				m_bPerformanceOverviewAltPotGlobalLabels = true;
				m_bPerformanceOverviewEditActive = false;
			}
			else
			{
				CMiniDexed::TTGParameter Param = m_pMiniDexed->GetAltPotTGParameter ();
				if (Param != CMiniDexed::TGParameterUnknown)
				{
					m_bPerformanceOverviewShowTGParameter = true;
					m_bPerformanceOverviewAltPotGlobalLabels = false;
					m_nPerformanceOverviewTGParameter = Param;
					m_bPerformanceOverviewNoteShiftFine =
						m_pMiniDexed->GetAltPotBank () == CMiniDexed::AltPotBankNoteShift;
					m_bPerformanceOverviewEditActive = false;
				}
			}
		}
		DisplayAltPotBankOverlay ();
		break;

	case MenuEventAltPotPrev:
		m_pMiniDexed->SelectPreviousAltPotBank ();
		if (IsPerformanceMenuActive ())
		{
			CMiniDexed::TTGParameter Param = m_pMiniDexed->GetAltPotTGParameter ();
			if (Param != CMiniDexed::TGParameterUnknown)
			{
				m_bPerformanceOverviewShowTGParameter = true;
				m_bPerformanceOverviewAltPotGlobalLabels = false;
				m_nPerformanceOverviewTGParameter = Param;
				m_bPerformanceOverviewNoteShiftFine =
					m_pMiniDexed->GetAltPotBank () == CMiniDexed::AltPotBankNoteShift;
				m_bPerformanceOverviewEditActive = false;
			}
		}
		DisplayAltPotBankOverlay ();
		break;

	case MenuEventAltPotNext:
		m_pMiniDexed->SelectNextAltPotBank ();
		if (IsPerformanceMenuActive ())
		{
			CMiniDexed::TTGParameter Param = m_pMiniDexed->GetAltPotTGParameter ();
			if (Param != CMiniDexed::TGParameterUnknown)
			{
				m_bPerformanceOverviewShowTGParameter = true;
				m_bPerformanceOverviewAltPotGlobalLabels = false;
				m_nPerformanceOverviewTGParameter = Param;
				m_bPerformanceOverviewNoteShiftFine =
					m_pMiniDexed->GetAltPotBank () == CMiniDexed::AltPotBankNoteShift;
				m_bPerformanceOverviewEditActive = false;
			}
		}
		DisplayAltPotBankOverlay ();
		break;

	case MenuEventAltPotMode:
		m_pMiniDexed->ToggleAltPotMode ();
		if (IsPerformanceMenuActive ())
		{
			if (m_pMiniDexed->IsAltPotGlobalMode ())
			{
				m_bPerformanceOverviewShowTGParameter = false;
				m_bPerformanceOverviewNoteShiftFine = false;
				m_bPerformanceOverviewAltPotGlobalLabels = true;
				m_bPerformanceOverviewEditActive = false;
			}
			else
			{
				CMiniDexed::TTGParameter Param = m_pMiniDexed->GetAltPotTGParameter ();
				m_bPerformanceOverviewShowTGParameter = Param != CMiniDexed::TGParameterUnknown;
				m_bPerformanceOverviewAltPotGlobalLabels = false;
				m_nPerformanceOverviewTGParameter = Param;
				m_bPerformanceOverviewNoteShiftFine =
					m_pMiniDexed->GetAltPotBank () == CMiniDexed::AltPotBankNoteShift;
				m_bPerformanceOverviewEditActive = false;
			}
		}
		DisplayMIDIButtonOverlay ("AltPot Mode", m_pMiniDexed->GetAltPotModeName (), 1000);
		break;

	default:
		(*m_pParentMenu[m_nCurrentMenuItem].Handler) (this, Event);
		break;
	}
}


void CUIMenu::Process (void)
{
	// Reserved for lightweight UI polling. The performance overview uses
	// kernel timers so the normal LCD page is not redrawn on every main loop.
}

bool CUIMenu::IsPerformanceMenuActive (void) const
{
	return m_pParentMenu &&
	       m_pParentMenu[m_nCurrentMenuItem].Handler == PerformanceMenu &&
	       !m_bPerformanceDeleteMode &&
	       !m_bSplashShow &&
	       !m_bSysExDisplayActive;
}

std::string CUIMenu::Short3 (const std::string &Text)
{
	std::string Result;
	for (unsigned i = 0; i < Text.length () && Result.length () < 3; i++)
	{
		char c = Text[i];
		if (c == ' ' || c == '\t')
		{
			continue;
		}
		if (c >= 'a' && c <= 'z')
		{
			c = c - 'a' + 'A';
		}
		Result += c;
	}

	while (Result.length () < 3)
	{
		Result += '-';
	}

	return Result;
}


const char *CUIMenu::GetMIDIButtonFunctionName (TMenuEvent Event)
{
	switch (Event)
	{
	case MenuEventTGVoice:		return "Voice";
	case MenuEventTGBank:		return "Bank";
	case MenuEventTGVolume:		return "Volume";
	case MenuEventTGPan:		return "Pan";
	case MenuEventTGReverbSend:	return "Reverb-Send";
	case MenuEventTGDetune:		return "Detune";
	case MenuEventTGOctave:		return "Octave";
	case MenuEventTGCutoff:		return "Cutoff";
	case MenuEventTGResonance:	return "Resonance";
	case MenuEventTGPitchBend:	return "Pitch Bend";
	case MenuEventTGPortamento:	return "Portamento";
	case MenuEventTGPolyMono:	return "Poly/Mono";
	case MenuEventTGModulation:	return "Modulation";
	case MenuEventTGChannel:	return "Channel";
	case MenuEventTGEditVoice:	return "Edit Voice";
	case MenuEventTGSolo:		return "Solo Edit";
	case MenuEventAltPot:		return "Alt Knobs";
	case MenuEventAltPotMode:	return "AltPot Mode";
	default:			return 0;
	}
}

const char *CUIMenu::GetMIDIButtonTGName (TMenuEvent Event)
{
	switch (Event)
	{
	case MenuEventTG1:	return "TG1";
	case MenuEventTG2:	return "TG2";
	case MenuEventTG3:	return "TG3";
	case MenuEventTG4:	return "TG4";
	case MenuEventTG5:	return "TG5";
	case MenuEventTG6:	return "TG6";
	case MenuEventTG7:	return "TG7";
	case MenuEventTG8:	return "TG8";
	default:		return 0;
	}
}

bool CUIMenu::HandlePerformanceOverviewShortcut (TMenuEvent Event)
{
	if (Event != MenuEventAltPot)
	{
		m_bPerformanceOverviewAltPotGlobalLabels = false;
	}

	switch (Event)
	{
	case MenuEventTGVoice:
		m_bPerformanceOverviewShowTGParameter = false;
		m_bPerformanceOverviewAltPotGlobalLabels = false;
		m_bPerformanceOverviewNoteShiftFine = false;
		m_bPerformanceOverviewEditActive = false;
		m_nPerformanceOverviewHoldRemainingMS = 0;
		return true;

	case MenuEventTGVolume:
		m_bPerformanceOverviewShowTGParameter = true;
		m_nPerformanceOverviewTGParameter = CMiniDexed::TGParameterVolume;
		m_bPerformanceOverviewNoteShiftFine = false;
		m_nPerformanceOverviewHoldRemainingMS = 0;
		return true;

	case MenuEventTGPan:
		m_bPerformanceOverviewShowTGParameter = true;
		m_nPerformanceOverviewTGParameter = CMiniDexed::TGParameterPan;
		m_bPerformanceOverviewNoteShiftFine = false;
		m_nPerformanceOverviewHoldRemainingMS = 0;
		return true;

	case MenuEventTGReverbSend:
		m_bPerformanceOverviewShowTGParameter = true;
		m_nPerformanceOverviewTGParameter = CMiniDexed::TGParameterReverbSend;
		m_bPerformanceOverviewNoteShiftFine = false;
		m_nPerformanceOverviewHoldRemainingMS = 0;
		return true;

	case MenuEventTGDetune:
		m_bPerformanceOverviewShowTGParameter = true;
		m_nPerformanceOverviewTGParameter = CMiniDexed::TGParameterMasterTune;
		m_bPerformanceOverviewNoteShiftFine = false;
		m_nPerformanceOverviewHoldRemainingMS = 0;
		return true;

	case MenuEventTGOctave:
		m_bPerformanceOverviewShowTGParameter = true;
		m_nPerformanceOverviewTGParameter = CMiniDexed::TGParameterNoteShift;
		m_bPerformanceOverviewNoteShiftFine = false;
		m_nPerformanceOverviewHoldRemainingMS = 0;
		return true;

	case MenuEventAltPot:
	{
		if (m_pMiniDexed->IsAltPotGlobalMode ())
		{
			m_bPerformanceOverviewShowTGParameter = false;
			m_bPerformanceOverviewNoteShiftFine = false;
			m_bPerformanceOverviewAltPotGlobalLabels = true;
			m_bPerformanceOverviewEditActive = false;
			m_nPerformanceOverviewHoldRemainingMS = 0;
			return true;
		}

		CMiniDexed::TTGParameter Param = m_pMiniDexed->GetAltPotTGParameter ();
		if (Param == CMiniDexed::TGParameterUnknown)
		{
			m_bPerformanceOverviewShowTGParameter = false;
			m_bPerformanceOverviewAltPotGlobalLabels = false;
			m_bPerformanceOverviewEditActive = false;
			m_nPerformanceOverviewHoldRemainingMS = 0;
			return true;
		}
		m_bPerformanceOverviewShowTGParameter = true;
		m_bPerformanceOverviewAltPotGlobalLabels = false;
		m_nPerformanceOverviewTGParameter = Param;
		m_bPerformanceOverviewNoteShiftFine =
			m_pMiniDexed->GetAltPotBank () == CMiniDexed::AltPotBankNoteShift;
		m_bPerformanceOverviewEditActive = false;
		m_nPerformanceOverviewHoldRemainingMS = 0;
		return true;
	}

	case MenuEventTGCutoff:
		m_bPerformanceOverviewShowTGParameter = true;
		m_nPerformanceOverviewTGParameter = CMiniDexed::TGParameterCutoff;
		m_bPerformanceOverviewNoteShiftFine = false;
		m_nPerformanceOverviewHoldRemainingMS = 0;
		return true;

	case MenuEventTGResonance:
		m_bPerformanceOverviewShowTGParameter = true;
		m_nPerformanceOverviewTGParameter = CMiniDexed::TGParameterResonance;
		m_bPerformanceOverviewNoteShiftFine = false;
		m_nPerformanceOverviewHoldRemainingMS = 0;
		return true;

	case MenuEventTGPitchBend:
		m_bPerformanceOverviewShowTGParameter = true;
		m_nPerformanceOverviewTGParameter = CMiniDexed::TGParameterPitchBendRange;
		m_bPerformanceOverviewNoteShiftFine = false;
		m_nPerformanceOverviewHoldRemainingMS = 0;
		return true;

	case MenuEventTGPortamento:
		m_bPerformanceOverviewShowTGParameter = true;
		m_nPerformanceOverviewTGParameter = CMiniDexed::TGParameterPortamentoTime;
		m_bPerformanceOverviewNoteShiftFine = false;
		m_nPerformanceOverviewHoldRemainingMS = 0;
		return true;

	case MenuEventTGPolyMono:
		m_bPerformanceOverviewShowTGParameter = true;
		m_nPerformanceOverviewTGParameter = CMiniDexed::TGParameterMonoMode;
		m_bPerformanceOverviewNoteShiftFine = false;
		m_nPerformanceOverviewHoldRemainingMS = 0;
		return true;

	case MenuEventTGChannel:
		m_bPerformanceOverviewShowTGParameter = true;
		m_nPerformanceOverviewTGParameter = CMiniDexed::TGParameterMIDIChannel;
		m_bPerformanceOverviewNoteShiftFine = false;
		m_nPerformanceOverviewHoldRemainingMS = 0;
		return true;

	default:
		return false;
	}
}

bool CUIMenu::GetPerformanceOverviewTGFromEvent (TMenuEvent Event, unsigned *pTG)
{
	assert (pTG);

	switch (Event)
	{
	case MenuEventTG1: *pTG = 0; return true;
	case MenuEventTG2: *pTG = 1; return true;
	case MenuEventTG3: *pTG = 2; return true;
	case MenuEventTG4: *pTG = 3; return true;
	case MenuEventTG5: *pTG = 4; return true;
	case MenuEventTG6: *pTG = 5; return true;
	case MenuEventTG7: *pTG = 6; return true;
	case MenuEventTG8: *pTG = 7; return true;
	default: return false;
	}
}

bool CUIMenu::HandlePerformanceOverviewTGSelect (TMenuEvent Event)
{
	if (!m_bPerformanceOverviewShowTGParameter)
	{
		return false;
	}

	unsigned nTG = 0;
	if (!GetPerformanceOverviewTGFromEvent (Event, &nTG))
	{
		return false;
	}

	if (nTG >= m_nToneGenerators)
	{
		return true;
	}

	m_nPerformanceOverviewEditTG = nTG;
	m_pMiniDexed->SetTGSoloTG (nTG);
	m_bPerformanceOverviewEditActive = true;
	return true;
}

bool CUIMenu::HandlePerformanceOverviewEditStep (TMenuEvent Event)
{
	if (!m_bPerformanceOverviewShowTGParameter || !m_bPerformanceOverviewEditActive)
	{
		return false;
	}

	if (Event != MenuEventStepDown && Event != MenuEventStepUp)
	{
		return false;
	}

	unsigned nTG = m_nPerformanceOverviewEditTG;
	if (nTG >= m_nToneGenerators)
	{
		return true;
	}

	CMiniDexed::TTGParameter Param =
		(CMiniDexed::TTGParameter) m_nPerformanceOverviewTGParameter;
	const TParameter &rParam = s_TGParameter[Param];
	int nIncrement = rParam.Increment;

	// TG Octave and AltPot Note Shift use the same internal Performance
	// parameter (TGParameterNoteShift).  Normal TG Octave intentionally steps
	// by 12 semitones, but AltPot Note Shift must edit by 1 semitone.
	if (Param == CMiniDexed::TGParameterNoteShift && m_bPerformanceOverviewNoteShiftFine)
	{
		nIncrement = 1;
	}

	int nValue = m_pMiniDexed->GetTGParameter (Param, nTG);
	if (Event == MenuEventStepDown)
	{
		nValue -= nIncrement;
		if (nValue < rParam.Minimum)
		{
			nValue = rParam.Minimum;
		}
	}
	else
	{
		nValue += nIncrement;
		if (nValue > rParam.Maximum)
		{
			nValue = rParam.Maximum;
		}
	}

	m_pMiniDexed->SetTGParameter (Param, nValue, nTG);
	return true;
}

std::string CUIMenu::FormatOverviewTGParameterValue (unsigned nTGParameter, int nValue)
{
	char Buffer[4];
	Buffer[3] = '\0';

	switch (nTGParameter)
	{
	case CMiniDexed::TGParameterPan:
	{
		// MiniDexed stores pan as 0..127. Show it as a compact center-based
		// scale for the 1602: -7 = full left, 0 = center, +7 = full right.
		int nPan = 0;
		if (nValue < 64)
		{
			nPan = -((64 - nValue) * 7 + 32) / 64;
		}
		else if (nValue > 64)
		{
			nPan = ((nValue - 64) * 7 + 31) / 63;
		}

		if (nPan < 0)
		{
			snprintf (Buffer, sizeof Buffer, "%2d", nPan);
		}
		else if (nPan > 0)
		{
			snprintf (Buffer, sizeof Buffer, "+%d", nPan);
		}
		else
		{
			strcpy (Buffer, " 0");
		}
		return std::string (Buffer);
	}

	case CMiniDexed::TGParameterNoteShift:
	{
		if (m_bPerformanceOverviewNoteShiftFine)
		{
			if (nValue > 0)
			{
				snprintf (Buffer, sizeof Buffer, "+%2d", nValue);
			}
			else
			{
				snprintf (Buffer, sizeof Buffer, "%3d", nValue);
			}
		}
		else
		{
			int nOct = nValue / 12;
			if (nOct > 0)
			{
				snprintf (Buffer, sizeof Buffer, "+%d", nOct);
			}
			else
			{
				snprintf (Buffer, sizeof Buffer, "%2d", nOct);
			}
		}
		return std::string (Buffer);
	}

	case CMiniDexed::TGParameterMIDIChannel:
		if (nValue == CMIDIDevice::Disabled)
		{
			return "---";
		}
		break;

	case CMiniDexed::TGParameterMonoMode:
		return nValue ? "MON" : "POL";

	case CMiniDexed::TGParameterFilterType:
		switch (nValue)
		{
		case CMiniDexed::FilterTypeClassic:	return "CLA";
		case CMiniDexed::FilterTypeOff:		return "OFF";
		case CMiniDexed::FilterTypeDirtyLP:	return "DLP";
		case CMiniDexed::FilterTypeAcidLP:	return "ALP";
		case CMiniDexed::FilterTypeNasalBP:	return "NBP";
		case CMiniDexed::FilterTypeTelephone:	return "TEL";
		case CMiniDexed::FilterTypeHollowNotch:	return "HOL";
		case CMiniDexed::FilterTypeCombMetal:	return "CMB";
		default:				return "???";
		}

	default:
		break;
	}

	if (nValue < -99)
	{
		nValue = -99;
	}
	else if (nValue > 999)
	{
		nValue = 999;
	}

	snprintf (Buffer, sizeof Buffer, "%3d", nValue);
	return std::string (Buffer);
}

void CUIMenu::DisplayPerformanceTGOverview (void)
{
	if (m_bPerformanceOverviewAltPotGlobalLabels
		&& (!m_bPerformanceOverviewShowTGParameter || m_nPerformanceOverviewHoldRemainingMS <= 3000))
	{
		DisplayAltPotGlobalLabels ();
		return;
	}

	std::string Line1;
	std::string Line2;

	for (unsigned nTG = 0; nTG < 8; nTG++)
	{
		std::string Token = "---";

		if (nTG < m_nToneGenerators)
		{
			int nChannel = m_pMiniDexed->GetTGParameter (CMiniDexed::TGParameterMIDIChannel, nTG);
			if (nChannel != CMIDIDevice::Disabled)
			{
				if (m_bPerformanceOverviewShowTGParameter)
				{
					int nValue = m_pMiniDexed->GetTGParameter (
						(CMiniDexed::TTGParameter) m_nPerformanceOverviewTGParameter, nTG);
					Token = FormatOverviewTGParameterValue (m_nPerformanceOverviewTGParameter, nValue);
				}
				else
				{
					Token = Short3 (m_pMiniDexed->GetVoiceName (nTG));
				}
			}
		}

		std::string &Line = (nTG < 4) ? Line1 : Line2;
		if (!Line.empty ())
		{
			Line += " ";
		}
		Line += Token;
	}

	m_pUI->DisplayWrite ("", Line1.c_str (), Line2.c_str (), false, false);
}


void CUIMenu::DisplayAltPotGlobalLabels (void)
{
	std::string Line1;
	std::string Line2;

	for (unsigned nControl = 0; nControl < 8; nControl++)
	{
		std::string Token = m_pMiniDexed->GetAltPotGlobalControlShortName (nControl);
		std::string &Line = (nControl < 4) ? Line1 : Line2;
		if (!Line.empty ())
		{
			Line += " ";
		}
		Line += Token;
	}

	m_pUI->DisplayWrite ("", Line1.c_str (), Line2.c_str (), false, false);
}

void CUIMenu::ArmPerformanceOverviewTimer (unsigned nDelayMS, bool bShowOverviewNext, bool bResetParameterHold)
{
	// Voice overview keeps the normal Performance page 1 / page 2 sequence.
	// When a TG parameter overview is selected (Volume, Pan, Detune, Octave,
	// Cutoff, etc.), page 2 becomes a pinned monitor: it stays there until the
	// user explicitly selects Voice again.  This gives enough time to read and
	// compare all 8 TG values.
	if ((m_bPerformanceOverviewShowTGParameter || m_bPerformanceOverviewAltPotGlobalLabels)
		&& nDelayMS >= 4000 && !bShowOverviewNext)
	{
		// Global AltPot page 2B still uses a short timed transition: first show
		// values, then show the global control labels.  When that transition
		// completes, stay pinned on page 2B instead of returning to page 1.
		if (m_bPerformanceOverviewAltPotGlobalLabels && m_nPerformanceOverviewHoldRemainingMS > 0)
		{
			nDelayMS = 150;
			bShowOverviewNext = true;
		}
		else
		{
			if (bResetParameterHold)
			{
				m_nPerformanceOverviewHoldRemainingMS = 0;
			}

			// Cancel any pending automatic page flip and leave page 2 displayed.
			m_nPerformanceOverviewSequence++;
			m_bPerformanceOverviewPage = true;
			return;
		}
	}
	else if (bResetParameterHold)
	{
		m_nPerformanceOverviewHoldRemainingMS = 0;
	}

	m_nPerformanceOverviewSequence++;
	m_bPerformanceOverviewPage = bShowOverviewNext;

	CTimer::Get ()->StartKernelTimer (MSEC2HZ (nDelayMS), PerformanceOverviewTimerHandler,
					 (void *)(uintptr_t) m_nPerformanceOverviewSequence, this);
}

void CUIMenu::PerformanceOverviewTimerHandler (TKernelTimerHandle hTimer, void *pParam, void *pContext)
{
	CUIMenu *pThis = (CUIMenu *) pContext;
	assert (pThis);

	unsigned nSequence = (unsigned)(uintptr_t) pParam;
	if (nSequence != pThis->m_nPerformanceOverviewSequence)
	{
		return;
	}

	if (!pThis->IsPerformanceMenuActive ())
	{
		return;
	}

	if (pThis->m_bPerformanceOverviewPage)
	{
		pThis->DisplayPerformanceTGOverview ();

		if ((pThis->m_bPerformanceOverviewShowTGParameter || pThis->m_bPerformanceOverviewAltPotGlobalLabels)
			&& pThis->m_nPerformanceOverviewHoldRemainingMS > 0)
		{
			if (pThis->m_nPerformanceOverviewHoldRemainingMS > 150)
			{
				pThis->m_nPerformanceOverviewHoldRemainingMS -= 150;
				pThis->ArmPerformanceOverviewTimer (150, true, false);
			}
			else
			{
				pThis->m_nPerformanceOverviewHoldRemainingMS = 0;
				if (pThis->m_bPerformanceOverviewShowTGParameter ||
				    pThis->m_bPerformanceOverviewAltPotGlobalLabels)
				{
					// Parameter pages are pinned.  Keep page 2/page 2B visible
					// after the optional global-label transition; do not resume
					// the page 1 / page 2 sequence until Voice is selected.
					pThis->DisplayPerformanceTGOverview ();
					pThis->ArmPerformanceOverviewTimer (4000, false);
				}
				else
				{
					pThis->m_bPerformanceOverviewSuppressArm = true;
					pThis->EventHandler (MenuEventUpdate);
					pThis->m_bPerformanceOverviewSuppressArm = false;
					pThis->ArmPerformanceOverviewTimer (2000, true);
				}
			}
		}
		else
		{
			pThis->ArmPerformanceOverviewTimer (4000, false);
		}
	}
	else
	{
		pThis->m_bPerformanceOverviewSuppressArm = true;
		pThis->EventHandler (MenuEventUpdate);
		pThis->m_bPerformanceOverviewSuppressArm = false;
		pThis->ArmPerformanceOverviewTimer (2000, true);
	}
}



void CUIMenu::DisplayAltPotBankOverlay (void)
{
	if (m_pMiniDexed->IsAltPotGlobalMode ())
	{
		DisplayMIDIButtonOverlay ("AltPot Mode", "Global", 1000);
	}
	else
	{
		DisplayMIDIButtonOverlay ("Alt Knobs", m_pMiniDexed->GetAltPotBankName (), 1000);
	}
}

void CUIMenu::DisplayMIDIButtonOverlay (const char *pLine1, const char *pLine2, unsigned nDelayMS)
{
	m_bSysExDisplayActive = true;
	m_nSysExDisplaySequence++;

	CTimer::Get ()->StartKernelTimer (MSEC2HZ (nDelayMS), SysExDisplayTimerHandler,
					 (void *)(uintptr_t) m_nSysExDisplaySequence, this);

	m_pUI->DisplayWrite ("", pLine1 ? pLine1 : "MIDI Button",
				      pLine2 ? pLine2 : "", false, false);
}

void CUIMenu::ShowAltPotController (unsigned nTG, const char *pParameterName, int nValue)
{
	(void) pParameterName;
	(void) nValue;

	if (!IsPerformanceMenuActive ())
	{
		return;
	}

	if (m_pMiniDexed->IsAltPotGlobalMode ())
	{
		m_bPerformanceOverviewAltPotGlobalLabels = true;
		m_nPerformanceOverviewAltPotGlobalControl = nTG;
		m_bPerformanceOverviewNoteShiftFine = false;
		m_bPerformanceOverviewEditActive = false;

		CMiniDexed::TTGParameter Param = m_pMiniDexed->GetAltPotGlobalTGParameter (nTG);
		if (Param != CMiniDexed::TGParameterUnknown)
		{
			m_bPerformanceOverviewShowTGParameter = true;
			m_nPerformanceOverviewTGParameter = Param;
			m_nPerformanceOverviewHoldRemainingMS = 4000;
			DisplayPerformanceTGOverview ();
		}
		else
		{
			// Volume Trim is live Expression, not the saved Volume parameter, so
			// showing TG Volume values would be misleading.  Go straight to page 2b.
			m_bPerformanceOverviewShowTGParameter = false;
			m_nPerformanceOverviewHoldRemainingMS = 3000;
			DisplayPerformanceTGOverview ();
		}

		ArmPerformanceOverviewTimer (4000, false);
		return;
	}

	if (nTG >= m_nToneGenerators)
	{
		return;
	}

	// Do not show a third/temporary AltPot page while a controller is moving.
	// Page 2 is the live monitor; keep it armed and let the fast refresh show
	// the real Performance NoteShift value.
	CMiniDexed::TTGParameter Param = m_pMiniDexed->GetAltPotTGParameter ();
	if (Param != CMiniDexed::TGParameterUnknown)
	{
		m_bPerformanceOverviewShowTGParameter = true;
		m_bPerformanceOverviewAltPotGlobalLabels = false;
		m_nPerformanceOverviewTGParameter = Param;
		m_bPerformanceOverviewNoteShiftFine =
			m_pMiniDexed->GetAltPotBank () == CMiniDexed::AltPotBankNoteShift;
		DisplayPerformanceTGOverview ();
		ArmPerformanceOverviewTimer (4000, false);
	}
}

void CUIMenu::ShowVoiceDataElement (unsigned nTG, unsigned nVoiceDataElement, unsigned nValue)
{
	if (nTG >= m_nToneGenerators)
	{
		return;
	}

	// Raw DX7 voice data layout:
	//   0..125   = 6 operators, 21 parameters each, ordered OP6..OP1
	//   126..144 = common voice parameters
	//   145..154 = voice name characters
	//   155      = operator enable mask (handled separately by MiniDexed)
	if (nVoiceDataElement >= 145)
	{
		// Name bytes and operator mask are not normal Edit Voice menu parameters.
		return;
	}

	// MiniDexed may call this once for every TG that matches the SysEx MIDI channel.
	// Accumulate consecutive calls for the same SysEx edit so the LCD can show, for example:
	//   TG1+2+3
	//   LFO Speed 45
	static unsigned s_nLastVoiceDataElement = 999;
	static unsigned s_nLastValue = 999;
	static unsigned s_nLastTG = 999;
	static unsigned s_nTGMask = 0;

	if (nVoiceDataElement != s_nLastVoiceDataElement ||
	    nValue != s_nLastValue ||
	    nTG <= s_nLastTG)
	{
		s_nTGMask = 0;
	}

	s_nTGMask |= (1U << nTG);
	s_nLastVoiceDataElement = nVoiceDataElement;
	s_nLastValue = nValue;
	s_nLastTG = nTG;

	string TGs;
	bool bFirstTG = true;
	for (unsigned i = 0; i < m_nToneGenerators; i++)
	{
		if (!(s_nTGMask & (1U << i)))
		{
			continue;
		}

		string Part;
		if (bFirstTG)
		{
			Part = "TG" + to_string (i + 1);
		}
		else
		{
			Part = "+" + to_string (i + 1);
		}

		if (TGs.length () + Part.length () > 16)
		{
			TGs += "+...";
			break;
		}

		TGs += Part;
		bFirstTG = false;
	}

	string ParameterName;
	if (nVoiceDataElement < 126)
	{
		unsigned nDX7OPBlock = nVoiceDataElement / 21;
		unsigned nOP = 5 - nDX7OPBlock; // DX7 SysEx stores OP6 first; UI shows OP1 first.
		unsigned nOPParameter = nVoiceDataElement % 21;

		unsigned nOPMenuIndex = 0;
		while (s_OperatorMenu[nOPMenuIndex].Name)
		{
			if (s_OperatorMenu[nOPMenuIndex].Handler == EditOPParameter &&
			    s_OperatorMenu[nOPMenuIndex].Parameter == nOPParameter)
			{
				break;
			}
			nOPMenuIndex++;
		}
		if (!s_OperatorMenu[nOPMenuIndex].Name)
		{
			return;
		}

		ParameterName = "OP" + to_string (nOP + 1) + " " + s_OperatorMenu[nOPMenuIndex].Name;
	}
	else
	{
		unsigned nVoiceParameter = nVoiceDataElement - 126;
		unsigned nVoiceMenuIndex = 0;
		while (s_EditVoiceMenu[nVoiceMenuIndex].Name)
		{
			if (s_EditVoiceMenu[nVoiceMenuIndex].Handler == EditVoiceParameter &&
			    s_EditVoiceMenu[nVoiceMenuIndex].Parameter == nVoiceParameter)
			{
				break;
			}
			nVoiceMenuIndex++;
		}
		if (!s_EditVoiceMenu[nVoiceMenuIndex].Name)
		{
			return;
		}

		ParameterName = s_EditVoiceMenu[nVoiceMenuIndex].Name;
	}

	string ValueLine = ParameterName + " " + to_string (nValue);
	if (ValueLine.length () > 16)
	{
		ValueLine = ValueLine.substr (0, 16);
	}

	m_bSysExDisplayActive = true;
	m_nSysExDisplaySequence++;

	// Keep the feedback visible briefly after the last received SysEx.
	// Every incoming message starts a new timer with a new sequence number;
	// older timers are ignored, so fast knob movements do not clear the
	// display between consecutive SysEx messages.
	CTimer::Get ()->StartKernelTimer (MSEC2HZ (3000), SysExDisplayTimerHandler,
					 (void *)(uintptr_t) m_nSysExDisplaySequence, this);

	m_pUI->DisplayWrite ("", TGs.c_str (), ValueLine.c_str (), false, false);
}

void CUIMenu::MenuHandler (CUIMenu *pUIMenu, TMenuEvent Event)
{
	switch (Event)
	{
	case MenuEventUpdate:
		break;

	case MenuEventSelect:				// push menu
		assert (pUIMenu->m_nCurrentMenuDepth < MaxMenuDepth);
		pUIMenu->m_MenuStackParent[pUIMenu->m_nCurrentMenuDepth] = pUIMenu->m_pParentMenu;
		pUIMenu->m_MenuStackMenu[pUIMenu->m_nCurrentMenuDepth] = pUIMenu->m_pCurrentMenu;
		pUIMenu->m_nMenuStackItem[pUIMenu->m_nCurrentMenuDepth]
			= pUIMenu->m_nCurrentMenuItem;
		pUIMenu->m_nMenuStackSelection[pUIMenu->m_nCurrentMenuDepth]
			= pUIMenu->m_nCurrentSelection;
		pUIMenu->m_nMenuStackParameter[pUIMenu->m_nCurrentMenuDepth]
			= pUIMenu->m_nCurrentParameter;
		pUIMenu->m_nCurrentMenuDepth++;

		pUIMenu->m_pParentMenu = pUIMenu->m_pCurrentMenu;
		pUIMenu->m_nCurrentParameter =
			pUIMenu->m_pCurrentMenu[pUIMenu->m_nCurrentSelection].Parameter;
		pUIMenu->m_pCurrentMenu =
			pUIMenu->m_pCurrentMenu[pUIMenu->m_nCurrentSelection].MenuItem;
		pUIMenu->m_nCurrentMenuItem = pUIMenu->m_nCurrentSelection;
		pUIMenu->m_nCurrentSelection = 0;
		if (pUIMenu->m_MenuStackMenu[pUIMenu->m_nCurrentMenuDepth-1] == s_MainMenu &&
		    pUIMenu->m_pCurrentMenu == s_TGMenu &&
		    pUIMenu->m_nMenuStackSelection[pUIMenu->m_nCurrentMenuDepth-1] < pUIMenu->m_nToneGenerators)
		{
			pUIMenu->m_pMiniDexed->SetTGSoloTG (pUIMenu->m_nMenuStackSelection[pUIMenu->m_nCurrentMenuDepth-1]);
		}
		break;

	case MenuEventStepDown:
		if (pUIMenu->m_nCurrentSelection == 0)
		{
			// If in main mennu, wrap around
			if (pUIMenu->m_pCurrentMenu == s_MainMenu)
			{
				// Find last entry with a name
				while (pUIMenu->m_pCurrentMenu[pUIMenu->m_nCurrentSelection+1].Name)
				{
					pUIMenu->m_nCurrentSelection++;
				}
			}
		}
		else if (pUIMenu->m_nCurrentSelection > 0)
		{
			pUIMenu->m_nCurrentSelection--;
		}
		// Might need to trim menu if number of TGs is configured to be less than the maximum supported
		while ((pUIMenu->m_pCurrentMenu == s_MainMenu) && (pUIMenu->m_nCurrentSelection > 0) &&
			  	(	// Skip any unused menus
			   		(pUIMenu->m_pCurrentMenu[pUIMenu->m_nCurrentSelection].MenuItem == s_TGMenu) &&
			   		(pUIMenu->m_pCurrentMenu[pUIMenu->m_nCurrentSelection].Parameter >= pUIMenu->m_nToneGenerators) &&
			   		(pUIMenu->m_pCurrentMenu[pUIMenu->m_nCurrentSelection].Parameter < CConfig::AllToneGenerators)
				)
			  )
		{
			pUIMenu->m_nCurrentSelection--;
		}
		break;

	case MenuEventStepUp:
		++pUIMenu->m_nCurrentSelection;
		if (!pUIMenu->m_pCurrentMenu[pUIMenu->m_nCurrentSelection].Name)  // more entries?
		{
			if (pUIMenu->m_pCurrentMenu == s_MainMenu)
			{
				// If in main mennu, wrap around
				pUIMenu->m_nCurrentSelection = 0;
			}
			else
			{
				// Return to last known good item
				pUIMenu->m_nCurrentSelection--;
			}
		}
		// Might need to trim menu if number of TGs is configured to be less than the maximum supported
		while ((pUIMenu->m_pCurrentMenu == s_MainMenu) && (pUIMenu->m_pCurrentMenu[pUIMenu->m_nCurrentSelection+1].Name) &&
			   	(	// Skip any unused TG menus
					(pUIMenu->m_pCurrentMenu[pUIMenu->m_nCurrentSelection].MenuItem == s_TGMenu) &&
					(pUIMenu->m_pCurrentMenu[pUIMenu->m_nCurrentSelection].Parameter >= pUIMenu->m_nToneGenerators) &&
					(pUIMenu->m_pCurrentMenu[pUIMenu->m_nCurrentSelection].Parameter < CConfig::AllToneGenerators)
				)
			  )
		{
			pUIMenu->m_nCurrentSelection++;
		}
		break;

	default:
		return;
	}

	if (pUIMenu->m_pCurrentMenu)				// if this is another menu?
	{
		bool bIsMainMenu = pUIMenu->m_pCurrentMenu == s_MainMenu;
		pUIMenu->m_pUI->DisplayWrite (
			pUIMenu->m_pParentMenu[pUIMenu->m_nCurrentMenuItem].Name,
			"",
			pUIMenu->m_pCurrentMenu[pUIMenu->m_nCurrentSelection].Name,
			pUIMenu->m_nCurrentSelection > 0 || bIsMainMenu,
			!!pUIMenu->m_pCurrentMenu[pUIMenu->m_nCurrentSelection+1].Name || bIsMainMenu);
	}
	else
	{
		pUIMenu->EventHandler (MenuEventUpdate);	// no, update parameter display
	}
}

void CUIMenu::EditGlobalParameter (CUIMenu *pUIMenu, TMenuEvent Event)
{
	CMiniDexed::TParameter Param = (CMiniDexed::TParameter) pUIMenu->m_nCurrentParameter;
	const TParameter &rParam = s_GlobalParameter[Param];

	int nValue = pUIMenu->m_pMiniDexed->GetParameter (Param);

	switch (Event)
	{
	case MenuEventUpdate:
	case MenuEventUpdateParameter:
		break;

	case MenuEventStepDown:
		nValue -= rParam.Increment;
		if (nValue < rParam.Minimum)
		{
			nValue = rParam.Minimum;
		}
		pUIMenu->m_pMiniDexed->SetParameter (Param, nValue);
		break;

	case MenuEventStepUp:
		nValue += rParam.Increment;
		if (nValue > rParam.Maximum)
		{
			nValue = rParam.Maximum;
		}
		pUIMenu->m_pMiniDexed->SetParameter (Param, nValue);
		break;

	default:
		return;
	}

	const char *pMenuName =
		pUIMenu->m_MenuStackParent[pUIMenu->m_nCurrentMenuDepth-1]
			[pUIMenu->m_nMenuStackItem[pUIMenu->m_nCurrentMenuDepth-1]].Name;

	string Value = GetGlobalValueString (Param, pUIMenu->m_pMiniDexed->GetParameter (Param));

	pUIMenu->m_pUI->DisplayWrite (pMenuName,
				      pUIMenu->m_pParentMenu[pUIMenu->m_nCurrentMenuItem].Name,
				      Value.c_str (),
				      nValue > rParam.Minimum, nValue < rParam.Maximum);
}

void CUIMenu::EditVoiceBankNumber (CUIMenu *pUIMenu, TMenuEvent Event)
{
	unsigned nTG = pUIMenu->m_nMenuStackParameter[pUIMenu->m_nCurrentMenuDepth-1];

	int nValue = pUIMenu->m_pMiniDexed->GetTGParameter (CMiniDexed::TGParameterVoiceBank, nTG);

	switch (Event)
	{
	case MenuEventUpdate:
	case MenuEventUpdateParameter:
		break;

	case MenuEventStepDown:
		nValue = pUIMenu->m_pMiniDexed->GetSysExFileLoader ()->GetNextBankDown(nValue);
		pUIMenu->m_pMiniDexed->SetTGParameter (
			CMiniDexed::TGParameterVoiceBank, nValue, nTG);
		break;

	case MenuEventStepUp:
		nValue = pUIMenu->m_pMiniDexed->GetSysExFileLoader ()->GetNextBankUp(nValue);
		pUIMenu->m_pMiniDexed->SetTGParameter (
			CMiniDexed::TGParameterVoiceBank, nValue, nTG);
		break;

	case MenuEventPressAndStepDown:
	case MenuEventPressAndStepUp:
		pUIMenu->TGShortcutHandler (Event);
		return;

	default:
		return;
	}

	string TG ("TG");
	TG += to_string (nTG+1);

	string Value =   to_string (nValue+1) + "="
		       + pUIMenu->m_pMiniDexed->GetSysExFileLoader ()->GetBankName (nValue);

	pUIMenu->m_pUI->DisplayWrite (TG.c_str (),
				      pUIMenu->m_pParentMenu[pUIMenu->m_nCurrentMenuItem].Name,
				      Value.c_str (),
				      nValue > 0, nValue < (int) CSysExFileLoader::MaxVoiceBankID);
}

void CUIMenu::EditProgramNumber (CUIMenu *pUIMenu, TMenuEvent Event)
{
	unsigned nTG = pUIMenu->m_nMenuStackParameter[pUIMenu->m_nCurrentMenuDepth-1];

	int nValue = pUIMenu->m_pMiniDexed->GetTGParameter (CMiniDexed::TGParameterProgram, nTG);

	switch (Event)
	{
	case MenuEventUpdate:
	case MenuEventUpdateParameter:
		break;

	case MenuEventStepDown:
		if (--nValue < 0)
		{
			// Switch down a voice bank and set to the last voice
			nValue = CSysExFileLoader::VoicesPerBank-1;
			int nVB = pUIMenu->m_pMiniDexed->GetTGParameter(CMiniDexed::TGParameterVoiceBank, nTG);
			nVB = pUIMenu->m_pMiniDexed->GetSysExFileLoader ()->GetNextBankDown(nVB);
			pUIMenu->m_pMiniDexed->SetTGParameter (CMiniDexed::TGParameterVoiceBank, nVB, nTG);
		}
		pUIMenu->m_pMiniDexed->SetTGParameter (CMiniDexed::TGParameterProgram, nValue, nTG);
		break;

	case MenuEventStepUp:
		if (++nValue > (int) CSysExFileLoader::VoicesPerBank-1)
		{
			// Switch up a voice bank and reset to voice 0
			nValue = 0;
			int nVB = pUIMenu->m_pMiniDexed->GetTGParameter(CMiniDexed::TGParameterVoiceBank, nTG);
			nVB = pUIMenu->m_pMiniDexed->GetSysExFileLoader ()->GetNextBankUp(nVB);
			pUIMenu->m_pMiniDexed->SetTGParameter (CMiniDexed::TGParameterVoiceBank, nVB, nTG);
		}
		pUIMenu->m_pMiniDexed->SetTGParameter (CMiniDexed::TGParameterProgram, nValue, nTG);
		break;

	case MenuEventPressAndStepDown:
	case MenuEventPressAndStepUp:
		pUIMenu->TGShortcutHandler (Event);
		return;

	default:
		return;
	}

	// Skip empty voices.
	// Use same criteria in PgmUpDownHandler() too.
	string voiceName = pUIMenu->m_pMiniDexed->GetVoiceName (nTG);
	if (voiceName == "EMPTY     "
	    || voiceName == "          "
	    || voiceName == "----------"
	    || voiceName == "~~~~~~~~~~" )
	{
		if (Event == MenuEventStepUp) {
			CUIMenu::EditProgramNumber (pUIMenu, MenuEventStepUp);
		}
		if (Event == MenuEventStepDown) {
			CUIMenu::EditProgramNumber (pUIMenu, MenuEventStepDown);
		}
	} else {
		// Format: 000:000      TG1 (bank:voice padded, TGx right-aligned)
		int nBank = pUIMenu->m_pMiniDexed->GetTGParameter(CMiniDexed::TGParameterVoiceBank, nTG);
		std::string left = "000";
		left += std::to_string(nBank+1);
		left = left.substr(left.length()-3,3);
		left += ":";
		std::string voiceNum = "000";
		voiceNum += std::to_string(nValue+1);
		voiceNum = voiceNum.substr(voiceNum.length()-3,3);
		left += voiceNum;

		std::string tgLabel = "TG" + std::to_string(nTG+1);
		unsigned lcdCols = pUIMenu->m_pConfig->GetLCDColumns();
		unsigned pad = 0;
		if (lcdCols > left.length() + tgLabel.length())
			pad = lcdCols - (unsigned)(left.length() + tgLabel.length());
		std::string topLine = left + std::string(pad, ' ') + tgLabel;

		std::string Value = pUIMenu->m_pMiniDexed->GetVoiceName (nTG);

		pUIMenu->m_pUI->DisplayWrite (topLine.c_str(),
					  "",
					  Value.c_str(),
					  nValue > 0, nValue < (int) CSysExFileLoader::VoicesPerBank);
	}
}

void CUIMenu::EditTGParameter (CUIMenu *pUIMenu, TMenuEvent Event)
{
	unsigned nTG = pUIMenu->m_nMenuStackParameter[pUIMenu->m_nCurrentMenuDepth-1];

	CMiniDexed::TTGParameter Param = (CMiniDexed::TTGParameter) pUIMenu->m_nCurrentParameter;
	const TParameter &rParam = s_TGParameter[Param];

	int nValue = pUIMenu->m_pMiniDexed->GetTGParameter (Param, nTG);

	switch (Event)
	{
	case MenuEventUpdate:
	case MenuEventUpdateParameter:
		break;

	case MenuEventStepDown:
		nValue -= rParam.Increment;
		if (nValue < rParam.Minimum)
		{
			nValue = rParam.Minimum;
		}
		pUIMenu->m_pMiniDexed->SetTGParameter (Param, nValue, nTG);
		break;

	case MenuEventStepUp:
		nValue += rParam.Increment;
		if (nValue > rParam.Maximum)
		{
			nValue = rParam.Maximum;
		}
		pUIMenu->m_pMiniDexed->SetTGParameter (Param, nValue, nTG);
		break;

	case MenuEventPressAndStepDown:
	case MenuEventPressAndStepUp:
		pUIMenu->TGShortcutHandler (Event);
		return;

	default:
		return;
	}

	string TG ("TG");
	TG += to_string (nTG+1);

	string Value = GetTGValueString (Param, pUIMenu->m_pMiniDexed->GetTGParameter (Param, nTG));

	pUIMenu->m_pUI->DisplayWrite (TG.c_str (),
				      pUIMenu->m_pParentMenu[pUIMenu->m_nCurrentMenuItem].Name,
				      Value.c_str (),
				      nValue > rParam.Minimum, nValue < rParam.Maximum);
}

void CUIMenu::EditTGParameter2 (CUIMenu *pUIMenu, TMenuEvent Event) // second menu level. Redundant code but in order to not modified original code
{

	unsigned nTG = pUIMenu->m_nMenuStackParameter[pUIMenu->m_nCurrentMenuDepth-2]; 

	CMiniDexed::TTGParameter Param = (CMiniDexed::TTGParameter) pUIMenu->m_nCurrentParameter;
	const TParameter &rParam = s_TGParameter[Param];

	int nValue = pUIMenu->m_pMiniDexed->GetTGParameter (Param, nTG);

	switch (Event)
	{
	case MenuEventUpdate:
	case MenuEventUpdateParameter:
		break;

	case MenuEventStepDown:
		nValue -= rParam.Increment;
		if (nValue < rParam.Minimum)
		{
			nValue = rParam.Minimum;
		}
		pUIMenu->m_pMiniDexed->SetTGParameter (Param, nValue, nTG);
		break;

	case MenuEventStepUp:
		nValue += rParam.Increment;
		if (nValue > rParam.Maximum)
		{
			nValue = rParam.Maximum;
		}
		pUIMenu->m_pMiniDexed->SetTGParameter (Param, nValue, nTG);
		break;

	case MenuEventPressAndStepDown:
	case MenuEventPressAndStepUp:
		pUIMenu->TGShortcutHandler (Event);
		return;

	default:
		return;
	}

	string TG ("TG");
	TG += to_string (nTG+1);

	string Value = GetTGValueString (Param, pUIMenu->m_pMiniDexed->GetTGParameter (Param, nTG));

	pUIMenu->m_pUI->DisplayWrite (TG.c_str (),
				      pUIMenu->m_pParentMenu[pUIMenu->m_nCurrentMenuItem].Name,
				      Value.c_str (),
				      nValue > rParam.Minimum, nValue < rParam.Maximum);
				   
}

void CUIMenu::EditVoiceParameter (CUIMenu *pUIMenu, TMenuEvent Event)
{
	unsigned nTG = pUIMenu->m_nMenuStackParameter[pUIMenu->m_nCurrentMenuDepth-2];

	unsigned nParam = pUIMenu->m_nCurrentParameter;
	const TParameter &rParam = s_VoiceParameter[nParam];

	int nValue = pUIMenu->m_pMiniDexed->GetVoiceParameter (nParam, CMiniDexed::NoOP, nTG);

	switch (Event)
	{
	case MenuEventUpdate:
	case MenuEventUpdateParameter:
		break;

	case MenuEventStepDown:
		nValue -= rParam.Increment;
		if (nValue < rParam.Minimum)
		{
			nValue = rParam.Minimum;
		}
		pUIMenu->m_pMiniDexed->SetVoiceParameter (nParam, nValue, CMiniDexed::NoOP, nTG);
		break;

	case MenuEventStepUp:
		nValue += rParam.Increment;
		if (nValue > rParam.Maximum)
		{
			nValue = rParam.Maximum;
		}
		pUIMenu->m_pMiniDexed->SetVoiceParameter (nParam, nValue, CMiniDexed::NoOP, nTG);
		break;

	case MenuEventPressAndStepDown:
	case MenuEventPressAndStepUp:
		pUIMenu->TGShortcutHandler (Event);
		return;

	default:
		return;
	}

	string TG ("TG");
	TG += to_string (nTG+1);

	string Value = GetVoiceValueString (nParam, nValue);

	pUIMenu->m_pUI->DisplayWrite (TG.c_str (),
				      pUIMenu->m_pParentMenu[pUIMenu->m_nCurrentMenuItem].Name,
				      Value.c_str (),
				      nValue > rParam.Minimum, nValue < rParam.Maximum);
}

void CUIMenu::EditOPParameter (CUIMenu *pUIMenu, TMenuEvent Event)
{
	unsigned nTG = pUIMenu->m_nMenuStackParameter[pUIMenu->m_nCurrentMenuDepth-3];
	unsigned nOP = pUIMenu->m_nMenuStackParameter[pUIMenu->m_nCurrentMenuDepth-1];

	unsigned nParam = pUIMenu->m_nCurrentParameter;
	const TParameter &rParam = s_OPParameter[nParam];

	int nValue = pUIMenu->m_pMiniDexed->GetVoiceParameter (nParam, nOP, nTG);

	switch (Event)
	{
	case MenuEventUpdate:
	case MenuEventUpdateParameter:
		break;

	case MenuEventStepDown:
		nValue -= rParam.Increment;
		if (nValue < rParam.Minimum)
		{
			nValue = rParam.Minimum;
		}
		pUIMenu->m_pMiniDexed->SetVoiceParameter (nParam, nValue, nOP, nTG);
		break;

	case MenuEventStepUp:
		nValue += rParam.Increment;
		if (nValue > rParam.Maximum)
		{
			nValue = rParam.Maximum;
		}
		pUIMenu->m_pMiniDexed->SetVoiceParameter (nParam, nValue, nOP, nTG);
		break;

	case MenuEventPressAndStepDown:
	case MenuEventPressAndStepUp:
		pUIMenu->OPShortcutHandler (Event);
		return;

	default:
		return;
	}

	string OP ("OP");
	OP += to_string (nOP+1);

	string Value;

	static const int FixedMultiplier[4] = {1, 10, 100, 1000};
	if (nParam == DEXED_OP_FREQ_COARSE)
	{
		if (!pUIMenu->m_pMiniDexed->GetVoiceParameter (DEXED_OP_OSC_MODE, nOP, nTG))
		{
			// Ratio
			if (!nValue)
			{
				Value = "0.50";
			}
			else
			{
				Value = to_string (nValue);
				Value += ".00";
			}
		}
		else
		{
			// Fixed
			Value = to_string (FixedMultiplier[nValue % 4]);
		}
	}
	else if (nParam == DEXED_OP_FREQ_FINE)
	{
		int nCoarse = pUIMenu->m_pMiniDexed->GetVoiceParameter (
							DEXED_OP_FREQ_COARSE, nOP, nTG);

		char Buffer[20];
		if (!pUIMenu->m_pMiniDexed->GetVoiceParameter (DEXED_OP_OSC_MODE, nOP, nTG))
		{
			// Ratio
			float fValue = 1.0f + nValue / 100.0f;
			fValue *= !nCoarse ? 0.5f : (float) nCoarse;
			sprintf (Buffer, "%.2f", (double) fValue);
		}
		else
		{
			// Fixed
			float fValue = powf (1.023293f, (float) nValue);
			fValue *= (float) FixedMultiplier[nCoarse % 4];
			sprintf (Buffer, "%.3fHz", (double) fValue);
		}

		Value = Buffer;
	}
	else
	{
		Value = GetOPValueString (nParam, nValue);
	}

	pUIMenu->m_pUI->DisplayWrite (OP.c_str (),
				      pUIMenu->m_pParentMenu[pUIMenu->m_nCurrentMenuItem].Name,
				      Value.c_str (),
				      nValue > rParam.Minimum, nValue < rParam.Maximum);
}

void CUIMenu::SavePerformance (CUIMenu *pUIMenu, TMenuEvent Event)
{
	if (Event != MenuEventUpdate)
	{
		return;
	}

	bool bOK = pUIMenu->m_pMiniDexed->SavePerformance (pUIMenu->m_nCurrentParameter == 1);

	const char *pMenuName =
		pUIMenu->m_MenuStackParent[pUIMenu->m_nCurrentMenuDepth-1]
			[pUIMenu->m_nMenuStackItem[pUIMenu->m_nCurrentMenuDepth-1]].Name;

	pUIMenu->m_pUI->DisplayWrite (pMenuName,
				      pUIMenu->m_pParentMenu[pUIMenu->m_nCurrentMenuItem].Name,
				      bOK ? "Completed" : "Error",
				      false, false);

	CTimer::Get ()->StartKernelTimer (MSEC2HZ (1500), TimerHandler, 0, pUIMenu);
}

string CUIMenu::GetGlobalValueString (unsigned nParameter, int nValue)
{
	string Result;

	assert (nParameter < sizeof CUIMenu::s_GlobalParameter / sizeof CUIMenu::s_GlobalParameter[0]);

	CUIMenu::TToString *pToString = CUIMenu::s_GlobalParameter[nParameter].ToString;
	if (pToString)
	{
		Result = (*pToString) (nValue);
	}
	else
	{
		Result = to_string (nValue);
	}

	return Result;
}

string CUIMenu::GetTGValueString (unsigned nTGParameter, int nValue)
{
	string Result;

	assert (nTGParameter < sizeof CUIMenu::s_TGParameter / sizeof CUIMenu::s_TGParameter[0]);

	CUIMenu::TToString *pToString = CUIMenu::s_TGParameter[nTGParameter].ToString;
	if (pToString)
	{
		Result = (*pToString) (nValue);
	}
	else
	{
		Result = to_string (nValue);
	}

	return Result;
}

string CUIMenu::GetVoiceValueString (unsigned nVoiceParameter, int nValue)
{
	string Result;

	assert (nVoiceParameter < sizeof CUIMenu::s_VoiceParameter / sizeof CUIMenu::s_VoiceParameter[0]);

	CUIMenu::TToString *pToString = CUIMenu::s_VoiceParameter[nVoiceParameter].ToString;
	if (pToString)
	{
		Result = (*pToString) (nValue);
	}
	else
	{
		Result = to_string (nValue);
	}

	return Result;
}

string CUIMenu::GetOPValueString (unsigned nOPParameter, int nValue)
{
	string Result;

	assert (nOPParameter < sizeof CUIMenu::s_OPParameter / sizeof CUIMenu::s_OPParameter[0]);

	CUIMenu::TToString *pToString = CUIMenu::s_OPParameter[nOPParameter].ToString;
	if (pToString)
	{
		Result = (*pToString) (nValue);
	}
	else
	{
		Result = to_string (nValue);
	}

	return Result;
}

string CUIMenu::ToVolume (int nValue)
{
    constexpr size_t NumSquares = 14;
    char VolumeBar[NumSquares + 1];
    size_t filled = (nValue * NumSquares + 63) / 127;
    for (size_t i = 0; i < NumSquares; ++i) {
        VolumeBar[i] = (i < filled) ? (char)0xFF : '.';
    }
    VolumeBar[NumSquares] = '\0';
    return VolumeBar;
}

string CUIMenu::ToPan (int nValue)
{
	assert (CConfig::LCDColumns == 16);
	static const size_t MaxChars = CConfig::LCDColumns-3;
	char PanMarker[MaxChars+1] = "......:......";
	unsigned nIndex = nValue * MaxChars / 127;
	if (nIndex == MaxChars)
	{
		nIndex--;
	}
	PanMarker[nIndex] = '\xFF';			// 0xFF is the block character

	return PanMarker;
}

string CUIMenu::ToOctave (int nValue)
{
	int nOct = nValue / 12;
	if (nOct > 0)
	{
		return "+" + to_string (nOct) + " oct";
	}
	if (nOct < 0)
	{
		return to_string (nOct) + " oct";
	}
	return "0";
}

string CUIMenu::ToFilterType (int nValue)
{
	switch (nValue)
	{
	case CMiniDexed::FilterTypeClassic:	return "Classic";
	case CMiniDexed::FilterTypeOff:		return "Off";
	case CMiniDexed::FilterTypeDirtyLP:	return "Dirty LP";
	case CMiniDexed::FilterTypeAcidLP:	return "Acid LP";
	case CMiniDexed::FilterTypeNasalBP:	return "Nasal BP";
	case CMiniDexed::FilterTypeTelephone:	return "Telephone";
	case CMiniDexed::FilterTypeHollowNotch:	return "Hollow";
	case CMiniDexed::FilterTypeCombMetal:	return "Comb Metal";
	default:				return "?";
	}
}

string CUIMenu::ToMIDIChannel (int nValue)
{
	switch (nValue)
	{
	case CMIDIDevice::OmniMode:	return "Omni";
	case CMIDIDevice::Disabled:	return "Off";
	default:			return to_string (nValue+1);
	}
}

string CUIMenu::ToAlgorithm (int nValue)
{
	return to_string (nValue + 1);
}

string CUIMenu::ToOnOff (int nValue)
{
	static const char *OnOff[] = {"Off", "On"};

	assert ((unsigned) nValue < sizeof OnOff / sizeof OnOff[0]);

	return OnOff[nValue];
}

string CUIMenu::ToLFOWaveform (int nValue)
{
	static const char *Waveform[] = {"Triangle", "Saw down", "Saw up",
					 "Square", "Sine", "Sample/Hold"};

	assert ((unsigned) nValue < sizeof Waveform / sizeof Waveform[0]);

	return Waveform[nValue];
}

string CUIMenu::ToTransposeNote (int nValue)
{
	nValue += NoteC3 - 24;

	assert ((unsigned) nValue < sizeof s_NoteName / sizeof s_NoteName[0]);

	return s_NoteName[nValue];
}

string CUIMenu::ToBreakpointNote (int nValue)
{
	assert ((unsigned) nValue < sizeof s_NoteName / sizeof s_NoteName[0]);

	return s_NoteName[nValue];
}

string CUIMenu::ToKeyboardCurve (int nValue)
{
	static const char *Curve[] = {"-Lin", "-Exp", "+Exp", "+Lin"};

	assert ((unsigned) nValue < sizeof Curve / sizeof Curve[0]);

	return Curve[nValue];
}

string CUIMenu::ToOscillatorMode (int nValue)
{
	static const char *Mode[] = {"Ratio", "Fixed"};

	assert ((unsigned) nValue < sizeof Mode / sizeof Mode[0]);

	return Mode[nValue];
}

string CUIMenu::ToOscillatorDetune (int nValue)
{
	string Result;

	nValue -= 7;

	if (nValue > 0)
	{
		Result = "+" + to_string (nValue);
	}
	else
	{
		Result = to_string (nValue);
	}

	return Result;
}

string CUIMenu::ToPortaMode (int nValue)
{
	switch (nValue)
	{
	case 0:		return "Fingered";
	case 1:		return "Full time";
	default:	return to_string (nValue);
	}
};

string CUIMenu::ToPortaGlissando (int nValue)
{
	switch (nValue)
	{
	case 0:		return "Off";
	case 1:		return "On";
	default:	return to_string (nValue);
	}
};

string CUIMenu::ToPolyMono (int nValue)
{
	switch (nValue)
	{
	case 0:		return "Poly";
	case 1:		return "Mono";
	default:	return to_string (nValue);
	}
}

void CUIMenu::TGShortcutHandler (TMenuEvent Event)
{
	assert (m_nCurrentMenuDepth >= 2);
	assert (m_MenuStackMenu[0] = s_MainMenu);
	unsigned nTG = m_nMenuStackSelection[0];
	assert (nTG < CConfig::AllToneGenerators);
	assert (m_nMenuStackItem[1] == nTG);
	assert (m_nMenuStackParameter[1] == nTG);

	assert (   Event == MenuEventPressAndStepDown
		|| Event == MenuEventPressAndStepUp);
	if (Event == MenuEventPressAndStepDown)
	{
		nTG--;
	}
	else
	{
		nTG++;
	}

	if (nTG < m_nToneGenerators)
	{
		m_nMenuStackSelection[0] = nTG;
		m_nMenuStackItem[1] = nTG;
		m_nMenuStackParameter[1] = nTG;

		EventHandler (MenuEventUpdate);
	}
}

void CUIMenu::OPShortcutHandler (TMenuEvent Event)
{
	assert (m_nCurrentMenuDepth >= 3);
	assert (m_MenuStackMenu[m_nCurrentMenuDepth-2] = s_EditVoiceMenu);
	unsigned nOP = m_nMenuStackSelection[m_nCurrentMenuDepth-2];
	assert (nOP < 6);
	assert (m_nMenuStackItem[m_nCurrentMenuDepth-1] == nOP);
	assert (m_nMenuStackParameter[m_nCurrentMenuDepth-1] == nOP);

	assert (   Event == MenuEventPressAndStepDown
		|| Event == MenuEventPressAndStepUp);
	if (Event == MenuEventPressAndStepDown)
	{
		nOP--;
	}
	else
	{
		nOP++;
	}

	if (nOP < 6)
	{
		m_nMenuStackSelection[m_nCurrentMenuDepth-2] = nOP;
		m_nMenuStackItem[m_nCurrentMenuDepth-1] = nOP;
		m_nMenuStackParameter[m_nCurrentMenuDepth-1] = nOP;

		EventHandler (MenuEventUpdate);
	}
}

void CUIMenu::PgmUpDownHandler (TMenuEvent Event)
{
	if (m_pMiniDexed->GetParameter (CMiniDexed::ParameterPerformanceSelectChannel) != CMIDIDevice::Disabled)
	{
		// Program Up/Down acts on performances
		unsigned nLastPerformance = m_pMiniDexed->GetLastPerformance();
		unsigned nPerformance = m_pMiniDexed->GetActualPerformanceID();
		unsigned nStart = nPerformance;
		//LOGNOTE("Performance actual=%d, last=%d", nPerformance, nLastPerformance);
		if (Event == MenuEventPgmDown)
		{
			do
			{
				if (nPerformance == 0)
				{
					// Wrap around
					nPerformance = nLastPerformance;
				}
				else if (nPerformance > 0)
				{
					--nPerformance;
				}
			} while ((m_pMiniDexed->IsValidPerformance(nPerformance) != true) && (nPerformance != nStart));
			m_nSelectedPerformanceID = nPerformance;
			m_pMiniDexed->SetNewPerformance(m_nSelectedPerformanceID);
			//LOGNOTE("Performance new=%d, last=%d", m_nSelectedPerformanceID, nLastPerformance);
		}
		else // MenuEventPgmUp
		{
			do
			{
				if (nPerformance == nLastPerformance)
				{
					// Wrap around
					nPerformance = 0;
				}
				else if (nPerformance < nLastPerformance)
				{
					++nPerformance;
				}
			} while ((m_pMiniDexed->IsValidPerformance(nPerformance) != true) && (nPerformance != nStart));
			m_nSelectedPerformanceID = nPerformance;
			m_pMiniDexed->SetNewPerformance(m_nSelectedPerformanceID);
			//LOGNOTE("Performance new=%d, last=%d", m_nSelectedPerformanceID, nLastPerformance);
		}
	}
	else
	{
		// Program Up/Down acts on voices within a TG.
	
		// If we're not in the root menu, then see if we are already in a TG menu,
		// then find the current TG number. Otherwise assume TG1 (nTG=0).
		unsigned nTG = 0;
		if (m_MenuStackMenu[0] == s_MainMenu && (m_pCurrentMenu == s_TGMenu) || (m_MenuStackMenu[1] == s_TGMenu)) {
			nTG = m_nMenuStackSelection[0];
		}
		assert (nTG < CConfig::AllToneGenerators);
		if (nTG < m_nToneGenerators)
		{
			int nPgm = m_pMiniDexed->GetTGParameter (CMiniDexed::TGParameterProgram, nTG);

			assert (Event == MenuEventPgmDown || Event == MenuEventPgmUp);
			if (Event == MenuEventPgmDown)
			{
				//LOGNOTE("PgmDown");
				if (--nPgm < 0)
				{
					// Switch down a voice bank and set to the last voice
					nPgm = CSysExFileLoader::VoicesPerBank-1;
					int nVB = m_pMiniDexed->GetTGParameter(CMiniDexed::TGParameterVoiceBank, nTG);
					nVB = m_pMiniDexed->GetSysExFileLoader ()->GetNextBankDown(nVB);
					m_pMiniDexed->SetTGParameter (CMiniDexed::TGParameterVoiceBank, nVB, nTG);
				}
				m_pMiniDexed->SetTGParameter (CMiniDexed::TGParameterProgram, nPgm, nTG);
			}
			else
			{
				//LOGNOTE("PgmUp");
				if (++nPgm > (int) CSysExFileLoader::VoicesPerBank-1)
				{
					// Switch up a voice bank and reset to voice 0
					nPgm = 0;
					int nVB = m_pMiniDexed->GetTGParameter(CMiniDexed::TGParameterVoiceBank, nTG);
					nVB = m_pMiniDexed->GetSysExFileLoader ()->GetNextBankUp(nVB);
					m_pMiniDexed->SetTGParameter (CMiniDexed::TGParameterVoiceBank, nVB, nTG);
				}
				m_pMiniDexed->SetTGParameter (CMiniDexed::TGParameterProgram, nPgm, nTG);
			}

			// Skip empty voices.
			// Use same criteria in EditProgramNumber () too.
			string voiceName = m_pMiniDexed->GetVoiceName (nTG);
			if (voiceName == "EMPTY     "
				|| voiceName == "          "
				|| voiceName == "----------"
				|| voiceName == "~~~~~~~~~~" )
			{
				if (Event == MenuEventStepUp) {
					PgmUpDownHandler (MenuEventStepUp);
				}
				if (Event == MenuEventStepDown) {
					PgmUpDownHandler (MenuEventStepDown);
				}
			}
		}
	}
}

void CUIMenu::BankUpDownHandler (TMenuEvent Event)
{
	if (m_pMiniDexed->GetParameter (CMiniDexed::ParameterPerformanceSelectChannel) != CMIDIDevice::Disabled)
	{
		// Bank Up/Down acts on performances
		unsigned nLastPerformanceBank = m_pMiniDexed->GetLastPerformanceBank();
		unsigned nPerformanceBank = m_nSelectedPerformanceBankID;
		unsigned nStartBank = nPerformanceBank;
		//LOGNOTE("Performance Bank actual=%d, last=%d", nPerformanceBank, nLastPerformanceBank);
		if (Event == MenuEventBankDown)
		{
			do
			{
				if (nPerformanceBank == 0)
				{
					// Wrap around
					nPerformanceBank = nLastPerformanceBank;
				}
				else if (nPerformanceBank > 0)
				{
					--nPerformanceBank;
				}
			} while ((m_pMiniDexed->IsValidPerformanceBank(nPerformanceBank) != true) && (nPerformanceBank != nStartBank));
			m_nSelectedPerformanceBankID = nPerformanceBank;
			// Switch to the new bank and select the first performance voice
			m_pMiniDexed->SetParameter (CMiniDexed::ParameterPerformanceBank, nPerformanceBank);
			m_pMiniDexed->SetFirstPerformance();
			//LOGNOTE("Performance Bank new=%d, last=%d", m_nSelectedPerformanceBankID, nLastPerformanceBank);
		}
		else // MenuEventBankUp
		{
			do
			{
				if (nPerformanceBank == nLastPerformanceBank)
				{
					// Wrap around
					nPerformanceBank = 0;
				}
				else if (nPerformanceBank < nLastPerformanceBank)
				{
					++nPerformanceBank;
				}
			} while ((m_pMiniDexed->IsValidPerformanceBank(nPerformanceBank) != true) && (nPerformanceBank != nStartBank));
			m_nSelectedPerformanceBankID = nPerformanceBank;
			m_pMiniDexed->SetParameter (CMiniDexed::ParameterPerformanceBank, nPerformanceBank);
			m_pMiniDexed->SetFirstPerformance();
			//LOGNOTE("Performance Bank new=%d, last=%d", m_nSelectedPerformanceBankID, nLastPerformanceBank);
		}
	}
	else
	{
		// Bank Up/Down acts on voices within a TG.
	
		// If we're not in the root menu, then see if we are already in a TG menu,
		// then find the current TG number. Otherwise assume TG1 (nTG=0).
		unsigned nTG = 0;
		if (m_MenuStackMenu[0] == s_MainMenu && (m_pCurrentMenu == s_TGMenu) || (m_MenuStackMenu[1] == s_TGMenu)) {
			nTG = m_nMenuStackSelection[0];
		}
		assert (nTG < CConfig::AllToneGenerators);
		if (nTG < m_nToneGenerators)
		{
			int nBank = m_pMiniDexed->GetTGParameter (CMiniDexed::TGParameterVoiceBank, nTG);

			assert (Event == MenuEventBankDown || Event == MenuEventBankUp);
			if (Event == MenuEventBankDown)
			{
				//LOGNOTE("BankDown");
				nBank = m_pMiniDexed->GetSysExFileLoader ()->GetNextBankDown(nBank);
				m_pMiniDexed->SetTGParameter (CMiniDexed::TGParameterVoiceBank, nBank, nTG);
			}
			else
			{
				//LOGNOTE("BankUp");
				nBank = m_pMiniDexed->GetSysExFileLoader ()->GetNextBankUp(nBank);
				m_pMiniDexed->SetTGParameter (CMiniDexed::TGParameterVoiceBank, nBank, nTG);
			}
		}
	}
}
	
void CUIMenu::TGUpDownHandler (TMenuEvent Event)
{
	// This will update the menus to position it for the next TG up or down
	unsigned nTG = 0;
	
	if (m_nToneGenerators <= 1) {
		// Nothing to do if only a single TG
		return;
	}

	// If we're not in the root menu, then see if we are already in a TG menu,
	// then find the current TG number. Otherwise assume TG1 (nTG=0).
	if (m_MenuStackMenu[0] == s_MainMenu && (m_pCurrentMenu == s_TGMenu) || (m_MenuStackMenu[1] == s_TGMenu)) {
		nTG = m_nMenuStackSelection[0];
	}

	assert (nTG < CConfig::AllToneGenerators);
	assert (Event == MenuEventTGDown || Event == MenuEventTGUp);
	if (Event == MenuEventTGDown)
	{
		//LOGNOTE("TGDown");
		if (nTG > 0) {
			nTG--;
		}
	}
	else
	{
		//LOGNOTE("TGUp");
		if (nTG < m_nToneGenerators - 1) {
			nTG++;
		}
	}

	TGSelectHandler (nTG);
}

void CUIMenu::TGSelectHandler (unsigned nTG)
{
	if (nTG >= m_nToneGenerators)
	{
		return;
	}

	m_pMiniDexed->SetTGSoloTG (nTG);

	// If we are already inside a TG branch below the TG menu itself, keep the
	// exact depth of the current UI position. This preserves not only the TG
	// sub-function (Cutoff, Detune, Volume, etc.) but also whether the user is
	// already editing the value.
	//
	// Example:
	//   TG2 -> Cutoff -> value 99
	//   press direct TG1 button
	//   result: TG1 -> Cutoff -> value, ready to edit
	//
	// The value editors get the active TG from m_nMenuStackParameter[1], so this
	// is the critical field to update when jumping between TGs.
	if (m_nCurrentMenuDepth >= 2 && m_MenuStackMenu[0] == s_MainMenu &&
	    m_MenuStackMenu[1] == s_TGMenu && m_pCurrentMenu != s_TGMenu)
	{
		m_nMenuStackSelection[0] = nTG;
		m_nMenuStackItem[1] = nTG;
		m_nMenuStackParameter[1] = nTG;

		EventHandler (MenuEventUpdate);
		return;
	}

	// Preserve the currently selected TG sub-function when moving between TGs.
	// Example: if the UI is on TG1 -> Cutoff, selecting TG2 should land on
	// TG2 -> Cutoff, not TG2 -> Voice.
	unsigned nSelection = 0;
	if (m_pCurrentMenu == s_TGMenu)
	{
		nSelection = m_nCurrentSelection;
	}
	else if (m_nCurrentMenuDepth >= 1 && m_MenuStackMenu[1] == s_TGMenu)
	{
		nSelection = m_nMenuStackSelection[1];
	}

	unsigned nTGMenuItems = 0;
	while (s_TGMenu[nTGMenuItems].Name)
	{
		++nTGMenuItems;
	}
	if (nSelection >= nTGMenuItems)
	{
		nSelection = 0;
	}

	// Set menu to the appropriate TG menu as follows:
	//  Top = Root
	//  Menu [0] = Main
	//  Menu [1] = TG Menu
	m_pParentMenu = s_MainMenu;
	m_pCurrentMenu = s_TGMenu;
	m_nCurrentMenuItem = nTG;
	m_nCurrentSelection = nSelection;
	// While positioned on the TG menu itself, m_nCurrentParameter must
	// carry the active TG number.  When the user presses Select, this
	// value is pushed to m_nMenuStackParameter[1], and the Voice/Bank/
	// Volume/etc. editors use that stack entry to know which TG to edit.
	// Using the selected menu item's parameter here makes Voice/Bank
	// fall back to TG1 because those menu items have parameter 0.
	m_nCurrentParameter = nTG;
	m_nCurrentMenuDepth = 1;

	// Place the main menu on the stack with Root as the parent
	m_MenuStackParent[0] = s_MenuRoot;
	m_MenuStackMenu[0] = s_MainMenu;
	m_nMenuStackItem[0] = 0;
	m_nMenuStackSelection[0] = nTG;
	m_nMenuStackParameter[0] = 0;

	EventHandler (MenuEventUpdate);
}

void CUIMenu::TGMenuSelectHandler (const char *pName)
{
	if (pName == 0)
	{
		return;
	}

	unsigned nTGMenuIndex = 0;
	while (s_TGMenu[nTGMenuIndex].Name)
	{
		if (strcmp (s_TGMenu[nTGMenuIndex].Name, pName) == 0)
		{
			break;
		}
		++nTGMenuIndex;
	}

	if (!s_TGMenu[nTGMenuIndex].Name)
	{
		return;
	}

	// Keep the current TG if we are already inside a TG context.
	unsigned nTG = 0;
	if (m_pCurrentMenu == s_TGMenu)
	{
		nTG = m_nCurrentMenuItem;
	}
	else if (m_nCurrentMenuDepth >= 1 && m_MenuStackMenu[1] == s_TGMenu)
	{
		nTG = m_nMenuStackSelection[0];
	}
	else if (m_pCurrentMenu == s_MainMenu && m_nCurrentSelection < m_nToneGenerators)
	{
		nTG = m_nCurrentSelection;
	}
	else if (m_MenuStackMenu[0] == s_MainMenu && m_nMenuStackSelection[0] < m_nToneGenerators)
	{
		nTG = m_nMenuStackSelection[0];
	}

	if (nTG >= m_nToneGenerators)
	{
		nTG = 0;
	}

	m_pMiniDexed->SetTGSoloTG (nTG);

	m_pParentMenu = s_MainMenu;
	m_pCurrentMenu = s_TGMenu;
	m_nCurrentMenuItem = nTG;
	m_nCurrentSelection = nTGMenuIndex;
	// Same rule as TGSelectHandler(): at TG-menu level the current
	// parameter is the active TG index, not the TG sub-menu parameter.
	// The sub-menu parameter is loaded after Select; the active TG must
	// be available on the stack for the editor.
	m_nCurrentParameter = nTG;
	m_nCurrentMenuDepth = 1;

	m_MenuStackParent[0] = s_MenuRoot;
	m_MenuStackMenu[0] = s_MainMenu;
	m_nMenuStackItem[0] = 0;
	m_nMenuStackSelection[0] = nTG;
	m_nMenuStackParameter[0] = 0;

	EventHandler (MenuEventUpdate);
}

void CUIMenu::MainMenuSelectHandler (const char *pName)
{
	if (pName == 0)
	{
		return;
	}

	unsigned nMainMenuIndex = 0;
	while (s_MainMenu[nMainMenuIndex].Name)
	{
		if (strcmp (s_MainMenu[nMainMenuIndex].Name, pName) == 0)
		{
			break;
		}
		++nMainMenuIndex;
	}

	if (!s_MainMenu[nMainMenuIndex].Name)
	{
		return;
	}

	// Jump directly to a top-level main menu item such as Effects, Master Volume,
	// or Performance. This mirrors selecting that item from the main menu.
	m_pParentMenu = s_MainMenu;
	m_pCurrentMenu = s_MainMenu[nMainMenuIndex].MenuItem;
	m_nCurrentMenuItem = nMainMenuIndex;
	m_nCurrentSelection = 0;
	m_nCurrentParameter = s_MainMenu[nMainMenuIndex].Parameter;
	m_nCurrentMenuDepth = 1;

	// Place the main menu on the stack with Root as the parent.
	m_MenuStackParent[0] = s_MenuRoot;
	m_MenuStackMenu[0] = s_MainMenu;
	m_nMenuStackItem[0] = 0;
	m_nMenuStackSelection[0] = nMainMenuIndex;
	m_nMenuStackParameter[0] = 0;

	EventHandler (MenuEventUpdate);
}

void CUIMenu::TimerHandler (TKernelTimerHandle hTimer, void *pParam, void *pContext)
{
	CUIMenu *pThis = static_cast<CUIMenu *> (pContext);
	assert (pThis);

	pThis->EventHandler (MenuEventBack);
}

void CUIMenu::TimerHandlerNoBack (TKernelTimerHandle hTimer, void *pParam, void *pContext)
{
	CUIMenu *pThis = static_cast<CUIMenu *> (pContext);
	assert (pThis);
	
	pThis->m_bSplashShow = false;
	
	pThis->EventHandler (MenuEventUpdate);
}

void CUIMenu::SysExDisplayTimerHandler (TKernelTimerHandle hTimer, void *pParam, void *pContext)
{
	CUIMenu *pThis = static_cast<CUIMenu *> (pContext);
	assert (pThis);

	unsigned nSequence = (unsigned)(uintptr_t) pParam;
	if (nSequence != pThis->m_nSysExDisplaySequence)
	{
		return;
	}

	pThis->m_bSysExDisplayActive = false;
	if (pThis->IsPerformanceMenuActive () &&
	    (pThis->m_bPerformanceOverviewShowTGParameter ||
	     pThis->m_bPerformanceOverviewAltPotGlobalLabels))
	{
		pThis->DisplayPerformanceTGOverview ();
		pThis->ArmPerformanceOverviewTimer (4000, false);
	}
	else
	{
		pThis->EventHandler (MenuEventUpdate);
	}
}

void CUIMenu::PerformanceMenu (CUIMenu *pUIMenu, TMenuEvent Event)
{
	bool bPerformanceSelectToLoad = pUIMenu->m_pMiniDexed->GetPerformanceSelectToLoad();
	unsigned nLastPerformance = pUIMenu->m_pMiniDexed->GetLastPerformance();
	unsigned nValue = pUIMenu->m_nSelectedPerformanceID;
	unsigned nStart = nValue;

	unsigned nLastPerformanceBank = pUIMenu->m_pMiniDexed->GetLastPerformanceBank();
	unsigned nBankValue = pUIMenu->m_nSelectedPerformanceBankID;
	unsigned nBankStart = nValue;

	if (pUIMenu->m_pMiniDexed->IsValidPerformance(nValue) != true)
	{
		// A bank change has left the selected performance out of sync
		nValue = pUIMenu->m_pMiniDexed->GetActualPerformanceID();
		pUIMenu->m_nSelectedPerformanceID = nValue;
	}
	std::string Value;
		
	if (Event == MenuEventUpdate || Event == MenuEventUpdateParameter)
	{
		pUIMenu->m_bPerformanceDeleteMode=false;
		// Ensure selected performance matches the actual loaded one
		pUIMenu->m_nSelectedPerformanceID = pUIMenu->m_pMiniDexed->GetActualPerformanceID();
	}
	
	if (pUIMenu->m_bSplashShow)
	{
		return;
	}		
	
	if(!pUIMenu->m_bPerformanceDeleteMode)
	{
		switch (Event)
		{
		case MenuEventUpdate:
			break;

		case MenuEventStepDown:
			do
			{
				if (nValue == 0)
				{
					// Wrap around
					nValue = nLastPerformance;
				}
				else if (nValue > 0)
				{
					--nValue;
				}
			} while ((pUIMenu->m_pMiniDexed->IsValidPerformance(nValue) != true) && (nValue != nStart));
			pUIMenu->m_nSelectedPerformanceID = nValue;
			if (!bPerformanceSelectToLoad && pUIMenu->m_nCurrentParameter==0)
			{
				pUIMenu->m_pMiniDexed->SetNewPerformance(nValue);
			}
			break;

		case MenuEventStepUp:
			do
			{
				if (nValue == nLastPerformance)
				{
					// Wrap around
					nValue = 0;
				}
				else if (nValue < nLastPerformance)
				{
					++nValue;
				}
			} while ((pUIMenu->m_pMiniDexed->IsValidPerformance(nValue) != true) && (nValue != nStart));
			pUIMenu->m_nSelectedPerformanceID = nValue;
			if (!bPerformanceSelectToLoad && pUIMenu->m_nCurrentParameter==0)
			{
				pUIMenu->m_pMiniDexed->SetNewPerformance(nValue);
			}
			break;

		case MenuEventPressAndStepDown:
			do
			{
				if (nBankValue == 0)
				{
					// Wrap around
					nBankValue = nLastPerformanceBank;
				}
				else if (nBankValue > 0)
				{
					--nBankValue;
				}
			} while ((pUIMenu->m_pMiniDexed->IsValidPerformanceBank(nBankValue) != true) && (nBankValue != nBankStart));
			pUIMenu->m_nSelectedPerformanceBankID = nBankValue;
			pUIMenu->m_pMiniDexed->SetParameter (CMiniDexed::ParameterPerformanceBank, nBankValue);
			pUIMenu->m_pMiniDexed->SetFirstPerformance();
			break;
		
		case MenuEventPressAndStepUp:
			do
			{
				if (nBankValue == nLastPerformanceBank)
				{
					// Wrap around
					nBankValue = 0;
				}
				else if (nBankValue < nLastPerformanceBank)
				{
					++nBankValue;
				}
			} while ((pUIMenu->m_pMiniDexed->IsValidPerformanceBank(nBankValue) != true) && (nBankValue != nBankStart));
			pUIMenu->m_nSelectedPerformanceBankID = nBankValue;
			pUIMenu->m_pMiniDexed->SetParameter (CMiniDexed::ParameterPerformanceBank, nBankValue);
			pUIMenu->m_pMiniDexed->SetFirstPerformance();
			break;

		case MenuEventSelect:	
			switch (pUIMenu->m_nCurrentParameter)
			{
			case 0:
				if (bPerformanceSelectToLoad)
				{
				pUIMenu->m_pMiniDexed->SetNewPerformance(nValue);
				}

				break;
			case 1:
				if (pUIMenu->m_pMiniDexed->IsValidPerformance(pUIMenu->m_nSelectedPerformanceID))
				{
					pUIMenu->m_bPerformanceDeleteMode=true;
					pUIMenu->m_bConfirmDeletePerformance=false;
				}
				break;
			default:
				break;
			}
			break;
		default:
			return;
		}
	}
	else
	{
		switch (Event)
		{
		case MenuEventUpdate:
			break;

		case MenuEventStepDown:
			pUIMenu->m_bConfirmDeletePerformance=false;
			break;

		case MenuEventStepUp:
			pUIMenu->m_bConfirmDeletePerformance=true;
			break;

		case MenuEventSelect:	
			pUIMenu->m_bPerformanceDeleteMode=false;
			if (pUIMenu->m_bConfirmDeletePerformance)
			{
				pUIMenu->m_nSelectedPerformanceID = 0;
				pUIMenu->m_bConfirmDeletePerformance=false;
				pUIMenu->m_pUI->DisplayWrite ("", "Delete", pUIMenu->m_pMiniDexed->DeletePerformance(nValue) ? "Completed" : "Error", false, false);
				pUIMenu->m_bSplashShow=true;
				CTimer::Get ()->StartKernelTimer (MSEC2HZ (1500), TimerHandlerNoBack, 0, pUIMenu);
				return;
			}
			else
			{
				break;
			}
			
		default:
			return;
		}		
	}
		
	if(!pUIMenu->m_bPerformanceDeleteMode)
	{
		Value = pUIMenu->m_pMiniDexed->GetPerformanceName(nValue);
		unsigned nBankNum = pUIMenu->m_pMiniDexed->GetPerformanceBank();
		
		std::string nPSelected = "000";
		nPSelected += std::to_string(nBankNum+1);  // Convert to user-facing bank number rather than index
		nPSelected = nPSelected.substr(nPSelected.length()-3,3);
		std::string nPPerf = "000";
		nPPerf += std::to_string(nValue+1);  // Convert to user-facing performance number rather than index
		nPPerf = nPPerf.substr(nPPerf.length()-3,3);

		nPSelected += ":"+nPPerf;
		if(bPerformanceSelectToLoad && nValue == pUIMenu->m_pMiniDexed->GetActualPerformanceID())
		{
			nPSelected += " [L]";
		}
					
		pUIMenu->m_pUI->DisplayWrite (pUIMenu->m_pParentMenu[pUIMenu->m_nCurrentMenuItem].Name, nPSelected.c_str(),
						  Value.c_str (), true, true);
//						 (int) nValue > 0, (int) nValue < (int) pUIMenu->m_pMiniDexed->GetLastPerformance());

		if (!pUIMenu->m_bPerformanceOverviewSuppressArm)
		{
			// Keep the normal performance screen for 2 seconds, then alternate to
			// the compact TG voice overview for 4 seconds. Any user action or
			// performance change restarts this cycle from the normal page.
			pUIMenu->ArmPerformanceOverviewTimer (2000, true);
		}
	}
	else
	{
		pUIMenu->m_pUI->DisplayWrite ("", "Delete?", pUIMenu->m_bConfirmDeletePerformance ? "Yes" : "No", false, false);
	}
}


void CUIMenu::CopyTG (CUIMenu *pUIMenu, TMenuEvent Event)
{
	if (pUIMenu->m_nToneGenerators < 2)
	{
		pUIMenu->m_pUI->DisplayWrite ("Copy TG", "", "Need 2 TGs", false, false);
		return;
	}

	unsigned nMaxTG = pUIMenu->m_nToneGenerators - 1;

	if (pUIMenu->m_nCopyTGFrom > nMaxTG)
	{
		pUIMenu->m_nCopyTGFrom = 0;
	}
	if (pUIMenu->m_nCopyTGTo > nMaxTG)
	{
		pUIMenu->m_nCopyTGTo = (nMaxTG > 0) ? 1 : 0;
	}
	if (pUIMenu->m_nCopyTGTo == pUIMenu->m_nCopyTGFrom)
	{
		pUIMenu->m_nCopyTGTo = (pUIMenu->m_nCopyTGFrom + 1) % pUIMenu->m_nToneGenerators;
	}

	unsigned *pCurrentTG = pUIMenu->m_bCopyTGSelectingTo ?
		&pUIMenu->m_nCopyTGTo : &pUIMenu->m_nCopyTGFrom;

	switch (Event)
	{
	case MenuEventUpdate:
	case MenuEventUpdateParameter:
		break;

	case MenuEventStepDown:
		if (*pCurrentTG == 0)
		{
			*pCurrentTG = nMaxTG;
		}
		else
		{
			(*pCurrentTG)--;
		}
		if (pUIMenu->m_bCopyTGSelectingTo && pUIMenu->m_nCopyTGTo == pUIMenu->m_nCopyTGFrom)
		{
			*pCurrentTG = (*pCurrentTG == 0) ? nMaxTG : *pCurrentTG - 1;
		}
		break;

	case MenuEventStepUp:
		*pCurrentTG = (*pCurrentTG + 1) % pUIMenu->m_nToneGenerators;
		if (pUIMenu->m_bCopyTGSelectingTo && pUIMenu->m_nCopyTGTo == pUIMenu->m_nCopyTGFrom)
		{
			*pCurrentTG = (*pCurrentTG + 1) % pUIMenu->m_nToneGenerators;
		}
		break;

	case MenuEventSelect:
		if (!pUIMenu->m_bCopyTGSelectingTo)
		{
			pUIMenu->m_bCopyTGSelectingTo = true;
			if (pUIMenu->m_nCopyTGTo == pUIMenu->m_nCopyTGFrom)
			{
				pUIMenu->m_nCopyTGTo = (pUIMenu->m_nCopyTGFrom + 1) % pUIMenu->m_nToneGenerators;
			}
		}
		else
		{
			unsigned nFromTG = pUIMenu->m_nCopyTGFrom;
			unsigned nToTG = pUIMenu->m_nCopyTGTo;

			pUIMenu->m_pMiniDexed->CopyTG (nFromTG, nToTG);
			pUIMenu->m_bCopyTGSelectingTo = false;

			std::string Msg = "TG" + std::to_string (nFromTG + 1) +
					  " -> TG" + std::to_string (nToTG + 1);

			pUIMenu->m_pUI->DisplayWrite ("Copy TG", Msg.c_str (), "Copied", false, false);
			CTimer::Get ()->StartKernelTimer (MSEC2HZ (1200), TimerHandler, 0, pUIMenu);
			return;
		}
		break;

	default:
		return;
	}

	std::string Value = "TG" + std::to_string (*pCurrentTG + 1);
	pUIMenu->m_pUI->DisplayWrite ("Copy TG",
				      pUIMenu->m_bCopyTGSelectingTo ? "To" : "From",
				      Value.c_str (),
				      true, true);
}


void CUIMenu::EditPerformanceBankNumber (CUIMenu *pUIMenu, TMenuEvent Event)
{
	bool bPerformanceSelectToLoad = pUIMenu->m_pMiniDexed->GetPerformanceSelectToLoad();
	unsigned nLastPerformanceBank = pUIMenu->m_pMiniDexed->GetLastPerformanceBank();
	unsigned nValue = pUIMenu->m_nSelectedPerformanceBankID;
	unsigned nStart = nValue;
	std::string Value;

	switch (Event)
	{
	case MenuEventUpdate:
		break;

	case MenuEventStepDown:
		do
		{
			if (nValue == 0)
			{
				// Wrap around
				nValue = nLastPerformanceBank;
			}
			else if (nValue > 0)
			{
				--nValue;
			}
		} while ((pUIMenu->m_pMiniDexed->IsValidPerformanceBank(nValue) != true) && (nValue != nStart));
		pUIMenu->m_nSelectedPerformanceBankID = nValue;
		if (!bPerformanceSelectToLoad)
		{
			// Switch to the new bank and select the first performance voice
			pUIMenu->m_pMiniDexed->SetParameter (CMiniDexed::ParameterPerformanceBank, nValue);
			pUIMenu->m_pMiniDexed->SetFirstPerformance();
		}
		break;

	case MenuEventStepUp:
		do
		{
			if (nValue == nLastPerformanceBank)
			{
				// Wrap around
				nValue = 0;
			}
			else if (nValue < nLastPerformanceBank)
			{
				++nValue;
			}
		} while ((pUIMenu->m_pMiniDexed->IsValidPerformanceBank(nValue) != true) && (nValue != nStart));
		pUIMenu->m_nSelectedPerformanceBankID = nValue;
		if (!bPerformanceSelectToLoad)
		{
			pUIMenu->m_pMiniDexed->SetParameter (CMiniDexed::ParameterPerformanceBank, nValue);
			pUIMenu->m_pMiniDexed->SetFirstPerformance();
		}
		break;

	case MenuEventSelect:	
		if (bPerformanceSelectToLoad)
		{
			pUIMenu->m_pMiniDexed->SetParameter (CMiniDexed::ParameterPerformanceBank, nValue);
			pUIMenu->m_pMiniDexed->SetFirstPerformance();
		}
		break;

	default:
		return;
	}

	Value = pUIMenu->m_pMiniDexed->GetPerformanceConfig ()->GetPerformanceBankName(nValue);
	std::string nPSelected = "000";
	nPSelected += std::to_string(nValue+1);  // Convert to user-facing number rather than index
	nPSelected = nPSelected.substr(nPSelected.length()-3,3);

	if(bPerformanceSelectToLoad && nValue == (unsigned)pUIMenu->m_pMiniDexed->GetParameter (CMiniDexed::ParameterPerformanceBank))
	{
		nPSelected += " [L]";
	}

	pUIMenu->m_pUI->DisplayWrite (pUIMenu->m_pParentMenu[pUIMenu->m_nCurrentMenuItem].Name, nPSelected.c_str(),
							Value.c_str (), true, true);
}

void CUIMenu::InputTxt (CUIMenu *pUIMenu, TMenuEvent Event)
{
	unsigned nTG=0;
	string TG ("TG");
	
	std::string MsgOk;
	std::string NoValidChars;
	unsigned MaxChars;
	std::string MenuTitleR;
	std::string MenuTitleL;
	std::string OkTitleL;
	std::string OkTitleR;
	
	switch(pUIMenu->m_nCurrentParameter)
	{
		case 1: // save new performance
			NoValidChars = {92, 47, 58, 42, 63, 34, 60,62, 124};
			MaxChars=14;
			MenuTitleL="Performance Name";
			MenuTitleR="";
			OkTitleL="New Performance"; // \E[?25l
			OkTitleR="";
		 break;
		 
		case 2: // Rename performance - NOT Implemented yet
			NoValidChars = {92, 47, 58, 42, 63, 34, 60,62, 124};
			MaxChars=14;
			MenuTitleL="Performance Name";
			MenuTitleR="";
			OkTitleL="Rename Perf."; // \E[?25l
			OkTitleR="";
		break;
		
		case 3: // Voice name
			nTG = pUIMenu->m_nMenuStackParameter[pUIMenu->m_nCurrentMenuDepth-2];
			NoValidChars = {127};
			MaxChars=10;
			MenuTitleL="Name";
			TG += to_string (nTG+1);
			MenuTitleR=TG;
			OkTitleL="";
			OkTitleR="";
		break;
			
		default:
		return;
	}
	
	bool bOK;
	unsigned nPosition = pUIMenu->m_InputTextPosition;
	unsigned nChar = pUIMenu->m_InputText[nPosition];

	
	switch (Event)
	{
	case MenuEventUpdate:
		if(pUIMenu->m_nCurrentParameter == 1 || pUIMenu->m_nCurrentParameter == 2)
		{
			pUIMenu->m_InputText = pUIMenu->m_pMiniDexed->GetNewPerformanceDefaultName();
			pUIMenu->m_InputText += "              ";
			pUIMenu->m_InputText =  pUIMenu->m_InputText.substr(0,14);
			pUIMenu->m_InputTextPosition=0;
			nPosition=pUIMenu->m_InputTextPosition;
			nChar = pUIMenu->m_InputText[nPosition];
		}
		else
		{
			
			pUIMenu->m_InputText = pUIMenu->m_pMiniDexed->GetVoiceName(nTG);
			pUIMenu->m_InputText += "          ";
			pUIMenu->m_InputText =  pUIMenu->m_InputText.substr(0,10);
			pUIMenu->m_InputTextPosition=0;
			nPosition=pUIMenu->m_InputTextPosition;
			nChar = pUIMenu->m_InputText[nPosition];
		}
		break;

	case MenuEventStepDown:
		if (nChar > 32)
		{
		do	{
			--nChar;
			}
		while (NoValidChars.find(nChar) != std::string::npos);
		}
		pUIMenu->m_InputTextChar = nChar;
		break;

	case MenuEventStepUp:
		if (nChar < 126)
		{
		do	{
				++nChar;
			}
		while (NoValidChars.find(nChar) != std::string::npos);			
		}
		pUIMenu->m_InputTextChar = nChar;
		break;	
		
		
		
	case MenuEventSelect:	
		if(pUIMenu->m_nCurrentParameter == 1)
		{	
			pUIMenu->m_pMiniDexed->SetNewPerformanceName(pUIMenu->m_InputText);
			bOK = pUIMenu->m_pMiniDexed->SavePerformanceNewFile ();
			MsgOk=bOK ? "Completed" : "Error";
			pUIMenu->m_pUI->DisplayWrite (OkTitleR.c_str(), OkTitleL.c_str(), MsgOk.c_str(), false, false);
			CTimer::Get ()->StartKernelTimer (MSEC2HZ (1500), TimerHandler, 0, pUIMenu);
			return;
		}
		else
		{
			break; // Voice Name Edit
		}
	
	case MenuEventPressAndStepDown:
		if (nPosition > 0)
			{
				--nPosition;
			}
		pUIMenu->m_InputTextPosition = nPosition;
		nChar = pUIMenu->m_InputText[nPosition];
		break;
	
	case MenuEventPressAndStepUp:
		if (nPosition < MaxChars-1)
		{
			++nPosition;
		}
		pUIMenu->m_InputTextPosition = nPosition;
		nChar = pUIMenu->m_InputText[nPosition];
		break;

	default:
		return;
	}
	
	
	// \E[2;%dH	Cursor move to row %1 and column %2 (starting at 1)
	// \E[?25h	Normal cursor visible
	// \E[?25l	Cursor invisible
	
	std::string escCursor="\E[?25h\E[2;"; // this is to locate cursor
	escCursor += to_string(nPosition + 2);
	escCursor += "H";
	

	std::string Value = pUIMenu->m_InputText;
	Value[nPosition]=nChar;
	pUIMenu->m_InputText = Value;
	
	if(pUIMenu->m_nCurrentParameter == 3)
		{
			pUIMenu->m_pMiniDexed->SetVoiceName(pUIMenu->m_InputText, nTG);
		}	
		
	Value = Value + " " + escCursor ;
	pUIMenu->m_pUI->DisplayWrite (MenuTitleR.c_str(),MenuTitleL.c_str(), Value.c_str(), false, false);
	
	
}

void CUIMenu::EditTGParameterModulation (CUIMenu *pUIMenu, TMenuEvent Event) 
{

	unsigned nTG = pUIMenu->m_nMenuStackParameter[pUIMenu->m_nCurrentMenuDepth-3]; 
	unsigned nController = pUIMenu->m_nMenuStackParameter[pUIMenu->m_nCurrentMenuDepth-1]; 
	unsigned nParameter = pUIMenu->m_nCurrentParameter + nController;
	
	CMiniDexed::TTGParameter Param = (CMiniDexed::TTGParameter) nParameter;
	const TParameter &rParam = s_TGParameter[Param];

	int nValue = pUIMenu->m_pMiniDexed->GetTGParameter (Param, nTG);

	switch (Event)
	{
	case MenuEventUpdate:
		break;

	case MenuEventStepDown:
		nValue -= rParam.Increment;
		if (nValue < rParam.Minimum)
		{
			nValue = rParam.Minimum;
		}
		pUIMenu->m_pMiniDexed->SetTGParameter (Param, nValue, nTG);
		break;

	case MenuEventStepUp:
		nValue += rParam.Increment;
		if (nValue > rParam.Maximum)
		{
			nValue = rParam.Maximum;
		}
		pUIMenu->m_pMiniDexed->SetTGParameter (Param, nValue, nTG);
		break;

	case MenuEventPressAndStepDown:
	case MenuEventPressAndStepUp:
		pUIMenu->TGShortcutHandler (Event);
		return;

	default:
		return;
	}

	string TG ("TG");
	TG += to_string (nTG+1);

	string Value = GetTGValueString (Param, pUIMenu->m_pMiniDexed->GetTGParameter (Param, nTG));

	pUIMenu->m_pUI->DisplayWrite (TG.c_str (),
				      pUIMenu->m_pParentMenu[pUIMenu->m_nCurrentMenuItem].Name,
				      Value.c_str (),
				      nValue > rParam.Minimum, nValue < rParam.Maximum);
				   
}

void CUIMenu::EditMasterVolume(CUIMenu *pUIMenu, TMenuEvent Event)
{
    TParameter rParam = {0, 127, 8, ToVolume};
    int nValue = pUIMenu->m_pMiniDexed->GetMasterVolume127();
    switch (Event)
    {
    case MenuEventUpdate:
    case MenuEventUpdateParameter:
        break;
    case MenuEventStepDown:
        nValue -= rParam.Increment;
        if (nValue < rParam.Minimum) nValue = rParam.Minimum;
        pUIMenu->m_pMiniDexed->setMasterVolume(nValue / 127.0f);
        break;
    case MenuEventStepUp:
        nValue += rParam.Increment;
        if (nValue > rParam.Maximum) nValue = rParam.Maximum;
        pUIMenu->m_pMiniDexed->setMasterVolume(nValue / 127.0f);
        break;
    default:
        return;
    }
    unsigned lcdCols = pUIMenu->m_pConfig->GetLCDColumns();
    unsigned barLen = (lcdCols > 2) ? lcdCols - 2 : 0;
    std::string valueStr(barLen, '.');
    if (barLen > 0) {
        size_t filled = (nValue * barLen + 63) / 127;
        for (unsigned i = 0; i < barLen; ++i) {
            if (i < filled) valueStr[i] = (char)0xFF;
        }
    }
    // Do NOT add < or > here; let DisplayWrite handle it
    pUIMenu->m_pUI->DisplayWrite("Master Volume", "", valueStr.c_str(), true, true);
}
