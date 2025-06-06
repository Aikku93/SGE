/************************************************/
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
/************************************************/
#include "DLS.h"
#include "MiniRIFF.h"
/************************************************/

//! This is defined by DLS_RIFF.c, but not exposed globally
//! in order to avoid having to include MiniRIFF.h everywhere.
extern const struct RIFF_CkListHdl_t RIFF_DLS[];

/************************************************/

//! Define a search key for sorting (and binary-searching) instruments by MIDI program
static inline
uint32_t Instrument_MakeSearchkey(const struct DLS_Instrument_t *x) {
	return x->Patch | x->CC0<<8 | x->CC32<<16 | x->DrumKit<<24;
}

//! Comparator to sort waveforms by wvplOffset value
static int WaveformSorter(const void *a_, const void *b_) {
	const struct DLS_Waveform_t *a = (const struct DLS_Waveform_t*)a_;
	const struct DLS_Waveform_t *b = (const struct DLS_Waveform_t*)b_;
	uint32_t va = a->wvplOffset;
	uint32_t vb = b->wvplOffset;
	return (int)va - (int)vb;
}

//! Comparator to sort regions by key ranges
static int RegionSorter(const void *a_, const void *b_) {
	const struct DLS_Region_t *a = (const struct DLS_Region_t*)a_;
	const struct DLS_Region_t *b = (const struct DLS_Region_t*)b_;
	uint32_t va = a->KeyLo; //! Because keys cannot overlap, sorting by the low key is enough
	uint32_t vb = b->KeyLo;
	return (int)va - (int)vb;
}

//! Comparator to sort layers by velocity ranges
static int LayerSorter(const void *a_, const void *b_) {
	const struct DLS_Layer_t *a = (const struct DLS_Layer_t*)a_;
	const struct DLS_Layer_t *b = (const struct DLS_Layer_t*)b_;
	uint32_t va = a->VelHi | (a->VelLo << 16);
	uint32_t vb = b->VelHi | (b->VelLo << 16);
	return (int)va - (int)vb;
}

//! Comparator to sort instruments by MIDI program
static int InstrumentSorter(const void *a_, const void *b_) {
	const struct DLS_Instrument_t *a = (const struct DLS_Instrument_t*)a_;
	const struct DLS_Instrument_t *b = (const struct DLS_Instrument_t*)b_;
	uint32_t va = Instrument_MakeSearchkey(a);
	uint32_t vb = Instrument_MakeSearchkey(b);
	return (int)va - (int)vb;
}

//! Comparator to search for a waveform by its wvplOffset
static int WaveformByCueTblIdxComparator(const void *a_, const void *b_) {
	const uint32_t *Key = (const uint32_t*)a_;
	const struct DLS_Waveform_t *Waveform = (const struct DLS_Waveform_t*)b_;
	uint32_t va = *Key;
	uint32_t vb = Waveform->wvplOffset;
	return (int)va - (int)vb;
}

/************************************************/

//! Check if a region is identical to another, aside from key/velocity ranges
static int RegionHasSameParams(const struct DLS_Region_t *a, const struct DLS_Region_t *b) {
	if(
		a->CueTblIdx == b->CueTblIdx &&
		a->UseLocalWavCtrl == b->UseLocalWavCtrl &&
		(!a->UseLocalWavCtrl  || !memcmp(&a->WavCtrl, &b->WavCtrl, sizeof(a->WavCtrl))) &&
		!memcmp(&a->Art, &b->Art, sizeof(a->Art))
	) return 1;
	return 0;
}

//! Get waveform pointer from a cue table index
static struct DLS_Waveform_t *WaveformByCueTblIdx(struct DLS_t *DLS, uint32_t CueTblIdx) {
	uint32_t wvplOffs = DLS->ptbl[CueTblIdx];
	return (struct DLS_Waveform_t*)bsearch(&wvplOffs, DLS->Waveforms, DLS->nWaveforms, sizeof(struct DLS_Waveform_t), WaveformByCueTblIdxComparator);
}

/************************************************/

//! Finalize instrument structure by sorting + cleaning layers and regions, and resolving waveform links
#define DELETE_INSTRUMENT (DLS_ERROR_NONE+1) //! <- Will be returned if the instrument has no layers or regions
static int FinalizeInstrument(struct DLS_t *DLS, struct DLS_Instrument_t *Instrument) {
	//! First, create a local copy of the articulation data into each region,
	//! and then determine the highest articulation level for this instrument.
	//! This removes any "compatibility" regions (we should use the cdl chunk
	//! to decide this, but this should do).
	uint32_t MaxArtLv = 1;
	uint32_t RegionIdx, nRegions = Instrument->Layers[0].nRegions;
	for(RegionIdx=0;RegionIdx<nRegions;RegionIdx++) {
		struct DLS_Region_t *Region = &Instrument->Layers[0].Regions[RegionIdx];
		if(Region->ArtLv == 0) memcpy(&Region->Art, &Instrument->Art, sizeof(struct DLS_Articulation_t));
		else if(Region->ArtLv > MaxArtLv) MaxArtLv = Region->ArtLv;
	}

	//! Next, resolve region wave indices and split into layers
	for(RegionIdx=0;RegionIdx<nRegions;RegionIdx++) {
		struct DLS_Region_t *Region = &Instrument->Layers[0].Regions[RegionIdx];

		//! Ignore regions belonging to a lower articulation level.
		//! Again, this should be done using cdl chunks, but it should
		//! be safe enough to just use the highest level articulation.
		if(Region->ArtLv != 0 && Region->ArtLv < MaxArtLv) continue;

		//! Resolve waveform index and ignore unresolved regions
		Region->Waveform = WaveformByCueTblIdx(DLS, Region->CueTblIdx);
		if(!Region->Waveform) continue;
		Region->WaveformIdx = Region->Waveform - DLS->Waveforms;

		//! Copy waveform control if needed
		if(!Region->UseLocalWavCtrl) {
			memcpy(&Region->WavCtrl, &Region->Waveform->wCtrl, sizeof(struct DLS_WaveformCtrl_t));
		}

		//! Find a layer that matches this region's velocity range
		struct DLS_Layer_t *TargetLayer = NULL;
		uint32_t LayerIdx, nLayers = Instrument->nLayers;
		for(LayerIdx=1;LayerIdx<nLayers;LayerIdx++) {
			struct DLS_Layer_t *Layer = &Instrument->Layers[LayerIdx];

			//! Velocity range match?
			if(Region->VelLo == Layer->VelLo && Region->VelHi == Layer->VelHi) {
				//! Ensure no key overlaps occur on this layer
				uint32_t TestRegionIdx, nTestRegions = Layer->nRegions;
				for(TestRegionIdx=0;TestRegionIdx<nTestRegions;TestRegionIdx++) {
					struct DLS_Region_t *TestRegion = &Layer->Regions[TestRegionIdx];
					if(Region->KeyLo <= TestRegion->KeyHi && Region->KeyHi >= TestRegion->KeyLo) break;
				}

				//! No overlaps? Target this layer
				if(TestRegionIdx == nTestRegions) {
					TargetLayer = Layer;
					break;
				}
			}
		}

		//! Do we need to create a new layer?
		if(TargetLayer == NULL) {
			uint32_t nLayers = Instrument->nLayers;
			struct DLS_Layer_t *NewLayers = (struct DLS_Layer_t*)realloc(Instrument->Layers, (nLayers+1)*sizeof(struct DLS_Layer_t));
			if(!NewLayers) return DLS_ERROR_OUT_OF_MEMORY;
			Instrument->nLayers = nLayers+1;
			Instrument->Layers  = NewLayers;
			TargetLayer = &NewLayers[nLayers];

			//! Initialize layer
			TargetLayer->VelLo    = Region->VelLo;
			TargetLayer->VelHi    = Region->VelHi;
			TargetLayer->nRegions = 0;
			TargetLayer->Regions  = NULL;
			TargetLayer->ArtLv    = Region->ArtLv;
			memcpy(&TargetLayer->Art, &Region->Art, sizeof(struct DLS_Articulation_t));
		}

		//! Append this region to the target layer
		uint32_t nRegions = TargetLayer->nRegions;
		struct DLS_Region_t *NewRegions = (struct DLS_Region_t*)realloc(TargetLayer->Regions, (nRegions+1)*sizeof(struct DLS_Region_t));
		if(!NewRegions) return DLS_ERROR_OUT_OF_MEMORY;
		TargetLayer->nRegions = nRegions+1;
		TargetLayer->Regions  = NewRegions;
		memcpy(&TargetLayer->Regions[nRegions], Region, sizeof(struct DLS_Region_t));
	}

	//! Remove the "global" layer and its regions
	free(Instrument->Layers[0].Regions);
	if(--Instrument->nLayers == 0) return DELETE_INSTRUMENT;

	//! Sort the remaining layers by velocity
	uint32_t LayerIdx, nLayers = Instrument->nLayers;
	memmove(Instrument->Layers, Instrument->Layers+1, nLayers * sizeof(struct DLS_Layer_t));
	qsort(Instrument->Layers, nLayers, sizeof(struct DLS_Layer_t), LayerSorter);

	//! Sort each layer's regions, try to move the articulation up, and merge contiguous regions
	for(LayerIdx=0;LayerIdx<nLayers;LayerIdx++) {
		struct DLS_Layer_t *Layer = &Instrument->Layers[LayerIdx];
		nRegions = Layer->nRegions;
		qsort(Layer->Regions, nRegions, sizeof(struct DLS_Region_t), RegionSorter);

		//! Check articulations, and scan regions for contiguity
		int MoveArticulationUp = 1;
		for(RegionIdx=0;RegionIdx<nRegions;RegionIdx++) {
			struct DLS_Region_t *Region = &Layer->Regions[RegionIdx];

			//! If the articulation is dissimilar, we can't move it
			if(MoveArticulationUp && memcmp(&Region->Art, &Layer->Art, sizeof(struct DLS_Articulation_t))) {
				MoveArticulationUp = 0;
			}

			//! Next, scan forwards until a region has different parameters
			uint32_t TestRegionIdx, nRegionsToMerge = 0;
			for(TestRegionIdx=RegionIdx+1;TestRegionIdx<nRegions;TestRegionIdx++) {
				struct DLS_Region_t *TestRegion = &Layer->Regions[TestRegionIdx];

				//! Contiguous keys with same parameters?
				if(TestRegion->KeyLo == Region->KeyHi+1 && RegionHasSameParams(Region, TestRegion)) {
					//! Merge with this region
					Region->KeyHi = TestRegion->KeyHi;
					nRegionsToMerge++;
				} else break;
			}

			//! Remove all regions we merged with
			if(nRegionsToMerge) {
				memmove(Region+1, Region+1 + nRegionsToMerge, (nRegions - nRegionsToMerge - (RegionIdx+1)) * sizeof(struct DLS_Region_t));
				nRegions -= nRegionsToMerge;
			}
		}
		Layer->nRegions = nRegions;

		//! Move articulation level up?
		if(MoveArticulationUp) {
			Layer->ArtLv = Layer->Regions[0].ArtLv;
			memcpy(&Layer->Art, &Layer->Regions[0].Art, sizeof(struct DLS_Articulation_t));
			for(RegionIdx=0;RegionIdx<nRegions;RegionIdx++) {
				Layer->Regions[RegionIdx].ArtLv = 0;
			}
		}
	}

	//! Try to move the articulation up and collapse identical velocity layers
	int MoveArticulationUp = 1;
	for(LayerIdx=0;LayerIdx<nLayers;LayerIdx++) {
		struct DLS_Layer_t *Layer = &Instrument->Layers[LayerIdx];

		//! If the articulation is dissimilar, we can't move it
		if(MoveArticulationUp && memcmp(&Layer->Art, &Instrument->Art, sizeof(struct DLS_Articulation_t))) {
			MoveArticulationUp = 0;
		}

		//! Next, scan forwards until a layer has different parameters
		uint32_t TestLayerIdx, nLayersToMerge = 0;
		for(TestLayerIdx=LayerIdx+1;TestLayerIdx<nLayers;TestLayerIdx++) {
			struct DLS_Layer_t *TestLayer = &Instrument->Layers[TestLayerIdx];

			//! Velocity is contiguous, and have same number of regions?
			nRegions = Layer->nRegions;
			if(TestLayer->VelLo == Layer->VelHi+1 && TestLayer->nRegions == nRegions) {
				//! Ensure all regions match
				for(RegionIdx=0;RegionIdx<nRegions;RegionIdx++) {
					const struct DLS_Region_t *a = &    Layer->Regions[RegionIdx];
					const struct DLS_Region_t *b = &TestLayer->Regions[RegionIdx];
					if(a->KeyLo != b->KeyLo || a->KeyHi != b->KeyHi || !RegionHasSameParams(a, b)) break;
				}
				if(RegionIdx == nRegions) {
					//! Layer is identical - merge it
					Layer->VelHi = TestLayer->VelHi;
					nLayersToMerge++;
					continue;
				}
			}

			//! Couldn't merge
			break;
		}

		//! Remove all layers we merged with
		if(nLayersToMerge) {
			memmove(Layer+1, Layer+1 + nLayersToMerge, nLayers - nLayersToMerge - (LayerIdx+1));
			nLayers -= nLayersToMerge;
		}
	}
	Instrument->nLayers = nLayers;

	//! Move articulation level up?
	if(MoveArticulationUp) {
		Instrument->ArtLv = Instrument->Layers[0].ArtLv;
		memcpy(&Instrument->Art, &Instrument->Layers[0].Art, sizeof(struct DLS_Articulation_t));
		for(LayerIdx=0;LayerIdx<nLayers;LayerIdx++) {
			Instrument->Layers[LayerIdx].ArtLv = 0;
		}
	}

	//! Success
	return DLS_ERROR_NONE;
}

/************************************************/

//! Read DLS file
int DLS_Read(struct DLS_t *DLS, FILE *DLSFile) {
	int ReturnValue;

	//! Reset structure
	DLS->nWaveforms = 0;
	DLS->nInstruments = 0;
	DLS->Waveforms = NULL;
	DLS->Instruments = NULL;
	DLS->ptbl = NULL;
	DLS->wvplOffs = 0;

	//! Parse file
	ReturnValue = RIFF_CkRead(DLSFile, DLS, NULL, RIFF_DLS, NULL);
	if(ReturnValue != 0) goto DLS_Read_Error_RIFFRead;

	//! Ensure we have all structures needed
	if(!DLS->nWaveforms || !DLS->ptbl || !DLS->wvplOffs) {
		ReturnValue = DLS_ERROR_INVALID;
		goto DLS_Read_Error_RIFFRead;
	}

	//! Sort waveforms and instruments
	//! This allows a binary search in WaveformByCueTblIdx() and DLS_InstrumentFromMIDIProgram()
	qsort(DLS->Waveforms,   DLS->nWaveforms,   sizeof(struct DLS_Waveform_t),   WaveformSorter);
	qsort(DLS->Instruments, DLS->nInstruments, sizeof(struct DLS_Instrument_t), InstrumentSorter);

	//! Finalize all instruments for this soundbank
	{
		uint32_t InstrumentIdx, nInstruments = DLS->nInstruments;
		struct DLS_Instrument_t *Instruments = DLS->Instruments;
		for(InstrumentIdx=0;InstrumentIdx<nInstruments;InstrumentIdx++) {
			int Success = FinalizeInstrument(DLS, &Instruments[InstrumentIdx]);
			if(Success == DELETE_INSTRUMENT) {
				memmove(Instruments, Instruments+1, (nInstruments - (InstrumentIdx+1)) * sizeof(*Instruments));
				InstrumentIdx--;
				nInstruments--;
			} else if(Success != DLS_ERROR_NONE) {
				ReturnValue = Success;
				goto DLS_Read_Error_FinalizeInstrument;
			}
		}
	}

	//! Done
DLS_Read_Error_RIFFRead:
DLS_Read_Error_FinalizeInstrument:
	if(ReturnValue != DLS_ERROR_NONE) DLS_Destroy(DLS);
	return ReturnValue;
}

/************************************************/

//! Destroy soundbank
void DLS_Destroy(struct DLS_t *DLS) {
	uint32_t i, j;

	//! Destroy all waveforms
	for(i=0;i<DLS->nWaveforms;i++) {
		free(DLS->Waveforms[i].Name);
		free(DLS->Waveforms[i].Comment);
	}
	free(DLS->Waveforms);

	//! Destroy all instruments
	for(i=0;i<DLS->nInstruments;i++) {
		struct DLS_Instrument_t *Instrument = &DLS->Instruments[i];
		struct DLS_Layer_t *Layers = Instrument->Layers;
		for(j=0;j<Instrument->nLayers;j++) free(Layers[j].Regions);
		free(Layers);
		free(Instrument->Name);
		free(Instrument->Comment);
	}
	free(DLS->Instruments);

	//! Destroy pool table
	free(DLS->ptbl);
}

/************************************************/

//! Convert error code to error string
const char *DLS_ErrorCodeToString(int Code) {
	switch(Code) {
		case DLS_ERROR_INVALID: return "Invalid DLS file.";
		case DLS_ERROR_OUT_OF_MEMORY: return "Out of memory.";
		case DLS_ERROR_IO: return "Error while performing IO operation (malformed file?).";
		default: return NULL;
	}
}

/************************************************/
//! EOF
/************************************************/
