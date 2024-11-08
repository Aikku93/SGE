/**************************************/
#include <math.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
/**************************************/
#define CLAMP(x,y,z) ((x) < (y) ? (y) : ((x) > (z) ? (z) : (x)))
/**************************************/

//! Input:  5.21fxp (ie. 0.0 .. 31.999999)
//! Output: 32.0fxp (adjust input accordingly)
static unsigned int exp2i(unsigned int x) {
	//! 2^15 * 2^x, x = [0..16]/16
	static const unsigned int LUT[32+1] = {
		32768, 33486, 34219, 34968, 35734, 36516, 37316, 38133,
		38968, 39821, 40693, 41584, 42495, 43425, 44376, 45348,
		46341, 47356, 48393, 49452, 50535, 51642, 52773, 53928,
		55109, 56316, 57549, 58809, 60097, 61413, 62757, 64132,
		65536
	};
	unsigned int e = x >> 21; x -= e << 21;
	unsigned int a = LUT[(x >> (21-5)) + 0];
	unsigned int b = LUT[(x >> (21-5)) + 1]; x &= (1u << (21-5)) - 1;
	unsigned int m = (a << (21-5)) + (b-a)*x;
	return m >> ((21-5) + 15 - e);
}

/**************************************/

//! Filter tap structure
struct RevFlt_t {
	int16_t  Len;
	int16_t  Cnt;
	int32_t *End;
	int32_t  zLp;
};

static inline void RevFlt_Init(struct RevFlt_t *Flt, int32_t *End, int Len) {
	Flt->Len =  Len;
	Flt->Cnt = -Len;
	Flt->End =  End;
	Flt->zLp =  0;
}

//! Lowpass is .8fxp, Phi is .8fxp
static inline int RevFlt_Process(struct RevFlt_t *Flt, int Smp, int Lp, int Phi) {
	Flt->zLp = Smp += (Flt->zLp - Smp)*Lp >> 8;

	int Out, Tap = Flt->End[Flt->Cnt];
	Out = Smp - (Tap*Phi >> 8); Flt->End[Flt->Cnt] = Out;
	Out = Tap + (Out*Phi >> 8);
	if(++Flt->Cnt >= 0) Flt->Cnt -= Flt->Len;
	return Out;
}

/**************************************/

/*!

For a given `FilterTime` (the delay line length) and desired `DecayTime`, `Feedback` is derived as:
 eps = 10^(-60/20) (for a target level of -60dB at DecayTime)
 Feedback = eps^(FilterTime/DecayTime)
          = (10^(-60/20))^(FilterTime/DecayTime)
          = 10^(-60/20 * FilterTime/DecayTime)
  thus
 FilterTime = DecayTime * -Log10[Feedback]/3

When the filters are processed in series, then the `FilterTime` is the sum of their parts. And as this
is derived from `Density`, solves as follows:
 FilterTime = MaxFilterTime * Sum[Density^n, {n,0,nTaps-1}]
            = MaxFilterTime * (1-Density^nTaps) / (1-Density)
  thus
 MaxFilterTime = (DecayTime * -Log10[Feedback]/3) * (1-Density)/(1-Density^nTaps)

However, `Feedback` is provided as dB gain (-48..0dB):
 Feedback = 10^(Feedback_dB/20)
 MaxFilterTime = (DecayTime * -Feedback_dB/60) * (1-Density)/(1-Density^nTaps)

!*/
//! NOTE: Returns .9fxp
static inline int GetMaximalTapLength(int RateHz, int Fb, int DecayT, int RoomD, int nTaps) {
	//! Maximum values: RateHz=384kHz, Fb=-48dB, DecayT=20.0s, RoomD=255/256
	unsigned int i, DelayLast = 1<<24, DelayInvScale = DelayLast;
	for(i=1;i<nTaps;i++) {
		DelayLast = DelayLast*RoomD >> 8;
		DelayInvScale += DelayLast;
	}

	//! 559241 = 1/60 * 2^25
	//! 32.0[RateHz] * .25[Scale] * .8[DecayT] * .8[Fb] / .8 / .24[DelayInvScale] = .9
	return ((RateHz * 559241ull * DecayT * -Fb + 0x80) >> 8) / DelayInvScale;
}

/**************************************/

int main(int argc, const char *argv[]) {
	//! Parse arguments
	int FltTaps = 6;
	int RateHz  = 44100;
	int RevFb   = lrintf(-6.000f * (1<<8));
	int RevLp   = 15000;
	int DelayT  = lrintf(20.000f * (1<<8));
	int DecayT  = lrintf( 3.000f * (1<<8));
	int RoomD   = lrintf( 0.750f * (1<<8));
	int StereoW = lrintf( 0.500f * (1<<8));
	const char *OutFilename = "Reverb.sw";
	if(argc < 2) {
		printf("SGE Reverb Test Suite\n");
		printf("Usage: reverbtest Input.sw [Options]\n");
		printf("Creates Reverb.sw as output.\n");
		printf("Note: Input must be raw 16bit PCM (as interleaved stereo).\n");
		printf("Options:\n");
		printf(" -o:Reverb.sw - Output File\n");
		printf(" -n:6         - Filter Taps       (1     .. 16)\n");
		printf(" -r:44100     - Playback Rate     (1kHz  .. 384kHz)\n");
		printf(" -fb:-6.0     - Feedback Gain     (-48   .. -0.004dB)\n");
		printf(" -lp:15000    - Lowpass Cutoff    (500Hz .. Nyquist)\n");
		printf(" -dl:0.20     - Delay Time        (0.00  .. 100.00ms)\n");
		printf(" -dt:3.00     - Decay Time        (0.10  .. 20.00s)\n");
		printf(" -rd:0.75     - Room Density      (0.50  .. 0.996)\n");
		printf(" -sw:0.50     - Stereo Width      (0.00  .. 50.00ms)\n");
		return 1;
	} else {
		int i;
		for(i=2;i<argc;i++) {
			//! Output File
			if(!memcmp(argv[i], "-o:", 3)) OutFilename = argv[i] + 3;

			//! Filter Taps. Range: 1 .. 16
			if(!memcmp(argv[i], "-n:", 3)) {
				const char *StrSrc = argv[i]+3;
				int N = lrint(atof(StrSrc));
				if(N >= 1 && N <= 16) FltTaps = N;
				else printf("WARNING: Ignoring invalid argument to Filter Taps (%s)\n", StrSrc);
				continue;
			}

			//! Playback Rate. Range: 1kHz .. 384kHz
			if(!memcmp(argv[i], "-r:", 3)) {
				const char *StrSrc = argv[i]+3;
				int Hz = lrint(atof(StrSrc));
				if(Hz >= 1000 && Hz <= 384000) RateHz = Hz;
				else printf("WARNING: Ignoring invalid argument to Playback Rate (%s)\n", StrSrc);
				continue;
			}

			//! Feedback Gain. Range: -48*256 .. -1/256
			if(!memcmp(argv[i], "-fb:", 4)) {
				const char *StrSrc = argv[i]+4;
				int Fb = lrint(atof(StrSrc) * (1<<8));
				if(Fb >= -48*256 && Fb < 0) RevFb = Fb;
				else printf("WARNING: Ignoring invalid argument to Feedback Gain (%s)\n", StrSrc);
				continue;
			}

			//! Lowpass Cutoff. Range: 500Hz .. Nyquist
			if(!memcmp(argv[i], "-lp:", 4)) {
				const char *StrSrc = argv[i]+4;
				int Lp = lrint(atof(StrSrc));
				if(Lp >= 500 && Lp*2 <= RateHz) RevLp = Lp;
				else printf("WARNING: Ignoring invalid argument to Lowpass Cutoff (%s)\n", StrSrc);
				continue;
			}

			//! Delay Time. Range: 0/256 .. 100.0*256
			if(!memcmp(argv[i], "-dl:", 4)) {
				const char *StrSrc = argv[i]+4;
				int Dl = lrint(atof(StrSrc) * (1<<8));
				if(Dl >= 0 && Dl <= 100*256) DelayT = Dl;
				else printf("WARNING: Ignoring invalid argument to Delay Time (%s)\n", StrSrc);
				continue;
			}

			//! Decay Time. Range: 25/256 .. 20.0*256
			if(!memcmp(argv[i], "-dt:", 4)) {
				const char *StrSrc = argv[i]+4;
				int Dt = lrint(atof(StrSrc) * (1<<8));
				if(Dt >= 25 && Dt <= 20*256) DecayT = Dt;
				else printf("WARNING: Ignoring invalid argument to Decay Time (%s)\n", StrSrc);
				continue;
			}

			//! Room Density. Range: 128/256 .. 255/256
			if(!memcmp(argv[i], "-rd:", 4)) {
				const char *StrSrc = argv[i]+4;
				int Rd = lrint(atof(StrSrc) * (1<<8));
				if(Rd >= 128 && Rd <= 255) RoomD = Rd;
				else printf("WARNING: Ignoring invalid argument to Room Density (%s)\n", StrSrc);
				continue;
			}

			//! Stereo Width. Range: 0/256 .. 50.0*256
			if(!memcmp(argv[i], "-sw:", 4)) {
				const char *StrSrc = argv[i]+4;
				int Sw = lrint(atof(StrSrc) * (1<<8));
				if(Sw >= 0 && Sw <= 50*256) StereoW = Sw;
				else printf("WARNING: Ignoring invalid argument to Stereo Width (%s)\n", StrSrc);
				continue;
			}

			printf("WARNING: Unrecognized option (%s)\n", argv[i]);
		}
	}

	//! Sanitize arguments
	if(RevLp*2 > RateHz) { printf("WARNING: Lowpass (%uHz) exceeded Nyquist (%uHz)\n", RevLp, RateHz>>1); RevLp = RateHz>>1; }

	//! Create filter structures
	int32_t *FilterMem;
	struct RevFlt_t FilterTaps[FltTaps][2];
	size_t FilterSamples = 0; {
		int i, j;

		//! Initialize structures and get samples to allocate
		int TapLen = GetMaximalTapLength(RateHz, RevFb, DecayT, RoomD, FltTaps);
		int Spread = RateHz * StereoW / 1000;
		for(i=0;i<FltTaps;i++) {
			for(j=0;j<2;j++) {
				int SpreadMul = ((i^j) & 1)*2 - 1; //! L: {-+-+-+...}, R: {+-+-+-...}
				int Len = (TapLen + Spread*SpreadMul) >> 9;
				if(Len <= 0) { printf("ERROR: Filter too small; try reducing Stereo Width or increasing Room Density.\n"); return -1; }
				FilterSamples += Len;
				RevFlt_Init(&FilterTaps[i][j], NULL, Len);
			}
			TapLen = TapLen*RoomD >> 8;
		}

		//! Allocate samples
		FilterMem = calloc(FilterSamples, sizeof(*FilterTaps[0][0].End));
		if(!FilterMem) { printf("ERROR: Out of memory\n"); return -1; }

		//! Set buffers in filters
		int32_t *Mem = FilterMem;
		for(i=0;i<FltTaps;i++) for(j=0;j<2;j++) FilterTaps[i][j].End = Mem += FilterTaps[i][j].Len;
	}

	//! Display configuration
	printf("Configuration:\n");
	printf(" Filter Taps:    %u\n",     FltTaps);
	printf(" Playback Rate:  %uHz\n",   RateHz);
	printf(" Lowpass Cutoff: %uHz\n",   RevLp);
	printf(" Feedback Gain:  %.2fdB\n", RevFb   / (float)(1<<8));
	printf(" Delay Time:     %.2fms\n", DelayT  / (float)(1<<8));
	printf(" Decay Time:     %.2fs\n",  DecayT  / (float)(1<<8));
	printf(" Room Density:   %.2f\n",   RoomD   / (float)(1<<8));
	printf(" Stereo Width:   %.2fms\n", StereoW / (float)(1<<8));
	printf(" Memory Usage:   %.1fKiB\n",  FilterSamples*sizeof(*FilterTaps[0][0].End) / 1024.0f);
	printf(" Filter sizes:\n"); {
		int n;
		printf("  L: { ");
			for(n=0;n<FltTaps;n++) printf("%4u ", FilterTaps[n][0].Len);
		printf("}\n");
		printf("  R: { ");
			for(n=0;n<FltTaps;n++) printf("%4u ", FilterTaps[n][1].Len);
		printf("}\n");
	}

	//! Get buffers
	int16_t *Input, *Output;
	size_t InitialDelaySamples = (uint64_t)DelayT * RateHz / (1000u << 8);
	if(!InitialDelaySamples) InitialDelaySamples = 1;
	size_t nInputSmp, nOutputSmp; {
		FILE *File = fopen(argv[1], "rb"); if(!File) { printf("ERROR: Couldn't open %s\n", argv[1]); free(FilterMem); return -1; }
		fseek(File, 0, SEEK_END);
		nInputSmp  = ftell(File) / sizeof(int16_t) / 2;
		nOutputSmp = nInputSmp + InitialDelaySamples + lrintf(DecayT / (float)(1<<8) * RateHz);
		rewind(File);

		Input  = malloc((nInputSmp + nOutputSmp) * 2*sizeof(int16_t));
		Output = Input + nInputSmp*2;
		if(!Input) { printf("ERROR: Out of memory\n"); fclose(File); free(FilterMem); return -1; }
		fread(Input, 2*sizeof(int16_t), nInputSmp, File);
		fclose(File);
	}

	//! Get processing parameters
	//! 1361 = 2^21 * Log2[10]/20 /  2^8
	//! 1160 = 2^21 * 2Pi*Log2[E] / 2^14
	RevFb = 0xFFFFFFFFu / exp2i(-RevFb             * 1361u + (24<<21)) + 1; //! 2^(Feedback_dB/20    * Log2[10])
	RevLp = 0xFFFFFFFFu / exp2i((RevLp<<14)/RateHz * 1160u + (24<<21)) + 1; //! 2^(-2Pi*RevLp/RateHz * Log2[E])

	//! Process samples
	size_t n;
	for(n=0;n<nOutputSmp;n++) {
		//! Pass through filters and swap L/R
		int i;
		int WetL = (n < InitialDelaySamples) ? 0 : Output[(n-InitialDelaySamples)*2+1];
		int WetR = (n < InitialDelaySamples) ? 0 : Output[(n-InitialDelaySamples)*2+0];
		for(i=0;i<FltTaps;i++) {
			WetL = RevFlt_Process(&FilterTaps[i][0], WetL, RevLp, +0.5 * (1<<8));
			WetR = RevFlt_Process(&FilterTaps[i][1], WetR, RevLp, -0.5 * (1<<8));
		}

		//! Store mixed output
		int DryL = (n < nInputSmp) ? Input[n*2+0] : 0;
		int DryR = (n < nInputSmp) ? Input[n*2+1] : 0;
		WetL = DryL + (WetL*RevFb >> 8);
		WetR = DryR + (WetR*RevFb >> 8);
		Output[n*2+0] = CLAMP(WetL, -0x8000, +0x7FFF);
		Output[n*2+1] = CLAMP(WetR, -0x8000, +0x7FFF);
	}

	/*! Write output !*/ {
		FILE *File = fopen(OutFilename, "wb");
		if(File) {
			fwrite(Output, 2*sizeof(int16_t), nOutputSmp, File);
			fclose(File);
			printf("Ok\n");
		} else printf("ERROR: Couldn't open output file\n");
	}

	//! Done
	free(Input);
	free(FilterMem);
	return 0;
}

/**************************************/
//! EOF
/**************************************/
