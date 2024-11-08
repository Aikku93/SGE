/**************************************/
#include <math.h>
#include <stdlib.h>
/**************************************/
#include "Fourier.h"
#include "Poly2Mono.h"
#include "Poly2Mono_Helper.h"
/**************************************/

void Poly2Mono_Process(struct Poly2Mono_t *State, float *Output, const float *Input) {
	int n, Chan, Hop;
	int nChan     = State->nChan;
	int BlockSize = State->BlockSize;
	int nHops     = State->nHops;
	int HopSize   = BlockSize / nHops;

	//! Process each hop as the outer loop
	float *Window   = State->Window;   POLY2MONO_ASSUME_ALIGNED(Window,   POLY2MONO_BUFFER_ALIGNMENT);
	float *BfTemp   = State->BfTemp;   POLY2MONO_ASSUME_ALIGNED(BfTemp,   POLY2MONO_BUFFER_ALIGNMENT);
	float *BfOutput = State->BfOutput; POLY2MONO_ASSUME_ALIGNED(BfOutput, POLY2MONO_BUFFER_ALIGNMENT);
	float *BfWeight = State->BfWeight; POLY2MONO_ASSUME_ALIGNED(BfWeight, POLY2MONO_BUFFER_ALIGNMENT);
	for(Hop=0;Hop<nHops;Hop++) {
		//! Clear Abs,Arg for accumulation across channels
		for(n=0;n<BlockSize;n++) BfOutput[n] = 0.0f;

		//! Accumulate all channels
		float *BfFwdLap = State->BfFwdLap; POLY2MONO_ASSUME_ALIGNED(BfFwdLap, POLY2MONO_BUFFER_ALIGNMENT);
		float *BfReSum  = BfTemp  + BlockSize;
		float *BfImSum  = BfReSum + BlockSize/2;
		for(n=0;n<BlockSize/2;n++) BfWeight[n] = 0.0f;
		for(n=0;n<BlockSize/2;n++) BfReSum [n] = 0.0f;
		for(n=0;n<BlockSize/2;n++) BfImSum [n] = 0.0f;
		for(Chan=0;Chan<nChan;Chan++) {
			//! Apply DFT
			float *BfDFT     = BfTemp;
			float *BfDFTTemp = BfImSum + BlockSize/2;
			for(n=0;n<BlockSize/2;n++) {
				BfDFT[            n] = Window[n] * BfFwdLap[n];
				BfDFT[BlockSize-1-n] = Window[n] * BfFwdLap[BlockSize-1-n];
			}
			Fourier_FFTReCenter(BfDFT, BfDFTTemp, BlockSize);

			//! Extract values and accumulate
			//! NOTE: We don't store weights for the Re/Im sums, as they're
			//! only used for phase information, so amplitude is irrelevant.
			for(n=0;n<BlockSize/2;n++) {
				float Re   = BfDFT[n*2+0];
				float Im   = BfDFT[n*2+1];
				float Abs2 = SQR(Re) + SQR(Im);
				BfOutput[n*2+0] += Abs2 * Abs2;
				BfWeight[n]     += Abs2;
				BfReSum [n]     += Re;
				BfImSum [n]     += Im;
			}

			//! Shift samples into buffer
			for(n=HopSize;n<BlockSize;n++) {
				BfFwdLap[n-HopSize] = BfFwdLap[n];
			}
			for(n=0;n<HopSize;n++) {
				BfFwdLap[BlockSize-HopSize+n] = Input[(Hop*HopSize+n)*nChan+Chan];
			}

			//! Next channel
			BfFwdLap += BlockSize;
		}

		//! Apply average of channels
		for(n=0;n<BlockSize/2;n++) {
			float Re = 0.0f, Im = 0.0f, w = BfWeight[n];
			if(w) {
				float ReSum = BfReSum[n];
				float ImSum = BfImSum[n];
				float Norm  = SQR(ReSum) + SQR(ImSum);
				if(Norm) {
					Norm = sqrtf(BfOutput[n*2+0] / (w * Norm));
					Re = ReSum * Norm;
					Im = ImSum * Norm;
				}
			}
			BfOutput[n*2+0] = Re;
			BfOutput[n*2+1] = Im;
		}

		//! Apply iDFT and accumulate frame
		float *BfInvLap = State->BfInvLap; POLY2MONO_ASSUME_ALIGNED(BfInvLap, POLY2MONO_BUFFER_ALIGNMENT);
		Fourier_iFFTReCenter(BfOutput, BfTemp, BlockSize);
		for(n=0;n<BlockSize/2;n++) {
			BfInvLap[            n] += Window[n] * BfOutput[n];
			BfInvLap[BlockSize-1-n] += Window[n] * BfOutput[BlockSize-1-n];
		}

		//! Shift samples into/out of buffers
		if(Output) {
			for(n=0;n<HopSize;n++) Output[Hop*HopSize+n] = BfInvLap[n];
		}
		for(n=HopSize;n<BlockSize;n++) {
			BfInvLap[n-HopSize] = BfInvLap[n];
		}
		for(n=0;n<HopSize;n++) {
			BfInvLap[BlockSize-HopSize+n] = 0.0f;
		}
	}
}

/**************************************/
//! EOF
/**************************************/
