/************************************************/
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/************************************************/
#include "GlobalHelpers.h"
#include "MML.h"
/************************************************/

static void DrawSeparator() {
	int i;
	const int SEPARATOR_WIDTH = 75;
	for(i=0;i<SEPARATOR_WIDTH;i++) putchar('-');
	putchar('\n');
}

/************************************************/

//! Initialize reader
int MML_Init(
	struct MML_t *MML,
	const char *Data,
	uint32_t DataSize,
	MML_NotifyMIDIProgramChangeFnc_t NotifyMIDIProgramChange,
	MML_NotifyKeyOnFnc_t             NotifyKeyOn,
	void                            *NotifyUserdata,
	uint8_t UseGlobalToneBank
) {
	//! Initialize struct
	MML->ErrorDepth = 0;
	MML->NotifyUserdata          = NotifyUserdata;
	MML->NotifyMIDIProgramChange = NotifyMIDIProgramChange;
	MML->NotifyKeyOn             = NotifyKeyOn;
	MML->UseGlobalToneBank = UseGlobalToneBank;
	MML->TicksPerBeat      = MML_TICKS_PER_QUARTER_NOTE;
	MML->Input.Data  = Data;
	MML->Input.Size  = DataSize;
	MML->Input.Offs.DataOffs = 0;
	MML->Input.Offs.LineOffs = 0;
	MML->Input.Offs.Line     = 0;
	MML->Output.Data = NULL;
	MML->Output.Offs = 0;
	MML->Output.Size = 0;
	MML->nTracks        = 0;
	MML->TracksList     = NULL;
	MML->nLabels        = 0;
	MML->LabelsList     = NULL;
	MML->nReferences    = 0;
	MML->ReferencesList = NULL;
	MML->nWarnings      = 0;
	MML->WarningsList   = NULL;
	MML_ResetTrackState(MML);

	//! Sanity check
	if(!Data || !DataSize) {
		MML_AppendErrorGlobal(MML, "No data provided.");
		return MML_ERROR;
	}
	return MML_OK;
}

/************************************************/

//! Destroy reader
void MML_Destroy(struct MML_t *MML) {
	uint32_t i;

	//! Destroy all allocated error strings
	for(i=0;i<MML->ErrorDepth;i++) {
		//! While ::String is normally `const char*`, when it's
		//! allocated dynamically, we need a const_cast<char*>.
		if(MML->ErrorData[i].IsAllocated) free((char*)MML->ErrorData[i].String);
	}

	//! Destroy all warnings
	free(MML->WarningsList);

	//! Destroy all references
	for(i=0;i<MML->nReferences;i++) {
		if(MML->ReferencesList[i].ReferenceType == MML_REFERENCE_TYPE_NAMED) {
			free(MML->ReferencesList[i].Name);
		}
	}
	free(MML->ReferencesList);

	//! Destroy all labels
	for(i=0;i<MML->nLabels;i++) {
		free(MML->LabelsList[i].Name);
	}
	free(MML->LabelsList);

	//! Delete all track data
	for(i=0;i<MML->nTracks;i++) {
		free(MML->TracksList[i].Name);
	}
	free(MML->TracksList);

	//! Destroy current output data (if any)
	free(MML->Output.Data);
}

/************************************************/

//! Consume characters (and optionally skip whitespace)
void MML_ConsumeChars(struct MML_t *MML, uint32_t nChars, uint8_t ConsumeWhitespace) {
	uint32_t Offs = MML->Input.Offs.DataOffs + nChars;
	if(ConsumeWhitespace) {
		uint32_t Size = MML->Input.Size;
		const char *Data = MML->Input.Data;
		while(Offs < Size) {
			int ThisChar = Data[Offs];
			if(ThisChar == '\n') {
				MML->Input.Offs.Line++;
				MML->Input.Offs.LineOffs = ++Offs;
				continue;
			}
			if(isspace(ThisChar)) {
				Offs++;
				continue;
			}
			if(ThisChar == ';') {
				//! Comments last until LF
				while(++Offs < Size && Data[Offs] != '\n');
				continue;
			}
			break;
		}
	}
	MML->Input.Offs.DataOffs = Offs;
}

/************************************************/

//! Perform string matching (with optional tokenization and consumption)
static int MML_StringMatch(struct MML_t *MML, const char *String, uint8_t AsToken, uint8_t Consume) {
	uint32_t Length = strlen(String);
	if(MML->Input.Offs.DataOffs+Length <= MML->Input.Size) {
		if(memcmp(MML->Input.Data + MML->Input.Offs.DataOffs, String, Length) == 0) {
			uint8_t IsMatch;
			if(AsToken) {
				//! Must match a token, so check that next char is not alphanum
				IsMatch = 0;
				if(MML->Input.Offs.DataOffs+Length < MML->Input.Size) {
					uint8_t c = MML->Input.Data[MML->Input.Offs.DataOffs + Length];
					if(!MML_IS_ALPHANUM(c)) IsMatch = 1;
				}
			} else IsMatch = 1;
			if(IsMatch) {
				if(Consume) MML_ConsumeChars(MML, Length, 0);
				return 1;
			}
		}
	}
	return 0;
}

//! Peek at next character without advancing
int MML_PeekNextChar(const struct MML_t *MML) {
	if(MML->Input.Offs.DataOffs < MML->Input.Size) {
		return MML->Input.Data[MML->Input.Offs.DataOffs];
	} else return MML_EOF;
}

//! Check if the following characters match a string
int MML_PeekStringMatch(const struct MML_t *MML, const char *String, uint8_t AsToken) {
	return MML_StringMatch((struct MML_t*)MML, String, AsToken, 0);
}

//! Check if the following characters match a string, and consume on success
int MML_StringMatchAndConsume(struct MML_t *MML, const char *String, uint8_t AsToken) {
	return MML_StringMatch(MML, String, AsToken, 1);
}

/************************************************/

//! Read a sign character (if any)
//! Returns -1 for negative (-), and 0 for positive (+; default)
int32_t MML_ReadSign(struct MML_t *MML) {
	int32_t Sign = 0;
	     if(MML_PeekNextChar(MML) == '+') { Sign =  0; MML_ConsumeChars(MML, 1, 0); }
	else if(MML_PeekNextChar(MML) == '-') { Sign = -1; MML_ConsumeChars(MML, 1, 0); }
	return Sign;
}

/************************************************/

//! Read decimal or hex number (hex numbers prefixed with 0x) in range 0..7FFFFFFFh
int32_t MML_ReadDecimalOrHex(struct MML_t *MML) {
	struct MML_InputOffs_t ValueOffs; MML_GetInputOffset(MML, &ValueOffs);

	//! Ensure we have a number
	int Next = MML_PeekNextChar(MML);
	if(!MML_IS_DIGIT(Next)) {
		MML_AppendErrorCurrentOffset(MML, "Expected a decimal or hexadecimal value.");
		return MML_ERROR;
	}

	//! Get first digit, then check for decimal/hex
	int32_t Value = MML_DIGIT_TO_NUMBER(Next);
	Next = MML_ConsumeCharAndPeekNext(MML);
	if(Value == 0 && Next == 'x') {
		//! Hexadecimal
		Next = MML_ConsumeCharAndPeekNext(MML);
		if(!MML_IS_HEX_DIGIT(Next)) {
			if(Next == MML_EOF) {
				MML_AppendErrorCurrentOffset(MML, "Unexpected EOF in hex value.");
			} else {
				MML_AppendErrorCurrentOffset(MML, "Unexpected character in hex value.");
			}
			return MML_ERROR;
		} else do {
			if(Value >= 0x08000000) {
				MML_AppendError(MML, "Hex value too large.", &ValueOffs);
				return MML_ERROR;
			}
			Value = Value*16 + MML_HEXDIGIT_TO_NUMBER(Next);
			Next = MML_ConsumeCharAndPeekNext(MML);
		} while(MML_IS_HEX_DIGIT(Next));
	} else {
		//! Decimal
		while(MML_IS_DIGIT(Next)) {
			int NextDigit = MML_DIGIT_TO_NUMBER(Next);
			if(Value >= (int32_t)((1u<<31)/10) && NextDigit >= (int32_t)((1u<<31)%10)) {
				MML_AppendError(MML, "Decimal value too large.", &ValueOffs);
				return MML_ERROR;
			}
			Value = Value*10 + NextDigit;
			Next = MML_ConsumeCharAndPeekNext(MML);
		}
	}

	//! All done
	return Value;
}

/************************************************/

//! Read a double-precision floating-point value
double MML_ReadDouble(struct MML_t *MML) {
	//! Read the sign value (if any) and ensure we have a number
	int Sign = MML_ReadSign(MML);
	int Next = MML_PeekNextChar(MML);
	if(!MML_IS_DIGIT(Next)) {
		MML_AppendErrorCurrentOffset(MML, "Expected a decimal floating-point value.");
		return NAN;
	}

	//! Next, begin parsing integer part
	//! Because the significand of a double is 53 bits, we stop when >= 2^53
	uint64_t Integer = 0;
	while(MML_IS_DIGIT(Next) && Integer < (1ull<<53)) {
		Integer = Integer*10 + MML_DIGIT_TO_NUMBER(Next);
		Next = MML_ConsumeCharAndPeekNext(MML);
	}

	//! If we stopped before the number ends, count exponent
	int Exp10Adjust = 0;
	while(MML_IS_DIGIT(Next)) {
		Exp10Adjust++;
		Next = MML_ConsumeCharAndPeekNext(MML);
	}

	//! Do we have a fractional part?
	if(Next == '.') {
		Next = MML_ConsumeCharAndPeekNext(MML);

		//! Read fractional bits, adjusting base-10 exponent as we go
		while(MML_IS_DIGIT(Next) && Integer < (1ull<<53)) {
			Integer = Integer*10 + MML_DIGIT_TO_NUMBER(Next);
			Exp10Adjust--;
			Next = MML_ConsumeCharAndPeekNext(MML);
		}

		//! Discard any remaining digits
		while(MML_IS_DIGIT(Next)) Next = MML_ConsumeCharAndPeekNext(MML);
	}

	//! Combine all the pieces together
	double ExpScale = pow(10.0, (Exp10Adjust < 0) ? (-Exp10Adjust) : Exp10Adjust);
	double Value = (double)Integer;
	if(Exp10Adjust < 0) Value /= ExpScale;
	else                Value *= ExpScale;
	return Sign ? (-Value) : Value;
}

/************************************************/

//! Check for duration (eg. "a32", "a..", ties, etc.) and get new duration in ticks.
static int32_t MML_ReadNestedDuration(struct MML_t *MML, uint32_t NestLevel) {
	//! Keep reading until we stop receiving tie events
	int32_t Duration = 0;
	struct MML_InputOffs_t DurationOffs; MML_GetInputOffset(MML, &DurationOffs);
	for(;;) {
		int32_t Sign = MML_ReadSign(MML);
		int NextChar = MML_PeekNextChar(MML);

		//! Check for nesting, raw ticks, or a time divider.
		//! If none are provided, use MML->State.Duration.
		int32_t ThisDuration;
		struct MML_InputOffs_t ThisDurationOffs; MML_GetInputOffset(MML, &ThisDurationOffs);
		if(NextChar == '(') {
			//! Exceeded the maximum nesting level?
			if(NestLevel >= MML_MAX_DURATION_NESTING) {
				MML_AppendErrorCurrentOffset(MML, "Exceeded maximum nesting level for duration.");
				return MML_ERROR;
			}

			//! Recurse in
			MML_ConsumeChars(MML, 1, 0);
			ThisDuration = MML_ReadNestedDuration(MML, NestLevel+1);
			if(ThisDuration == MML_ERROR) {
				if(NestLevel == 0) {
					//! Only append the context at the highest level
					MML_AppendErrorContext(MML, "While parsing nested duration:");
				}
				return MML_ERROR;
			}
			NextChar = MML_PeekNextChar(MML);
			if(NextChar != ')') {
				MML_AppendErrorCurrentOffset(MML, "Expected `)` to terminate nested duration.");
				return MML_ERROR;
			}
			MML_ConsumeChars(MML, 1, 0);
		} else if(NextChar == '#') {
			//! Read the raw number of ticks
			MML_ConsumeChars(MML, 1, 0);
			ThisDuration = MML_ReadDecimalOrHex(MML);
			if(ThisDuration == MML_ERROR) {
				MML_AppendErrorContext(MML, "While parsing duration as raw ticks:");
				return MML_ERROR;
			}
			if(ThisDuration == 0) {
				MML_AppendError(MML, "Number of ticks cannot be 0.", &ThisDurationOffs);
				return MML_ERROR;
			}
		} else if(MML_IS_DIGIT(NextChar)) {
			//! Read the divisor
			int Divisor = MML_ReadDecimalOrHex(MML);
			if(Divisor == MML_ERROR) {
				MML_AppendErrorContext(MML, "While parsing duration:");
				return MML_ERROR;
			}

			//! Ensure divisor gives an integer number of ticks
			if(Divisor == 0) {
				MML_AppendError(MML, "Time divisor cannot be 0.", &ThisDurationOffs);
				return MML_ERROR;
			}
			int Ticks     = (MML->TicksPerBeat * 4) / Divisor;
			int Remainder = (MML->TicksPerBeat * 4) % Divisor;
			if(Remainder != 0) {
				MML_AppendError(MML, "Divisor does not result in integer number of ticks.", &ThisDurationOffs);
				return MML_ERROR;
			}
			ThisDuration = Ticks;
		} else ThisDuration = MML->State.Duration;

		//! Check for dotted notes
		int32_t DottedAddition = ThisDuration;
		while(MML_PeekNextChar(MML) == '.') {
			//! Add progressively smaller halves to the duration
			if(DottedAddition % 2 != 0) {
				MML_AppendErrorCurrentOffset(MML, "Duration became non-integer.");
				return MML_ERROR;
			}
			DottedAddition /= 2;
			int64_t NewDuration = (int64_t)ThisDuration + (int64_t)DottedAddition;
			if(NewDuration != (int32_t)NewDuration) {
				MML_AppendError(MML, "Internal overflow (duration became too large).", &ThisDurationOffs);
				return MML_ERROR;
			}
			ThisDuration = (int32_t)NewDuration;
			MML_ConsumeChars(MML, 1, 0);
		}

		//! Check for multiplication
		while(MML_PeekNextChar(MML) == '*') {
			MML_ConsumeChars(MML, 1, 0);
			struct MML_InputOffs_t ThisMulOffs; MML_GetInputOffset(MML, &ThisMulOffs);

			int Mul = MML_ReadDecimalOrHex(MML);
			if(Mul == MML_ERROR) {
				MML_AppendErrorContext(MML, "While parsing duration multiplier:");
				return MML_ERROR;
			}
			int64_t ScaledDuration = (int64_t)ThisDuration * (int64_t)Mul;
			if(ScaledDuration != (int32_t)ScaledDuration) {
				MML_AppendError(MML, "Internal overflow (duration became too large).", &ThisMulOffs);
				return MML_ERROR;
			}
			ThisDuration = (int32_t)ScaledDuration;
		}

		//! Add to final duration
		int64_t FinalDuration = (int64_t)Duration + (int64_t)MML_ApplySign(ThisDuration, Sign);
		if(FinalDuration != (int32_t)FinalDuration) {
			MML_AppendError(MML, "Internal overflow (duration overflow/underflow).", &ThisDurationOffs);
			return MML_ERROR;
		}
		Duration = (int32_t)FinalDuration;

		//! Do we have a tie?
		if(MML_PeekNextChar(MML) != '^') break;
		MML_ConsumeChars(MML, 1, 0);
	}

	//! Modify into triplet as needed
	if(MML->State.IsTripletMode) {
		int64_t FinalDuration = (int64_t)Duration * 2ll / 3ll;
		int64_t Remainder     = (int64_t)Duration * 2ll % 3ll;
		if(Remainder != 0) {
			MML_AppendError(MML, "Triplet scaling caused non-integer number of ticks.", &DurationOffs);
			return MML_ERROR;
		}
		Duration = (int32_t)FinalDuration;
	}

	//! Sanity-check duration
	if(Duration <= 0) {
		MML_AppendError(MML, "Duration cannot be <= 0 ticks.", &DurationOffs);
		return MML_ERROR;
	}
	if(Duration > 65536 && NestLevel == 0) {
		MML_AppendError(MML, "Duration exceeded 65536 ticks.", &DurationOffs);
		return MML_ERROR;
	}

	//! All done
	return Duration;
}
int32_t MML_ReadDuration(struct MML_t *MML) {
	return MML_ReadNestedDuration(MML, 0);
}

/************************************************/

//! Read a label-compatible string
int MML_ReadLabelString(struct MML_t *MML, char **StrPtr) {
	//! Get extents of string
	uint32_t BegOffs = MML->Input.Offs.DataOffs; {
		for(;;) {
			int NextChar = MML_PeekNextChar(MML);
			if(!MML_IS_ALPHANUM_OR_UNDERSCORE(NextChar)) break;
			MML_ConsumeChars(MML, 1, 0);
		}
	}
	uint32_t EndOffs = MML->Input.Offs.DataOffs;
	uint32_t Length = EndOffs - BegOffs;
	if(BegOffs == EndOffs) { *StrPtr = NULL; return MML_OK; }

	//! Allocate memory and copy string
	char *String = *StrPtr = (char*)malloc((Length+1)*sizeof(char));
	if(!String) {
		MML->Input.Offs.DataOffs = BegOffs;
		MML_AppendErrorCurrentOffset(MML, "Out of memory while allocating string.");
		return MML_ERROR;
	}
	memcpy(String, MML->Input.Data + BegOffs, Length);
	String[Length] = '\0';
	return MML_OK;
}

/************************************************/

//! Create named label
static void PropagateNestingLevel(struct MML_Label_t *List, uint32_t LabelIdx, uint32_t MaxLevel) {
	if(LabelIdx == MML_LABELIDX_NULL) return;

	struct MML_Label_t *Label = &List[LabelIdx];
	if(MaxLevel > Label->NestLevel_Max) Label->NestLevel_Max = MaxLevel;
	PropagateNestingLevel(List, Label->ParentIdx, MaxLevel+1);
}
int MML_CreateLabel(
	struct MML_t *MML,
	char *LabelName,
	uint8_t LabelType,
	const struct MML_InputOffs_t *LabelNameOffs,
	const struct MML_InputOffs_t *CommandOffs
) {
	//! Ensure label does not already exist
	uint32_t i, ThisIdx = MML->nLabels;
	if(LabelName) for(i=0;i<ThisIdx;i++) {
		if(MML->LabelsList[i].Name && !strcmp(MML->LabelsList[i].Name, LabelName)) {
			MML_AppendError(MML, "Label already defined.", LabelNameOffs);
			return MML_ERROR;
		}
	}

	//! Check labels count
	if(ThisIdx >= MML_MAX_LABELS) {
		MML_AppendError(MML, "Exceeded maximum number of labels.", LabelNameOffs);
		return MML_ERROR;
	}

	//! Enlarge labels list
	struct MML_Label_t *NewList = (struct MML_Label_t*)realloc(MML->LabelsList, (ThisIdx+1) * sizeof(*NewList));
	if(!NewList) {
		MML_AppendError(MML, "Out of memory while allocating label entry.", LabelNameOffs);
		return MML_ERROR;
	}
	MML->LabelsList = NewList;

	//! Append label
	struct MML_Label_t *Label = &NewList[ThisIdx];
	uint32_t ParentIdx = MML_LABELIDX_NULL;
	if(MML->State.NestLevel_Current > 0) ParentIdx = MML->State.NestedLabelIdxList[MML->State.NestLevel_Current-1];
	Label->Type              = LabelType;
	Label->IsReferenced      = 0;
	Label->IsSelfReferenced  = 0;
	Label->UsesBreakCommand  = 0;
	Label->NestLevel_Max     = 0;
	Label->NestLevel_Pattern = MML->State.NestLevel_Pattern;
	Label->NestLevel_Repeat  = MML->State.NestLevel_Repeat;
	Label->EndLabelIdx       = MML_LABELIDX_NULL;
	Label->DataOffs          = MML->Output.Offs;
	Label->ParentIdx         = ParentIdx;
	Label->Name              = LabelName;
	Label->InputOffs         = *LabelNameOffs;
	MML->nLabels = ThisIdx+1;

	//! Reset running state, or commands such
	//! as MML_CMD_NOTE_LASTRUN can break!
	MML_ResetRunningState(MML);

	//! Prepare for pattern/repeat labels
	if(LabelType == MML_LABEL_TYPE_PATTERN || LabelType == MML_LABEL_TYPE_REPEAT) {
		//! Check current nesting level
		if(MML->State.NestLevel_Current >= MML_MAX_NESTING_LEVELS) {
			MML_AppendError(MML, "Maximum nesting level exceeded.", CommandOffs);
			return MML_ERROR;
		}

		//! Propagate nesting
		MML->State.NestedLabelIdxList[MML->State.NestLevel_Current++] = ThisIdx;
		switch(LabelType) {
			case MML_LABEL_TYPE_PATTERN: {
				MML->State.NestLevel_Pattern++;
			} break;
			case MML_LABEL_TYPE_REPEAT: {
				MML->State.NestLevel_Repeat++;
			} break;
		}
		PropagateNestingLevel(NewList, ParentIdx, 1);
	}

	//! All done
	return MML_OK;
}

/************************************************/

//! Create reference to label at current position
int MML_CreateReference(
	struct MML_t *MML,
	char *LabelName,
	uint32_t LabelIdx,
	uint8_t CmdType,
	uint8_t SelfRef,
	const struct MML_InputOffs_t *LabelNameOffs
) {
	//! Enlarge references list
	uint32_t ThisIdx = MML->nReferences;
	struct MML_Reference_t *NewList = (struct MML_Reference_t*)realloc(MML->ReferencesList, (ThisIdx+1) * sizeof(*NewList));
	if(!NewList) {
		MML_AppendErrorCurrentOffset(MML, "Out of memory while enlarging references list.");
		return MML_ERROR;
	}
	MML->ReferencesList = NewList;

	//! Append reference
	struct MML_Reference_t *Ref = &NewList[ThisIdx];
	Ref->CmdType   = CmdType;
	Ref->SelfRef   = SelfRef;
	Ref->NestLevel = MML->State.NestLevel_Current;
	Ref->DataOffs  = MML->Output.Offs;
	if(LabelName) Ref->ReferenceType = MML_REFERENCE_TYPE_NAMED,   Ref->Name = LabelName;
	else          Ref->ReferenceType = MML_REFERENCE_TYPE_INDEXED, Ref->Idx  = LabelIdx;
	Ref->InputOffs = *LabelNameOffs;
	MML->nReferences = ThisIdx+1;
	return MML_OK;
}

/************************************************/

//! Create new track allocation
int MML_CreateNewTrack(struct MML_t *MML) {
	//! Check track count
	if(MML->nTracks >= MML_MAX_TRACKS) {
		MML_AppendErrorCurrentOffset(MML, "Exceeded maximum number of tracks.");
		return MML_ERROR;
	}

	//! Enlarge tracks list
	struct MML_TrackListing_t *NewList = (struct MML_TrackListing_t*)realloc(MML->TracksList, (MML->nTracks+1) * sizeof(*NewList));
	if(!NewList) {
		MML_AppendErrorCurrentOffset(MML, "Out of memory while allocating track.");
		return MML_ERROR;
	}
	MML->TracksList = NewList;

	//! Reset track state
	NewList[MML->nTracks].Name     = NULL;
	NewList[MML->nTracks].DataOffs = MML->Output.Offs;
	MML->nTracks++;
	MML_ResetTrackState(MML);
	return MML_OK;
}

/************************************************/

//! Store last track to tracks list
void MML_StoreLastTrack(struct MML_t *MML) {
	if(!MML->nTracks) return;
	struct MML_TrackListing_t *TrackListing = &MML->TracksList[MML->nTracks-1];
	TrackListing->Size = MML->Output.Offs - TrackListing->DataOffs;
}

/************************************************/

//! Write nybble to stream
//! NOTE: It is NOT necessary to mask the nybble in Data.
int MML_WriteNybble(struct MML_t *MML, uint8_t Data) {
	struct MML_Output_t *Output = &MML->Output;
	if(!DynamicBuffer_WriteByte(Data, &Output->Data, &Output->Offs, &Output->Size)) {
		MML_AppendErrorCurrentOffset(MML, "Out of memory while enlarging allocation.");
		return MML_ERROR;
	}
	return MML_OK;
}

//! Write byte to stream
int MML_WriteByte(struct MML_t *MML, uint8_t Data) {
	if(
		MML_WriteNybble(MML, (uint8_t)(Data >> 4) & 0xF) == MML_ERROR ||
		MML_WriteNybble(MML, (uint8_t)(Data >> 0) & 0xF) == MML_ERROR
	) return MML_ERROR;
	return MML_OK;
}

//! Write word to stream
int MML_WriteWord(struct MML_t *MML, uint16_t Data) {
	if(
		MML_WriteNybble(MML, (uint8_t)(Data >> 12) & 0xF) == MML_ERROR ||
		MML_WriteNybble(MML, (uint8_t)(Data >>  8) & 0xF) == MML_ERROR ||
		MML_WriteNybble(MML, (uint8_t)(Data >>  4) & 0xF) == MML_ERROR ||
		MML_WriteNybble(MML, (uint8_t)(Data >>  0) & 0xF) == MML_ERROR
	) return MML_ERROR;
	return MML_OK;
}

//! Write command to stream
int MML_WriteCommandData(struct MML_t *MML, uint32_t Data) {
#define DO_NYBBLE(x) \
	if((x) == 0 || (Data) >= (1 << (4*(x)))) { \
		if(MML_WriteNybble(MML, (uint8_t)(Data >> (4*(x))) & 0xF) == MML_ERROR) { \
			return MML_ERROR; \
		} \
	}
	//DO_NYBBLE(3);
	//DO_NYBBLE(2);
	DO_NYBBLE(1);
	DO_NYBBLE(0);
	return MML_OK;
#undef DO_NYBBLE
}

//! Write TimeCode to stream
int MML_WriteTimeCode(struct MML_t *MML, uint32_t nTicks) {
	//! Up to 768 ticks, it might be cheaper to use time coding
	if(nTicks <= 768) {
		//! Get the code from the LUT
		const struct MML_TicksToTimeCode_t *CodeData = &MML_TicksToTimeCodeLUT[nTicks-1];
		int nNybbles = CodeData->nNybbles;
		if(nNybbles != 0) {
			//! Cheaper to encode using LUT: Write the code
			uint16_t Code = CodeData->a | (CodeData->b << 8);
			do {
				if(MML_WriteNybble(MML, (uint8_t)Code & 0xF) == MML_ERROR) {
					return MML_ERROR;
				}
				Code >>= 4;
			} while(--nNybbles);
			return MML_OK;
		}
	}

	//! Cheaper to write the number of ticks directly
	if(
		MML_WriteNybble(MML, 0xF)                  == MML_ERROR ||
		MML_WriteWord  (MML, (uint16_t)(nTicks-1)) == MML_ERROR
	) return MML_ERROR;
	return MML_OK;
}

//! Remove nybbles from stream
int MML_RemoveNybbles(struct MML_t *MML, uint32_t Offset, uint32_t Count) {
	struct MML_Output_t *Output = &MML->Output;

	//! Shift data to the left
	if(Output->Offs < Offset || Offset+Count > Output->Offs) {
		MML_AppendErrorCurrentOffset(MML, "Received request to remove nybbles that are out of range. Please report this error.");
		return MML_ERROR;
	}
	memcpy(Output->Data + Offset, Output->Data + Offset + Count, Output->Offs - Offset - Count);
	Output->Offs -= Count;

	//! Fix any labels and references that happen after Offset
	uint32_t i;
	for(i=0;i<MML->nLabels;i++) {
		struct MML_Label_t *Label = &MML->LabelsList[i];
		if(Label->DataOffs > Offset) {
			if(Label->DataOffs < Offset+Count) {
				MML_AppendErrorCurrentOffset(MML, "A label references nybbles to be removed. Please report this error.");
				return MML_ERROR;
			}
			Label->DataOffs -= Count;
		}
	}
	for(i=0;i<MML->nReferences;i++) {
		struct MML_Reference_t *Ref = &MML->ReferencesList[i];
		if(Ref->DataOffs > Offset) {
			if(Ref->DataOffs < Offset+Count) {
				MML_AppendErrorCurrentOffset(MML, "A reference references nybbles to be removed. Please report this error.");
				return MML_ERROR;
			}
			Ref->DataOffs -= Count;
		}
	}
	return MML_OK;
}

/************************************************/

//! Display error/warning
void MML_DisplayError(struct MML_t *MML, const struct MML_ErrorInfo_t *Error) {
#define CHAR_IS_DISPLAYABLE(x) ((uint8_t)(x) >= 0x20 && (uint8_t)(x) < 0x7F)
	if(!Error->String) return;

	int i;
	int64_t Offs;
	const int INDENT_WIDTH  = 2;
	const int DISPLAY_WIDTH = 80 - INDENT_WIDTH;
	const char *Data = MML->Input.Data;

	//! First, print the file offset and actual error
	if(Error->Offs.DataOffs < MML->Input.Size) {
		size_t Line   = Error->Offs.Line + 1;
		size_t Column = Error->Offs.DataOffs - Error->Offs.LineOffs + 1;
		DrawSeparator();
		printf("Line %zu, column %zu: %s\n", Line, Column, Error->String);
	} else {
		puts(Error->String);
		return;
	}

	//! Get display offsets
	int64_t ErrorOffs = (int64_t)Error->Offs.DataOffs;
	int64_t OffsBeg = ErrorOffs - DISPLAY_WIDTH/2;
	int64_t OffsEnd = OffsBeg + DISPLAY_WIDTH;

	//! If we have a LF somewhere in the string, crop
	//! NOTE: Some systems use CR+LF encoding, so check for that too
	for(Offs=ErrorOffs-1;Offs>=OffsBeg;Offs--) {
		if(Data[Offs] == '\n') {
			OffsBeg = Offs+1;
			break;
		}
	}
	for(Offs=ErrorOffs+1;Offs<OffsEnd;Offs++) {
		if(Data[Offs] == '\r' || Data[Offs] == '\n') {
			OffsEnd = Offs;
			break;
		}
	}

	//! Scan for undisplayable characters and count them.
	//! Undisplayable characters are shown with "\x??' coding, taking 4
	//! characters of width, so adjust offsets based on this behaviour
	int nUndisplayable = 0;
	for(Offs=OffsBeg;Offs<OffsEnd;Offs++) {
		if(OffsBeg > 0 && OffsEnd < (int64_t)MML->Input.Size) {
			if(!CHAR_IS_DISPLAYABLE(Data[Offs])) nUndisplayable++;
		}
	}
	OffsBeg += nUndisplayable*4 / 2 - nUndisplayable;
	OffsEnd -= nUndisplayable*4 / 2 - nUndisplayable;

	//! Ensure offset is not out of bounds
	if(OffsBeg < 0) OffsBeg = 0;
	if(OffsEnd > (int64_t)MML->Input.Size) OffsEnd = (int64_t)MML->Input.Size;

	//! Show line of text
	for(i=0;i<INDENT_WIDTH;i++) putchar(' ');
	for(Offs=OffsBeg;Offs<OffsEnd;Offs++) {
		char x = Data[Offs];
		if(CHAR_IS_DISPLAYABLE(x)) {
			putchar(x);
		} else printf("\\x%02X", x);
	}

	//! Next, put a ^ below the error point
	putchar('\n');
	for(i=0;i<INDENT_WIDTH;i++) putchar(' ');
	for(Offs=OffsBeg;Offs<ErrorOffs;Offs++) {
		if(CHAR_IS_DISPLAYABLE(Data[Offs])) putchar(' ');
		else printf("    ");
	}
	puts("^");
#undef CHAR_IS_DISPLAYABLE
}

/************************************************/

//! Display last error
void MML_DisplayLastError(struct MML_t *MML) {
	uint32_t i, N = MML->ErrorDepth;
	for(i=0;i<N;i++) {
		uint32_t n = i+1;
		do putchar(' '); while(--n);
		if(MML->ErrorData[N-1-i].String) MML_DisplayError(MML, &MML->ErrorData[N-1-i]);
		else printf("Received error condition with no error string. Please report this error.\n");
	}
}

/************************************************/

//! Append error to list
void MML_AppendError(struct MML_t *MML, const char *Error, const struct MML_InputOffs_t *CommandOffs) {
	//! Shift out oldest error if needed
	if(MML->ErrorDepth >= MML_MAX_ERROR_DEPTH) {
		uint32_t i;
		for(i=0;i<MML_MAX_ERROR_DEPTH-1;i++) {
			MML->ErrorData[i] = MML->ErrorData[i+1];
		}
		MML->ErrorDepth = MML_MAX_ERROR_DEPTH-1;
	}

	//! Write error data
	struct MML_ErrorInfo_t *Entry = &MML->ErrorData[MML->ErrorDepth++];
	Entry->IsAllocated = 0;
	Entry->String      = Error;
	if(CommandOffs) Entry->Offs = *CommandOffs;
	else Entry->Offs.DataOffs = MML->Input.Size;
}

//! Append formatted error context to list
void MML_vAppendErrorContext(struct MML_t *MML, const char *ErrorFormat, ...) {
	va_list vl;
	va_start(vl, ErrorFormat);
	int Length = vsnprintf(NULL, 0, ErrorFormat, vl);
	char *Buf = (Length > 0) ? malloc(Length + 1) : NULL;
	if(Buf) {
		vsnprintf(Buf, Length+1, ErrorFormat, vl);
		MML_AppendErrorContext(MML, Buf);
		MML->ErrorData[MML->ErrorDepth-1].IsAllocated = 1;
	} else {
		//! If we can't allocate the string, just use the format
		//! data directly. This is definitely not ideal, but
		//! gives more context than if the error message was
		//! disabled altogether
		MML_AppendErrorContext(MML, ErrorFormat);
	}
	va_end(vl);
}

/************************************************/

//! Append warning to list
int MML_AppendWarning(struct MML_t *MML, const char *Warning, const struct MML_InputOffs_t *CommandOffs) {
	//! Enlarge warnings list
	struct MML_ErrorInfo_t *NewList = (struct MML_ErrorInfo_t*)realloc(MML->WarningsList, (MML->nWarnings+1) * sizeof(*NewList));
	if(!NewList) {
		MML_AppendErrorCurrentOffset(MML, "Out of memory while allocating warning.");
		return MML_ERROR;
	}
	MML->WarningsList = NewList;

	//! Add warning
	struct MML_ErrorInfo_t *Entry = &NewList[MML->nWarnings++];
	Entry->String = Warning;
	if(CommandOffs) Entry->Offs = *CommandOffs;
	else Entry->Offs.DataOffs = MML->Input.Size;
	return MML_OK;
}

/************************************************/

//! Display all warnings
void MML_DisplayWarnings(struct MML_t *MML) {
	uint32_t i;
	for(i=0;i<MML->nWarnings;i++) {
		MML_DisplayError(MML, &MML->WarningsList[i]);
	}
}

/************************************************/
//! EOF
/************************************************/
