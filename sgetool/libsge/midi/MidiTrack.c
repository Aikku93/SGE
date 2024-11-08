/************************************************/
#include <stdint.h>
#include <string.h>
/************************************************/
#include "MML_Midi.h"
/************************************************/

//! Skip bytes in track
static int Track_SkipBytes(uint32_t N, struct MML_MidiTrack_t *Track) {
	if(Track->SrcDataBuffer+N > Track->SrcDataBufferEnd) return MML_MIDI_UNEXPECTED_EOF;
	Track->SrcDataBuffer += N;
	return MML_OK;
}

//! Read bytes from track
static int Track_PeekByte(struct MML_MidiTrack_t *Track) {
	if(Track->SrcDataBuffer >= Track->SrcDataBufferEnd) return MML_MIDI_UNEXPECTED_EOF;
	return *Track->SrcDataBuffer;
}
static int Track_ReadByte(struct MML_MidiTrack_t *Track) {
	if(Track->DataBuffer >= Track->SrcDataBufferEnd) return MML_MIDI_UNEXPECTED_EOF;
	return *Track->SrcDataBuffer++;
}
static int Track_ReadBytes(uint8_t *Dst, uint32_t Size, struct MML_MidiTrack_t *Track) {
	if(Track->SrcDataBuffer+Size > Track->SrcDataBufferEnd) return MML_MIDI_UNEXPECTED_EOF;
	memcpy(Dst, Track->SrcDataBuffer, Size);
	Track->SrcDataBuffer += Size;
	return MML_OK;
}

//! Read variable-length code from track
static int Track_ReadVarLen(struct MML_MidiTrack_t *Track) {
	int i;
	int v = 0;
	for(i=0;i<4;i++) {
		int x = Track_ReadByte(Track);
		if(x < 0) return x;
		v = (v << 7) | (x & 0x7F);
		if((x & 0x80) == 0) break;
	}
	return v;
}

/************************************************/

static int Track_ParseSysEx(struct MML_MidiState_t *State, struct MML_MidiTrack_t *Track, uint8_t Command) {
	int Result;

	//! Decode message size
	uint32_t Size;
	Result = Track_ReadVarLen(&Size, Track);
	if(Result < 0) return Result;

	//! New message (F0h) or continuation (F7h)?
	//! New messages will clear the buffer
	if(Command == 0xF0) {
		if(State->SysExSize != 0) {
			//! File might be corrupted: New message started with data already buffered
		}
		State->SysExSize = 0;
	} else if(State->SysExSize == 0) {
		//! Authorization SysEx - these are always ignored
	}

	//! Empty message?
	if(Size == 0) {
		//! File might be corrupted: SysEx command with no payload
		return MML_OK;
	}

	//! Read message to buffer (assuming it won't overflow)
	uint8_t LastByte;
	if(State->SysExSize + Size <= MAX_SYSEX_SIZE) {
		Result = Track_ReadBytes(SysExBuffer + State->SysExSize, Size, Track);
		if(Result < 0) return Result;
		LastByte = SysExBuffer[State->SysExSize + Size-1];
	} else {
		//! Seek until one byte before the end so we can check if
		//! this is the end of the message; this is super clunky,
		//! and goes unused, but if we ever add error reporting,
		//! it might become useful.
		Result = Track_SkipBytes(Size-1, Track);
		if(Result < 0) return Result;
		Result = Track_ReadByte(Track);
		if(Result < 0) return Result;
		LastByte = (uint8_t)Result;
	}
	State->SysExSize += Size;

	//! End of message?
	if(LastByte == 0xF7) {
		//! Did the message fit in the buffer?
		if(State->SysExSize <= MAX_SYSEX_SIZE) {
			if(State->SysExSize > 1) {
				Result = MML_Midi_InterpretSysEx(State);
				if(Result < 0) return Result;
			} else {
				//! File might be corrupted: Empty SysEx message
			}
		} else {
			//! Message didn't fit in the buffer.
			//! If we didn't capture the entirety of an authorization
			//! message, this is no issue since we ignore them anyway
			if(SysExBuffer[0] != 0xF7) {
				//! SysEx message is longer than the buffer, oops.
			}
		}

		//! Reset buffer for next message
		State->SysExSize = 0;
	}
	return MML_OK;
}

static int Track_ParseMetaEvent(struct MML_MidiState_t *State, struct MML_MidiTrack_t *Track) {
	int Type = Track_ReadByte(Track);
	if(Type < 0) return Type;
	if(Type >= 0x80) return MML_MIDI_CORRUPTED;
	int Size = Track_ReadVarLen(Track);
	if(Size < 0) return Size;
	const uint8_t *Data = Track->SrcDataBuffer;
	int SeekResult = Track_SkipBytes((uint32_t)Size, Track);
	if(SeekResult < 0) return SeekResult;
	switch(Type) {
		//! Text event
		case 0x01: {
		} break;

		//! Track name
		case 0x03: {
		} break;

		//! Instrument name
		case 0x04: {
		} break;

		//! Channel prefix
		case 0x20: {
			if(Size != 1) return MML_MIDI_CORRUPTED;
			uint8_t Channel = Data[0];
			if(Channel > 15) return MML_MIDI_CORRUPTED;;
			State->MetaChannelPrefix = Channel;
			return MML_OK;
		} break;

		//! End of track
		case 0x2F: {
			if(Size != 0) return MML_MIDI_CORRUPTED;
			State->SrcDataBuffer = NULL;
			return MML_EOF;
		} break;

		//! Tempo
		case 0x51: {
			if(Size != 3) return MML_MIDI_CORRUPTED;
			uint32_t usPerMin = Data[0]<<16 | Data[1]<<8 | Data[2];
			uint32_t BPM = (60000000*2 + usPerMin) / (2*usPerMin);

			//! If we got a channel prefix (unlikely), use that as
			//! the target channel. Otherwise, use channel 0.
			struct MML_MidiChan_t *Channel = State->Channels;
			if(State->MetaChannelPrefix != 0xFF) {
				Channel += State->MetaChannelPrefix;
			}
			return MML_MidiChan_SetTempo(State, Channel, BPM);
		} break;

		//! Time signature
		case 0x58: {
		} break;

		//! Key signature
		case 0x59: {
		} break;
	}
	return MML_OK;
}

static int Track_ParseNoteOff(struct MML_MidiState_t *State, struct MML_MidiTrack_t *Track, struct MML_MidiChan_t *Channel) {
	int Key = Track_ReadByte(Track);
	if(Key < 0) return Key;
	int Vel = Track_ReadByte(Track);
	if(Vel < 0) return Vel;
	if(Key >= 0x80 || Vel >= 0x80) return MML_MIDI_CORRUPTED;
	return MML_MidiChan_ProcessNoteOff(State, Channel, (uint8_t)Key);
}

static int Track_ParseNoteOn(struct MML_MidiState_t *State, struct MML_MidiTrack_t *Track, struct MML_MidiChan_t *Channel) {
	int Key = Track_ReadByte(Track);
	if(Key < 0) return Key;
	int Vel = Track_ReadByte(Track);
	if(Vel < 0) return Vel;
	if(Key >= 0x80 || Vel >= 0x80) return MML_MIDI_CORRUPTED;
	if(Vel != 0) {
		return MML_MidiChan_ProcessNoteOn(State, Channel, (uint8_t)Key, (uint8_t)Vel);
	} else {
		return MML_MidiChan_ProcessNoteOff(State, Channel, (uint8_t)Key);
	}
}

static int Track_ParseNoteAftertouch(struct MML_MidiState_t *State, struct MML_MidiTrack_t *Track, struct MML_MidiChan_t *Channel) {
	int Key = Track_ReadByte(Track);
	if(Key < 0) return Key;
	int Val = Track_ReadByte(Track);
	if(Val < 0) return Val;
	if(Key >= 0x80 || Val >= 0x80) return MML_MIDI_CORRUPTED;
	return MML_OK;
}

static int Track_ParseController(struct MML_MidiState_t *State, struct MML_MidiTrack_t *Track, struct MML_MidiChan_t *Channel) {
	int Ctrl = Track_ReadByte(Track);
	if(Ctrl < 0) return Ctrl;
	int Val = Track_ReadByte(Track);
	if(Val < 0) return Val;
	if(Ctrl >= 0x80 || Val >= 0x80) return MML_MIDI_CORRUPTED;
	switch(Ctrl) {
		case CC_VOLUME: {
			return MML_MidiChan_SetVolume(State, Channel, (uint8_t)Val);
		} break;

		case CC_PAN: {
			return MML_MidiChan_SetPan(State, Channel, (uint8_t)Val);
		} break;

		case CC_EXPRESSION: {
			return MML_MidiChan_SetExpression(State, Channel, (uint8_t)Val);
		} break;

		case CC_NRPN_LSB: /* Fall through */
		case CC_NRPN_MSB: {
			//! NRPN is unsupported so clear the parameter number
			Channel->ParamNumber = COMBINE_MSB_LSB_16(0x7F, 0x7F);
			return
		} break;

		case CC_RPN_LSB: {
			Channel->ParamNumber = COMBINE_MSB_LSB_16(Channel->ParamNumber >> 7, Val);
		} break;

		case CC_RPN_MSB: {
			Channel->ParamNumber = COMBINE_MSB_LSB_16(Val, Channel->ParamNumber & 0x7F);
		} break;

		case CC_DATA_LSB: {
			Channel->ParamData = COMBINE_MSB_LSB_16(Channel->ParamData >> 7, Val);
			return MML_MidiChan_SetParameter(State, Channel);
		} break;

		case CC_DATA_MSB: {
			//! Clear data LSB to 0 when receiving MSB message
			Channel->ParamData = COMBINE_MSB_LSB_16(Val, /*Channel->ParamData & 0x7F*/0);
			return MML_MidiChan_SetParameter(State, Channel);
		} break;

		case CC_DATA_INC: {
			if(Channel->ParamData < COMBINE_MSB_LSB_16(0x7F, 0x7F)) {
				Channel->ParamData++;
				return MML_MidiChan_SetParameter(State, Channel);
			}
		} break;

		case CC_DATA_DEC: {
			if(Channel->ParamData > 0) {
				Channel->ParamData--;
				return MML_MidiChan_SetParameter(State, Channel);
			}
		} break;

		case CC_PEDAL_DAMPER: {
			Channel->DamperPedal = (uint8_t)Val;
			if(Val < 64) return MML_MidiChan_DamperPedalUp(State, Channel);
		} break;

		case CC_BANK_MSB: {
			Channel->BankMSB = (uint8_t)Val;
		} break;

		case CC_BANK_LSB: {
			Channel->BankLSB = (uint8_t)Val;
		} break;
	}
	return MML_OK;
}

static int Track_ParseProgramChange(
	struct MML_MidiState_t *State,
	struct MML_MidiTrack_t *Track,
	struct MML_MidiChan_t *Channel,
	uint8_t MidiChannelIdx
) {
	int Program = Track_ReadByte(Track);
	if(Program < 0) return Program;
	if(Program >= 0x80) return MML_MIDI_CORRUPTED;

	uint8_t BankLSB = Channel->BankLSB;
	uint8_t BankMSB = Channel->BankMSB;
	uint8_t IsDrums = (MidiChannelIdx == 10) ? 1 : 0;
	if(State->SysMode == MIDI_SYSMODE_XG && BankMSB == 127) {
		//! BankMSB shouldn't need to change, but shouldn't hurt
		BankMSB = 0;
		IsDrums = 1;
	}
	if(State->SysMode == MIDI_SYSMODE_GS) {
		if(
			State->Ch9Drums  && MidiChannelIdx == 9 ||
			State->Ch11Drums && MidiChannelIdx == 11
		) {
			IsDrums = 1;
		}
	}
	return MML_MidiChan_SetProgram(State, Channel, (uint8_t)Program, BankMSB, BankLSB, IsDrums);
}

static int Track_ParseChanAftertouch(struct MML_MidiState_t *State, struct MML_MidiTrack_t *Track, struct MML_MidiChan_t *Channel) {
	int Val = Track_ReadByte(Track);
	if(Val < 0) return Val;
	if(Val >= 0x80) return MML_MIDI_CORRUPTED;
	return MML_OK;
}

static int Track_ParsePitchBend(struct MML_MidiState_t *State, struct MML_MidiTrack_t *Track, struct MML_MidiChan_t *Channel) {
	int LSB = Track_ReadByte(Track);
	if(LSB < 0) return LSB;
	int MSB = Track_ReadByte(Track);
	if(MSB < 0) return MSB;
	if(LSB >= 0x80 || MSB >= 0x80) return MML_MIDI_CORRUPTED;
	return MML_MidiChan_SetPitchBend(State, Channel, COMBINE_MSB_LSB_16(MSB, LSB));
}

/************************************************/

//! Parse next track command
int MML_MidiTrack_ParseNextCommand(struct MML_MidiState_t *State, struct MML_MidiTrack_t *Track) {
	//! Read command byte
	int CmdByte = Track_PeekByte(Track);
	if(CmdByte < 0) return Command;
	if(CmdByte >= 0x80) {
		//! Update running status
		Track_SkipBytes(1, Track);
		Track->LastCommand = CmdByte;
	} else {
		//! Running status command: Use last command
		CmdByte = Track->LastCommand;
		if(CmdByte == 0xFF) return MML_MIDI_CORRUPTED;
	}

	//! Interpret command and select target channel
	uint8_t Command = (uint8_t)CmdByte >> 4;
	uint8_t ChanIdx = (uint8_t)CmdByte & 0xF;
	uint8_t DstChanIdx = ChanIdx;
	if(Command == 0xF) Command = (uint8_t)CmdByte;
	if(State->UseSeparateTracks && State->Format != 0) DstChanIdx = TrackIdx;

	//! Non-meta events cancel the channel prefix
	if(Command != 0xFF) State->MetaChannelPrefix = 0xFF;

	//! Parse command
	//! Note that program change commands need to know about the
	//! "real" MIDI channel as well, as we must apply some checks
	//! (eg. Ch9/11 drums, etc.).
	struct MML_MidiChan_t *Channel = State->Channels[DstChanIdx];
	switch(Command) {
		case 0x8: return Track_ParseNoteOff       (State, Track, Channel);
		case 0x9: return Track_ParseNoteOn        (State, Track, Channel);
		case 0xA: return Track_ParseNoteAftertouch(State, Track, Channel);
		case 0xB: return Track_ParseController    (State, Track, Channel);
		case 0xC: return Track_ParseProgramChange (State, Track, Channel, ChanIdx);
		case 0xD: return Track_ParseChanAftertouch(State, Track, Channel);
		case 0xE: return Track_ParsePitchBend     (State, Track, Channel);
		case 0xF0: /* Fall through */
		case 0xF7: return Track_ParseSysEx        (State, Track, Command);
		case 0xFF: return Track_ParseMetaEvent    (State, Track);
	}

	//! No matches - unknown command
	return MML_MIDI_CORRUPTED;
}

/************************************************/
//! EOF
/************************************************/
