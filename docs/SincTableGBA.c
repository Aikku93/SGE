#include <math.h>
#include <stdint.h>
#include <stdio.h>

#define WINDOW_SINE 0
#define WINDOW_HANN 1

#define LOWPASS    1.0
#define SINCTAPS   4 //! Main lobe is N-1 samples, last sample is for phase offsets
#define NPHASES   64
#define GAIN     193 //! At Phase=0, we need to represent 1.0 exactly, so this must be < 256
#define WINDOW_TYPE WINDOW_SINE

static double sinc(double x) {
	return (x != 0.0) ? (sin(x) / x) : 1.0;
}

static double window(double x) {
#if (WINDOW_TYPE == WINDOW_SINE)
	return sin(x * M_PI);
#elif (WINDOW_TYPE == WINDOW_HANN)
	double y1 = sin(x * M_PI);
	return y1*y1;
#else
	return 1.0;
#endif
}

static void sincWindow(double Window[SINCTAPS], double Phase) {
	int i;
	for(i=0;i<SINCTAPS;i++) {
		double x = (double)(i+1) - Phase;
		double s = sinc((x - (double)SINCTAPS/2.0) * M_PI * LOWPASS);
		double w = window((double)x / (double)SINCTAPS);
		Window[i] = (s * w); //! <- Use sine window
	}
}

static double isincWindow(uint8_t Window[SINCTAPS], double Phase) {
	int i;
	double fw[SINCTAPS];
	sincWindow(fw, Phase);

	double Total = 0.0;
	for(i=0;i<SINCTAPS;i++) Total += fw[i];

	double Norm = (double)GAIN / Total;
	double e[SINCTAPS];
	int IntTotal = 0;
	for(i=0;i<SINCTAPS;i++) {
		double v = fw[i] * Norm;
		double vi = round(v);
		Window[i] = (uint8_t)fabs(vi);
		e[i] = v - vi;
		IntTotal += (int)vi;
	}

	while(IntTotal != GAIN) {
		if(IntTotal < GAIN) {
			int BestIdx = 0;
			double BestError = -INFINITY;
			for(i=0;i<SINCTAPS;i++) if(e[i] > BestError) {
				BestIdx = i;
				BestError = e[i];
			}
			if(Window[BestIdx] >= 255) {
				e[BestIdx] = -INFINITY;
			} else {
				Window[BestIdx]++;
				IntTotal++;
			}
		} else {
			int BestIdx = 0;
			double BestError = INFINITY;
			for(i=0;i<SINCTAPS;i++) if(e[i] < BestError) {
				BestIdx = i;
				BestError = e[i];
			}
			if(Window[BestIdx] == 0) {
				e[BestIdx] = INFINITY;
			} else {
				Window[BestIdx]--;
				IntTotal--;
			}
		}
	}

	double e2 = 0.0;
	for(i=0;i<SINCTAPS;i++) {
		double e = fabs(fw[i] * Norm) - (double)Window[i];
		e2 += e*e;
	}
	return e2;
}

int main(int argc, const char *argv[]) {
	int i, j;
	int MaxNorm = 0;
	double e2 = 0.0;
	for(i=0;i<NPHASES;i++) {
		uint8_t w[SINCTAPS];
		e2 += isincWindow(w, (double)i / (double)NPHASES);
		putchar('{');

		int Norm = 0;
		for(j=0;j<SINCTAPS;j++) {
			if(j > 0) putchar(',');
			printf("0x%02X", w[j]);
			Norm += w[j];
		}
		if(Norm > MaxNorm) MaxNorm = Norm;
		putchar('}');
		if(i%4 == 3) putchar('\n');
	}
	printf("Max norm = %u\n", MaxNorm);
	printf("RMSE = %.3f\n", sqrt(e2 / NPHASES));
	return 0;
}
