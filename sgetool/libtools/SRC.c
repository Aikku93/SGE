/************************************************/
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/************************************************/
#include "SRC.h"
#include "Poly2Mono.h"
#include "GlobalHelpers.h"
#include "SRC_Window.h"
/************************************************/

//! Input/output sample buffer size
//! This is mainly used to reduce processing overhead,
//! but must also be able to accomodate all samples from
//! a resampling kernel of the maximum number of taps
#define SRCDATA_BLOCK_SIZE 2048
#define DSTDATA_BLOCK_SIZE 512

//! Output sample taps
//! Currently only need 1 sample (for the high-shelf filter)
//! NOTE: Must be a power of 2.
#define DSTDATA_FILTER_TAPS 1

//! Resampling window maximum length
//! This refers to the window applied over the sinc function,
//! NOT the resampling kernel itself. Should be an even integer.
#define RESAMPLE_WINDOW_LENGTH 4096

/************************************************/

struct QuadOsc_t {
	double Cos, Sin;
	double kRe, kIm;
	double xScale, yScale;
	double xOffs;
};

//! NOTE: Sin and Cos are in an undefined state;
//! ResetPhase() needs to be called prior to using them.
FORCE_INLINE void Oscillator_InitMagicCircle(struct QuadOsc_t *Osc, double xScale, double yScale, double xOffs) {
	Osc->kRe    = 2.0*sin(0.5*xScale);
	Osc->xScale = xScale;
	Osc->yScale = yScale;
	Osc->xOffs  = xOffs;
}
FORCE_INLINE void Oscillator_ResetPhaseMagicCircle(struct QuadOsc_t *Osc, double x) {
	Osc->Cos = Osc->yScale * cos((x-0.5)*Osc->xScale + Osc->xOffs);
	Osc->Sin = Osc->yScale * sin((x    )*Osc->xScale + Osc->xOffs);
}
FORCE_INLINE void Oscillator_StepMagicCircle(struct QuadOsc_t *Osc) {
	double k = Osc->kRe;
	Osc->Cos -= k * Osc->Sin;
	Osc->Sin += k * Osc->Cos;
}

/************************************************/

//! Structure to hold resampling state
struct ResampleState_t {
	uint32_t SrcOffs;
	uint16_t SrcTapOffs; //! Points to oldest value
	uint16_t DstTapOffs;
	double   Fc;
	double   Phase;
	struct QuadOsc_t SincOsc;
};

/************************************************/

static const uint8_t BytesPerSampleLUT[] = {
	1, //! PCM8
	2, //! PCM16
	3, //! PCM24
	4, //! PCM32
	4, //! FLOAT32
};

//! NOTE: +1 for the case of x==1.0, and then +1 for lerp
static float WindowFunctionLUT[RESAMPLE_WINDOW_LENGTH+1+1];

/************************************************/

//! Read samples into float buffer
static void ConvertToFloat(float *Dst, const void *SrcBuf, uint32_t N, uint8_t Format) {
	switch(Format) {
		case SRC_FORMAT_PCM8: {
			const int8_t *Src = (const int8_t*)SrcBuf;
			while(N--) *Dst++ = (float)(*Src++ ^ 0x80) * 0x1.0p-7f;
		} break;
		case SRC_FORMAT_PCM16: {
			const int16_t *Src = (const int16_t*)SrcBuf;
			while(N--) {
				int16_t v = *Src++;
#if IS_BIG_ENDIAN
				v = (int16_t)FileIO_ByteSwap16((uint16_t)v);
#endif
				*Dst++ = (float)v * 0x1.0p-15f;
			}
		} break;
		case SRC_FORMAT_PCM24: {
			const uint8_t *Src = (const uint8_t*)SrcBuf;
			while(N--) {
				int32_t v  = *Src++ <<  8;
				        v |= *Src++ << 16;
				        v |= *Src++ << 24;
				*Dst++ = (float)v * 0x1.0p-31f;
			}
		} break;
		case SRC_FORMAT_PCM32: {
			const int32_t *Src = (const int32_t*)SrcBuf;
			while(N--) {
				int32_t v = *Src++;
#if IS_BIG_ENDIAN
				v = (int32_t)FileIO_ByteSwap32((uint32_t)v);
#endif
				*Dst++ = (float)v * 0x1.0p-31f;
			}
		} break;
		case SRC_FORMAT_FLOAT32: {
			//! Already done
		} break;
	}
}
static int ReadSamples(float *Dst, uint32_t N, FILE *SrcFile, const struct SRC_Config_t *Config) {
	//! Read raw data into buffer
	//! NOTE: Read aligned to the end of buffer to avoid over-writing smaller data.
	uint32_t SrcBytesPerSample = Config->SrcChans * BytesPerSampleLUT[Config->SrcFormat];
	void *RawBuf = (uint8_t*)(Dst + N*Config->SrcChans) - N*SrcBytesPerSample;
	if(!fread(RawBuf, N*SrcBytesPerSample, 1, SrcFile)) return 0;

	//! Promote data to float
	ConvertToFloat(Dst, RawBuf, N*Config->SrcChans, Config->SrcFormat);
	return 1;
}

/************************************************/

//! Write samples into float buffer
//! NOTE: Destroys Src buffer.
static void ConvertFromFloat(void *DstBuf, const float *Src, uint32_t N, uint8_t Format, float DitherLevel) {
	static uint32_t NoiseRNG_Seed = 0x12345678;

	//! Dithering is applied in the range -0.5 ~ +0.5 at DitherLevel == 1.0
	//! Because we average with the last noise output for a cheap lowpass,
	//! the floating-point dither is scaled by a further factor of 1/2.
	uint32_t NoiseRNG = NoiseRNG_Seed;
	float OldDitherValue, DitherValue = 0.0f;
#define UPDATE_DITHER() \
	NoiseRNG ^= NoiseRNG << 13, \
	NoiseRNG ^= NoiseRNG >> 17, \
	NoiseRNG ^= NoiseRNG <<  5, \
	OldDitherValue = DitherValue, DitherValue = (float)(int32_t)NoiseRNG * (0x1.0p-32f * 0.5f)
#define GET_DITHER_NOISE() (DitherLevel * (DitherValue + OldDitherValue))
	switch(Format) {
		case SRC_FORMAT_PCM8: {
			uint8_t *Dst = (uint8_t*)DstBuf;
			while(N--) {
				UPDATE_DITHER();
				float   x = (*Src++)*0x1.0p+7f;
				int32_t y = (int32_t)lrintf(x + GET_DITHER_NOISE());
				*Dst++ = (uint8_t)(CLAMP(y, -0x80, +0x7F) + 0x80);
			}
		} break;
		case SRC_FORMAT_PCM16: {
			uint16_t *Dst = (uint16_t*)DstBuf;
			while(N--) {
				UPDATE_DITHER();
				float   x = (*Src++)*0x1.0p+15f;
				int32_t y = (int32_t)lrintf(x + GET_DITHER_NOISE());
				*Dst++ = (uint16_t)(CLAMP(y, -0x8000, +0x7FFF) + 0x8000) ^ 0x8000;
			}
		} break;
		case SRC_FORMAT_PCM24: {
			uint8_t *Dst = (uint8_t*)DstBuf;
			while(N--) {
				UPDATE_DITHER();
				float   x = (*Src++)*0x1.0p+23f;
				int32_t y = (int32_t)lrintf(x + GET_DITHER_NOISE());
				uint32_t z = (uint32_t)(CLAMP(y, -0x800000, +0x7FFFFF) + 0x800000) ^ 0x800000;
				*Dst++ = (uint8_t)(z >> 0);
				*Dst++ = (uint8_t)(z >> 8);
				*Dst++ = (uint8_t)(z >> 16);
			}
		} break;
		case SRC_FORMAT_PCM32: {
			//! NOTE: Dithering is unsupported in this mode
			int32_t *Dst = (int32_t*)DstBuf;
			while(N--) {
				float x = *Src++ * 0x1.0p+31f;
				*Dst++ = (int32_t)lrintf(CLAMP(x, -(float)0x80000000u, +(float)0x7FFFFFFFu));
			}
		} break;
		case SRC_FORMAT_FLOAT32: {
			//! Already done
		} break;
	}
	NoiseRNG_Seed = NoiseRNG;
#undef UPDATE_DITHER
#undef GET_DITHER_NOISE
}
static int WriteSamples(float *Src, uint32_t N, FILE *DstFile, const struct SRC_Config_t *Config) {
	if(Config->DstFormat != SRC_FORMAT_CUSTOM) {
		//! Convert back to native format
		void *RawBuf = Src;
		ConvertFromFloat(RawBuf, Src, N*Config->DstChans, Config->DstFormat, Config->DitherLevel);

		//! Write data to file
		uint32_t DstBytesPerSample = Config->DstChans * BytesPerSampleLUT[Config->DstFormat];
		if(!fwrite(RawBuf, N*DstBytesPerSample, 1, DstFile)) return 0;
		return 1;
	} else {
		return Config->CustomWrite(DstFile, Src, N*Config->DstChans, Config->CustomWrite_Userdata);
	}
}

/************************************************/

//! Slide samples in resampling window
static void Resample_SlideSamples(
	struct ResampleState_t *State,
	const float *Src,
	      float *TapBuffer,
	uint32_t N,
	const struct SRC_Config_t *Config
) {
	uint32_t n, M = (uint32_t)Config->FilterHalfOrder*2+1;
	uint32_t Chan, nChan = Config->DstChans;
	uint32_t SrcOffs    = State->SrcOffs;
	uint32_t SrcTapOffs = (uint32_t)State->SrcTapOffs;
	for(Chan=0;Chan<nChan;Chan++) {
		SrcOffs    = State->SrcOffs;
		SrcTapOffs = State->SrcTapOffs;
		float *ThisTap = TapBuffer + Chan*M;
		for(n=0;n<N;n++) {
			ThisTap[SrcTapOffs] = Src[SrcOffs*nChan+Chan];
			SrcOffs++;
			if(++SrcTapOffs >= M) SrcTapOffs -= M;
		}
	}
	State->SrcOffs    = SrcOffs;
	State->SrcTapOffs = (uint16_t)SrcTapOffs;
}

//! Perform resampling
//! NOTE: We use (x/2) instead of ((x-1) / 2) a lot here. This is because
//! we need performance in this loop, and the latter form would just cause
//! unnecessary slowdown, since we already know that x%2==1 by design.
FORCE_INLINE double Resample_GetWindowFunction(double k, const float *Window) {
	double   kFloat = k * (double)RESAMPLE_WINDOW_LENGTH;
	uint32_t kInt   = (uint32_t)kFloat;
	double   kPhase = kFloat - (double)kInt;
	double a = (double)Window[kInt+0];
	double b = (double)Window[kInt+1];
	return (a + (b-a)*kPhase);
}
FORCE_INLINE double Resample_GetSampleZeroPhase(
	struct ResampleState_t *State,
	double   PhaseOffs,
	uint32_t FilterOrder,
	uint32_t BufferOffs,
	const float *Buffer
) {
	uint32_t k;
	double Sum = 0.0, SumW = 0.0;
	double InvFilterOrder = 1.0 / (double)(FilterOrder+1);
	Oscillator_ResetPhaseMagicCircle(&State->SincOsc, -PhaseOffs);
	for(k=0;k<=FilterOrder;k++) {
		//! The sinc oscillator generates:
		//!  Sin[x(=k-PhaseOffs)*Fc * Pi] * 1/(Fc*Pi)
		//! Thus, we have to divide by x ourselves.
		double x = (double)((int32_t)k - (int32_t)(FilterOrder/2)) - PhaseOffs;
		double w = (x != 0.0) ? (State->SincOsc.Sin / x) : 1.0;
		Oscillator_StepMagicCircle(&State->SincOsc);

		//! Now apply the window function
		w *= Resample_GetWindowFunction(((double)(k+1) - PhaseOffs)*InvFilterOrder, WindowFunctionLUT);

		//! Finally add the weighted sample
		float Sample; {
			uint32_t Idx = BufferOffs + k;
			if(Idx >= FilterOrder) Idx -= FilterOrder;
			Sample = Buffer[Idx];
		}
		Sum  += w * (double)Sample;
		SumW += w;
	}
	return (Sum / SumW);
}
static void Resample(
	struct ResampleState_t *State,
	      float *Dst,
	const float *Src,
	      float *TapBuffer,
	uint32_t N,
	const struct SRC_Config_t *Config
) {
	uint32_t n, Chan, nChan = Config->DstChans;
	uint32_t FilterOrder = (uint32_t)Config->FilterHalfOrder*2+1;
	uint32_t SrcOffs     = State->SrcOffs;
	uint32_t SrcTapOffs  = (uint32_t)State->SrcTapOffs;
	double   Phase       = State->Phase, InvDstRate = 1.0 / Config->DstRate;
	for(Chan=0;Chan<nChan;Chan++) {
		SrcOffs    = State->SrcOffs;
		SrcTapOffs = (uint32_t)State->SrcTapOffs;
		Phase      = State->Phase;
		float *ThisTap = TapBuffer + Chan*FilterOrder;
		for(n=0;n<N;n++) {
			//! Update ring buffer
			while(Phase >= Config->DstRate) {
				Phase -= Config->DstRate;

				//! Load next sample and step the tap buffer
				ThisTap[SrcTapOffs] = Src[SrcOffs*nChan+Chan];
				if(++SrcTapOffs >= FilterOrder) SrcTapOffs -= FilterOrder;
				SrcOffs++;
			}

			//! Store to output and update phase
			Dst[n*nChan+Chan] = (float)Resample_GetSampleZeroPhase(
				State,
				Phase*InvDstRate,
				FilterOrder,
				SrcTapOffs,
				ThisTap
			);
			Phase += Config->SrcRate;
		}
	}
	State->SrcOffs    = SrcOffs;
	State->SrcTapOffs = (uint16_t)SrcTapOffs;
	State->Phase      = Phase;
}

/************************************************/

//! Convert data, with input from one stream, and output to another stream
int SRC_ConvertStreamedData(
	FILE *DstFile,
	FILE *SrcFile,
	uint32_t nDstSamples,
	uint32_t nSrcSamples,
	uint32_t SrcLoopSize,
	const struct SRC_Config_t *Config
) {
	uint8_t NeedMonoConversion = (Config->DstChans == 1 && Config->SrcChans > 1);
	uint32_t FilterOrder = (uint32_t)Config->FilterHalfOrder*2+1;

	//! Initialize mono converter
	struct Poly2Mono_t MonoConvState;
	if(NeedMonoConversion) {
		MonoConvState.nChan     = Config->SrcChans;
		MonoConvState.BlockSize = Config->MonoConvWindowSize;
		MonoConvState.nHops     = Config->MonoConvHops;
		uint8_t WindowType;
		switch(Config->MonoConvWindow) {
			case SRC_MONOCONV_WINDOW_SINE:     WindowType = POLY2MONO_WINDOW_TYPE_SINE;     break;
			case SRC_MONOCONV_WINDOW_HANN:     WindowType = POLY2MONO_WINDOW_TYPE_HANN;     break;
			case SRC_MONOCONV_WINDOW_HAMMING:  WindowType = POLY2MONO_WINDOW_TYPE_HAMMING;  break;
			case SRC_MONOCONV_WINDOW_BLACKMAN: WindowType = POLY2MONO_WINDOW_TYPE_BLACKMAN; break;
			case SRC_MONOCONV_WINDOW_NUTTALL:  WindowType = POLY2MONO_WINDOW_TYPE_NUTTALL;  break;
			default: return 0;
		}
		if(!Poly2Mono_Init(&MonoConvState, WindowType)) return 0;
	}

	//! Create buffers
	uint32_t SrcBlockSize = NeedMonoConversion ? Config->MonoConvWindowSize : SRCDATA_BLOCK_SIZE;
	uint32_t DstBlockSize = DSTDATA_BLOCK_SIZE; {
		//! If we're not going to need the full buffer, just allocate the most we'll need
		if(Config->DstRate < Config->SrcRate) {
			DstBlockSize = (uint32_t)ceil((double)DSTDATA_BLOCK_SIZE * Config->DstRate / Config->SrcRate);
		}
	}
	uint8_t *Buffer;
	float *SrcSampleBuffer;
	float *DstSampleBuffer;
	float *SrcSampleTaps;
	float *DstSampleTaps; {
		//! Create buffer offsets and sizes
		uint32_t AllocSize = 0;
#define CREATE_BUFFER(Name, Size) \
	uint32_t Name##_Offs = AllocSize; \
	uint32_t Name##_Size = Size; \
	AllocSize += Name##_Size
		CREATE_BUFFER(SrcSampleBuffer, SrcBlockSize        * Config->SrcChans * sizeof(float));
		CREATE_BUFFER(DstSampleBuffer, DstBlockSize        * Config->DstChans * sizeof(float));
		CREATE_BUFFER(SrcSampleTaps,   FilterOrder         * Config->DstChans * sizeof(float));
		CREATE_BUFFER(DstSampleTaps,   DSTDATA_FILTER_TAPS * Config->DstChans * sizeof(float));
#undef CREATE_BUFFER
		//! Allocate memory and assign pointers
		Buffer = (uint8_t*)malloc(AllocSize);
		if(!Buffer) return 0;
		SrcSampleBuffer = (float*)(Buffer + SrcSampleBuffer_Offs);
		DstSampleBuffer = (float*)(Buffer + DstSampleBuffer_Offs);
		SrcSampleTaps   = (float*)(Buffer + SrcSampleTaps_Offs);
		DstSampleTaps   = (float*)(Buffer + DstSampleTaps_Offs);

		//! Clear the sample taps
		memset(SrcSampleTaps, 0, SrcSampleTaps_Size);
		memset(DstSampleTaps, 0, DstSampleTaps_Size);
	}

	//! Initialize state
	struct ResampleState_t State;
	State.SrcOffs    = 0;
	State.SrcTapOffs = 0;
	State.DstTapOffs = 0;
	State.Fc         = MIN(Config->DstRate / Config->SrcRate, Config->Cutoff);
	State.Phase      = 0.0;

	//! Resampling is expensive, so disable it when it's not needed
	int NeedResample = (Config->DstRate != Config->SrcRate || Config->Cutoff != 1.0);

	//! Initialize resampling state
	if(NeedResample) {
		Oscillator_InitMagicCircle(
			&State.SincOsc,
			M_PI * State.Fc,
			M_1_PI / State.Fc, //! <- This normalizes the output to a peak level of 1.0
			M_PI * -(double)Config->FilterHalfOrder * State.Fc
		);

		//! Initialize resampling window function
		GenerateWindowFunction(
			WindowFunctionLUT,
			Config->FilterWindow,
			RESAMPLE_WINDOW_LENGTH
		);
	}

	//! Begin processing
	uint32_t nSrcSamplesRem     = nSrcSamples;
	uint32_t nPrimingSamplesRem = (NeedMonoConversion ? SrcBlockSize : 0);
	uint32_t nBytesPerSrcSample = Config->SrcChans * BytesPerSampleLUT[Config->SrcFormat];
	uint32_t nDstSamplesRemToProcess = nDstSamples;
	if(NeedResample) nPrimingSamplesRem += Config->FilterHalfOrder;
	while(nDstSamplesRemToProcess) {
		uint32_t Chan, nChan = Config->DstChans;

		//! Read samples into buffer for this run
		{
			//! This slightly wonky setup is needed to work around end-of-sample/loops
			float *Dst = SrcSampleBuffer;
			uint32_t nSamplesRem = SrcBlockSize;
			while(nSamplesRem) {
				//! Out of samples?
				if(!nSrcSamplesRem) {
					//! Unlooped waveforms just fill with 0 until the end
					if(!SrcLoopSize) {
						memset(Dst, 0, nSamplesRem * Config->SrcChans * sizeof(float));
						break;
					} else {
						fseek(SrcFile, -(SrcLoopSize * nBytesPerSrcSample), SEEK_CUR);
						nSrcSamplesRem += SrcLoopSize;
					}
				}

				//! Read as many samples as possible
				uint32_t nSamplesThisRun = MIN(nSamplesRem, nSrcSamplesRem);
				if(!ReadSamples(Dst, nSamplesThisRun, SrcFile, Config)) {
					free(Buffer);
					return 0;
				}
				Dst            += nSamplesThisRun * Config->SrcChans;
				nSamplesRem    -= nSamplesThisRun;
				nSrcSamplesRem -= nSamplesThisRun;
			}

			//! Handle mono conversion
			if(NeedMonoConversion) {
				Poly2Mono_Process(&MonoConvState, SrcSampleBuffer, SrcSampleBuffer);
			}

			//! Reset source offset and done
			State.SrcOffs = 0;
		}

		//! Handle priming of the buffer
		const float *RawSrcSampleBuffer = SrcSampleBuffer;
		uint32_t ThisSrcBlockSize = SrcBlockSize;
		if(nPrimingSamplesRem) {
			uint32_t nPrimingSamplesThisRun = MIN(nPrimingSamplesRem, ThisSrcBlockSize);
			if(NeedResample) {
				Resample_SlideSamples(
					&State,
					SrcSampleBuffer,
					SrcSampleTaps,
					nPrimingSamplesThisRun,
					Config
				);
			} else RawSrcSampleBuffer += nPrimingSamplesThisRun*nChan;
			nPrimingSamplesRem -= nPrimingSamplesThisRun;
			ThisSrcBlockSize   -= nPrimingSamplesThisRun;
		}

		//! Finally, do actual resampling and output
		uint32_t nDstSamples = (uint32_t)ceil(((double)ThisSrcBlockSize*Config->DstRate - State.Phase) / Config->SrcRate);
		         nDstSamples = MIN(nDstSamples, nDstSamplesRemToProcess);
		{
			//! We need to do this in batches in case we can output more
			//! samples than the output buffer can hold at any one time.
			uint32_t nSamplesRem = nDstSamples;
			while(nSamplesRem) {
				uint32_t nOutSamplesThisRun = MIN(nSamplesRem, DstBlockSize);

				//! Do resampling
				if(NeedResample) Resample(
					&State,
					DstSampleBuffer,
					SrcSampleBuffer,
					SrcSampleTaps,
					nOutSamplesThisRun,
					Config
				);

				//! Apply high-shelf filter and final gain
				for(Chan=0;Chan<nChan;Chan++) {
					uint32_t n, TapOffs = State.DstTapOffs;
					const float *Src  = (NeedResample ? DstSampleBuffer : RawSrcSampleBuffer) + Chan;
					      float *Dst  = DstSampleBuffer + Chan;
					      float *Taps = DstSampleTaps + Chan*DSTDATA_FILTER_TAPS;
					      float a0 = Config->GlobalGain * (1.0f + Config->HighShelfGain) * 0.5f;
					      float a1 = Config->GlobalGain * (1.0f - Config->HighShelfGain) * 0.5f;
					for(n=0;n<nOutSamplesThisRun;n++) {
						float SmpA = *Src; Src += Config->DstChans;
						float SmpB = Taps[(n-1 + TapOffs) & (DSTDATA_FILTER_TAPS-1)];
						*Dst = a0*SmpA + a1*SmpB, Dst += Config->DstChans;

						Taps[(n + TapOffs) & (DSTDATA_FILTER_TAPS-1)] = SmpA;
						TapOffs++;
					}
				}
				State.DstTapOffs   += nOutSamplesThisRun;
				RawSrcSampleBuffer += nOutSamplesThisRun*nChan;

				//! Write output
				if(!WriteSamples(DstSampleBuffer, nOutSamplesThisRun, DstFile, Config)) {
					free(Buffer);
					return 0;
				}
				nSamplesRem -= nOutSamplesThisRun;
			}
		}

		//! Push any remaining samples into the ring buffers
		if(NeedResample) {
			uint32_t nSkip = SrcBlockSize - State.SrcOffs;
			if(nSkip) {
				Resample_SlideSamples(
					&State,
					SrcSampleBuffer,
					SrcSampleTaps,
					nSkip,
					Config
				);
				State.Phase -= (double)nSkip * Config->DstRate;
			}
		}

		//! Move to next samples
		nDstSamplesRemToProcess -= nDstSamples;
	}

	//! All done
	if(NeedMonoConversion) Poly2Mono_Destroy(&MonoConvState);
	free(Buffer);
	return 1;
}

/************************************************/
//! EOF
/************************************************/
