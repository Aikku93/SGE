/************************************************/
#include <stdint.h>
/************************************************/
#include "GlobalHelpers.h"
/************************************************/

static int Channel_ConvertToText_GetByte(const uint8_t **SrcBufPtr, const uint8_t *EndBuf) {
	if(*SrcBufPtr >= EndBuf) return MML_MIDI_INTERNAL_ERROR;

	const uint8_t *SrcBuf = *SrcBufPtr;
	uint8_t Byte = *SrcBuf++;
	*SrcBufPtr = SrcBuf;
	return Byte;
}

/************************************************/

static int Channel_ConvertToText_Note(
	const uint8_t **SrcBufPtr,
	const uint8_t  *EndBuf,
	char **BufPtr,
	uint32_t *BufOffsPtr,
	uint32_t *BufSizePtr
) {
	//! Notes are stored in 5 bytes:
	//!  NOTE_TIMED, DurationLo, DurationHi, Key, Vel
	const uint8_t *SrcBuf = *SrcBufPtr;
	if(*SrcBufPtr+5 > EndBuf) return MML_MIDI_INTERNAL_ERROR;
	*SrcBufPtr = SrcBuf+5;

	//! Get note parameters
	uint16_t Duration = (uint16_t)SrcBuf[1] | ((uint16_t)SrcBuf[2]) << 8;
	uint8_t  Key      = SrcBuf[3];
	uint8_t  Vel      = SrcBuf[4];

	//! Scan ahead to see how many notes of the same duration we have
	uint32_t NotesToStack = 1; {
		const uint8_t *ScanSrcBuf = SrcBuf + 5;
		for(;;) {
			if(ScanSrcBuf+5 > EndBuf) break;
			if(ScanSrcBuf[0] != MML_CMD_NOTE_TIMED) break;
			uint16_t TestDuration = (uint16_t)ScanSrcBuf[1] | ((uint16_t)ScanSrcBuf[2]) << 8;
			if(TestDuration != Duration) break;

			//! Found a note with matching duration
			NotesToStack++;
			ScanSrcBuf += 5;
		}
	}

	//! If we're stacking notes, insert braces
	if(NotesToStack > 1) {
		if(!DynamicBuffer_WriteByte('{', BufPtr, BufOffsPtr, BufSizePtr)) return MML_MIDI_PRINTF_ERROR;
	}

	//! Append velocity command if needed
	Result = DynamicBuffer_WriteFormatted(
		"v%u",
		BufPtr,
		BufOffsPtr,
		BufSizePtr,
		Vel
	);
	if(!Result) return MML_MIDI_PRINTF_ERROR;
}

/************************************************/

static int Channel_ConvertToText(struct MidiChannel_t *Channel, char **BufPtr, uint32_t *BufOffsPtr, uint32_t *BufSizePtr) {
	const uint8_t *SrcBuf = Channel->DataBuffer;
	const uint8_t *EndBuf = SrcBuf + Channel->DataBufferOffs;
	for(;;) {
		if(SrcBuf+1 > EndBuf) return MML_MIDI_INTERNAL_ERROR;
		switch(SrcBuf[0]) {
			case MML_CMD_NOTE_TIMED: {
				Result = Channel_ConvertToText_Note(&SrcBuf, EndBuf, BufPtr, BufOffsPtr, BufSizePtr);
				if(Result < 0) return Result;
			} break;
			case MML_CMD_REST: {
			} break;
			case MML_CMD_VOLUME: {
			} break;
			case MML_CMD_EXPRESSION: {
			} break;
			case MML_CMD_PANNING: {
			} break;
			case MML_CMD_PITCHBEND: {
			} break;
			case MML_CMD_PROGRAM: {
			} break;
			case MML_CMD_TEMPO: {
			} break;
			case MML_CMD_COMMENT: {
			} break;
			default: return MML_MIDI_INTERNAL_ERROR;
		}
	}
	return MML_OK;
}

/************************************************/
//! EOF
/************************************************/
