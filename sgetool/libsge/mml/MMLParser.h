/************************************************/
#pragma once
/************************************************/
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
/************************************************/
#include "MML.h"
/************************************************/

#define NOTESTACK_IS_NOTE(x) ((x) < 12)
#define NOTESTACK_OCTAVE_UP   0x0C
#define NOTESTACK_OCTAVE_DOWN 0x0D
#define NOTESTACK_OCTAVE_SET  0x0E //! 0Eh..18h
struct MML_Command_NoteStack_Entry_t {
	uint8_t  Key:7;
	uint8_t  VelOverride:1;
	uint8_t  Velocity; //! Bit0 = Sticky
};
struct MML_Command_NoteStack_t {
	uint8_t  Size;
	uint8_t  Overlay;
	uint16_t Duration;
	struct MML_Command_NoteStack_Entry_t Notes[MML_MAXIMUM_NOTESTACK_SIZE];
};

/************************************************/

static int MML_WriteCommand(struct MML_t *MML, uint32_t Command) {
	MML->State.LastCommand = Command;
	return MML_WriteCommandData(MML, Command);
}

static int MML_WritePatternRecall(
	struct MML_t *MML,
	char *LabelName,
	uint32_t LabelIdx,
	uint8_t SelfRef,
	const struct MML_InputOffs_t *LabelNameOffs
) {
	//! Have a repeat count?
	int32_t nRepeats = 1;
	int NextChar = MML_PeekNextChar(MML);
	if(MML_IS_DIGIT(NextChar)) {
		//! Get the number of repeats
		struct MML_InputOffs_t nRepeatsOffs; MML_GetInputOffset(MML, &nRepeatsOffs);
		nRepeats = MML_ReadDecimalOrHex(MML);
		if(nRepeats == MML_ERROR) {
			MML_AppendErrorContext(MML, "While parsing number of pattern recall repeats:");
			return MML_ERROR;
		}
		if(nRepeats == 0) {
			MML_AppendError(MML, "Invalid number of repeats.", &nRepeatsOffs);
			return MML_ERROR;
		}
		if(SelfRef) nRepeats -= 1;
		if(nRepeats > 256) {
			MML_AppendError(MML, "Too many repeats for pattern recall.", &nRepeatsOffs);
			return MML_ERROR;
		}
	}

	//! Decide which command to use based on repeat count
	if(nRepeats == 1) {
		if(MML_WriteCommand(MML, MML_CMD_CALL) == MML_ERROR) return MML_ERROR;
	} else {
		if(
			MML_WriteCommand(MML, MML_CMD_CALLCNT)       == MML_ERROR ||
			MML_WriteByte   (MML, (uint8_t)(nRepeats-1)) == MML_ERROR
		) return MML_ERROR;
	}

	//! Add reference to list and reset running state (may be inconsistent with the pattern end)
	if(MML_CreateReference(MML, LabelName, LabelIdx, MML_REFERENCE_CMDTYPE_PATTERN, SelfRef, LabelNameOffs) == MML_ERROR) return MML_ERROR;
	MML_ResetRunningState(MML);
	return MML_OK;
}

static int MML_WriteNoteStack(struct MML_t *MML, const struct MML_Command_NoteStack_t *Stack, uint32_t nNotes) {
	//! Determine the command to use for this stack
	uint32_t Command;
	uint32_t Duration = (uint32_t)Stack->Duration + 1;
	switch(Duration) {
		case MML_TICKS_PER_WHOLE_NOTE /  4: Command = MML_CMD_NOTE_1_4;  break;
		case MML_TICKS_PER_WHOLE_NOTE /  8: Command = MML_CMD_NOTE_1_8;  break;
		case MML_TICKS_PER_WHOLE_NOTE / 16: Command = MML_CMD_NOTE_1_16; break;
		case MML_TICKS_PER_WHOLE_NOTE / 32: Command = MML_CMD_NOTE_1_32; break;
		case MML_TICKS_PER_WHOLE_NOTE / 64: Command = MML_CMD_NOTE_1_64; break;
		default: {
			if(Duration == MML->State.OutputDuration) {
				Command = MML_CMD_NOTE_LASTRUN;
			} else {
				Command = MML_CMD_NOTE_TIMED;
			}
		} break;
	}

	//! Write command
	if(MML_WriteCommand(MML, Command) == MML_ERROR) return MML_ERROR;
	if(Command == MML_CMD_NOTE_TIMED) {
		if(MML_WriteTimeCode(MML, Duration) == MML_ERROR) return MML_ERROR;
		MML->State.OutputDuration = Duration;
	}
	if(Stack->Overlay) {
		if(MML_WriteNybble(MML, MML_CMD_NOTE_OVERLAY) == MML_ERROR) {
			return MML_ERROR;
		}
	}

	//! Dump all notes/commands for this stack
	uint32_t OutputStackSize = 1;
	uint32_t nNotesRem = nNotes;
	const struct MML_Command_NoteStack_Entry_t *CurEntry = Stack->Notes;
	const struct MML_Command_NoteStack_Entry_t *EndEntry = Stack->Notes + Stack->Size;
	do {
		//! Have any notes remaining?
		if(nNotesRem == 0) {
			MML_AppendErrorCurrentOffset(MML, "Note stack commands exist after last note. Please report this error.\n");
			return MML_ERROR;
		}

		//! Handle sub-commands
		while(!NOTESTACK_IS_NOTE(CurEntry->Key)) {
			int Command = CurEntry->Key;
			if(Command == NOTESTACK_OCTAVE_DOWN) {
				Command = MML_CMD_NOTE_OCTAVE_DOWN;
			} else if(Command == NOTESTACK_OCTAVE_UP) {
				Command = MML_CMD_NOTE_OCTAVE_UP;
			} else if(Command >= NOTESTACK_OCTAVE_SET+0 && Command <= NOTESTACK_OCTAVE_SET+10) {
				Command = MML_CMD_NOTE_OCTAVE_SET + (Command - NOTESTACK_OCTAVE_SET);
			} else {
				MML_AppendErrorCurrentOffset(MML, "Unknown command in note stack. Please report this error.\n");
				return MML_ERROR;
			}
			if(MML_WriteCommandData(MML, Command) == MML_ERROR) {
				return MML_ERROR;
			}
			CurEntry++;
		}

		//! Need to increase note stack size?
		if(OutputStackSize == 1 && nNotesRem > 1) {
			//! Issue a stack-push command
			uint32_t IncreaseCount = (nNotesRem-1 < 4) ? (nNotesRem-1) : 4;
			if(MML_WriteCommandData(MML, MML_CMD_NOTE_STACKPUSH + IncreaseCount-1) == MML_ERROR) {
				return MML_ERROR;
			}
			OutputStackSize += IncreaseCount;
		}

		//! Check for velocity override
		if(CurEntry->VelOverride) {
			//! Issue a velocity change command
			if(
				MML_WriteCommandData(MML, MML_CMD_NOTE_VELOCITY) == MML_ERROR ||
				MML_WriteByte       (MML, CurEntry->Velocity) == MML_ERROR
			) return MML_ERROR;
		}

		//! Finally write out the note and move to the next
		if(MML_WriteNybble(MML, CurEntry->Key) == MML_ERROR) {
			return MML_ERROR;
		}
		OutputStackSize--;
		nNotesRem--;
	} while(++CurEntry < EndEntry);

	//! All done
	return MML_OK;
}

/************************************************/

static int MML_FlushPendingCommands(struct MML_t *MML, int SkipSingleOctaveDelta) {
	//! Do we have an octave change?
	if(MML->State.QueuedOctaveSet != 0xFF) {
		if(
			MML_WriteCommand(MML, MML_CMD_OCTAVE)             == MML_ERROR ||
			MML_WriteNybble (MML, MML->State.QueuedOctaveSet) == MML_ERROR
		) return MML_ERROR;
		MML->State.QueuedOctaveSet = 0xFF;
	} else if(MML->State.QueuedOctaveChange) {
		int Delta = MML->State.QueuedOctaveChange;
#define WRITE_COMMAND(Command) \
	if( \
		MML_WriteCommand (MML, MML_CMD_OCTAVE) == MML_ERROR || \
		MML_WriteNybble  (MML, Command)        == MML_ERROR    \
	) return MML_ERROR
		while(Delta >= +3) {
			int t = Delta-3; if(t > +7) t = +7;
			WRITE_COMMAND(MML_CMD_OCTAVE_RELATIVE);
			if(MML_WriteNybble(MML, (uint8_t)t) == MML_ERROR) return MML_ERROR;
			Delta -= t+3;
		}
		while(Delta <= -3) {
			int t = Delta+2; if(t < -8) t = -8;
			WRITE_COMMAND(MML_CMD_OCTAVE_RELATIVE);
			if(MML_WriteNybble(MML, (uint8_t)t) == MML_ERROR) return MML_ERROR;
			Delta -= t-2;
		}
		if(Delta >= +2) {
			WRITE_COMMAND(MML_CMD_OCTAVE_UP2);
			Delta -= 2;
		}
		if(Delta <= -2) {
			WRITE_COMMAND(MML_CMD_OCTAVE_DOWN2);
			Delta += 2;
		}
		if(!SkipSingleOctaveDelta) {
			if(Delta >= +1) {
				WRITE_COMMAND(MML_CMD_OCTAVE_UP);
				Delta -= 1;
			}
			if(Delta <= -1) {
				WRITE_COMMAND(MML_CMD_OCTAVE_DOWN);
				Delta += 1;
			}
		}
#undef WRITE_COMMAND
		MML->State.QueuedOctaveChange = Delta;
	}
	return MML_OK;
}

static int MML_FinalizeTrack(struct MML_t *MML) {
	if(MML->nTracks) {
		//! Sanity check
		if(MML->State.NestLevel_Pattern > 0) {
			struct MML_Label_t *Label = MML->LabelsList + MML->nLabels;
			for(;;) {
				Label--;
				if(Label->Type == MML_LABEL_TYPE_PATTERN) break;
			}
			MML_AppendError(MML, "Unterminated pattern definition.", &Label->InputOffs);
			return MML_ERROR;
		}
		if(MML->State.NestLevel_Repeat > 0) {
			struct MML_Label_t *Label = MML->LabelsList + MML->nLabels;
			for(;;) {
				Label--;
				if(Label->Type == MML_LABEL_TYPE_REPEAT) break;
			}
			MML_AppendError(MML, "Unterminated repeat definition.", &Label->InputOffs);
			return MML_ERROR;
		}
		if(MML->State.IsTripletMode) {
			MML_AppendError(MML, "Unterminated triplets section.", &MML->State.TripletModeOffs);
			return MML_ERROR;
		}

		//! Append final END command
		uint32_t LastCmd = MML->State.LastCommand;
		if(LastCmd != MML_CMD_JUMP && LastCmd != MML_CMD_END) {
			if(MML_WriteCommand(MML, MML_CMD_END) == MML_ERROR) return MML_ERROR;
		}

		//! Ensure track is aligned to bytes
		if((MML->Output.Offs % 2) != 0 && MML_WriteNybble(MML, 0) == MML_ERROR) return MML_ERROR;

		//! Store track to list
		MML_StoreLastTrack(MML);
	}
	return MML_OK;
}

/************************************************/

//! Write controller command to output
static int MML_ParseController_WriteCommand(struct MML_t *MML, uint8_t Command, int nBits, int BiasedValue) {
	if(Command == MML_CMD_PITCHBEND && (BiasedValue & (0x7F<<1)) == 0) {
		//! PitchBend without fine tuning
		int St = (BiasedValue >> 8) - 128;
		if(St >= -8 && St <= +7) {
			return MML_ParseController_WriteCommand(
				MML,
				MML_CMD_PITCHBENDST1 + (BiasedValue & 1),
				4,
				St + 8
			);
		} else {
			return MML_ParseController_WriteCommand(
				MML,
				MML_CMD_PITCHBENDST2 + (BiasedValue & 1),
				8,
				St + 128
			);
		}
	}
#define DO_BITS_AT(x) \
	if(nBits > (x) && MML_WriteNybble(MML, (uint8_t)(BiasedValue >> (x)) & 0xF) == MML_ERROR) return MML_ERROR
	if(MML_WriteCommand(MML, Command) == MML_ERROR) return MML_ERROR;
	DO_BITS_AT(12);
	DO_BITS_AT( 8);
	DO_BITS_AT( 4);
	DO_BITS_AT( 0);
#undef DO_BITS_AT
	return MML_OK;
}

//! Parse controller value, and return an unbiased value
//! Min/Max are the ranges of the unbiased value, and Scale is applied to
//! a floating-point input when converting it to the integer value.
static int MML_ParseController_ParseValue(struct MML_t *MML, int Min, int Max, int ValueMul, int ValueDiv, double Scale) {
	struct MML_InputOffs_t ValueOffs; MML_GetInputOffset(MML, &ValueOffs);

	//! Read value as raw or standard
	int32_t Value;
	if(MML_PeekNextChar(MML) == '#') {
		MML_ConsumeChars(MML, 1, 0);

		int Sign = MML_ReadSign(MML);
		Value = MML_ReadDecimalOrHex(MML);
		if(Value == MML_ERROR) {
			MML_AppendErrorContext(MML, "While parsing raw controller value:");
			return MML_ERROR;
		}
		Value = MML_ApplySign(Value, Sign);
	} else {
		double fValue = MML_ReadDouble(MML);
		if(isnan(fValue)) {
			MML_AppendErrorContext(MML, "While parsing controller value:");
			return MML_ERROR;
		}
		Value = (int)lrint(fValue*Scale);
	}

	//! Apply scaling
	Value = Value * ValueMul / ValueDiv;

	//! Check range for value
	if(Value < Min || Value > Max) {
		MML_AppendError(MML, "Controller value out of range.", &ValueOffs);
		return MML_ERROR;
	}

	//! Return the biased value
	return Value - Min;
}

//! Parse controller (specified as either float value or #Raw)
static int MML_ParseController(
	struct MML_t *MML,
	uint8_t Command,
	int nBits,
	int Min,
	int Max,
	double Scale,
	int ValueMul,
	int ValueDiv
) {
	//! Check for sweep without immediate
	if(!MML_PeekStringMatch(MML, "->", 0)) {
		//! If we don't match `->`, then we need to parse the immediate value
		int Value = MML_ParseController_ParseValue(MML, Min, Max, ValueMul, ValueDiv, Scale);
		if(Value == MML_ERROR) return MML_ERROR;
		if(MML_ParseController_WriteCommand(MML, Command, nBits, Value << 1 | 0) == MML_ERROR) return MML_ERROR;
	}

	//! If we have a sweep, apply that next
	if(MML_PeekStringMatch(MML, "->", 0)) {
		MML_ConsumeChars(MML, strlen("->"), 0);

		//! Read the target value
		int Value = MML_ParseController_ParseValue(MML, Min, Max, ValueMul, ValueDiv, Scale);
		if(Value == MML_ERROR) return MML_ERROR;

		//! Ensure duration is preceded by '='
		if(MML_PeekNextChar(MML) != '=') {
			MML_AppendErrorCurrentOffset(MML, "Expected `=` while parsing controller sweep duration.");
			return MML_ERROR;
		}
		MML_ConsumeChars(MML, 1, 0);

		//! Read the duration
		struct MML_InputOffs_t DurationOffs; MML_GetInputOffset(MML, &DurationOffs);
		int Duration = MML_ReadDuration(MML);
		if(Duration == MML_ERROR) {
			MML_AppendErrorContext(MML, "While parsing sweep duration:");
			return MML_ERROR;
		}
		if(Duration > 256) {
			MML_AppendError(MML, "Controller sweep duration must be 256 ticks at most.", &DurationOffs);
			return MML_ERROR;
		}

		//! Output the command
		if(
			MML_ParseController_WriteCommand(MML, Command, nBits, Value << 1 | 1) == MML_ERROR ||
			MML_WriteTimeCode(MML, Duration) == MML_ERROR
		) return MML_ERROR;
	}

	//! All done
	return MML_OK;
}

/************************************************/
//! EOF
/************************************************/
