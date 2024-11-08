/************************************************/
#include <stdint.h>
#include <stdio.h>
/************************************************/
#include "MML_Midi.h"
/************************************************/

#define CHANNEL_NOTE_NOT_PLAYING 0xFFFFFFFFu //! Stored in MML_MidiChan_t::NoteBufferOffs[]

/************************************************/

static uint64_t Channel_TryResampleTicks(struct MML_MidiState_t *State, uint64_t Ticks) {
	if(!State->ResampleTimings) {
		return Ticks;
	} else {
#if 0
		return TargetTick * State->TimeDiv / State->TimeMul;
#else
		uint64_t q = TargetTick / State->TimeMul;
		uint64_t r = TargetTick % State->TimeMul;
		return q*State->TimeDiv + r*State->TimeDiv/State->TimeMul;
#endif
	}
}

static uint64_t Channel_TryInvertResampledTicks(struct MML_MidiState_t *State, uint64_t Ticks) {
	if(!State->ResampleTimings) {
		return Ticks;
	} else {
#if 0
		return TargetTick * State->TimeMul / State->TimeDiv;
#else
		uint64_t q = TargetTick / State->TimeDiv;
		uint64_t r = TargetTick % State->TimeDiv;
		return q*State->TimeMul + r*State->TimeMul/State->TimeDiv;
#endif
	}
}

/************************************************/

static int Channel_PushByte(struct MML_MidiChan_t *Channel, uint8_t Byte) {
	if(!DynamicBuffer_WriteByte(Byte, &Channel->DataBuffer, &Channel->DataBufferOffs, &Channel->DataBufferSize)) {
		return MML_MIDI_OUT_OF_MEMORY;
	}
	return MML_OK;
}

static int Channel_PushBytes(struct MML_MidiChan_t *Channel, const uint8_t *Bytes, uint32_t Length) {
	if(!DynamicBuffer_WriteBytes(Bytes, Length, &Channel->DataBuffer, &Channel->DataBufferOffs, &Channel->DataBufferSize)) {
		return MML_MIDI_OUT_OF_MEMORY;
	}
	return MML_OK;
}

/************************************************/

static void Channel_TerminateAllNotes(struct MML_MidiChan_t *x) {
	uint32_t Key;
	for(Key=0;Key<128;Key++) {
		if(x->NoteBufferOffs[Key] != CHANNEL_NOTE_NOT_PLAYING) {
			x->NoteOffMask[Key/32] |= ((uint32_t)1 << (Key%32));
		}
	}
}

static int Channel_PushRest(struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel, uint64_t Ticks) {
	int Result = MML_OK;
	Channel->LastCommandTick += Channel_TryInvertResampledTicks(State, Ticks);
	while(Ticks) {
		uint16_t n = (Ticks < 65536) ? (uint16_t)Ticks : 65536;
		Ticks -= n;

		Result = Channel_PushByte(Channel, MML_CMD_REST);
		if(Result < 0) return Result;
		Result = Channel_PushByte(Channel, (uint8_t)(n >> 0));
		if(Result < 0) return Result;
		Result = Channel_PushByte(Channel, (uint8_t)(n >> 8));
		if(Result < 0) return Result;
	}
	return MML_OK;
}

static int Channel_FlushPending(struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel) {
	int Result = MML_OK;
	if(Channel->VolPending) {
		Result = Channel_PushByte(Channel, MML_CMD_VOLUME);
		if(Result < 0) return Result;
		Result = Channel_PushByte(Channel, Channel->Vol);
		if(Result < 0) return Result;
		Channel->VolPending = 0;
	}
	if(Channel->PanPending) {
		Result = Channel_PushByte(Channel, MML_CMD_PANNING);
		if(Result < 0) return Result;
		Result = Channel_PushByte(Channel, Channel->Pan);
		if(Result < 0) return Result;
		Channel->PanPending = 0;
	}
	if(Channel->ExpPending) {
		Result = Channel_PushByte(Channel, MML_CMD_EXPRESSION);
		if(Result < 0) return Result;
		Result = Channel_PushByte(Channel, Channel->Exp);
		if(Result < 0) return Result;
		Channel->ExpPending = 0;
	}
	if(Channel->BendPending) {
		Result = Channel_PushByte(Channel, MML_CMD_PITCHBEND);
		if(Result < 0) return Result;
		Result = Channel_PushByte(Channel, (uint8_t)(Channel->Bend >> 0));
		if(Result < 0) return Result;
		Result = Channel_PushByte(Channel, (uint8_t)(Channel->Bend >> 8));
		if(Result < 0) return Result;
		Channel->BendPending = 0;
	}
	if(Channel->TempoPending) {
		Result = Channel_PushByte(Channel, MML_CMD_TEMPO);
		if(Result < 0) return Result;
		Result = Channel_PushByte(Channel, (uint8_t)(Channel->Tempo >> 0));
		if(Result < 0) return Result;
		Result = Channel_PushByte(Channel, (uint8_t)(Channel->Tempo >> 8));
		if(Result < 0) return Result;
		Channel->TempoPending = 0;
	}

	//! Now flush note-on/off events
	uint32_t i;
	for(i=0;i<128/32;i++) {
		uint32_t Mask = Channel->NoteOnMask[i];
		while(Mask) {
			uint32_t Idx = 31 - clz32(Mask);
			Mask &= ~(1 << Idx);
			uint32_t Key = i*32 + Idx;

			Channel->NoteStartTick [Key] = State->CurrentTick;
			Channel->NoteBufferOffs[Key] = Channel->DataBufferOffs;
			Result = Channel_PushByte(Channel, MML_CMD_NOTE_TIMED);
			if(Result < 0) return Result;
			Result = Channel_PushByte(Channel, 0); //! Duration is set to 0 until updated by note-off
			if(Result < 0) return Result;
			Result = Channel_PushByte(Channel, 0);
			if(Result < 0) return Result;
			Result = Channel_PushByte(Channel, Key);
			if(Result < 0) return Result;
			Result = Channel_PushByte(Channel, Channel->NoteOnVel[Key]);
			if(Result < 0) return Result;
		}
		Channel->NoteOnMask[i] = 0;
	}
	for(i=0;i<128/32;i++) {
		uint32_t Mask = Channel->NoteOffMask[i];
		while(Mask) {
			uint32_t Idx = 31 - clz32(Mask);
			Mask &= ~(1 << Idx);
			uint32_t Key = i*32 + Idx;
			if(Channel->NoteBufferOffs[Key] != CHANNEL_NOTE_NOT_PLAYING) {
				uint64_t Duration = (uint64_t)((uint32_t)State->CurrentTick - Channel->NoteStartTick[Key]);
				Duration = Channel_TryResampleTicks(State, Duration);
				if(Duration < 1) Duration = 1;
				if(Duration > 65536) Duration = 65536;

				//! Store note duration, minus the bias
				uint8_t *DurationPtr = Channel->DataBuffer + Channel->NoteBufferOffs[Key];
				Duration -= 1; //! <- Bias
				DurationPtr[1] = (uint8_t)(Duration >> 0);
				DurationPtr[2] = (uint8_t)(Duration >> 8);

				//! Clear note offset
				Channel->NoteBufferOffs[Key] = CHANNEL_NOTE_NOT_PLAYING;
			}
		}
		Channel->NoteOffMask[i] = 0;
	}
	return MML_OK;
}

static int Channel_TryFlushPending(struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel) {
	//! Only flush if we're on a different tick
	uint64_t RestTime = Channel_TryResampleTicks(State, State->CurrentTick - Channel->LastCommandTick);
	if(RestTime != 0) {
		int Result = Channel_PushRest(State, Channel, RestTime);
		if(Result < 0) return Result;
		return Channel_FlushPending(State, Channel);
	} else return MML_OK;
}

static int Channel_UpdatePitchBendParameters(struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel) {
	return Channel_UpdatePitchBend(State, Channel, Channel->RawBendValue);
}

/************************************************/

int MML_MidiChan_SetVolume(struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel, uint8_t Vol) {
	int Result = Channel_TryFlushPending(State, Channel);
	if(Result < 0) return Result;
	if(Channel->Vol != Vol) {
		Channel->Vol        = Vol;
		Channel->VolPending = 1;
	}
	return MML_OK;
}

int MML_MidiChan_SetPan(struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel, uint8_t Pan) {
	int Result = Channel_TryFlushPending(State, Channel);
	if(Result < 0) return Result;

	//! NOTE: MIDI has a pan center at 64, whereas we have
	//! a pan center at 63, so we need to do some adjusting.
#if 0 //! Unrounded
	Pan = (uint8_t)(((int32_t)Pan - 64) * 63/64 + 63);
#else //! Rounded
	//! Signedness doesn't matter for this
	Pan = (uint8_t)(((uint32_t)Pan * 63 + 32) / 64);
#endif
	if(Channel->Pan != Pan) {
		Channel->Pan        = Pan;
		Channel->PanPending = 1;
	}
	return MML_OK;
}

int MML_MidiChan_SetExpression(struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel, uint8_t Exp) {
	int Result = Channel_TryFlushPending(State, Channel);
	if(Result < 0) return Result;
	if(Channel->Exp != Exp) {
		Channel->Exp        = Exp;
		Channel->ExpPending = 1;
	}
	return MML_OK;
}

int MML_MidiChan_SetPitchBend(struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel, uint16_t RawBend) {
	int Result = Channel_TryFlushPending(State, Channel);
	if(Result < 0) return Result;
	Channel->RawBendValue = RawBend;

	//! Calculate new bend value, scaled to 1048576000.0 = 1.0st
	int64_t Bend = (int64_t)RawBend - 8192;
	Bend *= (int64_t)Channel->BendRange; //! <- This scales by 128
	Bend += (int64_t)Channel->MasterTune * 128;
	Bend += (int64_t)Channel->MasterTranspose * 8192 * 128;
	Bend += (int64_t)State->MasterTranspose   * 8192 * 128;
	Bend *= 1000; //! <- State->MasterTune is scaled by 8192*1000
	Bend += (int64_t)State->MasterTune * 128;

	//! Finally, translate the bend value into SGE's format
	//! and apply the bias level to get the "raw" bend value.
	int64_t BendSGE = Bend; {
		//! Round[Bend * 128/1048576000]
		if(BendSGE < 0) BendSGE -= 8192000/2;
		else            BendSGE += 8192000/2;
		BendSGE /= 8192000;
	}
	if(BendSGE < -(127<<7) + 1) BendSGE = -(127<<7) + 1;
	if(BendSGE > +(127<<7) - 1) BendSGE = +(127<<7) - 1;
	uint16_t RawBendSGE = (uint16_t)(BendSGE - (-(127<<7) + 1));
	if(Channel->Bend != RawBendSGE) {
		Channel->Bend        = RawBendSGE;
		Channel->BendPending = 1;
	}
	return MML_OK;
}

int MML_MidiChan_SetParameter(struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel) {
	switch(Channel->ParamNumber) {
		case RPN_BENDRANGE: {
			Channel->BendRange = Channel->ParamData;
			return Channel_UpdatePitchBendParameters(State, Channel);
		} break;
		case RPN_FINETUNE: {
			Channel->MasterTune = (int16_t)Channel->ParamData - 8192;
			return Channel_UpdatePitchBendParameters(State, Channel);
		} break;
		case RPN_COARSETUNE: {
			//! We only use the MSB for this
			Channel->MasterTranspose = (int32_t)(Channel->ParamData >> 7) - 64;
			return Channel_UpdatePitchBendParameters(State, Channel);
		} break;
		default: return MML_OK;
	}
}

int MML_MidiChan_SetTempo(struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel, uint32_t BPM) {
	int Result = Channel_TryFlushPending(State, Channel);
	if(Result < 0) return Result;

	//! Hard limit of 1023 BPM
	if(BPM > 1023) BPM = 1023;
	if(State->Tempo != Tempo) {
		State->Tempo = Tempo;
		Channel->Tempo        = Tempo;
		Channel->TempoPending = 1;
		Channel->IsPopulated  = 1;
	}
	return MML_OK;
}

/************************************************/

int MML_MidiChan_ProcessNoteOff(struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel, uint8_t Key) {
	int Result = Channel_TryFlushPending(State, Channel);
	if(Result < 0) return Result;

	//! If note is not playing, early exit
	if(Channel->NoteBufferOffs[Key] == CHANNEL_NOTE_NOT_PLAYING) return MML_OK;
	if(Channel->DamperPedal < 64) {
		Channel->NoteOffMask[Key/32] |= ((uint32_t)1 << (Key%32));
	} else {
		Channel->DampersMask[Key/32] |= ((uint32_t)1 << (Key%32));
	}
	return MML_OK;
}

int MML_MidiChan_ProcessNoteOn(struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel, uint8_t Key, uint8_t Vel) {
	int Result = Channel_TryFlushPending(State, Channel);
	if(Result < 0) return Result;

	//! If we don't have any program so far, forcibly set to program 0:0:0
	if(Channel->Program == 0xFF) {
		Result = MML_MidiChan_SetProgram(State, Channel, 0, 0, 0, Channel->IsDrumsChannel);
		if(Result < 0) return Result;
	}

	//! If this note is already playing, we need some special handling
	if(Channel->NoteBufferOffs[Key] != CHANNEL_NOTE_NOT_PLAYING) {
		//! Put the key into the note-off queue and flush the state
		//! We explicitly do this rather than call ProcessNoteOff()
		//! in case the dampers pedal is down.
		Channel->NoteOffMask[Key/32] |= ((uint32_t)1 << (Key%32));
		Result = Channel_FlushPending(State, Channel);
		if(Result < 0) return Result;
	}

	//! Apply the velocity curve MidiVelocity=1..127 -> LinearVelocity=1..128
	if(State->LinearizeVelocity) {
		//! NOTE: MidiVelocity < 8 is clipped to LinearVelocity=1
#if 0 //! Unrounded, and using division
		Vel = 128 * (Vel*Vel)/(127*127);
#else //! Rounded, without division (exact)
		Vel = ((uint32_t)Vel*Vel * 4161 + (1<<18)) >> 19;
#endif
		if(Vel < 1) Vel = 1;
	} else {
#if 0 //! Unrounded, and using division
		Vel = Vel * 128/127;
#else //! Rounded, without division (exact)
		Vel = (uint8_t)(((uint16_t)Vel * 129 + 64) >> 7);
#endif
	}

	//! Enqueue note-on command
	Channel->NoteOnMask[Key/32] |= ((uint32_t)1 << (Key%32));
	Channel->NoteOnVel[Key] = Vel;
	Channel->IsPopulated = 1;

	//! Note is now inserted into buffer
	return MML_OK;
}

int MML_MidiChan_DamperPedalUp(struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel) {
	int Result = Channel_TryFlushPending(State, Channel);
	if(Result < 0) return Result;

	//! Merge the dampers bits into note-off bits
	uint32_t i;
	for(i=0;i<128/32;i++) {
		Channel->NoteOffMask[i] |= Channel->DampersMask[i];
		Channel->DampersMask[i] = 0;
	}
	return MML_OK;
}

int MML_MidiChan_SetProgram(struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel, uint8_t Program, uint8_t BankMSB, uint8_t BankLSB, uint8_t IsDrums) {
	int Result = Channel_TryFlushPending(State, Channel);
	if(Result < 0) return Result;

	//! These checks are kinda broken, since we may receive a different program
	//! and we aren't allowed to overwrite the bank MSB/LSB values, etc.
	if(/*Channel->Program != Program && Channel->BankMSB != BankMSB && Channel->BankLSB != BankLSB*/1) {
		//Channel->BankLSB = BankLSB;
		//Channel->BankMSB = BankMSB;
		Channel->Program = Program;
		Result = Channel_PushByte(Channel, MML_CMD_PROGRAM);
		if(Result < 0) return Result;
		Result = Channel_PushByte(Channel, BankMSB);
		if(Result < 0) return Result;
		Result = Channel_PushByte(Channel, BankLSB);
		if(Result < 0) return Result;
		Result = Channel_PushByte(Channel, Program | IsDrums<<7);
		if(Result < 0) return Result;
	}
	return MML_OK;
}

int MML_MidiChan_InsertComment(struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel, const char *Comment) {
	int Result = Channel_TryFlushPending(State, Channel);
	if(Result < 0) return Result;
	Result = Channel_PushByte(Channel, MML_CMD_COMMENT);
	if(Result < 0) return Result;
	Result = Channel_PushBytes(Channel, Comment, strlen(Comment)+1);
	if(Result < 0) return Result;
	return MML_OK;
}

/************************************************/

//! Initialize channel
void MML_MidiChan_Init(struct MML_MidiChan_t *x, uint8_t IsDrumsChannel) {
	uint32_t i;
	x->IsPopulated    = 0;
	x->IsDrumsChannel = IsDrumsChannel;
	x->Vol  = 100, x->VolPending = 0;
	x->Pan  =  63, x->PanPending = 0;
	x->Exp  = 128, x->ExpPending = 0;
	x->Bend  = (127<<7)-1, x->BendPending  = 0;
	x->Tempo = 120,        x->TempoPending = 0;
	x->DamperPedal  = 0;
	x->BankLSB      = 0;
	x->BankMSB      = 0;
	x->Program      = 0xFF;
	x->RawBendValue = 8192;
	x->BendRange    = 2 << 7; //! 2.0st
	x->ParamNumber  = 0x3FFF;
	x->ParamData    = 0;
	x->MasterTranspose = 0;
	x->MasterTune      = 0;
	x->DataBuffer      = NULL;
	x->DataBufferOffs  = 0;
	x->DataBufferSize  = 0;
	x->LastCommandTick = 0;
	for(i=0;i<128/32;i++) x->NoteOnMask [i] = 0;
	for(i=0;i<128/32;i++) x->NoteOffMask[i] = 0;
	for(i=0;i<128/32;i++) x->DampersMask[i] = 0;
	for(i=0;i<128;i++) x->NoteBufferOffs[i] = CHANNEL_NOTE_NOT_PLAYING;
}

/************************************************/

//! Reset channel
void MML_MidiChan_Reset(struct MML_MidiChan_t *Channel) {
	Channel->BankLSB     = 0;
	Channel->BankMSB     = 0;
	Channel->Program     = 0xFF;
	Channel->BendRange   = 2 << 7;
	Channel->ParamNumber = 0x3FFF;
	Channel->ParamData   = 0;
	Channel->MasterTranspose = 0;
	Channel->MasterTune      = 0;
	Channel_TerminateAllNotes(Channel);
	MML_MidiChan_DamperPedalUp(State, Channel);
	MML_MidiChan_SetVolume    (State, Channel, 100);
	MML_MidiChan_SetPan       (State, Channel, 63);
	MML_MidiChan_SetExpression(State, Channel, 100);
	MML_MidiChan_SetPitchBend (State, Channel, 8192);
}

/************************************************/

//! Release all channel notes
int MML_MidiChan_ReleaseAllNotes(struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel) {
	(void)State;
	Channel_TerminateAllNotes(Channel);
	return MML_OK;
}

/************************************************/

//! Finalize channel data
int MML_MidiChan_Finalize(struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel) {
	int Result;

	//! Terminate any stuck notes and release damper pedal
	Channel_TerminateAllNotes(Channel);
	Result = MML_MidiChan_DamperPedalUp(State, Channel);
	if(Result < 0) return Result;

	//! Flush all pending commands
	Result = Channel_FlushPending(State, Channel);
	if(Result < 0) return Result;

	//! Push the END command
	Result = Channel_PushByte(Channel, MML_CMD_END);
	if(Result < 0) return Result;

	//! All done
	return MML_OK;
}

/************************************************/

//! Destroy channel
void MML_MidiChan_Destroy(struct MML_MidiChan_t *x) {
	free(x->DataBuffer);
}

/************************************************/
//! EOF
/************************************************/
