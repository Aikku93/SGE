/************************************************/
#pragma once
/************************************************/
#include <stdint.h>
#include <string.h>
/************************************************/
#include "MML.h"
#include "MMLParser.h"
/************************************************/

static int MML_Command_OctaveChange(struct MML_t *MML, int NegativeDelta) {
	struct MML_InputOffs_t CmdOffs; MML_GetInputOffset(MML, &CmdOffs);
	CmdOffs.DataOffs -= 1; //! Assume we entered after encountering '<'/'>'

	//! Get the delta change amount
	int32_t Delta = 1; {
		int Next = MML_PeekNextChar(MML);
		if(MML_IS_DIGIT(Next)) {
			struct MML_InputOffs_t DeltaOffs; MML_GetInputOffset(MML, &DeltaOffs);
			Delta = MML_ReadDecimalOrHex(MML);
			if(Delta == MML_ERROR) {
				MML_AppendErrorContext(MML, "While parsing octave delta:");
				return MML_ERROR;
			}
			if(Delta > 10) {
				MML_AppendError(MML, "Octave delta out of range (must be -10..+10).", &DeltaOffs);
				return MML_ERROR;
			}
		}
	}
	if(NegativeDelta) Delta = -Delta;

	//! If we don't have an octave-set command queued, then use
	//! a relative change. Otherwise, update the 'set' command.
	if(MML->State.QueuedOctaveSet == 0xFF) {
		MML->State.QueuedOctaveChange += Delta;
	} else {
		int Target = (int)MML->State.QueuedOctaveSet + Delta;
		if(Target < 0 || Target > 10) {
			MML_AppendError(MML, "Octave out of bounds (must be 0..10).", &CmdOffs);
			return MML_ERROR;
		} else MML->State.QueuedOctaveSet = (uint8_t)Target;
	}
	return MML_OK;
}
static int MML_Command_OctaveUp(struct MML_t *MML) {
	return MML_Command_OctaveChange(MML, 0);
}
static int MML_Command_OctaveDown(struct MML_t *MML) {
	return MML_Command_OctaveChange(MML, 1);
}

/************************************************/

static int MML_Command_OctaveSet(struct MML_t *MML) {
	struct MML_InputOffs_t OctaveValueOffs; MML_GetInputOffset(MML, &OctaveValueOffs);
	int32_t Octave = MML_ReadDecimalOrHex(MML);
	if(Octave == MML_ERROR) {
		MML_AppendErrorContext(MML, "While parsing octave target:");
		return MML_ERROR;
	}
	if(Octave > 10) {
		MML_AppendError(MML, "Octave out of bounds (must be 0..10).", &OctaveValueOffs);
		return MML_ERROR;
	}
	MML->State.QueuedOctaveChange = 0, MML->State.QueuedOctaveSet = Octave;
	return MML_OK;
}

/************************************************/

static int MML_Command_Note_TranslateNoteLetterToKey(struct MML_t *MML, int NoteLetter, int *OctaveDeltaPtr) {
	struct MML_InputOffs_t NoteOffs; MML_GetInputOffset(MML, &NoteOffs);
	NoteOffs.DataOffs -= 1;

	//! Translate letter to key and check for accidentals
	int Key;
	switch(NoteLetter) {
		case 'c': Key =  0; break;
		case 'd': Key =  2; break;
		case 'e': Key =  4; break;
		case 'f': Key =  5; break;
		case 'g': Key =  7; break;
		case 'a': Key =  9; break;
		case 'b': Key = 11; break;
		default: {
			MML_AppendError(MML, "Expected a note letter.", &NoteOffs);
		} return MML_ERROR;
	}
	while(MML_PeekNextChar(MML) == '+') { Key++; MML_ConsumeChars(MML, 1, 0); }
	while(MML_PeekNextChar(MML) == '-') { Key--; MML_ConsumeChars(MML, 1, 0); }

	//! Update octave as needed
	int OctaveDelta = 0;
	while(Key < 0) {
		if(MML_Command_OctaveDown(MML) == MML_ERROR) return MML_ERROR;
		OctaveDelta--;
		Key += 12;
	}
	while(Key >= 12) {
		if(MML_Command_OctaveUp(MML) == MML_ERROR) return MML_ERROR;
		OctaveDelta++;
		Key -= 12;
	}
	*OctaveDeltaPtr = OctaveDelta;
	return Key;
}
static int MML_Command_Note_ParseVelocityOverride(struct MML_t *MML) {
	//! 0 = No override. Otherwise: Bit0 = Sticky, Bit1... = Velocity.
	//! To write to output, the bias needs to be applied (ie. Velocity -= 1<<1).
	int32_t Velocity = 0;
	if(MML_PeekNextChar(MML) == '=') {
		MML_ConsumeChars(MML, 1, 0);

		//! Read target velocity
		struct MML_InputOffs_t VelocityOffs; MML_GetInputOffset(MML, &VelocityOffs);
		Velocity = MML_ReadDecimalOrHex(MML);
		if(Velocity == MML_ERROR) {
			MML_AppendErrorContext(MML, "While parsing note velocity override:");
			return MML_ERROR;
		}
		if(Velocity < 1 || Velocity > 128) {
			MML_AppendError(MML, "Immediate velocity out of range (must be 1..128).", &VelocityOffs);
			return MML_ERROR;
		}

		//! Shift up and combine with Sticky bit
		Velocity <<= 1;
		if(MML_PeekNextChar(MML) == '!') {
			MML_ConsumeChars(MML, 1, 0);
			Velocity |= 1;
		}
	}
	return Velocity;
}
static int MML_Command_Note_ReadNoteToStack(
	struct MML_t *MML,
	struct MML_Command_NoteStack_t *Stack,
	int NoteLetter,
	int AllowVelocityOverride
) {
	//! Get key from letter
	int OctaveDelta;
	int Key = MML_Command_Note_TranslateNoteLetterToKey(MML, NoteLetter, &OctaveDelta);
	if(Key == MML_ERROR) {
		MML_AppendErrorContext(MML, "While parsing note:");
		return MML_ERROR;
	}

	//! Using a for loop like this is kinda janky...
	for(;;) {
		//! Check number of notes in stack
		if(Stack->Size >= MML_MAXIMUM_NOTESTACK_SIZE) {
			MML_AppendErrorCurrentOffset(MML, "Maximum note stack size exceeded.");
			return MML_ERROR;
		}

		//! Check for octave changes
		if(MML->State.QueuedOctaveSet != 0xFF) {
			int Target = MML->State.QueuedOctaveSet;
			Target += MML->State.QueuedOctaveChange;
			if(Target < 0 || Target > 10) {
				MML_AppendErrorCurrentOffset(MML, "Octave out of bounds (must be 0..10).");
				return MML_ERROR;
			}
			Stack->Notes[Stack->Size].Key = NOTESTACK_OCTAVE_SET + Target;
			MML->State.QueuedOctaveSet    = 0xFF;
			MML->State.QueuedOctaveChange = 0;
			Stack->Size++;
		} else if(MML->State.QueuedOctaveChange) {
			if(MML->State.QueuedOctaveChange >= +1) {
				Stack->Notes[Stack->Size].Key = NOTESTACK_OCTAVE_UP;
				MML->State.QueuedOctaveChange--;
			} else if(MML->State.QueuedOctaveChange <= -1) {
				Stack->Notes[Stack->Size].Key = NOTESTACK_OCTAVE_DOWN;
				MML->State.QueuedOctaveChange++;
			}
			Stack->Size++;
		} else break;
	}

	//! Check for velocity override
	int32_t Velocity = 0;
	if(AllowVelocityOverride) {
		Velocity = MML_Command_Note_ParseVelocityOverride(MML);
		if(Velocity == MML_ERROR) return MML_ERROR;
	}

	//! Place note in stack
	Stack->Notes[Stack->Size].Key         = Key;
	Stack->Notes[Stack->Size].VelOverride = (Velocity == 0) ? 0 : 1;
	Stack->Notes[Stack->Size].Velocity    = (uint8_t)(Velocity - (1<<1));
	Stack->Size++;

	//! Restore octave as needed
	while(OctaveDelta <= -1) {
		if(MML_Command_OctaveUp(MML) == MML_ERROR) return MML_ERROR;
		OctaveDelta++;
	}
	while(OctaveDelta >= +1) {
		if(MML_Command_OctaveDown(MML) == MML_ERROR) return MML_ERROR;
		OctaveDelta--;
	}
	return MML_OK;
}
static int MML_Command_Note(struct MML_t *MML, int NoteLetter) {
	//! We either had `a`..`g` as input, or `{` for a stack; either way,
	//! it's always just a single character that triggers this command
	struct MML_InputOffs_t CommandOffs; MML_GetInputOffset(MML, &CommandOffs);
	CommandOffs.DataOffs -= 1;

	//! If we have an octave delta >= +/-2, it's cheaper to use a raw command.
	//! Note that this will /not/ flush a delta of +/-1, because /this/ is cheaper to
	//! embed as part of the note command.
	if(MML->State.QueuedOctaveChange <= -2 || MML->State.QueuedOctaveChange >= +2) {
		if(MML_FlushPendingCommands(MML, 1) == MML_ERROR) return MML_ERROR;
	}

	//! If we have a single note, just parse it now
	//! NOTE: If we got a stack command, then NoteLetter will be 0
	int nNotes = 0;
	struct MML_Command_NoteStack_t Stack;
	Stack.Size    = 0;
	Stack.Overlay = 0;
	if(NoteLetter) {
		if(MML_Command_Note_ReadNoteToStack(MML, &Stack, NoteLetter, 0) == MML_ERROR) {
			MML_AppendErrorContext(MML, "While parsing note command:");
			return MML_ERROR;
		}
		nNotes++;
	} else for(;;) {
		//! Keep going until we get a stack end `}` command
		//! NOTE: Allow whitespace in between commands
		MML_ConsumeWhitespace(MML);
		int Next = MML_PeekNextChar(MML);
		if(Next == '}') {
			MML_ConsumeChars(MML, 1, 0);
			break;
		}

		//! Check the command type
		switch(Next) {
			case 'a': /* Fall through */
			case 'b': /* Fall through */
			case 'c': /* Fall through */
			case 'd': /* Fall through */
			case 'e': /* Fall through */
			case 'f': /* Fall through */
			case 'g': {
				MML_ConsumeChars(MML, 1, 0);
				if(MML_Command_Note_ReadNoteToStack(MML, &Stack, Next, 1) == MML_ERROR) {
					MML_AppendErrorContext(MML, "While parsing stacked note:");
					return MML_ERROR;
				}
				nNotes++;
			} break;
			case '<': {
				MML_ConsumeChars(MML, 1, 0);
				if(MML_Command_OctaveDown(MML) == MML_ERROR) {
					MML_AppendErrorContext(MML, "While parsing stacked octave-down:");
					return MML_ERROR;
				}
			} break;
			case '>': {
				MML_ConsumeChars(MML, 1, 0);
				if(MML_Command_OctaveUp(MML) == MML_ERROR) {
					MML_AppendErrorContext(MML, "While parsing stacked octave-up:");
					return MML_ERROR;
				}
			} break;
			case 'o': {
				MML_ConsumeChars(MML, 1, 0);
				if(MML_Command_OctaveSet(MML) == MML_ERROR) {
					MML_AppendErrorContext(MML, "While parsing stacked octave-set:");
					return MML_ERROR;
				}
			} break;
			default: {
				MML_AppendErrorCurrentOffset(MML, "Expected a note command inside note stack.");
				return MML_ERROR;
			} break;
		}
	}

	//! Did we get any notes?
	if(nNotes == 0) {
		MML_AppendError(MML, "No notes present in stack.", &CommandOffs);
		MML_AppendErrorContext(MML, "While parsing note stack:");
		return MML_ERROR;
	}

	//! Parse duration
	int Duration = MML_ReadDuration(MML);
	if(Duration == MML_ERROR) {
		MML_AppendErrorContext(MML, "While parsing note duration:");
		return MML_ERROR;
	}
	Stack.Duration = (uint16_t)(Duration - 1);

	//! Check for a single-note velocity override
	//! This is a special case needed to make the syntax less awkward.
	//! With this hack, we can write:
	//!  a16=50
	//! Whereas without it, we would need to write something like:
	//!  a=50 16
	//! which would come up as invalid due to the whitespace being
	//! treated as the end of the note command.
	if(NoteLetter) {
		//! NOTE: We have to index at Size-1 rather than just assume
		//! that the note is in slot 0, because we might have octave
		//! change commands /before/ the note itself.
		int32_t Velocity = MML_Command_Note_ParseVelocityOverride(MML);
		if(Velocity == MML_ERROR) return MML_ERROR;
		Stack.Notes[Stack.Size-1].VelOverride = (Velocity == 0) ? 0 : 1;
		Stack.Notes[Stack.Size-1].Velocity    = (uint8_t)(Velocity - (1<<1));
	}

	//! Is this stack an overlay?
	if(MML_PeekNextChar(MML) == '_') {
		MML_ConsumeChars(MML, 1, 0);
		Stack.Overlay = 1;
	}

	//! Write the note stack
	if(MML_WriteNoteStack(MML, &Stack, nNotes) == MML_ERROR) return MML_ERROR;
	return MML_OK;
}

/************************************************/

static int MML_Command_Rest(struct MML_t *MML) {
	//! Get rest duration
	int32_t Duration = MML_ReadDuration(MML);
	if(Duration == MML_ERROR) {
		MML_AppendErrorContext(MML, "While parsing rest duration:");
		return MML_ERROR;
	}

	//! Output command sequence
	if(
		MML_WriteCommand (MML, MML_CMD_REST)       == MML_ERROR ||
		MML_WriteTimeCode(MML, (uint32_t)Duration) == MML_ERROR
	) return MML_ERROR;
	return MML_OK;
}

/************************************************/

static int MML_Command_Velocity(struct MML_t *MML) {
	int Result = MML_ParseController(MML, MML_CMD_VELOCITY, 8, 1, 128, 1.0, 1, 1);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "While parsing velocity controller:");
	}
	return Result;
}

/************************************************/

static int MML_Command_Volume(struct MML_t *MML) {
	int Result = MML_ParseController(MML, MML_CMD_VOLUME, 8, 1, 128, 1.0, 1, 1);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "While parsing volume controller:");
	}
	return Result;
}

/************************************************/

static int MML_Command_Expression(struct MML_t *MML) {
	int Result = MML_ParseController(MML, MML_CMD_EXPRESSION, 8, 1, 128, 1.0, 1, 1);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "While parsing expression controller:");
	}
	return Result;
}

/************************************************/

static int MML_Command_Panning(struct MML_t *MML) {
	int Result = MML_ParseController(MML, MML_CMD_PANNING, 8, -63, +63, 1.0, 1, 1);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "While parsing panning controller:");
	}
	return Result;
}

/************************************************/

static int MML_Command_PitchBend(struct MML_t *MML) {
	int Result = MML_ParseController(MML, MML_CMD_PITCHBEND, 16, -(128<<7), +(128<<7)-1, 128.0, 1, 1);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "While parsing pitch-bend controller:");
	}
	return Result;
}

/************************************************/

static int MML_Command_Tempo(struct MML_t *MML) {
	int Result = MML_ParseController(MML, MML_CMD_TEMPO, 12, 1, 1024, 1.0, MML->TicksPerBeat, MML_TICKS_PER_QUARTER_NOTE);
	if(Result == MML_ERROR) {
		MML_AppendErrorContext(MML, "While parsing tempo controller:");
	}
	return Result;
}

/************************************************/

static int MML_Command_NoteMul(struct MML_t *MML) {
	int Next;
	struct MML_InputOffs_t CommandOffs; MML_GetInputOffset(MML, &CommandOffs);

	//! Default to x*1.0+0.0
	int32_t Numerator = 8, Denominator = 8, Adder = 0;

	//! Check for a numerator/denominator pair
	Next = MML_PeekNextChar(MML);
	if(MML_IS_DIGIT(Next)) {
		Numerator = MML_ReadDecimalOrHex(MML);
		if(Numerator == MML_ERROR) {
			MML_AppendErrorContext(MML, "While parsing note duration multiplier:");
			return MML_ERROR;
		}

		//! Now check for a denominator
		Next = MML_PeekNextChar(MML);
		if(Next == '/') {
			MML_ConsumeChars(MML, 1, 0);

			//! Read value, and ensure the denominator is valid
			struct MML_InputOffs_t DenominatorOffs; MML_GetInputOffset(MML, &DenominatorOffs);
			Denominator = MML_ReadDecimalOrHex(MML);
			if(Denominator == MML_ERROR) {
				MML_AppendErrorContext(MML, "While parsing note duration divider:");
				return MML_ERROR;
			}
			if(!MML_IS_POWER_OF_2(Denominator)) {
				MML_AppendError(MML, "Note duration multiplier denominator must be a power of 2.", &DenominatorOffs);
				return MML_ERROR;
			}
		}
	}

	//! Now check for an adder
	Next = MML_PeekNextChar(MML);
	if(Next == '+' || Next == '-') {
		struct MML_InputOffs_t AdderOffs; MML_GetInputOffset(MML, &AdderOffs);
		int32_t Sign = MML_ReadSign(MML);

		//! Read duration code
		int32_t Duration = MML_ReadDuration(MML);
		if(Duration == MML_ERROR) {
			MML_AppendErrorContext(MML, "While parsing note duration accumulator:");
			return MML_ERROR;
		}

		//! Combine duration with sign and ensure it's valid
		Adder = MML_ApplySign(Duration, Sign);
		if(Adder < -128 || Adder > +127) {
			MML_AppendError(MML, "Note duration adder must be between -128 and +127 ticks.", &AdderOffs);
			return MML_ERROR;
		}
	}

	//! Adjust for the driver resolution of the multiplier
	int64_t NoteMul64 = Numerator * (int64_t)MML_NOTEMUL_RESOLUTION;
	if(NoteMul64 % Denominator != 0) {
		MML_AppendError(MML, "Note duration multiplier became quantized.", &CommandOffs);
		return MML_ERROR;
	}
	int32_t NoteMul32 = (int32_t)(NoteMul64 / Denominator);
	if(NoteMul32 > 0x7F) {
		MML_AppendError(MML, "Note duration multiplier is too large.", &CommandOffs);
		return MML_ERROR;
	}

	//! Output command sequence
	uint8_t NoteMul = (uint8_t)NoteMul32;
	NoteMul = (NoteMul*2) + ((Adder != 0) ? 1 : 0);
	if(
		MML_WriteCommand(MML, MML_CMD_NOTEMULADD) == MML_ERROR ||
		MML_WriteByte   (MML, NoteMul)            == MML_ERROR ||
		(Adder != 0 && MML_WriteByte(MML, (uint8_t)Adder) == MML_ERROR)
	) return MML_ERROR;
	return MML_OK;
}

/************************************************/

static int MML_Command_Program(struct MML_t *MML) {
	struct MML_InputOffs_t ProgramOffs;
	MML_GetInputOffset(MML, &ProgramOffs);

	//! Translate MIDI program
	int32_t Program; {
		uint8_t Data[3];
		uint8_t IsDrumKit = 0;

		//! Parse CC0:CC32:Patch or CC0:[000:]:Patch or [000:000:]Patch
		uint8_t nValsRead = 0;
		for(;;) {
			//! Read patch/bank
			struct MML_InputOffs_t CurrentValueOffs; MML_GetInputOffset(MML, &CurrentValueOffs);
			int32_t x = MML_ReadDecimalOrHex(MML);
			if(x == MML_ERROR) {
				MML_AppendErrorContext(MML, "While parsing MIDI patch:");
				return MML_ERROR;
			}
			if(x > 127) {
				MML_AppendError(MML, "Invalid MIDI program specified.", &CurrentValueOffs);
				return MML_ERROR;
			}
			Data[nValsRead++] = (uint8_t)x;
			if(nValsRead == 3) break;

			//! If no further bank specified, exit loop
			if(MML_PeekNextChar(MML) != ':') break;
			MML_ConsumeChars(MML, 1, 0);
		}

		//! Check for drumkit program
		if(MML_PeekNextChar(MML) == '*') {
			MML_ConsumeChars(MML, 1, 0);
			IsDrumKit = 1;
		}

		//! Translate to CC0:CC32:Patch
		uint8_t CC0, CC32, Patch;
		switch(nValsRead) {
			case 1: {
				CC0 = 0, CC32 = 0, Patch = Data[0];
			} break;
			case 2: {
				CC0 = Data[0], CC32 = 0, Patch = Data[1];
			} break;
			case 3: {
				CC0 = Data[0], CC32 = Data[1], Patch = Data[2];
			} break;
		}

		//! Finally, translate to a patch index
		if(!MML->NotifyMIDIProgramChange) {
			MML_AppendError(MML, "MIDI program handling not provided. Please report this error.", &ProgramOffs);
			return MML_ERROR;
		}
		Program = MML->NotifyMIDIProgramChange(MML->NotifyUserdata, Patch, CC0, CC32, IsDrumKit, MML->UseGlobalToneBank);
		if(Program < 0) {
			MML_AppendWarning(MML, "MIDI program not found in sound bank. Trying base bank.", &ProgramOffs);
			Program = MML->NotifyMIDIProgramChange(MML->NotifyUserdata, Patch, 0, 0, IsDrumKit, MML->UseGlobalToneBank);
			if(Program < 0 && IsDrumKit) {
				Program = MML->NotifyMIDIProgramChange(MML->NotifyUserdata, 0, 0, 0, IsDrumKit, MML->UseGlobalToneBank);
			}
			if(Program < 0) {
				MML_AppendError(MML, "MIDI program not found in sound bank, or out of memory.", &ProgramOffs);
				return MML_ERROR;
			}
		}
	}

	//! Ensure we got a valid program
	if(Program > 255) {
		MML_AppendError(MML, "Song program index out of range (too many programs in use?).", &ProgramOffs);
		return MML_ERROR;
	}

	//! Output command sequence
	if(
		MML_WriteCommand(MML, MML_CMD_PROGRAM)  == MML_ERROR ||
		MML_WriteByte   (MML, (uint8_t)Program) == MML_ERROR
	) return MML_ERROR;
	return MML_OK;
}

/************************************************/

static int MML_Command_NoteDuration(struct MML_t *MML) {
	int32_t Duration = MML_ReadDuration(MML);
	if(Duration == MML_ERROR) {
		MML_AppendErrorContext(MML, "While parsing default note duration:");
		return MML_ERROR;
	}
	MML->State.Duration = (uint32_t)Duration;
	return MML_OK;
}

/************************************************/

static int MML_Command_Pattern(struct MML_t *MML) {
	if(MML_FlushPendingCommands(MML, 0) == MML_ERROR) return MML_ERROR;

	int NextChar;
	struct MML_InputOffs_t CommandOffs; MML_GetInputOffset(MML, &CommandOffs);
	CommandOffs.DataOffs -= 1;

	//! Get label name
	//! NOTE: Allow whitespace between ()
	char *LabelName;
	struct MML_InputOffs_t LabelNameOffs; MML_GetInputOffset(MML, &LabelNameOffs);
	MML_ConsumeWhitespace(MML);
	if(MML_ReadLabelString(MML, &LabelName) == MML_ERROR) {
		MML_AppendErrorContext(MML, "While parsing pattern label:");
		return MML_ERROR;
	}
	if(!LabelName) {
		MML_AppendError(MML, "Pattern must have a name.", &LabelNameOffs);
		return MML_ERROR;
	}
	MML_ConsumeWhitespace(MML);
	NextChar = MML_PeekNextChar(MML);
	if(NextChar != ')') {
		MML_AppendErrorCurrentOffset(MML, "Expected `)`.");
		return MML_ERROR;
	}
	MML_ConsumeChars(MML, 1, 0);

	//! If we don't have a '[' immediately following the ')', then this is a recall
	NextChar = MML_PeekNextChar(MML);
	if(NextChar != '[') {
		if(MML_WritePatternRecall(MML, LabelName, 0, 0, &LabelNameOffs) == MML_ERROR) {
			MML_AppendErrorContext(MML, "While parsing pattern recall:");
			return MML_ERROR;
		}
	} else {
		//! If this pattern is nested, then we need to insert an
		//! InlinePattern command, so that the Return command will
		//! not early-exit the parent pattern.
		MML_ConsumeChars(MML, 1, 0);
		if(
			(MML->State.NestLevel_Current > 0 && MML_WriteCommand(MML, MML_CMD_INLINEPATTERN) == MML_ERROR) ||
			MML_CreateLabel(MML, LabelName, MML_LABEL_TYPE_PATTERN, &LabelNameOffs, &CommandOffs) == MML_ERROR
		) return MML_ERROR;
	}
	return MML_OK;
}

/************************************************/

static int MML_Command_AnonymousPatternOrRepeat(struct MML_t *MML) {
	if(MML_FlushPendingCommands(MML, 0) == MML_ERROR) return MML_ERROR;

	int NextChar = MML_PeekNextChar(MML);
	if(NextChar == '[') {
		struct MML_InputOffs_t CommandOffs; MML_GetInputOffset(MML, &CommandOffs);
		CommandOffs.DataOffs -= 1;
		MML_ConsumeChars(MML, 1, 0);

		//! Check for ambiguous "[[[" case
		if(MML_PeekNextChar(MML) == '[') {
			MML_AppendError(MML, "Ambiguous `[[[`; use a space to separate this.", &CommandOffs);
			return MML_ERROR;
		}

		//! Section repeat
		//! Note that we need to insert a RepeatStart if we're nesting at all.
		//! This is because we need to guard for [[ [[ ... | ... ]]X ]]Y, but also
		//! because we could recall a [ [[ ... | ... ]]X ] pattern.
		//! Once we finish the section repeat, we can check if we used a Break
		//! command, and if we didn't, we can remove this guard command.
		if(
			(MML->State.NestLevel_Current > 0 && MML_WriteCommand(MML, MML_CMD_REPEATSTART) == MML_ERROR) ||
			MML_CreateLabel(MML, NULL, MML_LABEL_TYPE_REPEAT, &CommandOffs, &CommandOffs) == MML_ERROR
		) return MML_ERROR;
		return MML_OK;
	} else {
		//! Anonymous pattern definition
		struct MML_InputOffs_t CommandOffs; MML_GetInputOffset(MML, &CommandOffs);
		CommandOffs.DataOffs -= 1;
		if(
			(MML->State.NestLevel_Current > 0 && MML_WriteCommand(MML, MML_CMD_INLINEPATTERN) == MML_ERROR) ||
			MML_CreateLabel(MML, NULL, MML_LABEL_TYPE_PATTERN, &CommandOffs, &CommandOffs) == MML_ERROR
		) return MML_ERROR;
	}
	return MML_OK;
}

/************************************************/

static int MML_Command_EndPatternOrRepeat(struct MML_t *MML) {
	if(MML_FlushPendingCommands(MML, 0) == MML_ERROR) return MML_ERROR;

	int NextChar = MML_PeekNextChar(MML);
	if(NextChar == ']') {
		MML_ConsumeChars(MML, 1, 0);
		struct MML_InputOffs_t CommandOffs; MML_GetInputOffset(MML, &CommandOffs);
		CommandOffs.DataOffs -= 2;

		//! Check for ambiguous "]]]" case
		if(MML_PeekNextChar(MML) == ']') {
			MML_AppendError(MML, "Ambiguous `]]]`; use a space to separate this.", &CommandOffs);
			return MML_ERROR;
		}

		//! Ensure we're defining a repeat section
		if(MML->State.NestLevel_Repeat < 1) {
			MML_AppendError(MML, "Unexpected `]]`.", &CommandOffs);
			return MML_ERROR;
		}

		//! Section repeat end
		//! If a pattern is being defined inside this block, error out if it's unterminated
		uint32_t LabelIdx = MML->State.NestedLabelIdxList[MML->State.NestLevel_Current-1];
		struct MML_Label_t *Label = &MML->LabelsList[LabelIdx];
		if(Label->Type != MML_LABEL_TYPE_REPEAT) {
			MML_AppendError(MML, "Unterminated pattern inside repeat section.", &Label->InputOffs);
			return MML_ERROR;
		}
		MML->State.NestLevel_Current--;
		MML->State.NestLevel_Repeat--;

		//! Repeats sections cannot be recalled and so must repeat immediately
		struct MML_InputOffs_t nRepeatsOffs; MML_GetInputOffset(MML, &nRepeatsOffs);
		NextChar = MML_PeekNextChar(MML);
		if(!MML_IS_DIGIT(NextChar)) {
			MML_AppendError(MML, "Expected repeat count.", &nRepeatsOffs);
			return MML_ERROR;
		}

		//! Read the number of repeats
		int32_t nRepeats = MML_ReadDecimalOrHex(MML);
		if(nRepeats == MML_ERROR) {
			MML_AppendErrorContext(MML, "While parsing section repeat count:");
			return MML_ERROR;
		}
		if(nRepeats == 0) {
			MML_AppendError(MML, "Invalid number of repeats.", &nRepeatsOffs);
			return MML_ERROR;
		}
		if(nRepeats == 1) {
			//! We can't ignore this, because if the repeat is nested and contains a Break
			//! command, then we don't have a way of popping the stack otherwise.
			MML_AppendError(MML, "Repeat section with only one iteration.", &nRepeatsOffs);
			return MML_OK;
		}
		if(nRepeats > 257) {
			MML_AppendError(MML, "Too many repeats (max 257 iterations).", &nRepeatsOffs);
			return MML_ERROR;
		}

		//! If the repeat doesn't use a Break command, we can remove the RepeatStart command
		if(MML->State.NestLevel_Current > 0 && !Label->UsesBreakCommand) {
			const uint32_t ReptStartLen = MML_GetCommandLength(MML_CMD_REPEATSTART);
			uint32_t ReptStartOffs = Label->DataOffs - ReptStartLen;
			if(MML_RemoveNybbles(MML, ReptStartOffs, ReptStartLen) == MML_ERROR) return MML_ERROR;
		}

		//! Write the command and create a reference
		//! NOTE: nRepeats-2 because we've already played one iteration
		//! before reaching the Repeat command, and then -1 because we
		//! count from 0.
		if(
			MML_WriteCommand(MML, MML_CMD_REPEAT)        == MML_ERROR ||
			MML_WriteByte   (MML, (uint8_t)(nRepeats-2)) == MML_ERROR ||
			MML_CreateReference(MML, NULL, LabelIdx, MML_REFERENCE_CMDTYPE_REPEAT, 1, &CommandOffs) == MML_ERROR
		) return MML_ERROR;

		//! Create a label to the end of the repeat if needed
		if(Label->UsesBreakCommand) {
			if(MML_CreateLabel(MML, NULL, MML_LABEL_TYPE_GENERIC, &CommandOffs, &CommandOffs) == MML_ERROR) return MML_ERROR;
			Label = &MML->LabelsList[LabelIdx]; //! <- CreateLabel() might reallocate the array
			Label->EndLabelIdx = MML->nLabels-1;
		}
		return MML_OK;
	} else {
		struct MML_InputOffs_t CommandOffs; MML_GetInputOffset(MML, &CommandOffs);
		CommandOffs.DataOffs -= 1;

		//! Ensure we're defining a pattern
		if(MML->State.NestLevel_Pattern < 1) {
			MML_AppendError(MML, "Unexpected `]`.", &CommandOffs);
			return MML_ERROR;
		}

		//! Pattern definition end
		//! If a repeat is being defined inside this block, error out if it's unterminated
		uint32_t LabelIdx = MML->State.NestedLabelIdxList[MML->State.NestLevel_Current-1];
		struct MML_Label_t *Label = &MML->LabelsList[LabelIdx];
		if(Label->Type != MML_LABEL_TYPE_PATTERN) {
			MML_AppendError(MML, "Unterminated repeat section inside pattern.", &Label->InputOffs);
			return MML_ERROR;
		}
		MML->State.NestLevel_Current--;
		MML->State.NestLevel_Pattern--;

		//! Assign this pattern as the one to use for `*` commands
		//! Doing this on pattern-end gives the most reasonable
		//! expectation for how the command /should/ work.
		MML->State.LastDefinedPatternLabelIdx = LabelIdx;

		//! Terminate the pattern and then issue a recall if needed
		MML_WriteCommand(MML, MML_CMD_RETURN);
		if(MML_IS_DIGIT(NextChar)) {
			if(MML_WritePatternRecall(MML, NULL, LabelIdx, 1, &CommandOffs) == MML_ERROR) {
				MML_AppendErrorContext(MML, "While parsing pattern recall:");
				return MML_ERROR;
			}
		}

		//! Create a label to the end of the repeat if needed
		if(Label->UsesBreakCommand) {
			if(MML_CreateLabel(MML, NULL, MML_LABEL_TYPE_GENERIC, &CommandOffs, &CommandOffs) == MML_ERROR) return MML_ERROR;
			Label = &MML->LabelsList[LabelIdx]; //! <- CreateLabel() might reallocate the array
			Label->EndLabelIdx = MML->nLabels-1;
		}
	}
	return MML_OK;
}

/************************************************/

static int MML_Command_NoteStackOrTripletMode(struct MML_t *MML) {
	int NextChar = MML_PeekNextChar(MML);
	if(NextChar == '{') {
		MML_ConsumeChars(MML, 1, 0);
		struct MML_InputOffs_t CommandOffs; MML_GetInputOffset(MML, &CommandOffs);
		CommandOffs.DataOffs -= 2;

		//! Check for ambiguous "{{{" case
		if(MML_PeekNextChar(MML) == '{') {
			MML_AppendError(MML, "Ambiguous `{{{`; use a space to separate this.", &CommandOffs);
			return MML_ERROR;
		}

		//! Triplets section
		if(MML->State.IsTripletMode) {
			MML_AppendError(MML, "Unexpected `{{`: a triplets section was already started.", &CommandOffs);
			return MML_ERROR;
		}
		MML->State.IsTripletMode = 1;
		MML->State.TripletModeOffs = CommandOffs;
		return MML_OK;
	} else {
		//! Note stack
		if(MML_Command_Note(MML, 0) == MML_ERROR) {
			MML_AppendErrorContext(MML, "While parsing note stack:");
			return MML_ERROR;
		} else return MML_OK;
	}
}

/************************************************/

static int MML_Command_EndTripletMode(struct MML_t *MML) {
	int NextChar = MML_PeekNextChar(MML);
	if(NextChar != '}') {
		MML_AppendErrorCurrentOffset(MML, "Unexpected `}`.");
		return MML_ERROR;
	}
	MML_ConsumeChars(MML, 1, 0);

	//! Check that a triplets section was already started
	struct MML_InputOffs_t CommandOffs; MML_GetInputOffset(MML, &CommandOffs);
	CommandOffs.DataOffs -= 2;
	if(!MML->State.IsTripletMode) {
		MML_AppendError(MML, "Unexpected `}}`: a triplets section was not previously started.", &CommandOffs);
		return MML_ERROR;
	}
	MML->State.IsTripletMode = 0;
	return MML_OK;
}

/************************************************/

static int MML_Command_PatternRecall(struct MML_t *MML) {
	if(MML_FlushPendingCommands(MML, 0) == MML_ERROR) return MML_ERROR;

	struct MML_InputOffs_t CommandOffs; MML_GetInputOffset(MML, &CommandOffs);
	CommandOffs.DataOffs -= 1;

	//! Recall the last pattern to be fully defined
	uint32_t Idx = MML->State.LastDefinedPatternLabelIdx;
	if(Idx == MML_LABELIDX_NULL) {
		MML_AppendError(MML, "No pattern previously defined.", &CommandOffs);
		return MML_ERROR;
	}
	if(MML_WritePatternRecall(MML, NULL, Idx, 0, &CommandOffs) == MML_ERROR) {
		MML_AppendErrorContext(MML, "While parsing pattern recall:");
		return MML_ERROR;
	}
	return MML_OK;
}

/************************************************/

static int MML_Command_Break(struct MML_t *MML) {
	if(MML_FlushPendingCommands(MML, 0) == MML_ERROR) return MML_ERROR;

	struct MML_InputOffs_t CommandOffs; MML_GetInputOffset(MML, &CommandOffs);
	CommandOffs.DataOffs -= 1;

	//! Check that we're defining a pattern /or/ a repeat
	uint32_t Level = MML->State.NestLevel_Current;
	if(Level == 0) {
		MML_AppendError(MML, "Unexpected `|` (not currently defining a pattern or repeat).", &CommandOffs);
		return MML_ERROR;
	}

	//! Now write the Break command, along with the end-of-repeat target
	uint32_t ThisLabelIdx = MML->State.NestedLabelIdxList[Level-1];
	MML->LabelsList[ThisLabelIdx].UsesBreakCommand = 1;
	if(
		MML_WriteCommand(MML, MML_CMD_BREAK) == MML_ERROR ||
		MML_CreateReference(MML, NULL, ThisLabelIdx, MML_REFERENCE_CMDTYPE_BREAK, 0, &CommandOffs) == MML_ERROR
	) return MML_ERROR;

	//! This is slightly hacky, but necessary.
	//! The reference is created as a standard indexed type,
	//! but we need an "end of" type, instead.
	MML->ReferencesList[MML->nReferences-1].ReferenceType = MML_REFERENCE_TYPE_ENDOF;
	return MML_OK;
}

/************************************************/

static int MML_Command_Label(struct MML_t *MML) {
	if(MML_FlushPendingCommands(MML, 0) == MML_ERROR) return MML_ERROR;

	struct MML_InputOffs_t LabelOffs; MML_GetInputOffset(MML, &LabelOffs);

	//! Get label name
	char *LabelName;
	if(MML_ReadLabelString(MML, &LabelName) == MML_ERROR) {
		MML_AppendErrorContext(MML, "While parsing label name:");
		return MML_ERROR;
	}
	if(!LabelName) {
		MML_AppendError(MML, "Label must have a valid name.", &LabelOffs);
		return MML_ERROR;
	}

	//! Ensure we haven't defined the label inside a repeat section
	uint8_t IsSubPattern = 0;
	if(MML->State.NestLevel_Current > 0) {
		uint32_t NestedLabelIdx = MML->State.NestedLabelIdxList[MML->State.NestLevel_Current-1];
		if(MML->LabelsList[NestedLabelIdx].Type != MML_LABEL_TYPE_PATTERN) {
			MML_AppendError(MML, "Labels can only be defined at the global scope or inside a pattern.", &LabelOffs);
			return MML_ERROR;
		} else IsSubPattern = 1;
	}

	//! Insert to labels list, and reset running state for sub-patterns
	if(MML_CreateLabel(MML, LabelName, MML_LABEL_TYPE_GENERIC, &LabelOffs, &LabelOffs) == MML_ERROR) return MML_ERROR;
	if(IsSubPattern) MML_ResetRunningState(MML);
	return MML_OK;
}

/************************************************/

static int MML_Command_Priority(struct MML_t *MML) {
	//! NOTE: Allow whitespace between $cmd and =
	MML_ConsumeWhitespace(MML);
	if(MML_PeekNextChar(MML) != '=') {
		MML_AppendErrorCurrentOffset(MML, "Expected `=`.");
		return MML_ERROR;
	}
	MML_ConsumeChars(MML, 1, 1);

	//! Get priority value
	struct MML_InputOffs_t ValueOffs; MML_GetInputOffset(MML, &ValueOffs);
	int32_t Sign = MML_ReadSign(MML);
	int32_t Value = MML_ReadDecimalOrHex(MML);
	if(Value == MML_ERROR) {
		MML_AppendErrorContext(MML, "While parsing priority level:");
		return MML_ERROR;
	}
	Value = MML_ApplySign(Value, Sign) + 8;
	if(Value < 0 || Value > 15) {
		MML_AppendError(MML, "Priority value out of range (must be -8..+7).", &ValueOffs);
		return MML_ERROR;
	}
	if(
		MML_WriteCommand(MML, MML_CMD_PRIORITY) == MML_ERROR ||
		MML_WriteNybble (MML, (uint8_t)Value)   == MML_ERROR
	) return MML_ERROR;
	return MML_OK;
}

/************************************************/

static int MML_Command_Transpose(struct MML_t *MML) {
	//! NOTE: Allow whitespace between $cmd and =/+=/-=
	MML_ConsumeWhitespace(MML);
	struct MML_InputOffs_t CommandOffs; MML_GetInputOffset(MML, &CommandOffs);

	//! Check type of command
	int Command = MML_PeekNextChar(MML);
	switch(Command) {
		//! Transpose set
		case '=': {
			//! NOTE: Allow whitespace after `=`
			MML_ConsumeChars(MML, 1, 1);

			//! Get transpose amount
			struct MML_InputOffs_t ValueOffs; MML_GetInputOffset(MML, &ValueOffs);
			int32_t Sign  = MML_ReadSign(MML);
			int32_t Value = MML_ReadDecimalOrHex(MML);
			if(Value == MML_ERROR) {
				MML_AppendErrorContext(MML, "While parsing transpose amount:");
				return MML_ERROR;
			}
			Value = MML_ApplySign(Value, Sign);
			if(Value < -127 || Value > +127) {
				MML_AppendError(MML, "Transpose value out of range.", &ValueOffs);
				return MML_ERROR;
			}
			if(
				MML_WriteCommand(MML, MML_CMD_TRANSPOSE) == MML_ERROR ||
				MML_WriteByte   (MML, (uint8_t)Value)    == MML_ERROR
			) return MML_ERROR;
			return MML_OK;
		} break;

		//! Transpose add
		case '+': /* Fall through */
		case '-': {
			//! Ensure there's a '=' immediately after +/-
			MML_ConsumeChars(MML, 1, 0);
			if(MML_PeekNextChar(MML) != '=') {
				MML_AppendError(MML, "Expected `=`, `+=`, or `-=`.", &CommandOffs);
				return MML_ERROR;
			}

			//! NOTE: Allow whitespace after +=/-=
			MML_ConsumeChars(MML, 1, 1);

			//! Read the signed transpose value, and apply sign again for '-='
			struct MML_InputOffs_t ValueOffs; MML_GetInputOffset(MML, &ValueOffs);
			int32_t Sign  = MML_ReadSign(MML);
			int32_t Value = MML_ReadDecimalOrHex(MML);
			if(Value == MML_ERROR) {
				MML_AppendErrorContext(MML, "While parsing relative transpose amount:");
				return MML_ERROR;
			}
			Value = MML_ApplySign(Value, Sign);
			if(Value < -127 || Value > +127) {
				MML_AppendError(MML, "Relative transpose value out of range.", &ValueOffs);
				return MML_ERROR;
			}
			if(Command == '-') Value = -Value;
			if(
				MML_WriteCommand(MML, MML_CMD_TRANSPOSEADD) == MML_ERROR ||
				MML_WriteByte   (MML, (uint8_t)Value)       == MML_ERROR
			) return MML_ERROR;
			return MML_OK;
		} break;

		//! Unknown
		default: {
			MML_AppendError(MML, "Expected `=`, `+=`, or `-=`.", &CommandOffs);
			return MML_ERROR;
		} break;
	}
}

/************************************************/

static int MML_Command_Portamento(struct MML_t *MML) {
	//! Check for on/off
	if(MML_PeekNextChar(MML) == '+') {
		if(MML_WriteCommand(MML, MML_CMD_PORTAMENTO_ON) == MML_ERROR) return MML_ERROR;
	} else if(MML_PeekNextChar(MML) == '-') {
		if(MML_WriteCommand(MML, MML_CMD_PORTAMENTO_OFF) == MML_ERROR) return MML_ERROR;
	} else {
		MML_AppendErrorCurrentOffset(MML, "Expected `+` or `-`.");
		return MML_ERROR;
	}
	MML_ConsumeChars(MML, 1, 1);
	return MML_OK;
}

/************************************************/

static int MML_Command_Goto(struct MML_t *MML) {
	if(MML_FlushPendingCommands(MML, 0) == MML_ERROR) return MML_ERROR;

	//! Ensure no patterns or repeats are being defined
	if(MML->State.NestLevel_Current > 0) {
		struct MML_InputOffs_t CommandOffs; MML_GetInputOffset(MML, &CommandOffs);
		MML_AppendError(MML, "$goto() cannot be used inside a pattern/repeat section.", &CommandOffs);
		return MML_ERROR;
	}

	//! NOTE: Allow whitespace inside (), but not between $cmd(
	if(MML_PeekNextChar(MML) != '(') { MML_AppendErrorCurrentOffset(MML, "Expected `(`."); return MML_ERROR; }
	MML_ConsumeChars(MML, 1, 1);

	//! Read label name
	char *LabelName;
	struct MML_InputOffs_t LabelOffs; MML_GetInputOffset(MML, &LabelOffs);
	if(MML_ReadLabelString(MML, &LabelName) == MML_ERROR) {
		MML_AppendErrorContext(MML, "While parsing target name:");
		return MML_ERROR;
	}
	if(!LabelName) {
		MML_AppendError(MML, "Target label must have a valid name.", &LabelOffs);
		return MML_ERROR;
	}
	MML_ConsumeWhitespace(MML);

	//! Ensure we end with ')'
	if(MML_PeekNextChar(MML) != ')') { MML_AppendErrorCurrentOffset(MML, "Expected `)`."); return MML_ERROR; }
	MML_ConsumeChars(MML, 1, 0);

	//! Write command
	if(
		MML_WriteCommand   (MML, MML_CMD_JUMP) == MML_ERROR ||
		MML_CreateReference(MML, LabelName, 0, MML_REFERENCE_CMDTYPE_GOTO, 0, &LabelOffs) == MML_ERROR
	) return MML_ERROR;
	return MML_OK;
}

/************************************************/

static int MML_GetPayload(struct MML_t *MML, int *Payload, int *ParsedPayload) {
	//! By default, Payload == 0
	*Payload = 0;
	*ParsedPayload = 0;

	//! NOTE: Allow whitespace between () and the contents
	int Next = MML_PeekNextChar(MML);
	if(MML_IS_DIGIT(Next)) {
		int Sign  = MML_ReadSign(MML);
		int Value = MML_ReadDecimalOrHex(MML);;
		if(Value == MML_ERROR) {
			MML_AppendErrorContext(MML, "While parsing payload:");
			return MML_ERROR;
		}
		*Payload       = MML_ApplySign(Value, Sign);
		*ParsedPayload = 1;
	}
	return MML_OK;
}

static int MML_WritePayloadCommand(struct MML_t *MML, uint32_t Command, int Payload) {
	//! Get the raw payload data
	uint32_t RawPayload = (Payload < 0) ? (-Payload) : (+Payload);
	RawPayload = (RawPayload << 1) | ((Payload < 0) ? 1 : 0);

	//! Get the payload size in nybbles
	uint8_t PayloadSize = 0; {
		uint32_t x = RawPayload;
		while(x) PayloadSize++, x >>= 4;
	}

	//! Write the payload size followed by the payload itself
	if(
		MML_WriteCommand(MML, Command)    == MML_ERROR ||
		MML_WriteNybble(MML, PayloadSize) == MML_ERROR
	) return MML_ERROR;
	while(PayloadSize--) {
		if(MML_WriteNybble(MML, (uint8_t)(RawPayload >> (PayloadSize*4))) == MML_ERROR) return MML_ERROR;
	}
	return MML_OK;
}

static int MML_Command_GotoIf(struct MML_t *MML) {
	//! NOTE: Allow whitespace inside (), but not between $cmd(
	if(MML_FlushPendingCommands(MML, 0) == MML_ERROR) return MML_ERROR;
	if(MML_PeekNextChar(MML) != '(') { MML_AppendErrorCurrentOffset(MML, "Expected `(`."); return MML_ERROR; }
	MML_ConsumeChars(MML, 1, 1);
		//! Read label name
		char *LabelName;
		struct MML_InputOffs_t LabelOffs; MML_GetInputOffset(MML, &LabelOffs);
		if(MML_ReadLabelString(MML, &LabelName) == MML_ERROR) {
			MML_AppendErrorContext(MML, "While parsing target name:");
			return MML_ERROR;
		}
		if(!LabelName) {
			MML_AppendError(MML, "Target label must have a valid name.", &LabelOffs);
			return MML_ERROR;
		}
		MML_ConsumeWhitespace(MML);

		//! If we have a `,` following the label, then we have a payload
		int Payload = 0;
		if(MML_PeekNextChar(MML) == ',') {
			MML_ConsumeChars(MML, 1, 1);

			//! Get payload
			int Parsed;
			struct MML_InputOffs_t PayloadOffs; MML_GetInputOffset(MML, &PayloadOffs);
			if(MML_GetPayload(MML, &Payload, &Parsed) == MML_ERROR) return MML_ERROR;
			if(!Parsed) {
				MML_AppendError(MML, "Expected a payload following `,`.", &PayloadOffs);
				return MML_ERROR;
			}
			MML_ConsumeWhitespace(MML);
		}

		//! Write command
		if(
			MML_WritePayloadCommand(MML, MML_CMD_GOTOIF, Payload) == MML_ERROR ||
			MML_CreateReference(MML, LabelName, 0, MML_REFERENCE_CMDTYPE_GOTO, 0, &LabelOffs) == MML_ERROR
		) return MML_ERROR;
	if(MML_PeekNextChar(MML) != ')') { MML_AppendErrorCurrentOffset(MML, "Expected `)`."); return MML_ERROR; }
	MML_ConsumeChars(MML, 1, 1);
	return MML_OK;
}

static int MML_Command_Signal(struct MML_t *MML) {
	//! NOTE: Allow whitespace inside (), but not between $cmd(
	if(MML_FlushPendingCommands(MML, 0) == MML_ERROR) return MML_ERROR;
	if(MML_PeekNextChar(MML) != '(') { MML_AppendErrorCurrentOffset(MML, "Expected `(`."); return MML_ERROR; }
	MML_ConsumeChars(MML, 1, 1);
		//! Get payload and write command
		int Payload, Parsed;
		if(
			MML_GetPayload(MML, &Payload, &Parsed) == MML_ERROR ||
			MML_WritePayloadCommand(MML, MML_CMD_SIGNAL, Payload) == MML_ERROR
		) return MML_ERROR;
	if(MML_PeekNextChar(MML) != ')') { MML_AppendErrorCurrentOffset(MML, "Expected `)`."); return MML_ERROR; }
	MML_ConsumeChars(MML, 1, 1);
	return MML_OK;
}

/************************************************/

static int MML_Command_End(struct MML_t *MML) {
	//! NOTE: Allow whitespace inside (), but not between $cmd(
	if(MML_FlushPendingCommands(MML, 0) == MML_ERROR) return MML_ERROR;
	if(MML_PeekNextChar(MML) != '(') { MML_AppendErrorCurrentOffset(MML, "Expected `(`."); return MML_ERROR; }
	MML_ConsumeChars(MML, 1, 1);
	if(MML_PeekNextChar(MML) != ')') { MML_AppendErrorCurrentOffset(MML, "Expected `)`."); return MML_ERROR; }
	MML_ConsumeChars(MML, 1, 1);
	return MML_WriteCommand(MML, MML_CMD_END);
}

static int MML_Command_Global_TicksPerBeat(struct MML_t *MML) {
	//! NOTE: Allow whitespace between $cmd and = and the actual value
	MML_ConsumeWhitespace(MML);
	if(MML_PeekNextChar(MML) != '=') {
		MML_AppendErrorCurrentOffset(MML, "Expected `=`.");
		return MML_ERROR;
	}
	MML_ConsumeChars(MML, 1, 1);
	struct MML_InputOffs_t TicksOffs; MML_GetInputOffset(MML, &TicksOffs);
	int32_t Ticks = MML_ReadDecimalOrHex(MML);
	if(Ticks == MML_ERROR) {
		MML_AppendErrorContext(MML, "While parsing ticks per beat:");
		return MML_ERROR;
	}
	if(Ticks > 1920) {
		//! Arbitrary limit of 1920 ticks per quarter note
		MML_AppendError(MML, "Ticks per beat must be at most 1920 ticks.", &TicksOffs);
		return MML_ERROR;
	}
	MML->TicksPerBeat = Ticks;
	return MML_OK;
}

static int MML_Command_Global_UseGlobalTones(struct MML_t *MML) {
	//! NOTE: Allow whitespace between $cmd and = and the actual value
	MML_ConsumeWhitespace(MML);
	if(MML_PeekNextChar(MML) != '=') {
		MML_AppendErrorCurrentOffset(MML, "Expected `=`.");
		return MML_ERROR;
	}
	MML_ConsumeChars(MML, 1, 1);
	struct MML_InputOffs_t ValueOffs; MML_GetInputOffset(MML, &ValueOffs);
	if(MML_StringMatchAndConsume(MML, "true", 1)) {
		MML->UseGlobalToneBank = 1;
	} else if(MML_StringMatchAndConsume(MML, "false", 1)) {
		MML->UseGlobalToneBank = 0;
	} else {
		MML_AppendErrorContext(MML, "Expected `true` or `false`.");
		return MML_ERROR;
	}
	return MML_OK;
}

/************************************************/
//! EOF
/************************************************/
