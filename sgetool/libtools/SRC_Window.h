/************************************************/
#include <math.h>
/************************************************/

//! Generate window to target
static void GenerateWindowFunction(float *Dst, uint8_t Window, uint32_t N) {
	uint32_t n, nOffs = 0;
	double InvN = 1.0 / (double)N;
	switch(Window) {
		case SRC_WINDOW_NONE: {
			for(n=0;n<N;n++) Dst[n] = 1.0f;
		} break;

		case SRC_WINDOW_SINE: {
			double InvNPi = M_PI * InvN;
			for(n=0;n<N;n++) {
				double xPi = (double)(n+nOffs) * InvNPi;
				double v = sin(xPi);
				Dst[n] = (float)v;
			}
		} break;

		case SRC_WINDOW_HANN: {
			double InvN2Pi = (2*M_PI) * InvN;
			for(n=0;n<N;n++) {
				double x2Pi = (double)(n+nOffs) * InvN2Pi;
				double v = (
					+0.5
					-0.5*cos(x2Pi)
				);
				Dst[n] = (float)v;
			}
		} break;

		case SRC_WINDOW_BLACKMAN: {
			double InvN2Pi = (2*M_PI) * InvN;
			for(n=0;n<N;n++) {
				double x2Pi = (double)(n+nOffs) * InvN2Pi;
				double v = (
					+0.42
					-0.50*cos(1*x2Pi)
					+0.08*cos(2*x2Pi)
				);
				Dst[n] = (float)v;
			}
		} break;

		//! "Some Windows with Very Good Sidelobe Behavior", A. Nuttall
		//! DOI: 10.1109/TASSP.1981.1163506
		//! Eq. 37 (minimum 4-term window)
		case SRC_WINDOW_NUTTALL: {
			double InvN2Pi = (2*M_PI) * InvN;
			for(n=0;n<N;n++) {
				double x2Pi = (double)(n+nOffs) * InvN2Pi;
				double v = (
					+0.3635819
					-0.4891775*cos(1*x2Pi)
					+0.1365995*cos(2*x2Pi)
					-0.0106411*cos(3*x2Pi)
				);
				Dst[n] = (float)v;
			}
		} break;

		case SRC_WINDOW_LANCZOS: {
			double InvN2Pi = (2*M_PI) * InvN;
			for(n=0;n<N;n++) {
				double x2Pi = (double)(n+nOffs) * InvN2Pi - M_PI;
				double v = (x2Pi != 0.0) ? (sin(x2Pi) / x2Pi) : 1.0;
				Dst[n] = (float)v;
			}
		} break;

		case SRC_WINDOW_LANCZOS2: {
			double InvN2Pi = (2*M_PI) * InvN;
			for(n=0;n<N;n++) {
				double x2Pi = (double)(n+nOffs) * InvN2Pi - M_PI;
				double v = (x2Pi != 0.0) ? (sin(x2Pi) / x2Pi) : 1.0;
				       v = (v * v);
				Dst[n] = (float)v;
			}
		} break;
	}
	Dst[N] = Dst[N+1] = 0.0f; //! <- Dst[N] for x==1.0, and Dst[N+1] for lerp
}

/************************************************/
//! EOF
/************************************************/
