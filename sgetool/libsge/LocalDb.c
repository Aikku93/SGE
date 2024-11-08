/************************************************/
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
/************************************************/
#include "SGE-Compiler.h"
#include "GlobalHelpers.h"
/************************************************/

//! Initialize local database
void SGE_LocalDb_Init(struct SGE_LocalDb_t *Db) {
	Db->nWaves = 0;
	Db->nTones = 0;
	Db->nSongs = 0;
	Db->Waves = NULL;
	Db->Tones = NULL;
	Db->Songs = NULL;
	Db->WavRemapTable = NULL;
}

/************************************************/

//! Destroy local database
static void DestroyTone(struct SGE_LocalTone_t *Tone) {
	uint32_t LayerIdx;
	for(LayerIdx=0;LayerIdx<Tone->nLayers;LayerIdx++) {
		struct SGE_LocalTone_Layer_t *Layer = &Tone->Layers[LayerIdx];
		free(Layer->Regions);
	}
	free(Tone->Layers);
}
void SGE_LocalDb_Destroy(struct SGE_LocalDb_t *Db) {
	//! Tones need to destroy all their data
	uint32_t ToneIdx;
	for(ToneIdx=0;ToneIdx<Db->nTones;ToneIdx++) {
		DestroyTone(&Db->Tones[ToneIdx]);
	}

	//! Songs also need to destroy their data
	uint32_t SongIdx;
	for(SongIdx=0;SongIdx<Db->nSongs;SongIdx++) {
		struct SGE_LocalSong_t *Song = &Db->Songs[SongIdx];
		for(ToneIdx=0;ToneIdx<Song->nTones;ToneIdx++) {
			DestroyTone(&Song->Tones[ToneIdx]);
		}
		free(Song->TrackData);
		free(Song->Tones);
	}

	//! Free arrays
	free(Db->Waves);
	free(Db->Tones);
	free(Db->Songs);
	free(Db->WavRemapTable);
}

/************************************************/

//! Error string from code
const char *SGE_LocalDb_Export_ErrorCodeToString(int ErrorCode) {
	switch(ErrorCode) {
		case SGE_LOCALDB_ERROR_OUT_OF_MEMORY:
			return "Out of memory.";
		case SGE_LOCALDB_ERROR_TOO_MANY_WAVEFORMS:
			return "Too many waveforms.";
		case SGE_LOCALDB_ERROR_TOO_MANY_SONGS:
			return "Too many songs.";
		case SGE_LOCALDB_ERROR_IO:
			return "Error while performing IO operation (out of space?).";
		case SGE_LOCALDB_ERROR_UNKNOWN:
			return "Unknown error. Please report this issue.";
		case SGE_LOCALDB_ERROR_NEED_RESAMPLING:
			return "Resampling was needed, with resampling disabled.";
		case SGE_LOCALDB_ERROR_PARAM_RANGE:
			return "Parameters went out of range.";
		case SGE_LOCALDB_ERROR_WRITE_WAVEDATA:
			return "Error while writing waveform data (out of memory, bad input, or bad output).";
		case SGE_LOCALDB_ERROR_INITMML:
			return "Error while initializing MML parser.";
		default:
			return NULL;
	}
}

/************************************************/
//! EOF
/************************************************/
