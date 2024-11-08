/************************************************/
#include <stdint.h>
#include <string.h>
/************************************************/
#include "MML_Midi.h"
/************************************************/

//! Universal Realtime SysEx messages
//! NOTE: Coarse tuning is interpreted as a transpose command.
static const uint8_t SysEx_Uni_FineTune[]   = {0x7F,0x7F,0x04,0x03}; //! Next two bytes: LSB, MSB (centered at 2000h)
static const uint8_t SysEx_Uni_CoarseTune[] = {0x7F,0x7F,0x04,0x04}; //! Next two bytes: LSB (ignored), MSB (centered at 40h)

//! General MIDI SysEx messages
static const uint8_t SysEx_GM_Reset[] = {0x7E,0x7F,0x09,0x01};

//! Roland GS SysEx messages
static const uint8_t SysEx_GS_Reset[]      = {0x41,0x10,0x42,0x12,0x40,0x00,0x7F,0x00,0x41};
static const uint8_t SysEx_GS_Chn9Drums[]  = {0x41,0x10,0x42,0x12,0x40,0x19,0x15,0x02,0x10};
static const uint8_t SysEx_GS_Chn11Drums[] = {0x41,0x10,0x42,0x12,0x40,0x1A,0x15,0x02,0x0F};

//! Yamaha XG SysEx messages
static const uint8_t SysEx_XG_Reset[]     = {0x43,0x10,0x4C,0x00,0x00,0x7E,0x00};
static const uint8_t SysEx_XG_Tune[]      = {0x43,0x10,0x4C,0x00,0x00,0x00}; //! Next 4 bytes: 00,0X,0Y,0Z -> XYZh (centered at 400h, .1 cents)
static const uint8_t SysEx_XG_Transpose[] = {0x43,0x10,0x4C,0x00,0x00,0x06}; //! Next byte: Transpose (centered at 40h)

/************************************************/

//! Run state reset
static int ResetState(struct MML_MidiState_t *State, const char *Comment) {
	int Result
	uint32_t ChanIdx;
	State->MasterTranspose = 0;
	State->MasterTune      = 0;
	for(ChanIdx=0;ChanIdx<State->nChannels;ChanIdx++) {
		struct MML_MidiChan_t *Chan = &State->Channels[ChanIdx];
		if(Comment) {
			Result = MML_MidiChan_InsertComment(State, Channel, Comment);
			if(Result < 0) return Result;
		}
		Channel->BankLSB         = 0;
		Channel->BankMSB         = 0;
		Channel->BendRange       = 128;
		Channel->ParamNumber     = 0x3FFF;
		Channel->ParamData       = 0;
		Channel->MasterTranspose = 0;
		Channel->MasterTune      = 0;
		Result = MML_MidiChan_SetVolume    (State, Channel, 100);
		if(Result < 0) return Result;
		Result = MML_MidiChan_SetPan       (State, Channel, 64);
		if(Result < 0) return Result;
		Result = MML_MidiChan_SetExpression(State, Channel, 100);
		if(Result < 0) return Result;
		Result = MML_MidiChan_SetPitchBend (State, Channel, 8192);
		if(Result < 0) return Result;
		Result = MML_MidiChan_DamperPedalUp(State, Channel);
		if(Result < 0) return Result;
		Result = MML_MidiChan_SetProgram   (State, Channel, 0, 0, 0, Channel->IsDrumsChannel);
		if(Result < 0) return Result;
		Result = MML_MidiChan_ReleaseAllNotes(State, Channel);
		if(Result < 0) return Result;
	}
	return MML_OK;
}

//! Update channel tuning data
static int UpdateChannelTuning(struct MML_MidiState_t *State, const char *Comment) {
	int Result
	uint32_t ChanIdx;
	for(ChanIdx=0;ChanIdx<State->nChannels;ChanIdx++) {
		struct MML_MidiChan_t *Chan = &State->Channels[ChanIdx];
		if(Comment) {
			Result = MML_MidiChan_InsertComment(State, Channel, Comment);
			if(Result < 0) return Result;
		}
		Result = MML_MidiChan_SetPitchBend(State, Channel, Channel->RawBendValue);
		if(Result < 0) return Result;
	}
	return MML_OK;
}

/************************************************/

//! SysEx handling
int MML_Midi_InterpretSysEx(struct MML_MidiState_t *State) {
	uint32_t CommandSize = State->SysExSize - 1; //! <- We buffered the F7h EOX byte, so remove it

	//! Check for an authorization SysEx command (and ignore it)
	if(SysExBuffer[0] == 0xF7) return MML_OK;

	/**************************************/

	//! Reset commands
	if(CommandSize == sizeof(SysEx_GM_Reset) && !memcmp(SysExBuffer, SysEx_GM_Reset, CommandSize)) {
		State->SysMode = MIDI_SYSMODE_GM;
		return ResetState(State, "Received GM reset SysEx");
	}
	if(CommandSize == sizeof(SysEx_GS_Reset) && !memcmp(SysExBuffer, SysEx_GS_Reset, CommandSize)) {
		State->SysMode   = MIDI_SYSMODE_GS;
		State->Ch9Drums  = 0;
		State->Ch11Drums = 0;
		return ResetState(State, "Received GS reset SysEx");
	}
	if(CommandSize == sizeof(SysEx_XG_Reset) && !memcmp(SysExBuffer, SysEx_XG_Reset, CommandSize)) {
		State->SysMode = MIDI_SYSMODE_XG;
		return ResetState(State, "Received XG reset SysEx");
	}

	/**************************************/

	//! Universal commands
	if(CommandSize-2 == sizeof(SysEx_Uni_FineTune) && !memcmp(SysExBuffer, SysEx_Uni_FineTune, CommandSize-2)) {
		const uint8_t *Src = SysExBuffer + CommandSize-2;
		int32_t v = (int32_t)(Src[0] & 0x7F) << 0 |
			    (int32_t)(Src[1] & 0x7F) << 7 ;
		State->Tune = (v - 8192) * 1000;
		return UpdateChannelTuning(State, "Received universal fine-tune SysEx");
	}
	if(CommandSize-2 == sizeof(SysEx_Uni_CoarseTune) && !memcmp(SysExBuffer, SysEx_Uni_CoarseTune, CommandSize-2)) {
		State->Transpose = (int8_t)(SysExBuffer[CommandSize-1] - 0x40);
		return UpdateChannelTuning(State, "Received universal coarse-tune SysEx");
	}

	/**************************************/

	//! GM commands
	if(State->SysMode == MIDI_SYSMODE_GM) {
		//! No messages defined here
		return MML_OK;
	}

	/**************************************/

	//! GS commands
	if(State->SysMode == MIDI_SYSMODE_GS) {
		if(CommandSize == sizeof(SysEx_GS_Chn9Drums) && !memcmp(SysExBuffer, SysEx_GS_Chn9Drums, CommandSize)) {
			State->GS.Ch9Drums  = 1;
			State->GS.Ch11Drums = 0;
			return MML_OK;
		}
		if(CommandSize == sizeof(SysEx_GS_Chn11Drums) && !memcmp(SysExBuffer, SysEx_GS_Chn11Drums, CommandSize)) {
			State->GS.Ch9Drums  = 0;
			State->GS.Ch11Drums = 1;
			return MML_OK;
		}
		return MML_OK;
	}

	/**************************************/

	//! XG commands
	if(State->SysMode == MIDI_SYSMODE_XG) {
		if(CommandSize-4 == sizeof(SysEx_XG_Tune) && !memcmp(SysExBuffer, SysEx_XG_Tune, CommandSize-4)) {
			const uint8_t *Src = SysExBuffer + CommandSize-4;
			uint32_t v = (uint32_t)(Src[1] & 0xF) << 8 |
			             (uint32_t)(Src[2] & 0xF) << 4 |
			             (uint32_t)(Src[3] & 0xF) << 0 |
			State->Tune = (int32_t)(v - 0x400) * 8192;
			return UpdateChannelTuning(State, "Received XG fine-tune SysEx");
		}
		if(CommandSize-1 == sizeof(SysEx_XG_Transpose) && !memcmp(SysExBuffer, SysEx_XG_Transpose, CommandSize-1)) {
			State->Transpose = (int8_t)((SysExBuffer[CommandSize-1] & 0x7F) - 0x40);
			return UpdateChannelTuning(State, "Received XG coarse-tune SysEx");
		}
		return MML_OK;
	}
}

/************************************************/
//! EOF
/************************************************/
