/************************************************/
#pragma once
/************************************************/
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
/************************************************/
#ifdef __cplusplus
extern "C" {
#endif
/************************************************/

//! Helper macros
#define MML_IS_IN_RANGE(x, Min, Max) ((x) >= (Min) && (x) <= (Max))
#define MML_IS_DIGIT(x)     (MML_IS_IN_RANGE(x, '0', '9'))
#define MML_IS_HEX_DIGIT(x) (MML_IS_IN_RANGE(x, '0', '9') || MML_IS_IN_RANGE(x, 'a', 'f') || MML_IS_IN_RANGE(x, 'A', 'F'))
#define MML_IS_ALPHANUM(x)  (MML_IS_DIGIT(x) || MML_IS_IN_RANGE(x, 'a', 'z') || MML_IS_IN_RANGE(x, 'A', 'Z'))
#define MML_IS_ALPHANUM_OR_UNDERSCORE(x) (MML_IS_ALPHANUM(x) || (x) == '_')
#define MML_IS_POWER_OF_2(x) (((x) &~ (-(x))) == 0)
#define MML_DIGIT_TO_NUMBER(x)    ((x) - '0')
#define MML_HEXDIGIT_TO_NUMBER(x) (MML_IS_DIGIT(x) ? MML_DIGIT_TO_NUMBER(x) : (10 + (((x) >= 'a') ? ((x) - 'a') : ((x) - 'A'))))

/************************************************/

//! MML definitions
enum {
	MML_TICKS_PER_QUARTER_NOTE    = 48,
	MML_TICKS_PER_WHOLE_NOTE      = 4*MML_TICKS_PER_QUARTER_NOTE,
	MML_MAX_DURATION_NESTING      = 8,     //! No hard limit, but may cause stack overflows if left too large
	MML_MAX_NOTESTACK_DEPTH       = 5,     //! This is based on the StackPush command
	MML_MAXIMUM_NOTESTACK_SIZE    = 16,    //! Max 255 (this includes commands embedded in note stacks)
	MML_MAX_TRACKS                = 255,   //! Max 255
	MML_MAX_LABELS                = 65536, //! Max INT_MAX
	MML_MAX_NESTING_LEVELS        = 4,     //! Max 255
	MML_LABELIDX_NULL             = 0xFFFFFFFFu, //! Used to mark label indices as NULL
	MML_REFERENCEIDX_NULL         = 0xFFFFFFFFu, //! Used to mark end-of-list for references
	MML_REFERENCE_TYPE_NAMED      = 0,     //! Use ::Name
	MML_REFERENCE_TYPE_INDEXED    = 1,     //! Use ::Idx
	MML_REFERENCE_TYPE_ENDOF      = 2,     //! Use ::Idx, then use that label's ::EndIdx
	MML_REFERENCE_CMDTYPE_PATTERN = 0,     //! Reference is used by a pattern recall
	MML_REFERENCE_CMDTYPE_REPEAT  = 1,     //! Reference is used by a section repeat
	MML_REFERENCE_CMDTYPE_BREAK   = 2,     //! Reference is used by a break command
	MML_REFERENCE_CMDTYPE_GOTO    = 3,     //! Reference is used by a goto-type command
	MML_NOTEMUL_RESOLUTION        = 32,    //! Must be a power of 2 and match the driver
	MML_MAX_ERROR_DEPTH           = 32,    //! Maximum error nesting level

	//! MML_Label_t
	MML_LABEL_TYPE_GENERIC = 0,
	MML_LABEL_TYPE_PATTERN = 1,
	MML_LABEL_TYPE_REPEAT  = 2,

	//! Commands
	MML_CMD_NOTE_TIMED     = 0x0,
	MML_CMD_NOTE_LASTRUN   = 0x1,
	MML_CMD_NOTE_1_4       = 0x2,
	MML_CMD_NOTE_1_8       = 0x3,
	MML_CMD_NOTE_1_16      = 0x4,
	MML_CMD_NOTE_1_32      = 0x5,
	MML_CMD_NOTE_1_64      = 0x6,
	MML_CMD_REST           = 0x7,
	MML_CMD_OCTAVE         = 0x8,
	MML_CMD_VELOCITY       = 0x9,
	MML_CMD_VOLUME         = 0xA,
	MML_CMD_EXPRESSION     = 0xB,
	MML_CMD_PANNING        = 0xC,
	MML_CMD_PITCHBEND      = 0xD,
	MML_CMD_PORTAMENTO_ON  = 0xE0,
	MML_CMD_PORTAMENTO_OFF = 0xE1,
	MML_CMD_REPEATSTART    = 0xE2, //! <- Only used by nested repeats
	MML_CMD_TRANSPOSE      = 0xF0,
	MML_CMD_TRANSPOSEADD   = 0xF1,
	MML_CMD_NOTEMULADD     = 0xF2,
	MML_CMD_PRIORITY       = 0xF3,
	MML_CMD_PROGRAM        = 0xF4,
	MML_CMD_TEMPO          = 0xF5,
	MML_CMD_JUMP           = 0xF6,
	MML_CMD_REPEAT         = 0xF7,
	MML_CMD_CALL           = 0xF8,
	MML_CMD_CALLCNT        = 0xF9,
	MML_CMD_GOTOIF         = 0xFA,
	MML_CMD_SIGNAL         = 0xFB,
	MML_CMD_BREAK          = 0xFC,
	MML_CMD_RETURN         = 0xFD,
	MML_CMD_INLINEPATTERN  = 0xFE,
	MML_CMD_END            = 0xFF,
	MML_CMD_NULL           = 0x10, //! <- Used by compiler only
	MML_CMD_COMMENT        = 0x11, //! <- Used by compiler only

	//! Sub-commands
	MML_CMD_NOTE_OCTAVE_DOWN = 0xC,
	MML_CMD_NOTE_OCTAVE_UP   = 0xD,
	MML_CMD_NOTE_OVERLAY     = 0xE,
	MML_CMD_NOTE_VELOCITY    = 0xF0,
	MML_CMD_NOTE_OCTAVE_SET  = 0xF1, //! Fh,1h..Bh = Octave 0..10
	MML_CMD_NOTE_STACKPUSH   = 0xFC, //! Fh,Ch..Fh = 1..4 extra notes
	MML_CMD_OCTAVE_RELATIVE  = 0xB,
	MML_CMD_OCTAVE_DOWN      = 0xC,
	MML_CMD_OCTAVE_UP        = 0xD,
	MML_CMD_OCTAVE_DOWN2     = 0xE,
	MML_CMD_OCTAVE_UP2       = 0xF,
};

/************************************************/

//! Function return codes
enum {
	MML_OK    =  0,
	MML_ERROR = -1, //! Error information is appended into the MML structure
	MML_EOF   = -2, //! Only used internally by MML_Parse() and MML_Audit()
	MML_REST  = -3, //! Only used internally by MML_Audit()
	MML_JUMP  = -4, //! Only used internally by MML_Audit()

	//! These codes are used by MML_FromMIDI()
	MML_MIDI_NOT_MIDIFILE   = -5,
	MML_MIDI_CORRUPTED      = -6,
	MML_MIDI_UNSUPPORTED    = -7,
	MML_MIDI_OUT_OF_MEMORY  = -8,
	MML_MIDI_UNEXPECTED_EOF = -9,
	MML_MIDI_TOO_LONG       = -10,
	MML_MIDI_PRINTF_ERROR   = -11,
	MML_MIDI_INTERNAL_ERROR = -12,
};

/************************************************/

//! Ticks -> TimeCode LUT (nTicks = 1..768)
//! This table was calculated by bruteforce search, so the timings might look weird...
struct MML_TicksToTimeCode_t {
	uint8_t nNybbles;
	uint8_t a, b; //! {Nybble0|Nybble1<<4, Nybble2|Nybble3<<4}
};
extern const struct MML_TicksToTimeCode_t MML_TicksToTimeCodeLUT[768];

/************************************************/

//! MML helper structures
struct MML_InputOffs_t {
	uint32_t DataOffs; //! Offset into MML->Input.Data[] for the data position
	uint32_t LineOffs; //! Offset into MML->Input.Data[] for the start of line
	uint32_t Line;     //! Line index
};
struct MML_Input_t {
	const char *Data;
	uint32_t    Size;
	struct MML_InputOffs_t Offs;
};
struct MML_Output_t {
	uint8_t *Data;
	uint32_t Offs; //! Offset into MML->Output.Data[] for next data entry
	uint32_t Size;
};

//! MML controller structure
struct MML_Controller_t {
	uint16_t Value;
	uint16_t Target;
	uint32_t Duration;
	uint64_t StartTick;
};

//! MML label and reference structures
struct MML_Label_t {
	uint8_t  Type;
	uint8_t  IsReferenced;
	uint8_t  IsSelfReferenced;
	uint8_t  UsesBreakCommand;  //! Create a label at the end of the definition
	uint8_t  NestLevel_Max;     //! Number of nesting levels used inside this label
	uint8_t  NestLevel_Pattern; //! Nesting levels at the time of definition (used for sanity checks)
	uint8_t  NestLevel_Repeat;
	uint32_t EndLabelIdx;       //! Label pointing to the end of this definition (if needed)
	uint32_t DataOffs;          //! Offset into MML->Output.Data[] for the start of the label data
	uint32_t ParentIdx;         //! This is used to propagate nesting levels upwards
	char    *Name;
	struct MML_InputOffs_t InputOffs;
};
struct MML_Reference_t {
	uint8_t  CmdType;           //! Type of command that uses the reference
	uint8_t  SelfRef;           //! Self-referencing command (eg. `[abc]2` vs `(Recall)`)
	uint8_t  NestLevel;         //! Nesting levels at the time of definition (used for sanity checks)
	uint32_t DataOffs;          //! Offset into MML->Output.Data[] where to insert the reference
	uint32_t ReferenceType;
	union { char *Name; uint32_t Idx; }; //! Label name OR index into LabelsList[]
	struct MML_InputOffs_t InputOffs;

	union {
		//! During resolving
		struct {
			uint32_t NextRefInDstOrder; //! Next reference (sorted by descending DstOffs)
			uint32_t DstOffs;      //! Offset into MML->Output.Data[] of resolved target
			uint32_t nNybblesLast; //! Number of nybbles needed for DstOffs-Offs in last iteration
			 int32_t DstOffsDelta; //! Nybbles to add to DstOffs
		};

		//! After resolving
		struct {
			uint32_t Value;    //! Value to encode in output
			 int32_t nNybbles; //! Nybbles in Value
		};
	};
};

//! MML error information structure
struct MML_ErrorInfo_t {
	uint8_t IsAllocated;
	const char *String;
	struct MML_InputOffs_t Offs;
};

//! MML track structure
struct MML_TrackListing_t {
	char     *Name;
	uint32_t  Size;
	uint32_t  DataOffs; //! Offset into MML->Output.Data[] for start of track
};

//! MML parser structure
//! MML_NotifyMIDIProgramChange() must return a local program index (or a negative value for errors).
typedef int (*MML_NotifyMIDIProgramChangeFnc_t)(
	void *Userdata,
	uint8_t Patch,
	uint8_t CC0,
	uint8_t CC32,
	uint8_t IsDrumKit,
	uint8_t UseGlobalToneBank
);
typedef int (*MML_NotifyKeyOnFnc_t)(
	void *Userdata,
	uint8_t Program,
	uint8_t Key,
	uint8_t Vel,
	uint8_t UseGlobalToneBank
);
struct MML_t {
	uint32_t ErrorDepth;
	struct MML_ErrorInfo_t ErrorData[MML_MAX_ERROR_DEPTH];

	//! Callbacks
	void *NotifyUserdata;
	MML_NotifyMIDIProgramChangeFnc_t NotifyMIDIProgramChange;
	MML_NotifyKeyOnFnc_t NotifyKeyOn;

	//! Global state
	uint8_t UseGlobalToneBank;
	int32_t TicksPerBeat;

	//! Current track state
	struct {
		uint32_t LastCommand;
		uint8_t  NestLevel_Current;
		uint8_t  NestLevel_Pattern;
		uint8_t  NestLevel_Repeat;
		 int8_t  QueuedOctaveChange;
		uint8_t  QueuedOctaveSet;
		uint8_t  IsTripletMode;
		uint32_t Duration;
		uint32_t OutputDuration;
		uint32_t LastDefinedPatternLabelIdx; //! Used for `*` recall command
		uint32_t NestedLabelIdxList[MML_MAX_NESTING_LEVELS];
		struct MML_InputOffs_t TripletModeOffs;
	} State;

	//! Input/output data streams
	struct MML_Input_t  Input;
	struct MML_Output_t Output;

	//! Track listings
	uint32_t nTracks;
	struct MML_TrackListing_t *TracksList;

	//! Label listings
	uint32_t nLabels;
	struct MML_Label_t *LabelsList;

	//! Reference fixup listings
	uint32_t nReferences;
	struct MML_Reference_t *ReferencesList;

	//! Warnings listings
	uint32_t nWarnings;
	struct MML_ErrorInfo_t *WarningsList;
};

/************************************************/

//! Initialize parser
int MML_Init(
	struct MML_t *MML,
	const char *Data,
	uint32_t DataSize,
	MML_NotifyMIDIProgramChangeFnc_t NotifyMIDIProgramChange,
	MML_NotifyKeyOnFnc_t             NotifyKeyOn,
	void                            *NotifyUserdata,
	uint8_t UseGlobalToneBank
);

//! Destroy parser
void MML_Destroy(struct MML_t *MML);

//! Get current input offset
inline
void MML_GetInputOffset(struct MML_t *MML, struct MML_InputOffs_t *Dst) {
	*Dst = MML->Input.Offs;
}

//! Display error/warning
//! Additionally, if the offset of the error is inside the file being
//! parsed (Error->Offs.DataOffs < MML->Input.Size), a graphical
//! display will be set up to show the position of the error.
void MML_DisplayError(struct MML_t *MML, const struct MML_ErrorInfo_t *Error);

//! Display last error
//! Note that errors will be displayed in reverse order that they were
//! pushed into the list (ie. LIFO order). For example:
//!  Append("Invalid value");
//!  Append("While parsing duration:");
//! Will output as:
//!  > While parsing duration:
//!  >  Invalid value
void MML_DisplayLastError(struct MML_t *MML);

//! Display all warnings
void MML_DisplayWarnings(struct MML_t *MML);

//! Append error to list
void MML_AppendError(struct MML_t *MML, const char *Error, const struct MML_InputOffs_t *CommandOffs);
inline
void MML_AppendErrorCurrentOffset(struct MML_t *MML, const char *Error) {
	MML_AppendError(MML, Error, &MML->Input.Offs);
}
inline
void MML_AppendErrorGlobal(struct MML_t *MML, const char *Error) {
	MML_AppendError(MML, Error, NULL);
}
inline
void MML_AppendErrorContext(struct MML_t *MML, const char *Error) {
	MML_AppendErrorGlobal(MML, Error);
}
void MML_vAppendErrorContext(struct MML_t *MML, const char *ErrorFormat, ...);

//! Append warning to list
int MML_AppendWarning(struct MML_t *MML, const char *Warning, const struct MML_InputOffs_t *CommandOffs);
inline
int MML_AppendWarningCurrentOffset(struct MML_t *MML, const char *Warning) {
	return MML_AppendWarning(MML, Warning, &MML->Input.Offs);
}
inline
int MML_AppendWarningGlobal(struct MML_t *MML, const char *Warning) {
	return MML_AppendWarning(MML, Warning, NULL);
}

//! Reset track state
inline
void MML_ResetTrackState(struct MML_t *MML) {
	MML->State.LastCommand        = MML_CMD_NULL;
	MML->State.NestLevel_Current  = 0;
	MML->State.NestLevel_Pattern  = 0;
	MML->State.NestLevel_Repeat   = 0;
	MML->State.QueuedOctaveChange = 0;
	MML->State.QueuedOctaveSet    = 0xFF;
	MML->State.IsTripletMode      = 0;
	MML->State.Duration           = 0;
	MML->State.LastDefinedPatternLabelIdx = MML_LABELIDX_NULL;
	MML->State.OutputDuration     = 0;
}

//! This is needed at the start of patterns and repeat sections to ensure
//! that any running state isn't carried over, since it can be different
//! when the section repeats itself/is recalled.
inline
void MML_ResetRunningState(struct MML_t *MML) {
	MML->State.OutputDuration = 0;
}

/************************************************/

//! Consume characters (and optionally skip whitespace)
//! NOTE: The line counter is only updated with ConsumeWhitespace=1.
void MML_ConsumeChars(struct MML_t *MML, uint32_t nChars, uint8_t ConsumeWhitespace);
inline
void MML_ConsumeWhitespace(struct MML_t *MML) {
	MML_ConsumeChars(MML, 0, 1);
}

//! Peek at next character without advancing
int MML_PeekNextChar(const struct MML_t *MML);

//! Check if the following characters match a string
//! When AsToken != 0, then a match is successful only if the next character
//! following the string is not alphanumeric.
//! Returns 0 on no match, 1 on match
int MML_PeekStringMatch(const struct MML_t *MML, const char *String, uint8_t AsToken);

//! Check if the following characters match a string, and consume on success
//! Exactly the same as MML_PeekStringMatch(), but includes consuming the characters.
//! Returns 0 on no match, 1 on match
int MML_StringMatchAndConsume(struct MML_t *MML, const char *String, uint8_t AsToken);

//! Consume one character and peek the next
//! NOTE: This does NOT consume whitespace.
inline
int MML_ConsumeCharAndPeekNext(struct MML_t *MML) {
	MML_ConsumeChars(MML, 1, 0);
	return MML_PeekNextChar(MML);
}

//! Read a sign character (if any)
//! Returns -1 for negative (-), and 0 for positive (+; default)
//! NOTE: Does NOT return MML_ERROR.
//! NOTE: Does NOT skip any whitespace following any sign character.
int32_t MML_ReadSign(struct MML_t *MML);
inline
int32_t MML_ApplySign(int32_t Value, int32_t Sign) {
	return (Value ^ Sign) - Sign;
}

//! Read decimal or hex number (hex numbers prefixed with 0x) in range 0..7FFFFFFFh
int32_t MML_ReadDecimalOrHex(struct MML_t *MML);

//! Read a double-precision floating-point value
//! NOTE: This is not necessarily precise and is designed for catching errors.
//! NOTE: Cannot return MML_ERROR; returns NAN (math.h) on error instead.
double MML_ReadDouble(struct MML_t *MML);

//! Read duration data (eg. "a32", "a..", ties, etc.) and return number of ticks
//! NOTE: Uses default duration whereever a divisor (or tick count) isn't specified.
int MML_ReadDuration(struct MML_t *MML);

//! Read a label-compatible string (alphanumeric plus underscore)
//! Returns MML_OK or MML_ERROR. If the string is empty, sets StrPtr=NULL.
//! Returned pointer must be destroyed using free().
int MML_ReadLabelString(struct MML_t *MML, char **StrPtr);

/************************************************/

//! Create named label
//! Notes:
//!  -LabelName can be NULL (used for anonymous patterns).
//!  -The label's parent will automatically be assigned from MML->State,
//!   and when used with PATTERN or REPEAT type, nesting is increased.
int MML_CreateLabel(
	struct MML_t *MML,
	char *LabelName,
	uint8_t LabelType,
	const struct MML_InputOffs_t *LabelNameOffs,
	const struct MML_InputOffs_t *CommandOffs
);

//! Create reference to label at current position
//! Notes:
//!  -If LabelName is NULL, then LabelIdx is used instead.
int MML_CreateReference(
	struct MML_t *MML,
	char *LabelName,
	uint32_t LabelIdx,
	uint8_t CmdType,
	uint8_t SelfRef,
	const struct MML_InputOffs_t *LabelNameOffs
);

//! Create new track allocation
//! NOTE: This will NOT save the current track data, and if the pointer is not
//! saved or freed, its allocated memory will become unreachable.
int MML_CreateNewTrack(struct MML_t *MML);

//! Store last track to tracks list
void MML_StoreLastTrack(struct MML_t *MML);

//! Write nybble to stream
//! NOTE: It is NOT necessary to mask nybble in Data.
int MML_WriteNybble(struct MML_t *MML, uint8_t Data);

//! Write byte/word to stream
int MML_WriteByte(struct MML_t *MML, uint8_t Data);
int MML_WriteWord(struct MML_t *MML, uint16_t Data);

//! Write command to stream
//! This writes nybbles of the command in Data, MSB first.
int MML_WriteCommandData(struct MML_t *MML, uint32_t Data);

//! Write TimeCode to stream
//! This uses raw ticks or standard timing, whatever is most efficient.
int MML_WriteTimeCode(struct MML_t *MML, uint32_t nTicks);

//! Remove nybbles from stream
//! NOTE: This should only be used while still parsing command data.
int MML_RemoveNybbles(struct MML_t *MML, uint32_t Offset, uint32_t Count);

//! Get length of command
//! NOTE: Because this is inline, then ideally the command should be constexpr.
static inline
int MML_GetCommandLength(uint32_t Command) {
	if(Command >= 0x1000) return 4;
	if(Command >= 0x0100) return 3;
	if(Command >= 0x0010) return 2;
	return 1;
}

/************************************************/

//! Parse data from input stream
//! Returns MML_OK or MML_ERROR (with MML->Error being set).
//! On success, stores to MML->nTracks and MML->TracksList[].
int MML_Parse(struct MML_t *MML);

//! Audit track data to cull any unused regions from the tone bank
//! NOTE: This function must only be used after MML_Parse() has
//! returned MML_OK.
int MML_Audit(struct MML_t *MML);

/************************************************/

//! Convert MIDI file to MML
//! Returns MML_OK or MML_ERROR (with MML->Error being set).
int MML_FromMIDI(
	struct MML_t *MML,
	FILE *MTFile,
	MML_NotifyMIDIProgramChangeFnc_t NotifyMIDIProgramChange,
	MML_NotifyKeyOnFnc_t             NotifyKeyOn,
	void                            *NotifyUserdata
);

//! Convert MIDI file to textual MML
//! Returns MML_OK, or an error code (see MML_MIDI_x error codes).
int MML_TextFromMIDI(char **MMLBufPtr, FILE *MTFile);

/************************************************/
#ifdef __cplusplus
}
#endif
/************************************************/
//! EOF
/************************************************/
