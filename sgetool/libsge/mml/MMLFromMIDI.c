#if 0
/************************************************/
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
/************************************************/
#include "GlobalHelpers.h"
/************************************************/
#include "FileIO_Int.h"
#include "MML.h"
#include "MML_MIDI.h"
/************************************************/

static int ParseMidiHeader(struct MidiState_t *State, FILE *MTFile) {
	//! Read and verify MIDI header
	uint32_t Magic = 0, Size = 0;
	FileIO_Get_u32be(&Magic, MTFile);
	FileIO_Get_u32be(&Size,  MTFile);
	if(Magic != 0x6468544D || Size != 6) return MML_MIDI_NOT_MIDIFILE;

	//! Read and verify format data
	uint16_t TicksPerBeat;
	if(
		!FileIO_Get_u16be(&State->Format,  MTFile) ||
		!FileIO_Get_u16be(&State->nTracks, MTFile) ||
		!FileIO_Get_u16be(&TicksPerBeat,   MTFile)
	) return MML_MIDI_UNEXPECTED_EOF;
	if(
		State->nTracks == 0 ||
		State->Format > 2 ||
		(State->Format == 0 && State->nTracks != 1) ||
		TicksPerBeat == 0
	) return MML_MIDI_NOT_MIDIFILE;

	//! Parse SMPTE timing as needed
	if((TicksPerBeat & 0x8000) != 0) {
		uint16_t FPS = (TicksPerBeat >> 7) & 0x7FU; //! Frames per second
		uint16_t TPF = (TicksPerBeat >> 0) & 0xFFU; //! Ticks per frame
		if(FPS == 0 || TPF == 0) return MML_MIDI_CORRUPTED;

		uint32_t t = ((uint32_t)TPF * 60 * 60 * 2 + FPS) / ((uint32_t)FPS * 2); //! Round[TPF*60*60/FPS]
		if(t == 0 || t > 65535) return MML_MIDI_UNSUPPORTED;
		TicksPerBeat = (uint16_t)t;
	}

	//! Store ticks per beat and done
	State->TicksPerBeat = TicksPerBeat;
	return MML_OK;
}

static int ParseTrackHeader(struct MidiTrack_t *Track, const uint8_t *DataBuffer, const uint8_t *DataBufferEnd) {
	//! Sanity check that we have a header
	const uint8_t *DataStart = DataBuffer + 8;
	if(DataStart > DataBufferEnd) return MML_MIDI_UNEXPECTED_EOF;

	//! Parse header and check magic value and size
	uint32_t Magic, Size;
	memcpy(&Magic, DataBuffer+0, 4);
	memcpy(&Size,  DataBuffer+4, 4);
	Magic = FileIO_ByteSwap32(Magic);
	Size  = FileIO_ByteSwap32(Size);
	if(Magic != 0x6B72544D || DataStart + Size > DataBufferEnd) return MML_MIDI_CORRUPTED;

	//! Setup the track state
	Track->LastCommandTick  = 0;
	Track->NextCommandTick  = 0;
	Track->SrcDataBuffer    = DataStart;
	Track->SrcDataBufferEnd = DataStart + Size;
	return MML_OK;
}

/************************************************/

//! Read MIDI file into MIDI state
static int ParseMIDIFile(struct MidiState_t *State, FILE *MTFile) {
	int ExitCode = MML_OK;

	//! Read MIDI header and sanity check
	ExitCode = ParseMidiHeader(State);
	if(ExitCode != MML_OK) goto FailReadHeader;

	//! Read track data into memory
	uint8_t *MidiDataBuffer;
	uint8_t *MidiDataBufferEnd; {
		fseek(MTFile, 0, SEEK_END);
		uint32_t Size = ftell(MTFile) - 0x14; //! 14h = Size of header
		fseek(MTFile, 0x14, SEEK_SET);
		MidiDataBuffer = (uint8_t*)malloc(Size);
		if(!MidiDataBuffer) {
			ExitCode = MML_OUT_OF_MEMORY;
			goto FailAllocFileBuffer;
		}
		MidiDataBufferEnd = MidiDataBuffer + Size;
	}

	//! Allocate tracks
	//! Note that Format 0 will allocate all 16 tracks/channels
	//! immediately; this is acceptable, because we will later
	//! remove any unused tracks.
	{
		uint32_t nTracksToAllocate = (State->Format != 0) ? State->nTracks : 16;
		struct MidiTrack_t *Tracks = (struct MidiTrack_t*)malloc(nTracksToAllocate * sizeof(struct MidiTrack_t));
		if(!Tracks) {
			ExitCode = MML_OUT_OF_MEMORY;
			goto FailAllocTracks;
		}
		State->Tracks = Tracks;

		//! Initialize tracks
		uint32_t i;
		const uint8_t *NextTrackDataPtr = MidiDataBuffer;
		for(i=0;i<nTracksToAllocate;i++) {
			ExitCode = ParseTrackHeader(&Tracks[i], NextTrackDataPtr, MidiDataBufferEnd);
			if(ExitCode != MML_OK) goto FailInitTracks;
			NextTrackDataPtr = Tracks[i].SrcDataBufferEnd;
		}
	}

	//! Begin parsing tracks
	State->CurrentTick = 0;
	uint32_t nActiveTracks = State->nTracks;
	while(nActiveTracks != 0) {
		uint32_t TrackIdx;

		//! Scan for which track expires soonest
		uint32_t NextTrackIdx  = State->CurrentTick;
		uint64_t NextTrackTick = ~0ull;
		for(TrackIdx=0;TrackIdx<State->nTracks;TrackIdx++) {
			struct MidiTrack_t *Track = &State->Tracks[TrackIdx];
			if(!Track->SrcDataBuffer) continue;
#if 0 //! Doesn't handle overflow wraparound properly
			if(Track->NextCommandTick <= NextTrackTick) {
#else
			if((int64_t)Track->NextCommandTick - (int64_t)NextTrackTick <= 0) {
#endif
				NextTrackIdx  = TrackIdx;
				NextTrackTick = Track->NextCommandTick;
			}
		}

		//! Apply delta time to current tick
		uint32_t DeltaTime;
		if(ReadTrackVarLen(&DeltaTime, &State->Tracks[NextTrackIdx])) {
			State->CurrentTick += (uint64_t)DeltaTime;
		} else {
			ExitCode = MML_UNEXPECTED_EOF;
			goto FailParseTrack;
		}

		//! Process next track command
		ExitCode = ParseNextCommand(State, &State->Tracks[NextTrackIdx]);
		if(ExitCode == MML_EOF) {
			nActiveTracks--;
			ExitCode = MML_OK;
		}
		if(ExitCode != MML_OK) goto FailParseTrack;
	}

	//! Exit points
FailParseTrack:
FailInitTracks:
	free(State->Tracks);
FailAllocTracks:
	free(MidiDataBuffer);
FailAllocFileBuffer:
FailReadHeader:
	return ExitCode;
}

/************************************************/

//! Convert MIDI file to textual MML
int MML_TextFromMIDI(char **MMLBufPtr, FILE *MTFile) {
	int Result;
	char *MMLBuf = NULL;
	uint32_t MMLBufOffs = 0;
	uint32_t MMLBufSize = 0;
	uint32_t ChannelIdx;

	//! Convert to intermediate format
	struct MidiState_t State;
	Result = ParseMIDIFile(&State, MTFile);
	if(Result < 0) return Result;

	//! If timings were resampled, we no longer need TimeMul/TimeDiv
	if(State.ResampleTimings) {
		State.TimeMul = 1;
		State.TimeDiv = 1;
	}

	//! Write start of MML data
	Result = DynamicBuffer_WriteFormatted(
		";;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n"
		"; %s\n"
		"; Converted from MIDI\n"
		";;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n"
		"\n",
		&MMLBuf,
		&MMLBufOffs,
		&MMLBufSize,
		State.TrackName ? State.TrackName : "Untitled song"
	);
	if(!Result) { Result = MML_MIDI_PRINTF_ERROR; goto Fail_TextFromMIDI; }
	if(State.TicksPerBeat != MML_TICKS_PER_QUARTER_NOTE) {
		Result = DynamicBuffer_WriteFormatted(
			"$ticksperbeat = %u\n"
			"\n"
			";;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n"
			"\n",
			&MMLBuf,
			&MMLBufOffs,
			&MMLBufSize,
			State.TicksPerBeat
		);
		if(!Result) { Result = MML_MIDI_PRINTF_ERROR; goto Fail_TextFromMIDI; }
	}

	//! Now parse each channel into text
	for(ChannelIdx=0;ChannelIdx<State.nChannels;ChannelIdx++) {
		if(!Channel->IsPopulated) continue;
		Result = Channel_ConvertToText(&State.Channels[ChannelIdx], &MMLBuf, &MMLBufOffs, &MMLBufSize);
		if(Result < 0) goto Fail_TextFromMIDI;
	}

	//! Store buffer pointer
	*MMLBufPtr = MMLBuf;

	//! Destroy state
	//! TODO
	return MML_OK;

Fail_TextFromMIDI:
	free(MMLBuf);
	return Result;
}

/************************************************/
//! EOF
/************************************************/
#endif
