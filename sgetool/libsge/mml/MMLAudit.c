/************************************************/
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
/************************************************/
#include "MML.h"
#include "SGE-Compiler.h"
/************************************************/

//! Maximum number of jump redirections before opting out
#define JUMP_REDIRECT_LIMIT 8

//! Maximum number of jump adjustments before opting out
#define JUMP_ITERATION_LIMIT 256

//! Track_t::RepeatType
#define REPEAT_TYPE_REPEAT  0
#define REPEAT_TYPE_PATTERN 1

/************************************************/

struct TrackRedirection_t {
	uint32_t SrcOffs, Iters;
	struct SGE_Track_t State;
};

struct TrackAuditData_t {
	const uint8_t *DataBeginOffs;
	struct TrackRedirection_t JumpRedirections[JUMP_REDIRECT_LIMIT];
};

//! Forward-declare this function because we recurse
//! into it from some commands for branch handling
static int MML_AuditTrack(struct MML_t *MML, struct SGE_Track_t *Track);

//! This function is used to test for early-exit of patterns/repeats/jumps
static int Track_StateMatch(const struct SGE_Track_t *a, const struct SGE_Track_t *b) {
	if(
		a->Program   == b->Program   &&
		a->Priority  == b->Priority  &&
		a->Octave    == b->Octave    &&
		a->Transpose == b->Transpose &&
		a->NoteLengthMul == b->NoteLengthMul &&
		a->NoteLengthAdd == b->NoteLengthAdd &&
		!memcmp(&a->Vel, &b->Vel, sizeof(a->Vel)) &&
		!memcmp(&a->Vol, &b->Vol, sizeof(a->Vol)) &&
		!memcmp(&a->Exp, &b->Exp, sizeof(a->Exp)) &&
		!memcmp(&a->Pan, &b->Pan, sizeof(a->Pan)) &&
		!memcmp(&a->Bnd, &b->Bnd, sizeof(a->Bnd))
	) return 1;
	return 0;
}

/************************************************/

static void Track_InitCtrl(union SGE_CtrlUnion_t *Ctrl, uint8_t Is16BitCtrl, uint16_t InitValue) {
	Ctrl->Phase = 0, Ctrl->Duration = 0;
	if(Is16BitCtrl) {
		Ctrl->Ctrl16.Value = Ctrl->Ctrl16.Target = InitValue;
	} else {
		Ctrl->Ctrl8.Value  = Ctrl->Ctrl8.Target  = (uint8_t)InitValue;
	}
}
static void Track_Init(struct SGE_Track_t *Track, const uint8_t *Data) {
	uint32_t i;
	Track->NybbleSrc   = 0;
	Track->PortamentoEnable = 0;
	Track->StackDepth  = 0;
	Track->Program     = 0;
	Track->Priority    = 8;
	Track->Octave      = 5*12;
	Track->Transpose   = 0;
	Track->Rest        = 0;
	Track->LastTimeCode = 0;
	Track->NoteLengthMul = MML_NOTEMUL_RESOLUTION;
	Track->NoteLengthAdd = 0;
	Track_InitCtrl((union SGE_CtrlUnion_t*)(&Track->Vel), 0, 100);
	Track_InitCtrl((union SGE_CtrlUnion_t*)(&Track->Vol), 0, 100);
	Track_InitCtrl((union SGE_CtrlUnion_t*)(&Track->Exp), 0, 128);
	Track_InitCtrl((union SGE_CtrlUnion_t*)(&Track->Pan), 0,  64);
	Track_InitCtrl((union SGE_CtrlUnion_t*)(&Track->Bnd), 1, 0x7F << 7);
	Track->RepeatType  = 0;
	Track->NybbleStack = 0;
	Track_InitCtrl((union SGE_CtrlUnion_t*)(&Track->Portamento), 0, 0);
	Track->Src = Data;
	for(i=0;i<MML_MAX_NESTING_LEVELS;i++) Track->Stack[i] = NULL;
}

static int Track_CheckRedirection(struct MML_t *MML, const struct SGE_Track_t *Track, struct TrackAuditData_t *AuditData) {
	//! Try to find this jump in the redirection table
	uint32_t i;
	uint32_t SrcOffs = (Track->Src - AuditData->DataBeginOffs)*2 + Track->NybbleSrc;
	struct TrackRedirection_t *JumpRedir = AuditData->JumpRedirections;
	for(i=0;i<JUMP_REDIRECT_LIMIT;i++) {
		//! Matched redirection?
		if(JumpRedir[i].SrcOffs == SrcOffs) {
			//! Found a match. Has the state stopped mutating?
			if(Track_StateMatch(Track, &JumpRedir[i].State)) {
				JumpRedir[i].State.Src = NULL;
				return MML_EOF;
			}

			//! Hit the iteration limit?
			if(++JumpRedir[i].Iters >= JUMP_ITERATION_LIMIT) {
				MML_AppendErrorGlobal(MML, "Audit: Iteration limit reached during Jump command.");
				return MML_ERROR;
			}

			//! Copy the newest state and continue
			memcpy(&JumpRedir[i].State, Track, sizeof(JumpRedir[i].State));
			return MML_OK;
		}
	}

	//! Add redirection to list
	for(i=0;i<JUMP_REDIRECT_LIMIT;i++) {
		if(JumpRedir[i].State.Src == NULL) {
			JumpRedir[i].SrcOffs = SrcOffs;
			memcpy(&JumpRedir[i].State, Track, sizeof(JumpRedir[i].State));
			return MML_OK;
		}
	}

	//! No entries left in table
	MML_AppendErrorGlobal(MML, "Audit: Jump redirection limit reached; too many Jump commands found.");
	return MML_ERROR;
}

static void Track_UpdateCtrl(union SGE_CtrlUnion_t *Ctrl, uint8_t Is16BitCtrl, int32_t nTicks) {
	//! Running this in a loop is horrific, but is the only
	//! way of getting the exact behaviour of the driver,
	//! as the ramp is a compact /approximation/ to the true
	//! linear curve formed between the start and end.
	int32_t Val, Target;
	int32_t Phase    = (int32_t)Ctrl->Phase;
	int32_t Duration = (int32_t)Ctrl->Duration + 1;
	if(Is16BitCtrl) {
		Val    = Ctrl->Ctrl16.Value;
		Target = Ctrl->Ctrl16.Target;
	} else {
		Val    = Ctrl->Ctrl8.Value;
		Target = Ctrl->Ctrl8.Target;
	}
	int32_t Delta = Target - Val;
	while(Delta != 0 && nTicks--) {
		Delta  = ((Delta << 8) / Duration) + Phase;
		Val   += Delta >> 8;
		Phase  = Delta & 0xFF;
		Delta  = Target - Val;
		Duration--;
	}
	Ctrl->Phase    = (uint8_t)Phase;
	Ctrl->Duration = (uint8_t)(Duration-1);
	if(Is16BitCtrl) {
		Ctrl->Ctrl16.Value = (uint16_t)Val;
	} else {
		Ctrl->Ctrl8.Value  = ( uint8_t)Val;
	}
}
static void Track_UpdateControllers(struct SGE_Track_t *Track, int32_t nTicks) {
	Track_UpdateCtrl((union SGE_CtrlUnion_t*)(&Track->Vel), 0, nTicks);
	Track_UpdateCtrl((union SGE_CtrlUnion_t*)(&Track->Vol), 0, nTicks);
	Track_UpdateCtrl((union SGE_CtrlUnion_t*)(&Track->Exp), 0, nTicks);
	Track_UpdateCtrl((union SGE_CtrlUnion_t*)(&Track->Pan), 0, nTicks);
	Track_UpdateCtrl((union SGE_CtrlUnion_t*)(&Track->Bnd), 1, nTicks);
}

static int Track_OctaveChange(struct MML_t *MML, struct SGE_Track_t *Track, int Delta) {
	int Octave = (int)Track->Octave + Delta;
	if(Octave < 0 || Octave > 10*12) {
		MML_AppendErrorGlobal(MML, "Audit: Found out-of-range octave change.");
		return MML_ERROR;
	}
	Track->Octave = (uint8_t)Octave;
	return MML_OK;
}

static void Track_SeekNybbles(struct SGE_Track_t *Track, int DeltaNybbles) {
	Track->Src       += (DeltaNybbles >> 1) + (Track->NybbleSrc & DeltaNybbles);
	Track->NybbleSrc += DeltaNybbles;
}

/************************************************/

//! Read nybble value
static int Track_ReadNybble(struct SGE_Track_t *Track) {
	int v = *Track->Src;
	if((Track->NybbleSrc ^= 1) == 0) { v >>= 4; Track->Src++; }
	return v & 0xF;
}

//! Read multi-nybble value
static int Track_ReadMulti(struct SGE_Track_t *Track, uint8_t nNybbles) {
	uint32_t v = 0;
	do {
		v = (v << 4) | (uint32_t)Track_ReadNybble(Track);
	} while(--nNybbles);
	return v;
}

//! Read 8-bit byte value
static int Track_ReadByte(struct SGE_Track_t *Track) {
	return Track_ReadMulti(Track, 2);
}

//! Read 16-bit word value
static int Track_ReadWord(struct SGE_Track_t *Track) {
	return Track_ReadMulti(Track, 4);
}

//! Read SeekAddr value
static int Track_ReadSeekAddr(struct SGE_Track_t *Track) {
	uint32_t x = Track_ReadByte(Track);
	     if(x == 0xFD) { x = Track_ReadByte(Track); x += 0x00FD; }
	else if(x == 0xFE) { x = Track_ReadWord(Track); x += 0x01FD; }
	else if(x == 0xFF) { x = Track_ReadWord(Track); x = ((x << 8) | Track_ReadByte(Track)) + 0x0101FD; }
	x += (1 << 1); //! Unbias
	return ((x & 1) != 0) ? (x >> 1) : (-(x >> 1));
}

//! Read TimeCode value
//! Returns nTicks-1 (!)
static int Track_ReadTimeCode(struct MML_t *MML, struct SGE_Track_t *Track) {
	uint32_t nTicks = 0;
	for(;;) {
		int v = Track_ReadNybble(Track);

		//! Tick-coded word
		if(v == 0xF) {
			//! Tick-coded words should NOT specify any durations
			if(nTicks != 0) {
				MML_AppendErrorGlobal(MML, "Audit: Found tick-coded word with extra duration coding. Please report this error.");
				return MML_ERROR;
			}

			//! The word value contains nTicks-1 directly
			return Track_ReadWord(Track);
		}

		//! Choose between normal and triplet timing
		uint16_t Base = MML_TICKS_PER_WHOLE_NOTE;
		if(v == 0xE) {
			//! Eh,Eh/Fh = 1/128 triplet
			v = Track_ReadNybble(Track);
			if(v >= 0xE) {
				nTicks += 1;
				if(v == 0xF) continue; //! Eh,Fh = 1/128 triplet (tied)
				else break;            //! Eh,Eh = 1/128 triplet
			}

			//! Standard triplet note
			Base = MML_TICKS_PER_WHOLE_NOTE * 2/3;
		}

		//! Add to current duration
		nTicks += Base >> ((v >= 0x7) ? (v - 0x7) : v);
		if(v >= 0x7) continue; //! 7h..Dh = Tied
		else break;
	}

	//! Ensure nTicks is in range
	if(nTicks == 0) {
		MML_AppendErrorGlobal(MML, "Audit: Decoded duration of 0 ticks. Please report this error.");
		return MML_ERROR;
	}
	if(nTicks > 65536) {
		MML_AppendErrorGlobal(MML, "Audit: Decoded duration longer than 65536 ticks. Please report this error.");
		return MML_ERROR;
	}
	return (int)(nTicks-1);
}

//! Parse controller value
static int Track_ReadController(
	struct MML_t *MML,
	union  SGE_CtrlUnion_t *Ctrl,
	struct SGE_Track_t *Track,
	uint32_t Value,
	uint8_t  Is16BitCtrl
) {
	uint16_t Target = Value >> 1;
	if(Ctrl) {
		if(Is16BitCtrl) Ctrl->Ctrl16.Target = Target;
		else            Ctrl->Ctrl8.Target  = (uint8_t)Target;
	}

	//! Non-ramping controller?
	if((Value & 1) == 0) {
		if(Ctrl) {
			if(Is16BitCtrl) Ctrl->Ctrl16.Value = Target;
			else            Ctrl->Ctrl8.Value  = (uint8_t)Target;
		}
		return MML_OK;
	}

	//! Ramping controller - read a duration as well
	int Duration = Track_ReadTimeCode(MML, Track);
	if(Duration == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While decoding controller ramp duration:");
		return MML_ERROR;
	}
	if(Duration > 255) {
		MML_AppendErrorGlobal(MML, "Audit: Decoded a controller ramp duration longer than 256 ticks. Please report this error.");
		return MML_ERROR;
	}
	if(Ctrl) {
		Ctrl->Phase    = 0;
		Ctrl->Duration = (uint8_t)Duration;
	}
	return MML_OK;
}

/************************************************/
#define BEGIN_COMMAND(x, ...) \
	static int Track_Command##x(struct MML_t *MML, struct SGE_Track_t *Track, struct TrackAuditData_t *AuditData __VA_OPT__(,) __VA_ARGS__)
#define UNALLOCATED_COMMAND(Cmd) \
	BEGIN_COMMAND(Cmd) { \
		(void)Track; \
		(void)AuditData; \
		MML_AppendErrorGlobal(MML, "Audit: Found instance of unallocated command (" #Cmd "). Please report this error."); \
		return MML_ERROR; \
	}

//! Generic note command
BEGIN_COMMAND(_NoteEvent, uint16_t TicksMinusOne) {
	(void)AuditData;
	int Cmd;
	uint8_t nNotesRem     = 1;
	uint8_t nNotesPlayed  = 0;
	uint8_t RestAfterNote = 1;
	uint8_t Vel = Track->Vel.Value;
	for(;;) {
		Cmd = Track_ReadNybble(Track);

		//! Ch,Dh: Octave change
		if(Cmd == 0xC) {
			if(Track_OctaveChange(MML, Track, -12) == MML_ERROR) {
				MML_AppendErrorContext(MML, "Audit: While processing octave down (note sub-command):");
				return MML_ERROR;
			}
			continue;
		}
		if(Cmd == 0xD) {
			if(Track_OctaveChange(MML, Track, +12) == MML_ERROR) {
				MML_AppendErrorContext(MML, "Audit: While processing octave up (note sub-command):");
				return MML_ERROR;
			}
			continue;
		}

		//! Eh: Overlay
		if(Cmd == 0xE) {
			if(!RestAfterNote) {
				MML_AppendErrorGlobal(MML, "Audit: Found Overlay command, with Overlay already enabled. Please report this error.");
				return MML_ERROR;
			}
			RestAfterNote = 0;
			continue;
		}

		//! Fh: Special
		if(Cmd == 0xF) {
			Cmd = Track_ReadNybble(Track);

			//! Fh,0h: Velocity change
			if(Cmd == 0x0) {
				int NewVel = Track_ReadByte(Track);
				Vel = ((uint8_t)NewVel >> 1) + 1;

				//! Reset global velocity with Sticky bit
				if((NewVel & 1) != 0) Track->Vel.Value = Track->Vel.Target = Vel;
				continue;
			}

			//! Fh,1h..Bh: Octave set
			if(Cmd >= 0x1 && Cmd <= 0xB) {
				Track->Octave = (Cmd-1) * 12;
				continue;
			}

			//! Fh,Ch..Fh: Stack push
			nNotesRem += Cmd - 0xC + 1;
			if(nNotesRem > MML_MAX_NOTESTACK_DEPTH) {
				MML_AppendErrorGlobal(MML, "Audit: Stack-push depth exceeded. Please report this error.");
				return MML_ERROR;
			}
			continue;
		}

		//! 0h..Bh: Normal note
		if(
			MML->NotifyKeyOn &&
			MML->NotifyKeyOn(MML->NotifyUserdata, Track->Program, Track->Octave + Track->Transpose + Cmd, Vel, MML->UseGlobalToneBank) < 0
		) {
			MML_AppendWarningGlobal(MML, "Audit: Key-on event with missing region or program.");
		}
		nNotesPlayed++;
		if(--nNotesRem == 0) break;

		//! Ensure to reset velocity back to what it used to be, in case
		//! we had a velocity override without a sticky bit being used
		Vel = Track->Vel.Value;
	}

	//! Warn if using a chord with portamento mode
	if(nNotesPlayed > 1 && Track->PortamentoEnable) {
		MML_AppendWarningGlobal(MML, "Audit: Multi-note event in portamento mode; sweep will apply to last note only.");
	}

	//! If we rest after this note event, store the rest time
	if(RestAfterNote) {
		Track->Rest = TicksMinusOne;
		return MML_REST;
	}
	return MML_OK;
}

//! 0h: Note (time-coded)
BEGIN_COMMAND(0h) {
	int TicksMinusOne = Track_ReadTimeCode(MML, Track);
	if(TicksMinusOne == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While decoding duration for time-coded note command (0h):");
		return MML_ERROR;
	}
	Track->LastTimeCode = (uint16_t)TicksMinusOne;
	int Result = Track_Command_NoteEvent(MML, Track, AuditData, TicksMinusOne);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While processing time-coded note command (0h):");
	}
	return Result;
}

//! 1h: Note (last time-coded)
BEGIN_COMMAND(1h) {
	int Result = Track_Command_NoteEvent(MML, Track, AuditData, Track->LastTimeCode);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While processing previous-duration note command (1h):");
	}
	return Result;
}

//! 2h: Note (1/4)
BEGIN_COMMAND(2h) {
	int Result = Track_Command_NoteEvent(MML, Track, AuditData, MML_TICKS_PER_WHOLE_NOTE/4 - 1);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While processing 1/4 note command (1h):");
	}
	return Result;
}

//! 3h: Note (1/8)
BEGIN_COMMAND(3h) {
	int Result = Track_Command_NoteEvent(MML, Track, AuditData, MML_TICKS_PER_WHOLE_NOTE/8 - 1);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While processing 1/8 note command (3h):");
	}
	return Result;
}

//! 4h: Note (1/16)
BEGIN_COMMAND(4h) {
	int Result = Track_Command_NoteEvent(MML, Track, AuditData, MML_TICKS_PER_WHOLE_NOTE/16 - 1);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While processing 1/16 note command (4h):");
	}
	return Result;
}

//! 5h: Note (1/32)
BEGIN_COMMAND(5h) {
	int Result = Track_Command_NoteEvent(MML, Track, AuditData, MML_TICKS_PER_WHOLE_NOTE/32 - 1);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While processing 1/32 note command (5h):");
	}
	return Result;
}

//! 6h: Note (1/64)
BEGIN_COMMAND(6h) {
	int Result = Track_Command_NoteEvent(MML, Track, AuditData, MML_TICKS_PER_WHOLE_NOTE/64 - 1);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While processing 1/64 note command (6h):");
	}
	return Result;
}

//! 7h: Rest
BEGIN_COMMAND(7h) {
	(void)AuditData;
	int TicksMinusOne = Track_ReadTimeCode(MML, Track);
	if(TicksMinusOne == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While processing rest command (7h):");
		return MML_ERROR;
	}
	Track->Rest = (uint16_t)TicksMinusOne;
	return MML_REST;
}

//! 8h: Octave
BEGIN_COMMAND(8h) {
	(void)AuditData;
	int v = Track_ReadNybble(Track);

	//! 0h..Ah: Set octave
	if(v >= 0x0 && v <= 0xA) {
		Track->Octave = (uint8_t)(v*12);
		return MML_OK;
	}

	//! Ch..Fh: Change octave
	if(v >= 0xC && v <= 0xF) switch(v) {
		//! Ch = Down
		case 0xC: {
			if(Track_OctaveChange(MML, Track, -12) == MML_ERROR) {
				MML_AppendErrorContext(MML, "Audit: While processing octave-down command (8h,Ch):");
				return MML_ERROR;
			} else return MML_OK;
		} break;

		//! Dh = Up
		case 0xD: {
			if(Track_OctaveChange(MML, Track, +12) == MML_ERROR) {
				MML_AppendErrorContext(MML, "Audit: While processing octave-up command (8h,Dh):");
				return MML_ERROR;
			} else return MML_OK;
		} break;

		//! Eh = Down*2
		case 0xE: {
			if(Track_OctaveChange(MML, Track, -24) == MML_ERROR) {
				MML_AppendErrorContext(MML, "Audit: While processing octave-down*2 command (8h,Eh):");
				return MML_ERROR;
			} else return MML_OK;
		} break;

		//! Fh = Up*2
		case 0xF: {
			if(Track_OctaveChange(MML, Track, +24) == MML_ERROR) {
				MML_AppendErrorContext(MML, "Audit: While processing octave-up*2 command (8h,Fh):");
				return MML_ERROR;
			} else return MML_OK;
		} break;
	}

	//! Bh: Relative change
	int Delta = (Track_ReadNybble(Track) ^ 8) - 8; //! <- Sign expand
	Delta = (Delta < 0) ? (Delta-2) : (Delta+3); //! -10..+10 octaves change
	if(Track_OctaveChange(MML, Track, Delta * 12) == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While processing octave-change command (8h,Bh):");
		return MML_ERROR;
	} else return MML_OK;
}

//! 9h: Velocity
BEGIN_COMMAND(9h) {
	(void)AuditData;
	int Result = Track_ReadController(MML, (union SGE_CtrlUnion_t*)(&Track->Vel), Track, Track_ReadByte(Track) + (1<<1), 0);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While reading velocity control command (9h):");
	}
	return Result;
}

//! Ah: Volume
BEGIN_COMMAND(Ah) {
	(void)AuditData;
	int Result = Track_ReadController(MML, (union SGE_CtrlUnion_t*)(&Track->Vol), Track, Track_ReadByte(Track) + (1<<1), 0);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While reading volume control command (Ah):");
	}
	return Result;
}

//! Bh: Expression
BEGIN_COMMAND(Bh) {
	(void)AuditData;
	int Result = Track_ReadController(MML, (union SGE_CtrlUnion_t*)(&Track->Exp), Track, Track_ReadByte(Track) + (1<<1), 0);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While reading expression control command (Bh):");
	}
	return Result;
}

//! Ch: Panning
BEGIN_COMMAND(Ch) {
	(void)AuditData;
	int Result = Track_ReadController(MML, (union SGE_CtrlUnion_t*)(&Track->Pan), Track, Track_ReadByte(Track) + (1<<1), 0);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While reading panning control command (Ch):");
	}
	return Result;
}

//! Dh: PitchBend
BEGIN_COMMAND(Dh) {
	(void)AuditData;
	int Result = Track_ReadController(MML, (union SGE_CtrlUnion_t*)(&Track->Bnd), Track, Track_ReadWord(Track), 1);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While reading pitch-bend control command (Dh):");
	}
	return Result;
}

//! Eh,0h: PortamentoOn
BEGIN_COMMAND(Eh0h) {
	(void)MML;
	(void)AuditData;
	Track->PortamentoEnable = 1;
	return MML_OK;
}

//! Eh,1h: PortamentoOff
BEGIN_COMMAND(Eh1h) {
	(void)MML;
	(void)AuditData;
	Track->PortamentoEnable = 0;
	return MML_OK;
}

//! Eh,2h: RepeatStart
BEGIN_COMMAND(Eh2h) {
	(void)AuditData;
	uint8_t Depth = Track->StackDepth;
	if(Depth >= MML_MAX_NESTING_LEVELS) {
		MML_AppendErrorGlobal(MML, "Audit: Maximum nesting level exceeded. Please report this error.");
		return MML_ERROR;
	}
	Track->RepeatType  = (Track->RepeatType  << 1) | REPEAT_TYPE_REPEAT;
	Track->NybbleStack = (Track->NybbleStack << 1) | 0;
	Track->Stack[Depth] = NULL;
	Track->StackDepth = Depth+1;
	return MML_OK;
}

//! Eh,3h: PitchBendSt1
BEGIN_COMMAND(Eh3h) {
	(void)AuditData;
	uint32_t Value = (Track_ReadNybble(Track) + 128 - 8) << 8 | 0;
	int Result = Track_ReadController(MML, (union SGE_CtrlUnion_t*)(&Track->Bnd), Track, Value, 1);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While reading pitch-bend control command (Eh,3h):");
	}
	return Result;
}

//! Eh,4h: PitchBendSt1Ramp
BEGIN_COMMAND(Eh4h) {
	(void)AuditData;
	uint32_t Value = (Track_ReadNybble(Track) + 128 - 8) << 8 | 1;
	int Result = Track_ReadController(MML, (union SGE_CtrlUnion_t*)(&Track->Bnd), Track, Value, 1);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While reading pitch-bend control command (Eh,4h):");
	}
	return Result;
}

//! Eh,5h: PitchBendSt2
BEGIN_COMMAND(Eh5h) {
	(void)AuditData;
	uint32_t Value = Track_ReadByte(Track) << 8 | 0;
	int Result = Track_ReadController(MML, (union SGE_CtrlUnion_t*)(&Track->Bnd), Track, Value, 1);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While reading pitch-bend control command (Eh,5h):");
	}
	return Result;
}

//! Eh,6h: PitchBendSt2Ramp
BEGIN_COMMAND(Eh6h) {
	(void)AuditData;
	uint32_t Value = Track_ReadByte(Track) << 8 | 1;
	int Result = Track_ReadController(MML, (union SGE_CtrlUnion_t*)(&Track->Bnd), Track, Value, 1);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While reading pitch-bend control command (Eh,6h):");
	}
	return Result;
}

//! Eh,7h: RelPitchBend
BEGIN_COMMAND(Eh7h) {
	(void)AuditData;
	int32_t Value = (int32_t)Track->Bnd.Value + (int32_t)Track_ReadWord(Track) - (128<<7);
	if(Value < -(128<<7)  ) Value = -(128<<7);
	if(Value > +(128<<7)-1) Value = +(128<<7)-1;
	Value = Value<<1 | 0;
	int Result = Track_ReadController(MML, (union SGE_CtrlUnion_t*)(&Track->Bnd), Track, Value, 1);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While reading relative pitch-bend control command (Eh,7h):");
	}
	return Result;
}

//! Eh,8h: RelPitchBendSt1
BEGIN_COMMAND(Eh8h) {
	(void)AuditData;
	int32_t Value = (int32_t)Track->Bnd.Value + (((int32_t)Track_ReadNybble(Track) - 8) << 7);
	if(Value < -(128<<7)  ) Value = -(128<<7);
	if(Value > +(128<<7)-1) Value = +(128<<7)-1;
	Value = Value<<1 | 0;
	int Result = Track_ReadController(MML, (union SGE_CtrlUnion_t*)(&Track->Bnd), Track, Value, 1);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While reading relative pitch-bend control command (Eh,8h):");
	}
	return Result;
}

//! Eh,9h: RelPitchBendSt1Ramp
BEGIN_COMMAND(Eh9h) {
	(void)AuditData;
	int32_t Value = (int32_t)Track->Bnd.Value + (((int32_t)Track_ReadNybble(Track) - 8) << 7);
	if(Value < -(128<<7)  ) Value = -(128<<7);
	if(Value > +(128<<7)-1) Value = +(128<<7)-1;
	Value = Value<<1 | 1;
	int Result = Track_ReadController(MML, (union SGE_CtrlUnion_t*)(&Track->Bnd), Track, Value, 1);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While reading relative pitch-bend control command (Eh,9h):");
	}
	return Result;
}

//! Eh,Ah: RelPitchBendSt2
BEGIN_COMMAND(EhAh) {
	(void)AuditData;
	int32_t Value = (int32_t)Track->Bnd.Value + (((int32_t)Track_ReadByte(Track) - 128) << 7);
	if(Value < -(128<<7)  ) Value = -(128<<7);
	if(Value > +(128<<7)-1) Value = +(128<<7)-1;
	Value = Value<<1 | 0;
	int Result = Track_ReadController(MML, (union SGE_CtrlUnion_t*)(&Track->Bnd), Track, Value, 1);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While reading relative pitch-bend control command (Eh,Ah):");
	}
	return Result;
}

//! Eh,Bh: RelPitchBendSt2Ramp
BEGIN_COMMAND(EhBh) {
	(void)AuditData;
	int32_t Value = (int32_t)Track->Bnd.Value + (((int32_t)Track_ReadByte(Track) - 128) << 7);
	if(Value < -(128<<7)  ) Value = -(128<<7);
	if(Value > +(128<<7)-1) Value = +(128<<7)-1;
	Value = Value<<1 | 1;
	int Result = Track_ReadController(MML, (union SGE_CtrlUnion_t*)(&Track->Bnd), Track, Value, 1);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While reading relative pitch-bend control command (Eh,Bh):");
	}
	return Result;
}

//! Eh,7h..Fh: Unallocated
UNALLOCATED_COMMAND(EhCh);
UNALLOCATED_COMMAND(EhDh);
UNALLOCATED_COMMAND(EhEh);
UNALLOCATED_COMMAND(EhFh);

//! Fh,0h: Transpose
BEGIN_COMMAND(Fh0h) {
	(void)MML;
	(void)AuditData;
	Track->Transpose = (int8_t)Track_ReadByte(Track);
	return MML_OK;
}

//! Fh,1h: TransposeAdd
BEGIN_COMMAND(Fh1h) {
	(void)AuditData;
	int Transpose = (int)Track->Transpose + ((Track_ReadByte(Track) ^ 0x80) - 0x80);
	if(Transpose < -128 || Transpose > +127) {
		MML_AppendErrorGlobal(MML, "Audit: Transpose-add overflow.");
		return MML_ERROR;
	}
	Track->Transpose = (int8_t)Transpose;
	return MML_OK;
}

//! Fh,2h: NoteDurationMulAdd
BEGIN_COMMAND(Fh2h) {
	(void)MML;
	(void)AuditData;

	int Mul = Track_ReadByte(Track), Add = 0;
	if((Mul & 1) != 0) Add = Track_ReadByte(Track);
	Track->NoteLengthMul = (uint8_t)(Mul >> 1);
	Track->NoteLengthAdd = ( int8_t)(Add);
	return MML_OK;
}

//! Fh,3h: Priority
BEGIN_COMMAND(Fh3h) {
	(void)MML;
	(void)AuditData;
	Track->Priority = (uint8_t)Track_ReadNybble(Track);
	return MML_OK;
}

//! Fh,4h: Program
BEGIN_COMMAND(Fh4h) {
	(void)MML;
	(void)AuditData;
	Track->Program = (uint8_t)Track_ReadByte(Track);
	return MML_OK;
}

//! Fh,5h: Tempo
BEGIN_COMMAND(Fh5h) {
	(void)AuditData;

	//! We don't actually care about the tempo controller here, so just parse and ignore
	int Result = Track_ReadController(MML, NULL, Track, Track_ReadMulti(Track, 3) + (1<<1), 1);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While reading tempo control command (Fh,5h):");
	}
	return Result;
}

//! Fh,6h: Jump
BEGIN_COMMAND(Fh6h) {
	(void)MML;
	(void)AuditData;
	int Target = Track_ReadSeekAddr(Track);
	Track_SeekNybbles(Track, Target);
	return MML_JUMP;
}

//! Fh,7h/8h/9h: Repeat/Call/CallWithCounter
BEGIN_COMMAND(_RepeatHandler, uint8_t nRepeatsMinusOne, uint8_t RepeatType) {
	(void)AuditData;
	uint8_t Depth = Track->StackDepth;

	//! If this is the end point for a repeat, exit now; repeats
	//! are handled via recursion.
	if(Depth > 0 && RepeatType == REPEAT_TYPE_REPEAT && (Track->RepeatType & 1) == REPEAT_TYPE_REPEAT) {
		//! Ensure that we're matching against the same repeat
		if((Track->NybbleStack & 1) == Track->NybbleSrc && Track->Stack[Depth-1] == Track->Src) {
			return MML_EOF;
		}
	}

	//! Keep repeating until the state "stabilizes" or we reach
	//! the final iteration. The latter case is used to handle
	//! cases that use the Break command.
	if(Depth >= MML_MAX_NESTING_LEVELS) {
		MML_AppendErrorGlobal(MML, "Audit: Maximum nesting level exceeded. Please report this error.");
		return MML_ERROR;
	}
	Track->RepeatType     = (Track->RepeatType  << 1) | RepeatType;
	Track->NybbleStack    = (Track->NybbleStack << 1) | Track->NybbleSrc;
	Track->Stack[Depth]   = Track->Src;
	Track->ReptCnt[Depth] = nRepeatsMinusOne;
	Track->StackDepth     = Depth + 1;
	for(;;) {
		//! Clone track state before the repeat
		struct SGE_Track_t CloneTrack;
		memcpy(&CloneTrack, Track, sizeof(CloneTrack));

		//! Seek to target offset and audit
		int32_t ReptTargetOffs = Track_ReadSeekAddr(Track);
		Track_SeekNybbles(Track, ReptTargetOffs);
		if(MML_AuditTrack(MML, Track) == MML_ERROR) {
			//MML_AppendErrorContext(MML, "Audit: While recursing repeat command:");
			return MML_ERROR;
		}

		//! Restore track offset and prepare for next iteration
		Track->Src = Track->Stack[Depth];
		Track->NybbleSrc = Track->NybbleStack & 1;

		//! If this was the last iteration, exit now
		if(Track->ReptCnt[Depth] == 0) {
			//! Skip target offset, pop stack and exit
			Track_ReadSeekAddr(Track);
			Track->RepeatType  >>= 1;
			Track->NybbleStack >>= 1;
			Track->StackDepth = Depth;
			return MML_OK;
		}

		//! If the state has stabilized, jump to the last iteration
		if(--Track->ReptCnt[Depth] > 0 && Track_StateMatch(Track, &CloneTrack)) {
			Track->ReptCnt[Depth] = 0;
		}
	}
}

//! Fh,7h: Repeat
BEGIN_COMMAND(Fh7h) {
	int Result = Track_Command_RepeatHandler(MML, Track, AuditData, Track_ReadByte(Track), REPEAT_TYPE_REPEAT);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While processing repeat command (Fh,7h):");
	}
	return Result;
}

//! Fh,8h: Call
BEGIN_COMMAND(Fh8h) {
	int Result = Track_Command_RepeatHandler(MML, Track, AuditData, 0, REPEAT_TYPE_PATTERN);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While processing call command (Fh,8h):");
	}
	return Result;
}

//! Fh,9h: CallWithCounter
BEGIN_COMMAND(Fh9h) {
	int Result = Track_Command_RepeatHandler(MML, Track, AuditData, Track_ReadByte(Track), REPEAT_TYPE_PATTERN);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While processing call-with-counter command (Fh,9h):");
	}
	return Result;
}

//! Fh,Ah: GotoIf
BEGIN_COMMAND(FhAh) {
	(void)AuditData;

	//! Skip payload
	int PayloadSize = Track_ReadNybble(Track);
	Track_SeekNybbles(Track, +PayloadSize);

	//! Read the target for the 'true' condition
	int TrueCondOffs = Track_ReadSeekAddr(Track);

	//! Clone the track and audit the 'false' path
	struct SGE_Track_t CloneTrack;
	memcpy(&CloneTrack, Track, sizeof(CloneTrack));
	if(MML_AuditTrack(MML, &CloneTrack) == MML_ERROR) {
		MML_AppendErrorContext(MML, "Audit: While processing goto-if command:");
		return MML_ERROR;
	}

	//! Now take the 'true' path and treat as a jump
	Track_SeekNybbles(Track, TrueCondOffs);
	return MML_JUMP;
}

//! Fh,Bh: Signal
BEGIN_COMMAND(FhBh) {
	(void)MML;
	(void)AuditData;

	//! Skip payload
	int PayloadSize = Track_ReadNybble(Track);
	Track_SeekNybbles(Track, +PayloadSize);
	return MML_OK;
}

//! Fh,Ch: Break
BEGIN_COMMAND(FhCh) {
	(void)MML;
	(void)AuditData;

	//! Read end-of-repeat offset
	int ReptEndOffs = Track_ReadSeekAddr(Track);

	//! Not currently nesting? Ignore the command
	uint8_t Depth = Track->StackDepth;
	if(Depth == 0) return MML_OK;

	//! If this Break was for an inline pattern or Repeat-Start, ignore it
	if(!Track->Stack[Depth-1]) {
		//! ... but make sure to pop the stack!
		Track->RepeatType  >>= 1;
		Track->NybbleStack >>= 1;
		Track->StackDepth = Depth-1;
		return MML_OK;
	}

	//! ReptCnt > 0: Not the last iteration - ignore the command
	if(Track->ReptCnt[Depth-1] > 0) return MML_OK;

	//! Repeats are handled via recursion, so just terminate now
	(void)ReptEndOffs;
	return MML_EOF;
}

//! Fh,Dh: Return
BEGIN_COMMAND(FhDh) {
	(void)MML;
	(void)AuditData;

	//! Not currently nesting? Ignore the command
	uint8_t Depth = Track->StackDepth;
	if(Depth == 0) return MML_OK;

	//! If we're not currently inside a pattern, just ignore the command
	if((Track->RepeatType & 1) != REPEAT_TYPE_PATTERN) return MML_OK;

	//! If this is an InlinePattern command, just pop the stack and ignore
	if(!Track->Stack[Depth-1]) {
		Track->RepeatType  >>= 1;
		Track->NybbleStack >>= 1;
		Track->StackDepth = Depth-1;
		return MML_OK;
	}

	//! Patterns are handled via recursion, so terminate now
	return MML_EOF;
}

//! Fh,Eh: InlinePattern
BEGIN_COMMAND(FhEh) {
	(void)AuditData;
	uint8_t Depth = Track->StackDepth;
	if(Depth >= MML_MAX_NESTING_LEVELS) {
		MML_AppendErrorGlobal(MML, "Audit: Maximum nesting level exceeded. Please report this error.");
		return MML_ERROR;
	}
	Track->RepeatType  = (Track->RepeatType  << 1) | REPEAT_TYPE_PATTERN;
	Track->NybbleStack = (Track->NybbleStack << 1) | 0;
	Track->Stack[Depth] = NULL;
	Track->StackDepth = Depth+1;
	return MML_OK;
}

//! Fh,Fh: End
BEGIN_COMMAND(FhFh) {
	(void)MML;
	(void)Track;
	(void)AuditData;
	return MML_EOF;
}

#undef BEGIN_COMMAND
/************************************************/

static int MML_AuditTrack(struct MML_t *MML, struct SGE_Track_t *Track) {
	//! Command handler table
	typedef int (*CmdFuncPtr_t)(struct MML_t *MML, struct SGE_Track_t *Track, struct TrackAuditData_t *AuditData);
	static const CmdFuncPtr_t CmdHandlerTable[] = {
		Track_Command0h, Track_Command1h, Track_Command2h, Track_Command3h,
		Track_Command4h, Track_Command5h, Track_Command6h, Track_Command7h,
		Track_Command8h, Track_Command9h, Track_CommandAh, Track_CommandBh,
		Track_CommandCh, Track_CommandDh,
	};
	static const CmdFuncPtr_t CmdEhHandlerTable[] = {
		Track_CommandEh0h, Track_CommandEh1h, Track_CommandEh2h, Track_CommandEh3h,
		Track_CommandEh4h, Track_CommandEh5h, Track_CommandEh6h, Track_CommandEh7h,
		Track_CommandEh8h, Track_CommandEh9h, Track_CommandEhAh, Track_CommandEhBh,
		Track_CommandEhCh, Track_CommandEhDh, Track_CommandEhEh, Track_CommandEhFh,
	};
	static const CmdFuncPtr_t CmdFhHandlerTable[] = {
		Track_CommandFh0h, Track_CommandFh1h, Track_CommandFh2h, Track_CommandFh3h,
		Track_CommandFh4h, Track_CommandFh5h, Track_CommandFh6h, Track_CommandFh7h,
		Track_CommandFh8h, Track_CommandFh9h, Track_CommandFhAh, Track_CommandFhBh,
		Track_CommandFhCh, Track_CommandFhDh, Track_CommandFhEh, Track_CommandFhFh,
	};

	//! Keep a cloned copy for Jump command handling
	struct TrackAuditData_t AuditData;
	memset(&AuditData, 0, sizeof(AuditData));
	for(;;) {
		//! Unpack command data and call appropriate handler
		int Result;
		int Command = Track_ReadNybble(Track);
		     if(Command <  0xE) Result = CmdHandlerTable  [Command](MML, Track, &AuditData);
		else if(Command == 0xE) Result = CmdEhHandlerTable[Track_ReadNybble(Track)](MML, Track, &AuditData);
		else                    Result = CmdFhHandlerTable[Track_ReadNybble(Track)](MML, Track, &AuditData);
		if(Result == MML_ERROR) return MML_ERROR;

		//! EOF is returned for End commands, as well as Break
		//! and Return commands, since the latter use recursion.
		if(Result == MML_EOF) return MML_OK;

		//! On commands that rest, update until rest is done
		if(Result == MML_REST) {
			//! NOTE: +1 because any command that rests immediately
			//! enters a 1-tick rest before counting down
			int32_t RestTime = (int32_t)Track->Rest + 1;
			Track->Rest = 0;
			Track_UpdateControllers(Track, RestTime);
		}

		//! Jump commands will keep processing until the iteration
		//! limit is reached, or the state stops mutating
		if(Result == MML_JUMP) {
			int Result = Track_CheckRedirection(MML, Track, &AuditData);
			if(Result == MML_ERROR) return MML_ERROR;
			if(Result == MML_EOF)   return MML_OK;
		}
	}
}

/************************************************/

//! Audit track data to cull any unused regions from the tone bank
int MML_Audit(struct MML_t *MML) {
	uint32_t TrackIdx;
	for(TrackIdx=0;TrackIdx<MML->nTracks;TrackIdx++) {
		const uint8_t *DataPtr = MML->Output.Data + MML->TracksList[TrackIdx].DataOffs;

		struct SGE_Track_t TrackState;
		Track_Init(&TrackState, DataPtr);
		if(MML_AuditTrack(MML, &TrackState) == MML_ERROR) {
			const char *TrackName = MML->TracksList[TrackIdx].Name;
			if(TrackName) {
				MML_vAppendErrorContext(MML, "Audit: While parsing track %u (%s):", TrackIdx+1, TrackName);
			} else {
				MML_vAppendErrorContext(MML, "Audit: While parsing track %u:", TrackIdx);
			}
			return MML_ERROR;
		}
	}
	return MML_OK;
}

/************************************************/
//! EOF
/************************************************/
