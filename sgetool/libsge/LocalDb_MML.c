/************************************************/
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
/************************************************/
#include "MML.h"
#include "SGE-Compiler.h"
#include "GlobalHelpers.h"
/************************************************/
#include <stdio.h>

//! NotifyMIDIProgram MML callback
static int NotifyMIDIProgramChangeFnc(void *Userdata, uint8_t Patch, uint8_t CC0, uint8_t CC32, uint8_t IsDrumKit) {
#define MATCH_MIDI_PATCH(Tone) \
	(Tone->Patch == Patch && Tone->CC0 == CC0 && Tone->CC32 == CC32 && Tone->DrumKit == IsDrumKit)
	const struct SGE_LocalDb_t *Db = (struct SGE_LocalDb_t*)Userdata;
	struct SGE_LocalSong_t *Song = &Db->Songs[Db->nSongs-1];

	//! First, try to re-use a tone
	uint32_t ToneIdx, nTones = Song->nTones;
	for(ToneIdx=0;ToneIdx<nTones;ToneIdx++) {
		const struct SGE_LocalTone_t *Tone = &Song->Tones[ToneIdx];
		if(MATCH_MIDI_PATCH(Tone)) {
			return ToneIdx;
		}
	}

	//! Need to assign a new tone from database
	nTones = Db->nTones;
	for(ToneIdx=0;ToneIdx<nTones;ToneIdx++) {
		const struct SGE_LocalTone_t *SrcTone = &Db->Tones[ToneIdx];
		if(MATCH_MIDI_PATCH(SrcTone)) {
			//! Append this tone to the song tones list
			nTones = Song->nTones;
			struct SGE_LocalTone_t *NewTones = realloc(Song->Tones, (nTones+1)*sizeof(struct SGE_LocalTone_t));
			if(!NewTones) return -1;
			Song->Tones  = NewTones;
			Song->nTones = nTones+1;

			//! Copy the entire tone structure
			uint32_t LayerIdx;
			struct SGE_LocalTone_t *DstTone = &NewTones[nTones];
			DstTone->Patch   = SrcTone->Patch;
			DstTone->CC0     = SrcTone->CC0;
			DstTone->CC32    = SrcTone->CC32;
			DstTone->DrumKit = SrcTone->DrumKit;
			DstTone->nLayers = SrcTone->nLayers;
			DstTone->Layers  = malloc(SrcTone->nLayers*sizeof(struct SGE_LocalTone_Layer_t));
			if(!DstTone->Layers) {
				DstTone->nLayers = 0; //! <- Compatibility with LocalDb_Destroy()
				return -1;
			}
			for(LayerIdx=0;LayerIdx<SrcTone->nLayers;LayerIdx++) {
				      struct SGE_LocalTone_Layer_t *DstLayer = &DstTone->Layers[LayerIdx];
				const struct SGE_LocalTone_Layer_t *SrcLayer = &SrcTone->Layers[LayerIdx];
				DstLayer->VelLo   = SrcLayer->VelLo;
				DstLayer->VelHi   = SrcLayer->VelHi;
				DstLayer->nReg    = SrcLayer->nReg;
				DstLayer->Regions = malloc(SrcLayer->nReg*sizeof(struct SGE_LocalTone_Region_t));
				if(!DstLayer->Regions) {
					DstLayer->nReg   = 0;        //! <- Compatibility with LocalDb_Destroy()
					DstTone->nLayers = LayerIdx; //! <- Compatibility with LocalDb_Destroy()
					return -1;
				}
				memcpy(DstLayer->Regions, SrcLayer->Regions, SrcLayer->nReg*sizeof(struct SGE_LocalTone_Region_t));
#if 0
				//! Mark all waveforms as referenced.
				for(RegionIdx=0;RegionIdx<SrcLayer->nReg;RegionIdx++) {
					Db->Waves[DstLayer->Regions[RegionIdx].WaveIdx].dbLink.Referenced = 1;
				}
#endif
			}
			return nTones;
		}
	}

	//! None found
	return -1;
#undef MATCH_MIDI_PATCH
}

/************************************************/

//! NotifyKeyOn MML callback
static int NotifyKeyOnFnc(void *Userdata, uint8_t Program, uint8_t Key, uint8_t Vel) {
	const struct SGE_LocalDb_t *Db = (struct SGE_LocalDb_t*)Userdata;
	struct SGE_LocalSong_t *Song = &Db->Songs[Db->nSongs-1];

	//! First, ensure the program is valid
	if(Program >= Song->nTones) return -1;

	//! Now mark all regions' waveforms as referenced on match
	uint32_t nHits = 0;
	uint32_t LayerIdx, RegionIdx;
	const struct SGE_LocalTone_t *Tone = &Song->Tones[Program];
	for(LayerIdx=0;LayerIdx<Tone->nLayers;LayerIdx++) {
		struct SGE_LocalTone_Layer_t *Layer = &Tone->Layers[LayerIdx];

		//! Early exit once out of reach of lowest matching velocity layer
		if(Layer->VelLo > Vel) break;
		if(Vel <= Layer->VelHi) {
			for(RegionIdx=0;RegionIdx<Layer->nReg;RegionIdx++) {
				struct SGE_LocalTone_Region_t *Region = &Layer->Regions[RegionIdx];
				if(Region->KeyLo > Key) break;
				if(Key <= Region->KeyHi) {
					//! Got a hit - mark as referenced
					Region->Referenced = 1;
					Db->Waves[Region->WaveIdx].Referenced = 1;
					nHits++;
				}
			}
		}
	}
	return (nHits > 0) ? 1 : (-1);
}

/************************************************/

//! Load MML song into local database
int SGE_LocalDb_LoadMML(struct SGE_LocalDb_t *Db, FILE *SongFile, struct MML_t *MML) {
	uint32_t TrackIdx;

	//! Add space for a new song
	struct SGE_LocalSong_t *Song; {
		uint32_t nSongs = Db->nSongs;
		struct SGE_LocalSong_t *NewSongs = realloc(Db->Songs, (nSongs+1)*sizeof(struct SGE_LocalSong_t));
		if(!NewSongs) return SGE_LOCALDB_ERROR_OUT_OF_MEMORY;
		Db->Songs  = NewSongs;
		Db->nSongs = nSongs+1;

		//! Initialize
		Song = &NewSongs[nSongs];
		Song->nTracks       = 0;
		Song->TrackDataSize = 0;
		Song->nTones        = 0;
		Song->dbOffs        = 0;
		Song->dbSize        = 0;
		Song->TrackData     = NULL;
		Song->Tones         = NULL;
	}

	//! Read entire file into memory
	fseek(SongFile, 0, SEEK_END);
	size_t FileSize = ftell(SongFile);
	rewind(SongFile);
	char *Data = (char*)malloc(FileSize);
	if(!Data) return SGE_LOCALDB_ERROR_OUT_OF_MEMORY;
	if(!fread(Data, FileSize, 1, SongFile)) return SGE_LOCALDB_ERROR_IO;

	//! Parse MML
	if(
		MML_Init(
			MML,
			Data,
			FileSize,
			NotifyMIDIProgramChangeFnc,
			NotifyKeyOnFnc,
			Db
		) == MML_ERROR
	) return SGE_LOCALDB_ERROR_INITMML;
	if(MML_Parse(MML) == MML_ERROR) return SGE_LOCALDB_ERROR_MML;
	if(MML_Audit(MML) == MML_ERROR) return SGE_LOCALDB_ERROR_MML;

	//! Allocate space for all combined track data
	uint32_t TrackDataSize = 0; {
		for(TrackIdx=0;TrackIdx<MML->nTracks;TrackIdx++) {
			TrackDataSize += MML->TracksList[TrackIdx].Size;
		}
	}
	uint8_t *TrackData = (uint8_t*)malloc(MML->nTracks*sizeof(uint32_t) + TrackDataSize);
	if(!TrackData) return SGE_LOCALDB_ERROR_OUT_OF_MEMORY;

	//! Assign memory to song
	Song->nTracks       = MML->nTracks;
	Song->TrackDataSize = TrackDataSize;
	Song->TrackData     = TrackData;

	//! Write track offsets, then copy all track data
	uint32_t *NextTrackOffset = (uint32_t*)TrackData;
	for(TrackIdx=0;TrackIdx<MML->nTracks;TrackIdx++) {
		*NextTrackOffset++ = MML->TracksList[TrackIdx].DataOffs;
	}
	uint8_t *NextTrackData = (uint8_t*)NextTrackOffset;
	for(TrackIdx=0;TrackIdx<MML->nTracks;TrackIdx++) {
		size_t Size = MML->TracksList[TrackIdx].Size;
		memcpy(NextTrackData, MML->Output.Data + MML->TracksList[TrackIdx].DataOffs, Size);
		NextTrackData += Size;
	}

	//! All done
	return SGE_LOCALDB_ERROR_NONE;
}

/************************************************/
//! EOF
/************************************************/
