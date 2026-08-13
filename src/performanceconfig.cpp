//
// performanceconfig.cpp
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
#include <circle/logger.h>
#include "performanceconfig.h"
#include "mididevice.h"
#include <cstring> 
#include <algorithm>

LOGMODULE ("Performance");

//#define VERBOSE_DEBUG

#define PERFORMANCE_DIR "performance" 
#define DEFAULT_PERFORMANCE_FILENAME "performance.ini"
#define DEFAULT_PERFORMANCE_NAME "Default"

CPerformanceConfig::CPerformanceConfig (FATFS *pFileSystem)
:	m_Properties (DEFAULT_PERFORMANCE_FILENAME, pFileSystem)
{
	m_pFileSystem = pFileSystem; 
}

CPerformanceConfig::~CPerformanceConfig (void)
{
}

bool CPerformanceConfig::Init (unsigned nToneGenerators)
{
	// Different versions of Pi allow different TG configurations.
	// On loading, performances will load up to the number of
	// supported/active TGs.
	//
	// On saving, the active/supported number of TGs is used.
	//
	// This means that if an 8TG performance is loaded into
	// a 16 TG system and then saved, the saved performance
	// will include all 16 TG configurations.
	//
	m_nToneGenerators = nToneGenerators;

	// Check intermal performance directory exists
	DIR Directory;
	FRESULT Result;
	//Check if internal "performance" directory exists
	Result = f_opendir (&Directory, "SD:/" PERFORMANCE_DIR);
	if (Result == FR_OK)
	{
		m_bPerformanceDirectoryExists=true;		
		Result = f_closedir (&Directory);
	}
	else
	{
		m_bPerformanceDirectoryExists = false;
	}
	
	// List banks if present
	ListPerformanceBanks();

#ifdef VERBOSE_DEBUG
#warning "PerformanceConfig in verbose debug printing mode"
	LOGNOTE("Testing loading of banks");
	for (unsigned i=0; i<NUM_PERFORMANCE_BANKS; i++)
	{
		if (!m_PerformanceBankName[i].empty())
		{
			SetNewPerformanceBank(i);
			SetNewPerformance(0);
		}
	}
#endif
	// Set to default initial bank
	SetNewPerformanceBank(0);
	SetNewPerformance(0);

	LOGNOTE ("Loaded Default Performance Bank - Last Performance: %d", m_nLastPerformance + 1); // Show "user facing" index

	return true;
}

bool CPerformanceConfig::Load (void)
{
	if (!m_Properties.Load ())
	{
		return false;
	}

	bool bResult = false;
	unsigned nPerformanceFilterType = m_Properties.GetNumber ("FilterType", m_Properties.GetNumber ("FilterType1", 1));

	for (unsigned nTG = 0; nTG < CConfig::AllToneGenerators; nTG++)
	{
		CString PropertyName;

		PropertyName.Format ("BankNumber%u", nTG+1);
		m_nBankNumber[nTG] = m_Properties.GetNumber (PropertyName, 0);

		PropertyName.Format ("VoiceNumber%u", nTG+1);
		m_nVoiceNumber[nTG] = m_Properties.GetNumber (PropertyName, 1);
		if (m_nVoiceNumber[nTG] > 0)
		{
			m_nVoiceNumber[nTG]--;
		}

		PropertyName.Format ("MIDIChannel%u", nTG+1);
		unsigned nMIDIChannel = m_Properties.GetNumber (PropertyName, 0);
		if (nMIDIChannel == 0)
		{
			m_nMIDIChannel[nTG] = CMIDIDevice::Disabled;
		}
		else if (nMIDIChannel <= CMIDIDevice::Channels)
		{
			m_nMIDIChannel[nTG] = nMIDIChannel-1;
			bResult = true;
		}
		else
		{
			m_nMIDIChannel[nTG] = CMIDIDevice::OmniMode;
			bResult = true;
		}

		PropertyName.Format ("Volume%u", nTG+1);
		m_nVolume[nTG] = m_Properties.GetNumber (PropertyName, 100);

		PropertyName.Format ("Pan%u", nTG+1);
		m_nPan[nTG] = m_Properties.GetNumber (PropertyName, 64);

		PropertyName.Format ("Detune%u", nTG+1);
		m_nDetune[nTG] = m_Properties.GetSignedNumber (PropertyName, 0);

		PropertyName.Format ("Cutoff%u", nTG+1);
		m_nCutoff[nTG] = m_Properties.GetNumber (PropertyName, 99);

		PropertyName.Format ("Resonance%u", nTG+1);
		m_nResonance[nTG] = m_Properties.GetNumber (PropertyName, 0);

		// Single Performance-wide Filter Type.  Old FilterType1 is accepted as
		// a migration fallback when FilterType is not present.
		m_nFilterType[nTG] = nPerformanceFilterType;

		PropertyName.Format ("NoteLimitLow%u", nTG+1);
		m_nNoteLimitLow[nTG] = m_Properties.GetNumber (PropertyName, 0);

		PropertyName.Format ("NoteLimitHigh%u", nTG+1);
		m_nNoteLimitHigh[nTG] = m_Properties.GetNumber (PropertyName, 127);

		PropertyName.Format ("NoteShift%u", nTG+1);
		m_nNoteShift[nTG] = m_Properties.GetSignedNumber (PropertyName, 0);

		PropertyName.Format ("ReverbSend%u", nTG+1);
		m_nReverbSend[nTG] = m_Properties.GetNumber (PropertyName, 50);

		PropertyName.Format ("DelaySend%u", nTG+1);
		m_nDelaySend[nTG] = m_Properties.GetNumber (PropertyName, 50);
		
		PropertyName.Format ("PitchBendRange%u", nTG+1);
		m_nPitchBendRange[nTG] = m_Properties.GetNumber (PropertyName, 2);

		PropertyName.Format ("PitchBendStep%u", nTG+1);
		m_nPitchBendStep[nTG] = m_Properties.GetNumber (PropertyName, 0);

		PropertyName.Format ("PortamentoMode%u", nTG+1);
		m_nPortamentoMode[nTG] = m_Properties.GetNumber (PropertyName, 0);

		PropertyName.Format ("PortamentoGlissando%u", nTG+1);
		m_nPortamentoGlissando[nTG] = m_Properties.GetNumber (PropertyName, 0);

		PropertyName.Format ("PortamentoTime%u", nTG+1);
		m_nPortamentoTime[nTG] = m_Properties.GetNumber (PropertyName, 0);
		
		PropertyName.Format ("VoiceData%u", nTG+1); 
		m_nVoiceDataTxt[nTG] = m_Properties.GetString (PropertyName, "");
		
		PropertyName.Format ("MonoMode%u", nTG+1);
		m_bMonoMode[nTG] = m_Properties.GetNumber (PropertyName, 0) != 0;
				
		PropertyName.Format ("ModulationWheelRange%u", nTG+1);
		m_nModulationWheelRange[nTG] = m_Properties.GetNumber (PropertyName, 99); 
		
		PropertyName.Format ("ModulationWheelTarget%u", nTG+1);
		m_nModulationWheelTarget[nTG] = m_Properties.GetNumber (PropertyName, 1);
		
		PropertyName.Format ("FootControlRange%u", nTG+1);
		m_nFootControlRange[nTG] = m_Properties.GetNumber (PropertyName, 99); 
		
		PropertyName.Format ("FootControlTarget%u", nTG+1);
		m_nFootControlTarget[nTG] = m_Properties.GetNumber (PropertyName, 0);
		
		PropertyName.Format ("BreathControlRange%u", nTG+1);
		m_nBreathControlRange[nTG] = m_Properties.GetNumber (PropertyName, 99); 
		
		PropertyName.Format ("BreathControlTarget%u", nTG+1);
		m_nBreathControlTarget[nTG] = m_Properties.GetNumber (PropertyName, 0);
		
		PropertyName.Format ("AftertouchRange%u", nTG+1);
		m_nAftertouchRange[nTG] = m_Properties.GetNumber (PropertyName, 99); 
		
		PropertyName.Format ("AftertouchTarget%u", nTG+1);
		m_nAftertouchTarget[nTG] = m_Properties.GetNumber (PropertyName, 0);
		
		}

	m_bCompressorEnable = m_Properties.GetNumber ("CompressorEnable", 1) != 0;

	m_bReverbEnable = m_Properties.GetNumber ("ReverbEnable", 1) != 0;
	m_nReverbSize = m_Properties.GetNumber ("ReverbSize", 70);
	m_nReverbHighDamp = m_Properties.GetNumber ("ReverbHighDamp", 50);
	m_nReverbLowDamp = m_Properties.GetNumber ("ReverbLowDamp", 50);
	m_nReverbLowPass = m_Properties.GetNumber ("ReverbLowPass", 30);
	m_nReverbDiffusion = m_Properties.GetNumber ("ReverbDiffusion", 65);
	m_nReverbLevel = m_Properties.GetNumber ("ReverbLevel", 99);

	// New delay bus. Old performances remain dry-compatible because DelayEnable
	// and every DelaySend default to 50 so enabling Delay is immediately audible.
	m_bDelayEnable = m_Properties.GetNumber ("DelayEnable", 0) != 0;
	m_nDelayMode = m_Properties.GetNumber ("DelayMode", 2);           // PingPong
	m_nDelayTempo = m_Properties.GetNumber ("DelayTempo", 120);
	m_nDelayTimeL = m_Properties.GetNumber ("DelayTimeL", 5);        // 1/4
	m_nDelayTimeR = m_Properties.GetNumber ("DelayTimeR", 8);        // 1/8T
	m_nDelayFeedback = m_Properties.GetNumber ("DelayFeedback", 55);
	m_nDelayHighCut = m_Properties.GetNumber ("DelayHighCut", 6300);
	m_nDelayLevel = m_Properties.GetNumber ("DelayLevel", 70);
	m_nDryLevel = m_Properties.GetNumber ("DryLevel", 99);
	m_nReverbSendTrim = m_Properties.GetNumber ("ReverbSendTrim", 99);
	m_nDelaySendTrim = m_Properties.GetNumber ("DelaySendTrim", 99);

	return bResult;
}

bool CPerformanceConfig::Save (void)
{
	m_Properties.RemoveAll ();

	// New canonical storage: one FilterType for the whole Performance.
	m_Properties.SetNumber ("FilterType", m_nFilterType[0]);

	for (unsigned nTG = 0; nTG < m_nToneGenerators; nTG++)
	{
		CString PropertyName;

		PropertyName.Format ("BankNumber%u", nTG+1);
		m_Properties.SetNumber (PropertyName, m_nBankNumber[nTG]);

		PropertyName.Format ("VoiceNumber%u", nTG+1);
		m_Properties.SetNumber (PropertyName, m_nVoiceNumber[nTG]+1);

		PropertyName.Format ("MIDIChannel%u", nTG+1);
		unsigned nMIDIChannel = m_nMIDIChannel[nTG];
		if (nMIDIChannel < CMIDIDevice::Channels)
		{
			nMIDIChannel++;
		}
		else if (nMIDIChannel == CMIDIDevice::OmniMode)
		{
			nMIDIChannel = 255;
		}
		else
		{
			nMIDIChannel = 0;
		}
		m_Properties.SetNumber (PropertyName, nMIDIChannel);

		PropertyName.Format ("Volume%u", nTG+1);
		m_Properties.SetNumber (PropertyName, m_nVolume[nTG]);

		PropertyName.Format ("Pan%u", nTG+1);
		m_Properties.SetNumber (PropertyName, m_nPan[nTG]);

		PropertyName.Format ("Detune%u", nTG+1);
		m_Properties.SetSignedNumber (PropertyName, m_nDetune[nTG]);

		PropertyName.Format ("Cutoff%u", nTG+1);
		m_Properties.SetNumber (PropertyName, m_nCutoff[nTG]);

		PropertyName.Format ("Resonance%u", nTG+1);
		m_Properties.SetNumber (PropertyName, m_nResonance[nTG]);

		// Compatibility line for older builds/tools.  It is always the same
		// value as the Performance-wide FilterType.
		PropertyName.Format ("FilterType%u", nTG+1);
		m_Properties.SetNumber (PropertyName, m_nFilterType[0]);

		PropertyName.Format ("NoteLimitLow%u", nTG+1);
		m_Properties.SetNumber (PropertyName, m_nNoteLimitLow[nTG]);

		PropertyName.Format ("NoteLimitHigh%u", nTG+1);
		m_Properties.SetNumber (PropertyName, m_nNoteLimitHigh[nTG]);

		PropertyName.Format ("NoteShift%u", nTG+1);
		m_Properties.SetSignedNumber (PropertyName, m_nNoteShift[nTG]);

		PropertyName.Format ("ReverbSend%u", nTG+1);
		m_Properties.SetNumber (PropertyName, m_nReverbSend[nTG]);

		PropertyName.Format ("DelaySend%u", nTG+1);
		m_Properties.SetNumber (PropertyName, m_nDelaySend[nTG]);
		
		PropertyName.Format ("PitchBendRange%u", nTG+1);
		m_Properties.SetNumber (PropertyName, m_nPitchBendRange[nTG]);

		PropertyName.Format ("PitchBendStep%u", nTG+1);
		m_Properties.SetNumber (PropertyName, m_nPitchBendStep[nTG]);

		PropertyName.Format ("PortamentoMode%u", nTG+1);
		m_Properties.SetNumber (PropertyName, m_nPortamentoMode[nTG]);

		PropertyName.Format ("PortamentoGlissando%u", nTG+1);
		m_Properties.SetNumber (PropertyName, m_nPortamentoGlissando[nTG]);

		PropertyName.Format ("PortamentoTime%u", nTG+1);
		m_Properties.SetNumber (PropertyName, m_nPortamentoTime[nTG]);
		
		PropertyName.Format ("VoiceData%u", nTG+1);
		char *cstr = &m_nVoiceDataTxt[nTG][0];
		m_Properties.SetString (PropertyName, cstr);
		
		PropertyName.Format ("MonoMode%u", nTG+1);
		m_Properties.SetNumber (PropertyName, m_bMonoMode[nTG] ? 1 : 0);
				
		PropertyName.Format ("ModulationWheelRange%u", nTG+1);
		m_Properties.SetNumber (PropertyName, m_nModulationWheelRange[nTG]);
	
		PropertyName.Format ("ModulationWheelTarget%u", nTG+1);
		m_Properties.SetNumber (PropertyName, m_nModulationWheelTarget[nTG]);	
			
		PropertyName.Format ("FootControlRange%u", nTG+1);
		m_Properties.SetNumber (PropertyName, m_nFootControlRange[nTG]);	
		
		PropertyName.Format ("FootControlTarget%u", nTG+1);
		m_Properties.SetNumber (PropertyName, m_nFootControlTarget[nTG]);	
		
		PropertyName.Format ("BreathControlRange%u", nTG+1);
		m_Properties.SetNumber (PropertyName, m_nBreathControlRange[nTG]);	
		
		PropertyName.Format ("BreathControlTarget%u", nTG+1);
		m_Properties.SetNumber (PropertyName, m_nBreathControlTarget[nTG]);	
		
		PropertyName.Format ("AftertouchRange%u", nTG+1);
		m_Properties.SetNumber (PropertyName, m_nAftertouchRange[nTG]);	
		
		PropertyName.Format ("AftertouchTarget%u", nTG+1);
		m_Properties.SetNumber (PropertyName, m_nAftertouchTarget[nTG]);			

		}

	m_Properties.SetNumber ("CompressorEnable", m_bCompressorEnable ? 1 : 0);

	m_Properties.SetNumber ("ReverbEnable", m_bReverbEnable ? 1 : 0);
	m_Properties.SetNumber ("ReverbSize", m_nReverbSize);
	m_Properties.SetNumber ("ReverbHighDamp", m_nReverbHighDamp);
	m_Properties.SetNumber ("ReverbLowDamp", m_nReverbLowDamp);
	m_Properties.SetNumber ("ReverbLowPass", m_nReverbLowPass);
	m_Properties.SetNumber ("ReverbDiffusion", m_nReverbDiffusion);
	m_Properties.SetNumber ("ReverbLevel", m_nReverbLevel);

	m_Properties.SetNumber ("DelayEnable", m_bDelayEnable ? 1 : 0);
	m_Properties.SetNumber ("DelayMode", m_nDelayMode);
	m_Properties.SetNumber ("DelayTempo", m_nDelayTempo);
	m_Properties.SetNumber ("DelayTimeL", m_nDelayTimeL);
	m_Properties.SetNumber ("DelayTimeR", m_nDelayTimeR);
	m_Properties.SetNumber ("DelayFeedback", m_nDelayFeedback);
	m_Properties.SetNumber ("DelayHighCut", m_nDelayHighCut);
	m_Properties.SetNumber ("DelayLevel", m_nDelayLevel);
	m_Properties.SetNumber ("DryLevel", m_nDryLevel);
	m_Properties.SetNumber ("ReverbSendTrim", m_nReverbSendTrim);
	m_Properties.SetNumber ("DelaySendTrim", m_nDelaySendTrim);

	return m_Properties.Save ();
}

unsigned CPerformanceConfig::GetBankNumber (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nBankNumber[nTG];
}

unsigned CPerformanceConfig::GetVoiceNumber (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nVoiceNumber[nTG];
}

unsigned CPerformanceConfig::GetMIDIChannel (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nMIDIChannel[nTG];
}

unsigned CPerformanceConfig::GetVolume (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nVolume[nTG];
}

unsigned CPerformanceConfig::GetPan (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nPan[nTG];
}

int CPerformanceConfig::GetDetune (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nDetune[nTG];
}

unsigned CPerformanceConfig::GetCutoff (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nCutoff[nTG];
}

unsigned CPerformanceConfig::GetResonance (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nResonance[nTG];
}

unsigned CPerformanceConfig::GetFilterType (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nFilterType[nTG];
}

unsigned CPerformanceConfig::GetNoteLimitLow (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nNoteLimitLow[nTG];
}

unsigned CPerformanceConfig::GetNoteLimitHigh (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nNoteLimitHigh[nTG];
}

int CPerformanceConfig::GetNoteShift (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nNoteShift[nTG];
}

unsigned CPerformanceConfig::GetReverbSend (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nReverbSend[nTG];
}

unsigned CPerformanceConfig::GetDelaySend (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nDelaySend[nTG];
}

void CPerformanceConfig::SetBankNumber (unsigned nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nBankNumber[nTG] = nValue;
}

void CPerformanceConfig::SetVoiceNumber (unsigned nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nVoiceNumber[nTG] = nValue;
}

void CPerformanceConfig::SetMIDIChannel (unsigned nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nMIDIChannel[nTG] = nValue;
}

void CPerformanceConfig::SetVolume (unsigned nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nVolume[nTG] = nValue;
}

void CPerformanceConfig::SetPan (unsigned nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nPan[nTG] = nValue;
}

void CPerformanceConfig::SetDetune (int nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nDetune[nTG] = nValue;
}

void CPerformanceConfig::SetCutoff (unsigned nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nCutoff[nTG] = nValue;
}

void CPerformanceConfig::SetResonance (unsigned nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nResonance[nTG] = nValue;
}

void CPerformanceConfig::SetFilterType (unsigned nValue, unsigned nTG)
{
	(void) nTG;
	for (unsigned i = 0; i < CConfig::AllToneGenerators; i++)
	{
		m_nFilterType[i] = nValue;
	}
}

void CPerformanceConfig::SetNoteLimitLow (unsigned nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nNoteLimitLow[nTG] = nValue;
}

void CPerformanceConfig::SetNoteLimitHigh (unsigned nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nNoteLimitHigh[nTG] = nValue;
}

void CPerformanceConfig::SetNoteShift (int nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nNoteShift[nTG] = nValue;
}

void CPerformanceConfig::SetReverbSend (unsigned nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nReverbSend[nTG] = nValue;
}

void CPerformanceConfig::SetDelaySend (unsigned nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nDelaySend[nTG] = nValue;
}

bool CPerformanceConfig::GetCompressorEnable (void) const
{
	return m_bCompressorEnable;
}

bool CPerformanceConfig::GetReverbEnable (void) const
{
	return m_bReverbEnable;
}

unsigned CPerformanceConfig::GetReverbSize (void) const
{
	return m_nReverbSize;
}

unsigned CPerformanceConfig::GetReverbHighDamp (void) const
{
	return m_nReverbHighDamp;
}

unsigned CPerformanceConfig::GetReverbLowDamp (void) const
{
	return m_nReverbLowDamp;
}

unsigned CPerformanceConfig::GetReverbLowPass (void) const
{
	return m_nReverbLowPass;
}

unsigned CPerformanceConfig::GetReverbDiffusion (void) const
{
	return m_nReverbDiffusion;
}

unsigned CPerformanceConfig::GetReverbLevel (void) const
{
	return m_nReverbLevel;
}

bool CPerformanceConfig::GetDelayEnable (void) const { return m_bDelayEnable; }
unsigned CPerformanceConfig::GetDelayMode (void) const { return m_nDelayMode; }
unsigned CPerformanceConfig::GetDelayTempo (void) const { return m_nDelayTempo; }
unsigned CPerformanceConfig::GetDelayTimeL (void) const { return m_nDelayTimeL; }
unsigned CPerformanceConfig::GetDelayTimeR (void) const { return m_nDelayTimeR; }
unsigned CPerformanceConfig::GetDelayFeedback (void) const { return m_nDelayFeedback; }
unsigned CPerformanceConfig::GetDelayHighCut (void) const { return m_nDelayHighCut; }
unsigned CPerformanceConfig::GetDelayLevel (void) const { return m_nDelayLevel; }
unsigned CPerformanceConfig::GetDryLevel (void) const { return m_nDryLevel; }
unsigned CPerformanceConfig::GetReverbSendTrim (void) const { return m_nReverbSendTrim; }
unsigned CPerformanceConfig::GetDelaySendTrim (void) const { return m_nDelaySendTrim; }

void CPerformanceConfig::SetCompressorEnable (bool bValue)
{
	m_bCompressorEnable = bValue;
}

void CPerformanceConfig::SetReverbEnable (bool bValue)
{
	m_bReverbEnable = bValue;
}

void CPerformanceConfig::SetReverbSize (unsigned nValue)
{
	m_nReverbSize = nValue;
}

void CPerformanceConfig::SetReverbHighDamp (unsigned nValue)
{
	m_nReverbHighDamp = nValue;
}

void CPerformanceConfig::SetReverbLowDamp (unsigned nValue)
{
	m_nReverbLowDamp = nValue;
}

void CPerformanceConfig::SetReverbLowPass (unsigned nValue)
{
	m_nReverbLowPass = nValue;
}

void CPerformanceConfig::SetReverbDiffusion (unsigned nValue)
{
	m_nReverbDiffusion = nValue;
}

void CPerformanceConfig::SetReverbLevel (unsigned nValue)
{
	m_nReverbLevel = nValue;
}

void CPerformanceConfig::SetDelayEnable (bool bValue) { m_bDelayEnable = bValue; }
void CPerformanceConfig::SetDelayMode (unsigned nValue) { m_nDelayMode = nValue; }
void CPerformanceConfig::SetDelayTempo (unsigned nValue) { m_nDelayTempo = nValue; }
void CPerformanceConfig::SetDelayTimeL (unsigned nValue) { m_nDelayTimeL = nValue; }
void CPerformanceConfig::SetDelayTimeR (unsigned nValue) { m_nDelayTimeR = nValue; }
void CPerformanceConfig::SetDelayFeedback (unsigned nValue) { m_nDelayFeedback = nValue; }
void CPerformanceConfig::SetDelayHighCut (unsigned nValue) { m_nDelayHighCut = nValue; }
void CPerformanceConfig::SetDelayLevel (unsigned nValue) { m_nDelayLevel = nValue; }
void CPerformanceConfig::SetDryLevel (unsigned nValue) { m_nDryLevel = nValue; }
void CPerformanceConfig::SetReverbSendTrim (unsigned nValue) { m_nReverbSendTrim = nValue; }
void CPerformanceConfig::SetDelaySendTrim (unsigned nValue) { m_nDelaySendTrim = nValue; }

// Pitch bender and portamento:
void CPerformanceConfig::SetPitchBendRange (unsigned nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nPitchBendRange[nTG] = nValue;
}

unsigned CPerformanceConfig::GetPitchBendRange (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nPitchBendRange[nTG];
}


void CPerformanceConfig::SetPitchBendStep (unsigned nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nPitchBendStep[nTG] = nValue;
}

unsigned CPerformanceConfig::GetPitchBendStep (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nPitchBendStep[nTG];
}


void CPerformanceConfig::SetPortamentoMode (unsigned nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nPortamentoMode[nTG] = nValue;
}

unsigned CPerformanceConfig::GetPortamentoMode (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nPortamentoMode[nTG];
}


void CPerformanceConfig::SetPortamentoGlissando (unsigned nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nPortamentoGlissando[nTG] = nValue;
}

unsigned CPerformanceConfig::GetPortamentoGlissando (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nPortamentoGlissando[nTG];
}


void CPerformanceConfig::SetPortamentoTime (unsigned nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nPortamentoTime[nTG] = nValue;
}

unsigned CPerformanceConfig::GetPortamentoTime (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nPortamentoTime[nTG];
}

void CPerformanceConfig::SetMonoMode (bool bValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_bMonoMode[nTG] = bValue;
}

bool CPerformanceConfig::GetMonoMode (unsigned nTG) const
{
	return m_bMonoMode[nTG];
}

void CPerformanceConfig::SetModulationWheelRange (unsigned nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nModulationWheelRange[nTG] = nValue;
}

unsigned CPerformanceConfig::GetModulationWheelRange (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nModulationWheelRange[nTG];
}

void CPerformanceConfig::SetModulationWheelTarget (unsigned nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nModulationWheelTarget[nTG] = nValue;
}

unsigned CPerformanceConfig::GetModulationWheelTarget (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nModulationWheelTarget[nTG];
}

void CPerformanceConfig::SetFootControlRange (unsigned nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nFootControlRange[nTG] = nValue;
}

unsigned CPerformanceConfig::GetFootControlRange (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nFootControlRange[nTG];
}

void CPerformanceConfig::SetFootControlTarget (unsigned nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nFootControlTarget[nTG] = nValue;
}

unsigned CPerformanceConfig::GetFootControlTarget (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nFootControlTarget[nTG];
}

void CPerformanceConfig::SetBreathControlRange (unsigned nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nBreathControlRange[nTG] = nValue;
}

unsigned CPerformanceConfig::GetBreathControlRange (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nBreathControlRange[nTG];
}

void CPerformanceConfig::SetBreathControlTarget (unsigned nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nBreathControlTarget[nTG] = nValue;
}

unsigned CPerformanceConfig::GetBreathControlTarget (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nBreathControlTarget[nTG];
}

void CPerformanceConfig::SetAftertouchRange (unsigned nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nAftertouchRange[nTG] = nValue;
}

unsigned CPerformanceConfig::GetAftertouchRange (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nAftertouchRange[nTG];
}

void CPerformanceConfig::SetAftertouchTarget (unsigned nValue, unsigned nTG)
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nAftertouchTarget[nTG] = nValue;
}

unsigned CPerformanceConfig::GetAftertouchTarget (unsigned nTG) const
{
	assert (nTG < CConfig::AllToneGenerators);
	return m_nAftertouchTarget[nTG];
}

void CPerformanceConfig::SetVoiceDataToTxt (const uint8_t *pData, unsigned nTG)  
{
	assert (nTG < CConfig::AllToneGenerators);
	m_nVoiceDataTxt[nTG] = "";
	char nDtoH[]="0123456789ABCDEF";
	for (int i = 0; i < NUM_VOICE_PARAM; i++)
	{
	m_nVoiceDataTxt[nTG]  += nDtoH[(pData[i] & 0xF0)/16];
	m_nVoiceDataTxt[nTG]  += nDtoH[pData[i] & 0x0F]  ;
	if ( i < (NUM_VOICE_PARAM-1)  ) 
    	{
     	 m_nVoiceDataTxt[nTG] += " ";
    	}
	}
}

uint8_t *CPerformanceConfig::GetVoiceDataFromTxt (unsigned nTG) 
{
	assert (nTG < CConfig::AllToneGenerators);
	static uint8_t pData[NUM_VOICE_PARAM];
	std::string nHtoD="0123456789ABCDEF";
	 
 	for (int i=0; i<NUM_VOICE_PARAM * 3; i=i+3)
	{
  	pData[i/3] = ((nHtoD.find(toupper(m_nVoiceDataTxt[nTG][i]),0) * 16 + nHtoD.find(toupper(m_nVoiceDataTxt[nTG][i+1]),0))) & 0xFF ;
	} 
  
	return pData;
}

bool CPerformanceConfig::VoiceDataFilled(unsigned nTG) 
{
	return (strcmp(m_nVoiceDataTxt[nTG].c_str(),"") != 0) ;
}

std::string CPerformanceConfig::GetPerformanceFileName(unsigned nID)
{
	assert (nID < NUM_PERFORMANCES);
	std::string FileN = "";
	if ((m_nPerformanceBank==0) && (nID == 0)) // in order to assure retrocompatibility
	{
		FileN += DEFAULT_PERFORMANCE_FILENAME;
	}
	else
	{
		// Build up from the index, "_", performance name, and ".ini"
		// NB: Index on disk = index in memory + 1
		std::string nIndex = "000000";
		nIndex += std::to_string(nID+1);
		nIndex = nIndex.substr(nIndex.length()-6,6);
		FileN += nIndex;
		FileN += "_";
		FileN += m_PerformanceFileName[nID];
		FileN += ".ini";
	}
	return FileN;
}

std::string CPerformanceConfig::GetPerformanceFullFilePath(unsigned nID)
{
	assert (nID < NUM_PERFORMANCES);
	std::string FileN = "SD:/";
	if ((m_nPerformanceBank == 0) && (nID == 0))
	{
		// Special case for the legacy Bank 1/Default performance
		FileN += DEFAULT_PERFORMANCE_FILENAME;
	}
	else
	{
		if (m_bPerformanceDirectoryExists)
		{
			FileN += PERFORMANCE_DIR;
			FileN += AddPerformanceBankDirName(m_nPerformanceBank);
			FileN += "/";
			FileN += GetPerformanceFileName(nID);
		}
	}
	return FileN;
}

std::string CPerformanceConfig::GetPerformanceName(unsigned nID)
{
	assert (nID < NUM_PERFORMANCES);
	if ((m_nPerformanceBank==0) && (nID == 0)) // in order to assure retrocompatibility
	{
		return DEFAULT_PERFORMANCE_NAME;
	}
	else
	{
		return m_PerformanceFileName[nID];
	}
}

unsigned CPerformanceConfig::GetLastPerformance()
{
	return m_nLastPerformance;
}

unsigned CPerformanceConfig::GetLastPerformanceBank()
{
	return m_nLastPerformanceBank;
}

unsigned CPerformanceConfig::GetActualPerformanceID()
{
	return m_nActualPerformance;
}

void CPerformanceConfig::SetActualPerformanceID(unsigned nID)
{
	assert (nID < NUM_PERFORMANCES);
	m_nActualPerformance = nID;
}

unsigned CPerformanceConfig::GetActualPerformanceBankID()
{
	return m_nActualPerformanceBank;
}

void CPerformanceConfig::SetActualPerformanceBankID(unsigned nBankID)
{
	assert (nBankID < NUM_PERFORMANCE_BANKS);
	m_nActualPerformanceBank = nBankID;
}

bool CPerformanceConfig::GetInternalFolderOk()
{
	return m_bPerformanceDirectoryExists;
}

bool CPerformanceConfig::IsValidPerformance(unsigned nID)
{
	if (nID < NUM_PERFORMANCES)
	{
		if (!m_PerformanceFileName[nID].empty())
		{
			return true;
		}
	}
	
	return false;
}


bool CPerformanceConfig::CheckFreePerformanceSlot(void)
{
	if (m_nLastPerformance < NUM_PERFORMANCES-1)
	{
		// There is a free slot...
		return true;
	}
	else
	{
		return false;
	}
}

bool CPerformanceConfig::CreateNewPerformanceFile(void)
{
	if (!m_bPerformanceDirectoryExists)
	{
		// Nothing can be done if there is no performance directory
		LOGNOTE("Performance directory does not exist");
		return false;
	}
	if (m_nLastPerformance >= NUM_PERFORMANCES) {
		// No space left for new performances
		LOGWARN ("No space left for new performance");
		return false;
	}
	
	// Note: New performances are created at the end of the currently selected bank.
	//       Sould we default to a new bank just for user-performances?
	//
	//       There is the possibility of MIDI changing the Bank Number and the user
	//       not spotting the bank has changed...
	//
	//       Another option would be to find empty slots in the current bank
	//       rather than always add at the end.
	//
	//       Sorting this out is left for a future update.

	std::string sPerformanceName = NewPerformanceName;
	NewPerformanceName=""; 
	unsigned nNewPerformance = m_nLastPerformance + 1;
	std::string nFileName;
	std::string nPath;
	std::string nIndex = "000000";
	nIndex += std::to_string(nNewPerformance+1);  // Index on disk = index in memory+1
	nIndex = nIndex.substr(nIndex.length()-6,6);
	

	nFileName = nIndex;
	nFileName += "_";
	if (strcmp(sPerformanceName.c_str(),"") == 0)
	{
		nFileName += "Perf";
		nFileName += nIndex;
	}		
	else
	{
		nFileName +=sPerformanceName.substr(0,14);
	}
	nFileName += ".ini";
	m_PerformanceFileName[nNewPerformance]= sPerformanceName;
	
	nPath = "SD:/" ;
	nPath += PERFORMANCE_DIR;
	nPath += AddPerformanceBankDirName(m_nPerformanceBank);
	nPath += "/";
	nFileName = nPath + nFileName;
	
	FIL File;
	FRESULT Result = f_open (&File, nFileName.c_str(), FA_WRITE | FA_CREATE_ALWAYS);
	if (Result != FR_OK)
	{
		m_PerformanceFileName[nNewPerformance].clear();
		return false;
	}

	if (f_close (&File) != FR_OK)
	{
		m_PerformanceFileName[nNewPerformance].clear();
		return false;
	}
	
	m_nLastPerformance = nNewPerformance;
	m_nActualPerformance = nNewPerformance;
	new (&m_Properties) CPropertiesFatFsFile(nFileName.c_str(), m_pFileSystem);
	
	return true;
}

bool CPerformanceConfig::ListPerformances()
{
	// Clear any existing lists of performances
	for (unsigned i=0; i<NUM_PERFORMANCES; i++)
	{
		m_PerformanceFileName[i].clear();
	}
	m_nLastPerformance=0;
	if (m_nPerformanceBank == 0)
	{
		// The first bank is the default performance directory
	   	m_PerformanceFileName[0]=DEFAULT_PERFORMANCE_NAME; // in order to assure retrocompatibility
	}
	
	if (m_bPerformanceDirectoryExists)
	{
		DIR Directory;
		FILINFO FileInfo;
		FRESULT Result;
		std::string PerfDir = "SD:/" PERFORMANCE_DIR + AddPerformanceBankDirName(m_nPerformanceBank);
#ifdef VERBOSE_DEBUG
		LOGNOTE("Listing Performances from %s", PerfDir.c_str());
#endif
		Result = f_opendir (&Directory, PerfDir.c_str());
		if (Result != FR_OK)
		{
			return false;
		}
		unsigned nPIndex;

		Result = f_findfirst (&Directory, &FileInfo, PerfDir.c_str(), "*.ini");
		for (unsigned i = 0; Result == FR_OK && FileInfo.fname[0]; i++)
		{
			if (!(FileInfo.fattrib & (AM_HID | AM_SYS)))  
			{
				std::string OriFileName = FileInfo.fname;
				size_t nLen = OriFileName.length();
				if (   nLen > 8 && nLen <26	 && strcmp(OriFileName.substr(6,1).c_str(), "_")==0)
				{
					// Note: m_nLastPerformance - refers to the number (index) of the last performance in memory,
					//       which includes a default performance.
					//
					//       Filenames on the disk start from 1 to match what the user might see in MIDI.
					//       So this means that actually file 000001_ will correspond to index position [0].
					//       For the default bank though, ID 1 is the default performance, so will already exist.
					//          m_PerformanceFileName[0] = default performance (file 000001)
					//          m_PerformanceFileName[1] = first available on-disk performance (file 000002)
					//
					// Note2: filenames assume 6 digits, underscore, name, finally ".ini"
					//        i.e.   123456_Performance Name.ini
					//
					nPIndex=stoi(OriFileName.substr(0,6));
					if ((nPIndex < 1) || (nPIndex >= (NUM_PERFORMANCES+1)))
					{
						// Index is out of range - skip to next file
						LOGNOTE ("Performance number out of range: %s (%d to %d)", FileInfo.fname, 1, NUM_PERFORMANCES);
					}
					else
					{
						// Convert from "user facing" 1..indexed number to internal 0..indexed
						nPIndex = nPIndex-1;
						if (m_PerformanceFileName[nPIndex].empty())
						{
							if(nPIndex > m_nLastPerformance)
							{
								m_nLastPerformance=nPIndex;
							}

							std::string FileName = OriFileName.substr(0,OriFileName.length()-4).substr(7,14);

							m_PerformanceFileName[nPIndex] = FileName;
#ifdef VERBOSE_DEBUG
							LOGNOTE ("Loading performance %s (%d, %s)", OriFileName.c_str(), nPIndex, FileName.c_str());
#endif
						}
						else
						{
							LOGNOTE ("Duplicate performance %s", OriFileName.c_str());
						}
					}
				}
			}

			Result = f_findnext (&Directory, &FileInfo);
		}
		f_closedir (&Directory);
	}
	
	return true;
}

void CPerformanceConfig::SetNewPerformance (unsigned nID)
{
	assert (nID < NUM_PERFORMANCES);
	m_nActualPerformance=nID;
	std::string FileN = GetPerformanceFullFilePath(nID);

	new (&m_Properties) CPropertiesFatFsFile(FileN.c_str(), m_pFileSystem);
#ifdef VERBOSE_DEBUG
	LOGNOTE("Selecting Performance: %d (%s)", nID+1, FileN.c_str());
#endif
}

unsigned CPerformanceConfig::FindFirstPerformance (void)
{
	for (int nID=0; nID < NUM_PERFORMANCES; nID++)
	{
		if (IsValidPerformance(nID))
		{
			return nID;
		}
	}

	return 0; // Even though 0 is a valid performance, not much else to do
}

std::string CPerformanceConfig::GetNewPerformanceDefaultName(void)
{
	std::string nIndex = "000000";
	nIndex += std::to_string(m_nLastPerformance+1+1); // Convert from internal 0.. index to a file-based 1.. index to show the user
	nIndex = nIndex.substr(nIndex.length()-6,6);
	return "Perf" + nIndex;
}

void CPerformanceConfig::SetNewPerformanceName(const std::string &Name)
{
	NewPerformanceName = Name.substr(0, Name.find_last_not_of(' ') + 1);
}

bool CPerformanceConfig::DeletePerformance(unsigned nID)
{
	if (!m_bPerformanceDirectoryExists)
	{
		// Nothing can be done if there is no performance directory
		LOGNOTE("Performance directory does not exist");
		return false;
	}
	bool bOK = false;
	if((m_nPerformanceBank == 0) && (nID == 0)){return bOK;} // default (performance.ini at root directory) can't be deleted
	DIR Directory;
	FILINFO FileInfo;
	std::string FileN = "SD:/";
	FileN += PERFORMANCE_DIR;
	FileN += AddPerformanceBankDirName(m_nPerformanceBank);
	
	FRESULT Result = f_findfirst (&Directory, &FileInfo, FileN.c_str(), GetPerformanceFileName(nID).c_str());
	if (Result == FR_OK && FileInfo.fname[0])
	{
		FileN += "/";
		FileN += GetPerformanceFileName(nID);
		Result=f_unlink (FileN.c_str());
		if (Result == FR_OK)
		{
			SetNewPerformance(0);
			m_nActualPerformance =0;
			//nMenuSelectedPerformance=0;
			m_PerformanceFileName[nID].clear();
			// If this was the last performance in the bank...
			if (nID == m_nLastPerformance)
			{
				do
				{
					// Find the new last performance
					m_nLastPerformance--;
				} while (!IsValidPerformance(m_nLastPerformance) && (m_nLastPerformance > 0));
			}
			bOK=true;
		}
		else
		{
			LOGNOTE ("Failed to delete %s", FileN.c_str());
		}
	}
	return bOK;
}

bool CPerformanceConfig::ListPerformanceBanks()
{
	m_nPerformanceBank = 0;
	m_nLastPerformance = 0;
	m_nLastPerformanceBank = 0;

	// Open performance directory
	DIR Directory;
	FILINFO FileInfo;
	FRESULT Result;
	Result = f_opendir (&Directory, "SD:/" PERFORMANCE_DIR);
	if (Result != FR_OK)
	{
		// No performance directory, so no performance banks.
		// So nothing else to do here
		LOGNOTE ("No performance banks detected");
		m_bPerformanceDirectoryExists = false;
		return false;
	}

	unsigned nNumBanks = 0;
	m_nLastPerformanceBank = 0;

	// List directories with names in format 01_Perf Bank Name
	Result = f_findfirst (&Directory, &FileInfo, "SD:/" PERFORMANCE_DIR, "*");
	for (unsigned i = 0; Result == FR_OK && FileInfo.fname[0]; i++)
	{
		// Check to see if it is a directory
		if ((FileInfo.fattrib & AM_DIR) != 0)
		{
			// Looking for Performance banks of the form: 01_Perf Bank Name
			// So positions 0,1,2 = decimal digit
			//    position  3   = "_"
			//    positions 4.. = actual name
			//
			std::string OriFileName = FileInfo.fname;
			size_t nLen = OriFileName.length();
			if (   nLen > 4 && nLen <26	 && strcmp(OriFileName.substr(3,1).c_str(), "_")==0)
			{
				unsigned nBankIndex = stoi(OriFileName.substr(0,3));
				// Recall user index numbered 002..NUM_PERFORMANCE_BANKS
				// NB: Bank 001 is reserved for the default performance directory
				if ((nBankIndex > 0) && (nBankIndex <= NUM_PERFORMANCE_BANKS))
				{
					// Convert from "user facing" 1..indexed number to internal 0..indexed
					nBankIndex = nBankIndex-1;
					if (m_PerformanceBankName[nBankIndex].empty())
					{
						std::string BankName = OriFileName.substr(4,nLen);

						m_PerformanceBankName[nBankIndex] = BankName;
#ifdef VERBOSE_DEBUG
						LOGNOTE ("Found performance bank %s (%d, %s)", OriFileName.c_str(), nBankIndex, BankName.c_str());
#endif
						nNumBanks++;
						if (nBankIndex > m_nLastPerformanceBank)
						{
							m_nLastPerformanceBank = nBankIndex;
						}
					}
					else
					{
						LOGNOTE ("Duplicate Performance Bank: %s", FileInfo.fname);
						if (nBankIndex==0)
						{
							LOGNOTE ("(Bank 001 is the default performance directory)");
						}
					}
				}
				else
				{
					LOGNOTE ("Performance Bank number out of range: %s (%d to %d)", FileInfo.fname, 1, NUM_PERFORMANCE_BANKS);
				}
			}
			else
			{
#ifdef VERBOSE_DEBUG
				LOGNOTE ("Skipping: %s", FileInfo.fname);
#endif
			}
		}
		
		Result = f_findnext (&Directory, &FileInfo);
	}
	
	if (nNumBanks > 0)
	{
		LOGNOTE ("Number of Performance Banks: %d (last = %d)", nNumBanks, m_nLastPerformanceBank+1);
	}
	
	f_closedir (&Directory);
	return true;
}

void CPerformanceConfig::SetNewPerformanceBank(unsigned nBankID)
{
	assert (nBankID < NUM_PERFORMANCE_BANKS);
	if (IsValidPerformanceBank(nBankID))
	{
#ifdef VERBOSE_DEBUG
		LOGNOTE("Selecting Performance Bank: %d", nBankID+1);
#endif
		m_nPerformanceBank = nBankID;
		m_nActualPerformanceBank = nBankID;
		ListPerformances();
	}
	else
	{
#ifdef VERBOSE_DEBUG
		LOGNOTE("Not selecting invalid Performance Bank: %d", nBankID+1);
#endif
	}
}

unsigned CPerformanceConfig::GetPerformanceBank(void)
{
	return m_nPerformanceBank;
}

std::string CPerformanceConfig::GetPerformanceBankName(unsigned nBankID)
{
	assert (nBankID < NUM_PERFORMANCE_BANKS);
	if (IsValidPerformanceBank(nBankID))
	{
		return m_PerformanceBankName[nBankID];
	}
	return "";
}

std::string CPerformanceConfig::AddPerformanceBankDirName(unsigned nBankID)
{
	assert (nBankID < NUM_PERFORMANCE_BANKS);
	if (IsValidPerformanceBank(nBankID))
	{
		// Performance Banks directories in format "001_Bank Name"
		std::string Index;
		if (nBankID < 9)
		{
			Index = "00" + std::to_string(nBankID+1);
		}
		else if (nBankID < 99)
		{
			Index = "0" + std::to_string(nBankID+1);
		}
		else
		{
			Index = std::to_string(nBankID+1);
		}

		return "/" + Index + "_" + m_PerformanceBankName[nBankID];
	}
	else
	{
		return "";
	}
}

bool CPerformanceConfig::IsValidPerformanceBank(unsigned nBankID)
{
	if (nBankID >= NUM_PERFORMANCE_BANKS) {
		return false;
	}
	if (m_PerformanceBankName[nBankID].empty())
	{
		return false;
	}
	return true;
}
