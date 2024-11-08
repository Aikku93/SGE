/************************************************/
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
/************************************************/
#include "ADPCM.h"
#include "SRC.h"
#include "SGE-Compiler.h"
#include "GlobalHelpers.h"
/************************************************/

//! Number of samples to append past the end of the sample
//! This is mostly needed for high-order resampling filters.
#define WAV_PADDING_SAMPLES 16 //! Allows for a 1+16*2 = 33-tap filter

/************************************************/

//! Align file to 32-byte boundary
//! This is needed to generate offsets in 32-byte chunks
static int AlignFile(FILE *f) {
	uint8_t nAlign = (uint8_t)(32 - ftell(f)) % 32;
	while(nAlign--) if(fputc(0, f) == EOF) return EOF;
	return 0;
}

/************************************************/

//! SRC->ADPCM handler
struct ADPCM_Write_t {
	const struct SGE_Wav_t *WavHeader;
	float *WavData;
	uint32_t nOutputSamples;
	uint32_t nSamplesInBuffer; //! <- Pre-multiplied by nChan
};
static int ADPCM_Write(FILE *DstFile, const float *Src, uint32_t N, void *Userdata) {
	struct ADPCM_Write_t *State = (struct ADPCM_Write_t*)Userdata;

	//! NOTE: We generated extra samples to prime the ADPCM state,
	//! so exclude them from the frame count
	uint32_t Chan, nChan = State->WavHeader->Chan;
	uint32_t Frame, nFrames = (State->nOutputSamples - ADPCM_FILTER_ORDER) / ADPCM_FRAME_SIZE;

	//! Ensure we have a buffer
	float *WavData = State->WavData;
	if(!WavData) {
		WavData = malloc(State->nOutputSamples * nChan * sizeof(float));
		if(!WavData) return 0;
		State->WavData = WavData;
	}

	//! Keep reading data until we're done
	memcpy(WavData + State->nSamplesInBuffer, Src, N * sizeof(float));
	State->nSamplesInBuffer += N;
	if(State->nSamplesInBuffer < State->nOutputSamples*nChan) return 1;

	//! Initialize ADPCM encoders and fill out headers
	uint32_t HeadersOffs = ftell(DstFile);
	struct ADPCM_t ADPCM_State[SGE_WAV_CHAN_MAX];
	struct SGE_ADPCMHeader_t Headers[SGE_WAV_CHAN_MAX];
	for(Chan=0;Chan<nChan;Chan++) {
		struct ADPCM_t *ThisState = &ADPCM_State[Chan];
		ADPCM_Init(ThisState, WavData+Chan, State->nOutputSamples, nChan);
		Headers[Chan].Init[0] = ThisState->zM1;
		Headers[Chan].Init[1] = ThisState->zM2;
		Headers[Chan].Coef[0] = ThisState->cM1 - (1 << ADPCM_FILTER_BITS);
		Headers[Chan].Coef[1] = ThisState->cM2;
		Headers[Chan].LpPt    = (State->WavHeader->Size - State->WavHeader->Loop) / ADPCM_FRAME_SIZE;

	}
	fseek(DstFile, sizeof(struct SGE_ADPCMHeader_t)*nChan, SEEK_CUR);

	//! Begin encoding and then destroy buffer
	for(Frame=0;Frame<nFrames;Frame++) {
		for(Chan=0;Chan<nChan;Chan++) {
			struct ADPCM_t *ThisState = &ADPCM_State[Chan];
			if(Frame == Headers[Chan].LpPt) {
				Headers[Chan].Loop[0] = ThisState->zM1;
				Headers[Chan].Loop[1] = ThisState->zM2;
			}
			uint32_t Data = ADPCM_EncodeFrame(ThisState, WavData+Chan + (Frame*ADPCM_FRAME_SIZE+ADPCM_FILTER_ORDER)*nChan, nChan);
			if(!fwrite(&Data, sizeof(Data), 1, DstFile)) {
				free(WavData);
				return 0;
			}
		}
	}
	free(WavData);

	//! Write headers
	fseek(DstFile, HeadersOffs, SEEK_SET);
	if(!fwrite(Headers, sizeof(struct SGE_ADPCMHeader_t)*nChan, 1, DstFile)) {
		return 0;
	}
	fseek(DstFile, 0, SEEK_END);

	//! All done
	return 1;
}

/************************************************/

//! Sort waveforms in order
static void SortWaveforms(const struct SGE_LocalDb_t *Db, uint16_t *WavRemapTable) {
	//! Iterate through all instruments and assign waveform indices
	uint32_t NextWaveIdx = 0;
	uint32_t ToneIdx;
	for(ToneIdx=0;ToneIdx<Db->nTones;ToneIdx++) {
		const struct SGE_LocalTone_t *Tone = &Db->Tones[ToneIdx];
		uint32_t LayerIdx;
		for(LayerIdx=0;LayerIdx<Tone->nLayers;LayerIdx++) {
			const struct SGE_LocalTone_Layer_t *Layer = &Tone->Layers[LayerIdx];
			uint32_t RegionIdx;
			for(RegionIdx=0;RegionIdx<Layer->nReg;RegionIdx++) {
				const struct SGE_LocalTone_Region_t *Region = &Layer->Regions[RegionIdx];
				struct SGE_LocalWav_t *Wav = &Db->Waves[Region->WaveIdx];
				if(!Wav->IsSorted) {
					Wav->IsSorted = 1;
					WavRemapTable[NextWaveIdx++] = Region->WaveIdx;
				}
			}
		}
	}

	//! Now assign all remaining unreferenced waveforms
	uint32_t WaveIdx;
	for(WaveIdx=0;WaveIdx<Db->nWaves;WaveIdx++) {
		struct SGE_LocalWav_t *Wav = &Db->Waves[WaveIdx];
		if(!Wav->IsSorted) {
			Wav->IsSorted = 1;
			WavRemapTable[NextWaveIdx++] = WaveIdx;
		}
	}
}

/************************************************/

//! Export local database to final file
int SGE_LocalDb_Export(struct SGE_LocalDb_t *Db, FILE *SGEFile, FILE *WavFile, const struct SGE_gOptions_t *Options) {
	uint32_t FileBeginOffs = ftell(SGEFile);
	uint8_t WaveUseCulling = (Options->WavEnableCull != 0) && (Db->nSongs != 0);

	//! First, re-map all waveforms to be in instrument
	//! order. This should improve IO access when the
	//! data goes through a cache.
	//! Since this is NOT mandatory, failure to allocate
	//! the memory will NOT be considered an error.
	uint16_t *WavRemapTable = malloc(sizeof(uint16_t)*Db->nWaves);
	if(WavRemapTable) {
		Db->WavRemapTable = WavRemapTable;
		SortWaveforms(Db, WavRemapTable);
	}

	//! Prepare but skip over the header - will be written later
	struct SGE_Db_t DbHeader;
	DbHeader.Magic = SGE_DB_MAGIC;
	DbHeader.nWave = 0;
	DbHeader.nSong = 0;
	fseek(SGEFile, sizeof(DbHeader), SEEK_CUR);

	//! Align start of data
	if(AlignFile(SGEFile) == EOF) return SGE_LOCALDB_ERROR_IO;

	//! Write out waveforms
	uint32_t WaveIdx;
	for(WaveIdx=0;WaveIdx<Db->nWaves;WaveIdx++) {
		struct SGE_LocalWav_t *Wav = &Db->Waves[WavRemapTable ? WavRemapTable[WaveIdx] : WaveIdx];
		const struct SGE_gOptions_t *WavOpt = &Wav->Options;
		if(!Wav->Referenced && WaveUseCulling) continue;

		//! Display progress
		printf("\rExporting waveform %u/%u... ", WaveIdx+1, Db->nWaves);

		//! Create header data and ADPCM state
		struct SGE_Wav_t OutputHeader;
		struct ADPCM_Write_t ADPCM_State;

		//! Get the source and target formatting
		struct SRC_Config_t SRCConfig;
		switch(Wav->Header.Frmt) {
			case SGE_WAV_FRMT_PCM8: {
				SRCConfig.SrcFormat = SRC_FORMAT_PCM8;
				SRCConfig.DstFormat = SRC_FORMAT_PCM8;
			} break;
			case SGE_WAV_FRMT_PCM16: {
				SRCConfig.SrcFormat = SRC_FORMAT_PCM16;
				SRCConfig.DstFormat = SRC_FORMAT_PCM16;
			} break;
			case SGE_WAV_FRMT_PCM24: {
				SRCConfig.SrcFormat = SRC_FORMAT_PCM24;
				SRCConfig.DstFormat = SRC_FORMAT_PCM16;
			} break;
			case SGE_WAV_FRMT_PCM32: {
				SRCConfig.SrcFormat = SRC_FORMAT_PCM32;
				SRCConfig.DstFormat = SRC_FORMAT_PCM16;
			} break;
			case SGE_WAV_FRMT_FLOAT32: {
				SRCConfig.SrcFormat = SRC_FORMAT_FLOAT32;
				SRCConfig.DstFormat = SRC_FORMAT_PCM16;
			} break;
			default: {
				return SGE_LOCALDB_ERROR_UNKNOWN;
			} break;
		}

		//! Override target format as needed
		uint8_t OutputFormat = WavOpt->WavFormat;
		switch(OutputFormat) {
			case SGE_WAV_FRMT_ADPCM4: {
				SRCConfig.DstFormat = SRC_FORMAT_CUSTOM;
			} break;
			case SGE_WAV_FRMT_PCM8: {
				SRCConfig.DstFormat = SRC_FORMAT_PCM8;
			} break;
			case SGE_WAV_FRMT_PCM16: {
				SRCConfig.DstFormat = SRC_FORMAT_PCM16;
			} break;
			case SGE_OPT_WAVFORMAT_DEFAULT: break;
			default: return SGE_LOCALDB_ERROR_UNKNOWN;
		}

		//! Assign resampling window
		switch(WavOpt->SRCWindow) {
			case SGE_OPT_SRCWINDOW_NONE: {
				SRCConfig.FilterWindow = SRC_WINDOW_NONE;
			} break;
			case SGE_OPT_SRCWINDOW_SINE: {
				SRCConfig.FilterWindow = SRC_WINDOW_SINE;
			} break;
			case SGE_OPT_SRCWINDOW_HANN: {
				SRCConfig.FilterWindow = SRC_WINDOW_HANN;
			} break;
			case SGE_OPT_SRCWINDOW_HAMMING: {
				SRCConfig.FilterWindow = SRC_WINDOW_HAMMING;
			} break;
			case SGE_OPT_SRCWINDOW_BLACKMAN: {
				SRCConfig.FilterWindow = SRC_WINDOW_BLACKMAN;
			} break;
			case SGE_OPT_SRCWINDOW_NUTTALL: {
				SRCConfig.FilterWindow = SRC_WINDOW_NUTTALL;
			} break;
			case SGE_OPT_SRCWINDOW_LANCZOS: {
				SRCConfig.FilterWindow = SRC_WINDOW_LANCZOS;
			} break;
			case SGE_OPT_SRCWINDOW_LANCZOS2: {
				SRCConfig.FilterWindow = SRC_WINDOW_LANCZOS2;
			} break;
			default: return SGE_LOCALDB_ERROR_UNKNOWN;
		}

		//! Set mono conversion parameters
		SRCConfig.MonoConvWindowSize = WavOpt->WavMonoConvWindowSize;
		SRCConfig.MonoConvHops       = WavOpt->WavMonoConvHops;
		switch(WavOpt->WavMonoConvWindowType) {
			case SGE_OPT_MONOCONV_WINDOW_SINE: {
				SRCConfig.MonoConvWindow = SRC_MONOCONV_WINDOW_SINE;
			} break;
			case SGE_OPT_MONOCONV_WINDOW_HANN: {
				SRCConfig.MonoConvWindow = SRC_MONOCONV_WINDOW_HANN;
			} break;
			case SGE_OPT_MONOCONV_WINDOW_HAMMING: {
				SRCConfig.MonoConvWindow = SRC_MONOCONV_WINDOW_HAMMING;
			} break;
			case SGE_OPT_MONOCONV_WINDOW_BLACKMAN: {
				SRCConfig.MonoConvWindow = SRC_MONOCONV_WINDOW_BLACKMAN;
			} break;
			case SGE_OPT_MONOCONV_WINDOW_NUTTALL: {
				SRCConfig.MonoConvWindow = SRC_MONOCONV_WINDOW_NUTTALL;
			} break;
			default: return SGE_LOCALDB_ERROR_UNKNOWN;
		}

		//! Select actual resampling rate
		double SrcRate = (double)Wav->Header.Freq, DstRate = SrcRate;
		if(WavOpt->WavResampleRate != 0 && (
			WavOpt->WavResampleCondition == SGE_OPT_WAVRESAMPCOND_ALWAYS ||
			(WavOpt->WavResampleCondition == SGE_OPT_WAVRESAMPCOND_GT && WavOpt->WavResampleRate > Wav->Header.Freq) ||
			(WavOpt->WavResampleCondition == SGE_OPT_WAVRESAMPCOND_LT && WavOpt->WavResampleRate < Wav->Header.Freq)
		)) DstRate = (double)WavOpt->WavResampleRate;
		if(SrcRate != DstRate && WavOpt->WavResampleCondition == SGE_OPT_WAVRESAMPCOND_NEVER) {
			//! NOTE: When a waveform has a sampling rate >= 2^24, it is adjusted
			//! to something below that. But this adjustment might cause issues.
			//! In practice, it probably doesn't matter since it is hilariously
			//! unlikely that any waveform has such a sampling rate, but...
			return SGE_LOCALDB_ERROR_NEED_RESAMPLING;
		}
		DstRate *= WavOpt->WavOversampleRate;

		//! Adjust the resampling rate based on loop size
		uint32_t LoopRepeats = 1;
		double NewSampleRate = (double)Wav->Header.Freq * DstRate / SrcRate;
		double NewLoopSize   = (double)Wav->Header.Loop * DstRate / SrcRate;
		if(Wav->Header.Loop) {
			//! First, increase loop repeats until the minimum length is reached
			double MinLoopSize;
			if(WavOpt->WavMinLoopSize < 0) {
				//! Specified in milliseconds
				MinLoopSize = (double)(-WavOpt->WavMinLoopSize) * DstRate / 1000.0;
			} else {
				//! Specified in samples
				MinLoopSize = (double)WavOpt->WavMinLoopSize;
			}
			while(NewLoopSize*(double)LoopRepeats < MinLoopSize) {
				LoopRepeats++;
			}
		}
		if(Wav->Header.Loop && (WavOpt->SRCAlign == SGE_OPT_SRCALIGN_LOOPS || WavOpt->SRCAlign == SGE_OPT_SRCALIGN_LOOPS_NONPERC)) {
			NewLoopSize *= (double)LoopRepeats;
			switch(WavOpt->SRCRound) {
				case SGE_OPT_SRCROUND_DOWN: {
					NewLoopSize = floor(NewLoopSize);
				} break;
				case SGE_OPT_SRCROUND_MIDDLE: {
					NewLoopSize = round(NewLoopSize);
				} break;
				case SGE_OPT_SRCROUND_UP: {
					NewLoopSize = ceil(NewLoopSize);
				} break;
				default: return SGE_LOCALDB_ERROR_UNKNOWN;
			}
			SrcRate = (double)Wav->Header.Loop * (double)LoopRepeats;
			DstRate = NewLoopSize;

			//! Re-calculate sampling rate
			if(
				WavOpt->SRCAlign == SGE_OPT_SRCALIGN_LOOPS ||
				(WavOpt->SRCAlign == SGE_OPT_SRCALIGN_LOOPS_NONPERC && !Wav->IsPercussive)
			) NewSampleRate = (double)Wav->Header.Freq * DstRate / SrcRate;
		}
		double NewWaveSize   = ceil((double)(Wav->Header.Size - Wav->Header.Loop) * DstRate / SrcRate + NewLoopSize);
		       NewSampleRate = round(NewSampleRate);
		       NewLoopSize   = floor(NewLoopSize);

		//! Set low-pass cut-off frequency
		double LowpassFc; {
			LowpassFc = NewSampleRate;
			if(WavOpt->WavLowpassCutoff != 0.0) {
				//! SRCConfig.Cutoff is 2*Fc, so multiply Options->WavLowpassCutoff by 2
				LowpassFc = MIN(LowpassFc, WavOpt->WavLowpassCutoff*2.0);
			}
			LowpassFc = MIN(LowpassFc / (double)Wav->Header.Freq, 1.0);
		}

		//! Do final adjustments and sanity check
		//! NOTE: If we need any resampling for looped waveforms, we have
		//! to add N/2 samples (where N is the filter order) to the end of
		//! the waveform to avoid ringing discontinuities in the loop.
		//! Additionally, the high-shelf filter modifies the output based
		//! on the last sample, so that adds an extra sample.
		//! Finally, mono conversion adds N/2 samples to counter any
		//! potentially discontinuities.
		if(NewLoopSize != 0.0) {
			if(DstRate != SrcRate) {
				NewWaveSize += (double)WavOpt->SRCHalfOrder * DstRate / SrcRate;
			}
			if(WavOpt->WavHighShelfGain != 1.0) {
				NewWaveSize += 1.0;
			}
			if(WavOpt->WavForceMono) {
				NewWaveSize += (double)(WavOpt->WavMonoConvWindowSize/2);
			}
		}
		if(
			(NewWaveSize   < 1.0 || NewWaveSize   >= (double)(1<<24)) ||
			(NewSampleRate < 1.0 || NewSampleRate >= (double)(1<<24)) ||
			(Wav->Header.Loop && (NewLoopSize < 2.0 || NewLoopSize >= (double)(1<<24)))
		) return SGE_LOCALDB_ERROR_PARAM_RANGE;

		//! Add padding samples and prepare for ADPCM
		uint32_t nOutputSamples = (uint32_t)NewWaveSize + WAV_PADDING_SAMPLES;
		if(OutputFormat == SGE_WAV_FRMT_ADPCM4) {
			uint32_t Mod = nOutputSamples % ADPCM_FRAME_SIZE;
			if(Mod) nOutputSamples += ADPCM_FRAME_SIZE - Mod;

			//! NOTE: Append samples to prime the ADPCM state
			nOutputSamples += ADPCM_FILTER_ORDER;

			//! Initialize ADPCM writer state
			ADPCM_State.WavHeader = &OutputHeader;
			ADPCM_State.WavData   = NULL;
			ADPCM_State.nOutputSamples   = nOutputSamples;
			ADPCM_State.nSamplesInBuffer = 0;
			SRCConfig.CustomWrite_Userdata = &ADPCM_State;
			SRCConfig.CustomWrite          =  ADPCM_Write;
		}

		//! Fill database information and write header
		uint32_t FileOffs; {
			FileOffs = ftell(SGEFile) - FileBeginOffs;
			memcpy(&OutputHeader, &Wav->Header, sizeof(OutputHeader));
			OutputHeader.Interpolate = WavOpt->WavInterpolate;
			OutputHeader.Frmt        = OutputFormat;
			if(WavOpt->WavForceMono) {
				OutputHeader.Chan = 1;
			}
			OutputHeader.Size  = (uint32_t)NewWaveSize;
			OutputHeader.Loop  = (uint32_t)NewLoopSize;
			OutputHeader.Freq  = (uint32_t)NewSampleRate;
			OutputHeader.dbIdx = DbHeader.nWave++;
			OutputHeader.dbLink.isRaw  = 1;
			OutputHeader.dbLink.dbOffs = FileOffs / 32;
			OutputHeader.dbLink.r1     = 0;
			if(!fwrite(&OutputHeader, sizeof(OutputHeader), 1, SGEFile)) return SGE_LOCALDB_ERROR_IO;
		}

		//! Finally, pass off writing the data to the resampler
		SRCConfig.DstChans        = (Wav->Header.Chan > 2 || WavOpt->WavForceMono) ? 1 : Wav->Header.Chan;
		SRCConfig.SrcChans        = Wav->Header.Chan;
		SRCConfig.FilterHalfOrder = WavOpt->SRCHalfOrder;
		SRCConfig.DstRate         = DstRate;
		SRCConfig.SrcRate         = SrcRate;
		SRCConfig.Cutoff          = LowpassFc;
		SRCConfig.GlobalGain      = (float)WavOpt->WavGlobalGain;
		SRCConfig.HighShelfGain   = (float)WavOpt->WavHighShelfGain;
		SRCConfig.DitherLevel     = (float)WavOpt->SRCDitherLevel;
		fseek(WavFile, Wav->FileOffs, SEEK_SET);
		if(!SRC_ConvertStreamedData(
			SGEFile,
			WavFile,
			nOutputSamples,
			Wav->Header.Size,
			Wav->Header.Loop,
			&SRCConfig
		)) {
			return SGE_LOCALDB_ERROR_WRITE_WAVEDATA;
		}

		//! Apply alignment
		if(AlignFile(SGEFile) == EOF) return SGE_LOCALDB_ERROR_IO;

		//! Re-write database linkage
		Wav->Header.dbIdx = OutputHeader.dbIdx;
		Wav->FileOffs = FileOffs;
		Wav->FileSize = ftell(SGEFile) - FileOffs;
	}
	printf("\nSuccessfully exported %u waveforms.\n", DbHeader.nWave);

	//! Begin writing out songs
	uint32_t SongIdx;
	for(SongIdx=0;SongIdx<Db->nSongs;SongIdx++) {
		uint32_t FileOffs_SongHeader = ftell(SGEFile);
		struct SGE_LocalSong_t *Song = &Db->Songs[SongIdx];

		//! Display progress
		printf("\rExporting song %u/%u... ", SongIdx+1, Db->nSongs);

		//! Fill database information and write header
		uint32_t FileOffs; {
			FileOffs = ftell(SGEFile) - FileBeginOffs;
			struct SGE_Song_t Header;
			Header.nTrack = (uint8_t)Song->nTracks;
			Header.nTone  = (uint8_t)Song->nTones;
			Header.dbIdx  = DbHeader.nSong++;
			Header.dbLink.isRaw  = 1;
			Header.dbLink.dbOffs = FileOffs / 32;
			Header.dbLink.r1     = 0;
			if(!fwrite(&Header, sizeof(Header), 1, SGEFile)) return SGE_LOCALDB_ERROR_IO;
		}

		//! Skip over the track offsets - need to write the tones first
		uint32_t FileOffs_TrackOffsets = ftell(SGEFile);
		fseek(SGEFile, Song->nTracks*sizeof(uint32_t), SEEK_CUR);

		//! Begin writing tones
		uint32_t ToneIdx, LayerIdx, RegionIdx, ArtIdx;
		for(ToneIdx=0;ToneIdx<Song->nTones;ToneIdx++) {
			const struct SGE_LocalTone_t *Tone = &Song->Tones[ToneIdx];

			//! Skip over header for now, and begin writing layers
			struct SGE_Tone_t ToneHeader;
			ToneHeader.Size   = sizeof(ToneHeader);
			ToneHeader.nLayer = 0;
			uint32_t FileOffs_ToneHeader = ftell(SGEFile);
			fseek(SGEFile, sizeof(ToneHeader), SEEK_CUR);
			for(LayerIdx=0;LayerIdx<Tone->nLayers;LayerIdx++) {
				const struct SGE_LocalTone_Layer_t *Layer = &Tone->Layers[LayerIdx];

				//! Again, skip over the header and begin writing regions
				struct SGE_ToneLayer_t LayerHeader;
				LayerHeader.VelLo = Layer->VelLo;
				LayerHeader.VelHi = Layer->VelHi;
				LayerHeader.nReg  = 0;
				LayerHeader.nArt  = 0;
				uint32_t FileOffs_LayerHeader = ftell(SGEFile);
				fseek(SGEFile, sizeof(LayerHeader), SEEK_CUR);
				struct SGE_WavArt_t *Articulations = NULL;
				for(RegionIdx=0;RegionIdx<Layer->nReg;RegionIdx++) {
					struct SGE_LocalTone_Region_t *Region = &Layer->Regions[RegionIdx];
					if(!Region->Referenced) continue;

					struct SGE_ToneRegion_t RegionData;
					RegionData.KeyLo  = Region->KeyLo;
					RegionData.KeyHi  = Region->KeyHi;
					RegionData.Wave   = Db->Waves[Region->WaveIdx].Header.dbIdx;

					//! Check for a match in the articulations
					for(ArtIdx=0;ArtIdx<LayerHeader.nArt;ArtIdx++) {
						if(!memcmp(&Region->Art, &Articulations[ArtIdx], sizeof(struct SGE_WavArt_t))) {
							break;
						}
					}
					RegionData.ArtIdx = ArtIdx;

					//! If we need to create a new articulation, do so now
					if(ArtIdx >= LayerHeader.nArt) {
						uint32_t nArt = LayerHeader.nArt;
						struct SGE_WavArt_t *NewArt = realloc(Articulations, (nArt+1)*sizeof(struct SGE_WavArt_t));
						if(!NewArt) {
							free(Articulations);
							return SGE_LOCALDB_ERROR_OUT_OF_MEMORY;
						}
						Articulations = NewArt;
						LayerHeader.nArt = nArt+1;
						memcpy(&Articulations[nArt], &Region->Art, sizeof(struct SGE_WavArt_t));
					}

					//! Write region
					if(!fwrite(&RegionData, sizeof(RegionData), 1, SGEFile)) {
						free(Articulations);
						return SGE_LOCALDB_ERROR_IO;
					}
					LayerHeader.nReg++;
				}

				//! If this layer has no regions, skip it
				if(LayerHeader.nReg == 0) {
					//! Need to rewind because we skipped the [non-existent] header
					fseek(SGEFile, FileOffs_LayerHeader, SEEK_SET);
					continue;
				}

				//! Write the articulations
				if(!fwrite(Articulations, LayerHeader.nArt*sizeof(struct SGE_WavArt_t), 1, SGEFile)) {
					free(Articulations);
					return SGE_LOCALDB_ERROR_IO;
				}
				free(Articulations);

				//! Write the layer header
				fseek(SGEFile, FileOffs_LayerHeader, SEEK_SET);
				if(!fwrite(&LayerHeader, sizeof(LayerHeader), 1, SGEFile)) return SGE_LOCALDB_ERROR_IO;
				fseek(SGEFile, 0, SEEK_END);
				ToneHeader.nLayer++;

				//! Increase tone size
				ToneHeader.Size += sizeof(LayerHeader);
				ToneHeader.Size += LayerHeader.nReg*sizeof(struct SGE_ToneRegion_t);
				ToneHeader.Size += LayerHeader.nArt*sizeof(struct SGE_WavArt_t);
			}

			//! Write the tone header
			fseek(SGEFile, FileOffs_ToneHeader, SEEK_SET);
			if(!fwrite(&ToneHeader, sizeof(ToneHeader), 1, SGEFile)) return SGE_LOCALDB_ERROR_IO;
			fseek(SGEFile, 0, SEEK_END);
		}

		//! Write the track offsets and then the final track data
		uint32_t TrackIdx;
		uint32_t TrackOffsetsAdjust = ftell(SGEFile) - FileOffs_SongHeader;
		const uint32_t *TrackOffsList = (const uint32_t*)Song->TrackData;
		fseek(SGEFile, FileOffs_TrackOffsets, SEEK_SET);
		for(TrackIdx=0;TrackIdx<Song->nTracks;TrackIdx++) {
			uint32_t TrackOffs = (*TrackOffsList++) + TrackOffsetsAdjust;
			if(!fwrite(&TrackOffs, sizeof(TrackOffs), 1, SGEFile)) return SGE_LOCALDB_ERROR_IO;
		}
		fseek(SGEFile, 0, SEEK_END);
		if(!fwrite(TrackOffsList, Song->TrackDataSize, 1, SGEFile)) return SGE_LOCALDB_ERROR_IO;

		//! Apply alignment
		if(AlignFile(SGEFile) == EOF) return SGE_LOCALDB_ERROR_IO;

		//! Re-write database linkage
		Song->dbOffs = FileOffs;
		Song->dbSize = ftell(SGEFile) - FileOffs;
	}
	printf("\nSuccessfully exported %u songs.\n", DbHeader.nSong);

	//! Write the tables
	//! NOTE: Instead of writing Offs+Size pairs, we write Offs+1 offsets,
	//! so that Size is implied by Offs[n+1]-Offs[n].
	uint32_t NextEntryOffs = 0; {
		//! Write the waveform table
		DbHeader.WaveTabOffs = (uint32_t)ftell(SGEFile) - FileBeginOffs;
		for(WaveIdx=0;WaveIdx<Db->nWaves;WaveIdx++) {
			struct SGE_LocalWav_t *Wav = &Db->Waves[WavRemapTable ? WavRemapTable[WaveIdx] : WaveIdx];
			if(!Wav->Referenced && WaveUseCulling) continue;

			uint32_t Offs = Wav->FileOffs;
			if(!fwrite(&Offs, sizeof(Offs), 1, SGEFile)) return SGE_LOCALDB_ERROR_IO;
#if 0
			uint32_t Size = Wav->FileSize;
			if(!fwrite(&Size, sizeof(Size), 1, SGEFile)) return SGE_LOCALDB_ERROR_IO;
#else
			NextEntryOffs = Offs + Wav->FileSize;
#endif
		}

		//! Write the song table
		DbHeader.SongTabOffs = (uint32_t)ftell(SGEFile) - FileBeginOffs;
		for(SongIdx=0;SongIdx<Db->nSongs;SongIdx++) {
			struct SGE_LocalSong_t *Song = &Db->Songs[SongIdx];

			uint32_t Offs = Song->dbOffs;
			if(!fwrite(&Offs, sizeof(Offs), 1, SGEFile)) return SGE_LOCALDB_ERROR_IO;
#if 0
			uint32_t Size = Song->dbSize;
			if(!fwrite(&Size, sizeof(Size), 1, SGEFile)) return SGE_LOCALDB_ERROR_IO;
#else
			NextEntryOffs = Offs + Song->dbSize;
#endif
		}
	}
	if(!fwrite(&NextEntryOffs, sizeof(NextEntryOffs), 1, SGEFile)) return SGE_LOCALDB_ERROR_IO;

	//! Finally, write the header
	fseek(SGEFile, FileBeginOffs, SEEK_SET);
	if(!fwrite(&DbHeader, sizeof(DbHeader), 1, SGEFile)) return SGE_LOCALDB_ERROR_IO;
	return SGE_LOCALDB_ERROR_NONE;
}

/************************************************/
//! EOF
/************************************************/
