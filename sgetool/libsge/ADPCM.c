/************************************************/
#include <math.h>
#include <stddef.h>
#include <stdint.h>
/************************************************/
#include "ADPCM.h"
#include "GlobalHelpers.h"
/************************************************/

//! Size of analysis windows for use in LPC solving
//! The window used is a 50%-overlap Hann window
#define ANALYSIS_WINDOW_SIZE 32

/************************************************/

//! Signed rounding 2^n division
FORCE_INLINE
int32_t Div2nRound(int32_t x, int n) {
	return (x + (((1 << n) - (x < 0)) >> 1)) >> n;
}

//! Convert float to PCM16
FORCE_INLINE
int16_t FloatToPCM16(float x) {
	return (int16_t)lrintf(fmaxf(-32768.0f, fminf(+32767.0f, 32768.0f * x)));
}

/************************************************/

//! 2nd order LPC->LSP
//! Returns 0 on failure, 1 on success.
static int lpc2lsp(double LSP[2], const double LPC[2]) {
	double Pr = -0.5*(LPC[0] + LPC[1] - 1.0); if(fabs(Pr) >= 1.0) return 0;
	double Qr = -0.5*(LPC[0] - LPC[1] + 1.0); if(fabs(Qr) >= 1.0) return 0;
	LSP[0] = acos(Pr);
	LSP[1] = acos(Qr);
	return 1;
}

//! 2nd order LSP->LPC
static void lsp2lpc(double LPC[2], const double LSP[2]) {
	double Freq[2] = {cos(LSP[0]), cos(LSP[1])};
	LPC[0] = 0.0 - Freq[0] - Freq[1];
	LPC[1] = 1.0 - Freq[0] + Freq[1];
}

//! 2nd order equation solver (Cramer's rule)
//!  a1*x + b1*y = c1
//!  a2*x + b2*y = c2
//! Returns 0.0 on unsolvable and doesn't store to R[]
static double Cramer(double R[2], double a1, double b1, double a2, double b2, double c1, double c2) {
	double Det = a1*b2 - b1*a2;
	if(Det != 0.0) {
		R[0] = (c1*b2 - b1*c2) / Det;
		R[1] = (c2*a1 - a2*c1) / Det;
	}
	return Det;
}

/************************************************/

//! Get LPC coefficients of windowed segment
//! Returns determinant of matrix (0.0 = Unsolvable)
static double LPCSolve(double LPC[2], const double *Data, size_t DataSize) {
	//! Compute covariances
	size_t n;
	//double w00 = 0.0;
	double w01 = 0.0, w11 = 0.0;
	double w02 = 0.0, w12 = 0.0, w22 = 0.0;
	for(n=2;n<DataSize;n++) {
		double x0 = Data[n-2];
		double x1 = Data[n-1];
		double x2 = Data[n-0];
		//w00 += x0*x0;
		w01 += x0*x1;
		w02 += x0*x2;
		w11 += x1*x1;
		w12 += x1*x2;
		w22 += x2*x2;
	}

	//! Perform diagonal loading
	//! This provides robustness in noisy signals (eg. from quantized residuals)
	//w00 *= 1.0 + 1.0e-3;
	w11 *= 1.0 + 1.0e-3;
	w22 *= 1.0 + 1.0e-3;

	//! w11*x[n-1] + w12*x[n-2] = -w01
	//! w21*x[n-1] + w22*x[n-2] = -w02
	return Cramer(LPC, w11, w12, /*w21*/w12, w22, -w01, -w02);
}

//! Get LPC coefficients by averaging over small windows
//! Returns 0.0 if no solutions were found and doesn't store to LPC[]
static double WindowedLPCSolve(double LPC[2], const float *Data, size_t nSamples, size_t DataStride) {
	size_t n;
	double Frame[ANALYSIS_WINDOW_SIZE];

	//! Not enough samples to form windows?
	if(nSamples < ANALYSIS_WINDOW_SIZE*2) {
		for(n=0;n<nSamples;n++) Frame[n] = (double)Data[n*DataStride];
		return LPCSolve(LPC, Frame, nSamples);
	}

	//! Accumulate coefficients
	double CoefSum[2] = {0.0, 0.0};
	double CoefWeight = 0.0; {
		//! Generate window function
		double Window[ANALYSIS_WINDOW_SIZE];
		for(n=0;n<ANALYSIS_WINDOW_SIZE;n++) {
			//! NOTE: Scaling doesn't matter for LPC analysis
			Window[n] = 1.0 - cos((n+0.5) * M_PI / ANALYSIS_WINDOW_SIZE);
		}

		//! Perform windowed analysis
		size_t Offs = 0;
		for(;;) {
			//! If we don't have enough data for a full frame,
			//! rewind a few samples until we have enough data
			size_t nSamplesRem = nSamples - Offs;
			if(nSamplesRem < ANALYSIS_WINDOW_SIZE) {
				Offs -= (ANALYSIS_WINDOW_SIZE - nSamplesRem);
			}

			//! Create windowed samples
			for(n=0;n<ANALYSIS_WINDOW_SIZE;n++) {
				size_t ThisOffs = Offs + n;
				if(ThisOffs >= ANALYSIS_WINDOW_SIZE/2)
					Frame[n] = (double)Data[(ThisOffs - ANALYSIS_WINDOW_SIZE/2)*DataStride] * Window[n];
				else
					Frame[n] = 0.0;
			}

			//! Window -> LPC -> LSP -> Sum
			double LPC[2], LSP[2];
			if(LPCSolve(LPC, Frame, ANALYSIS_WINDOW_SIZE) != 0.0 && lpc2lsp(LSP, LPC)) {
				//! Give importance to filters with higher resonance, and at lower frequencies
				double w = (LSP[1] + LSP[0]) * (LSP[1] - LSP[0]);
				       w = 1.0 / MAX(w, 0x1.0p-15);
				CoefWeight += w;
				CoefSum[0] += w*LSP[0];
				CoefSum[1] += w*LSP[1];
			}

			//! Adjust offset. Last frame?
			Offs += ANALYSIS_WINDOW_SIZE/2;
			if(nSamplesRem <= ANALYSIS_WINDOW_SIZE) break;
		}
	}

	//! Have coefficients?
	if(CoefWeight != 0.0) {
		//! Take mean, LSP -> LPC
		CoefSum[0] /= CoefWeight;
		CoefSum[1] /= CoefWeight;
		lsp2lpc(LPC, CoefSum);
	}
	return CoefWeight;
}

/************************************************/

//! Initialize ADPCM state
void ADPCM_Init(struct ADPCM_t *State, const float *Data, size_t nSamples, size_t DataStride) {
	//! Solve for LPC coefficients
	double LPC[2] = {-1.0, 0.0}; //! <- Default to DPCM
	WindowedLPCSolve(LPC, Data, nSamples, DataStride);
	int cM1 = lrint(LPC[0] * -(double)(1 << ADPCM_FILTER_BITS));
	int cM2 = lrint(LPC[1] * -(double)(1 << ADPCM_FILTER_BITS));
	State->cM1 = (int8_t)CLAMP(cM1, -0x80, +0x7F);
	State->cM2 = (int8_t)CLAMP(cM2, -0x80, +0x7F);

	//! Store first two samples
	State->zM1 = FloatToPCM16(Data[1*DataStride]);
	State->zM2 = FloatToPCM16(Data[0*DataStride]);
}

/************************************************/

//! Encode ADPCM frame
uint32_t ADPCM_EncodeFrame(struct ADPCM_t *State, const float *Data, size_t DataStride) {
	//! Bruteforce the optimal shift factor
	int n, Shift;
	uint32_t BestFrame = 0; //! <- Shuts gcc up
	uint32_t BestError = ~0u;
	int16_t  ShiftTap[15][2];
	for(Shift=0;Shift<15;Shift++) {
		//! Compress frame as best as possible and get peak error
		uint32_t  ThisFrame = 0;
		uint32_t  ThisError = 0;
		 int16_t *ThisTap = ShiftTap[Shift];
		ThisTap[0] = State->zM1;
		ThisTap[1] = State->zM2;
		for(n=0;n<ADPCM_FRAME_SIZE;n++) {
			//! Compute prediction and quantize residue
			int Xn = lrintf(32768.0 * Data[n*DataStride]);
			int Pn = (ThisTap[0]*State->cM1 + ThisTap[1]*State->cM2) >> ADPCM_FILTER_BITS;
			int Rn = Xn - Pn;
			int Qn = Div2nRound(Rn, Shift);
			    Qn = CLAMP(Qn, -8, +7);

			//! Decode and adjust residue to avoid overflow
			int Yn = Pn + (Qn << Shift);
			if((int16_t)Yn != Yn) {
				//! Adjust residual to get Yn in range
				if(Yn < 0) while((int16_t)Yn != Yn && Qn < +7)
					Yn = Pn + (++Qn << Shift);
				else while((int16_t)Yn != Yn && Qn > -8)
					Yn = Pn + (--Qn << Shift);

				//! Impossible? Skip this shift factor
				if((int16_t)Yn != Yn) { ThisError = ~0u; break; }
			}

			//! Check peak error
			//! PONDER: Does this optimization fall on a convex hull? If so,
			//! it might work better to work our way down the Shift factor
			//! and early exit when ThisError > BestError.
			uint32_t En = (uint32_t)ABS(Xn - Yn);
			if(En > ThisError) {
				ThisError = En;
				if(ThisError > BestError) break;
			}

			//! Adjust taps and push residue
			ThisTap[1] = ThisTap[0];
			ThisTap[0] = Yn;
			ThisFrame = (ThisFrame << 4) | ((uint32_t)Qn & 0xF);
		}

		//! Lower error?
		if(ThisError < BestError) {
			BestFrame = (ThisFrame << 4) | (Shift+1); //! Shifter in low bits
			BestError = ThisError;
			if(BestError == 0) break;
		}
	}
	int BestShift = (int)(BestFrame & 0xF) - 1;
	State->zM1 = ShiftTap[BestShift][0];
	State->zM2 = ShiftTap[BestShift][1];
	return BestFrame;
}

/************************************************/
//! EOF
/************************************************/
