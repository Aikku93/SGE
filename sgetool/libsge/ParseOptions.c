/************************************************/
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
/************************************************/
#include "GlobalHelpers.h"
#include "SGE-Compiler.h"
/************************************************/

//! Parse options from strings/arguments
int SGE_ParseOptions(
	struct SGE_gOptions_t *Options,
	const char **OptArgv,
	uint32_t OptArgc,
	uint8_t  UseBlockTag,
	SGE_ParseOptions_ErrorLogger_t ErrorLogger,
	void *ErrorLogger_Userdata,
	const struct SGE_ParseOptions_ExtraOpt_t *ExtraOpt,
	void *ExtraOpt_Userdata
) {
	//! Get initial string
	if(!OptArgc) return 0;
	const char *OptStr = *OptArgv++;
	if(!OptStr) return 0;
	if(UseBlockTag) {
		//! Search for the "SGE(" identifier
		OptStr = strstr(OptStr, "SGE(");
		if(!OptStr) return 0;
		OptStr += strlen("SGE(");
	}

	//! Now begin parsing
	int HadErrors     = 0;
	int OptionsParsed = 0;
	for(;;) {
#define OPT_MATCH(x) (\
	!memcmp(OptStr, x, strlen(x)) && \
	((OptStr += strlen(x)) || 1) \
)
#define OPT_MATCH_ARG(x) (\
	!memcmp(OptStr, x, strlen(x)) && \
	(OptStr[strlen(x)] == '\0' || OptStr[strlen(x)] == ')' || isspace(OptStr[strlen(x)])) && \
	((OptStr += strlen(x)) || 1) \
)
		for(;;) {
			char x = *OptStr;

			//! End of string? Move to next argument string
			if(x == '\0') {
				if(--OptArgc == 0) goto EndOptionsParsing;
				OptStr = *OptArgv++;
				continue;
			}

			//! End of SGE block or end of options?
			if(UseBlockTag) {
				if(x == ')') goto EndOptionsParsing;
			} else {
				if(x != '-' || !strchr(OptStr, ':')) goto EndOptionsParsing;
			}

			//! Otherwise just break on non-whitespace
			if(!isspace(x)) break;
			OptStr++;
		}

		//! NOTE: If Parsed = 0, then Success will be set to 0 after this if-elseif block
		int Parsed  = 1;
		int Success = 1;
		int FoundExtraOpt = 0;
		if(OPT_MATCH("-wavgain:")) {
			double Gain;
			const char *GainStr = OptStr;
			if(!SGE_ReadGain(&Gain, OptStr, &OptStr)) {
				Parsed = 0;
			} else if(!isnormal(Gain) || Gain < -100.0 || Gain > +100.0) {
				Success = 0;
			} else {
				Options->WavGlobalGain = Gain;
			}
			if(!Parsed || !Success) {
				if(ErrorLogger) ErrorLogger(ErrorLogger_Userdata, "ERROR: Invalid waveform gain (-wavgain:%s).\n", GainStr);
			}
		} else if(OPT_MATCH("-wavinterpolate:")) {
			const char *CondStr = OptStr;
			       if(OPT_MATCH_ARG("y") || OPT_MATCH_ARG("Y")) {
				Options->WavInterpolate = SGE_OPT_WAVINTERPOLATE_ON;
			} else if(OPT_MATCH_ARG("n") || OPT_MATCH_ARG("N")) {
				Options->WavInterpolate = SGE_OPT_WAVINTERPOLATE_OFF;
			} else if(OPT_MATCH_ARG("lt")) {
				Options->WavInterpolate = SGE_OPT_WAVINTERPOLATE_RATELT;
			} else if(OPT_MATCH_ARG("gt")) {
				Options->WavInterpolate = SGE_OPT_WAVINTERPOLATE_RATEGT;
			} else {
				Parsed = 0;
			}
			if(!Parsed) {
				if(ErrorLogger) ErrorLogger(ErrorLogger_Userdata, "ERROR: Unknown interpolation option (-wavinterpolate:%s).\n", CondStr);
			}
		} else if(OPT_MATCH("-wavgainadj:")) {
			double Gain;
			const char *GainStr = OptStr;
			if(!SGE_ReadGain(&Gain, OptStr, &OptStr)) {
				Parsed = 0;
			} else if(!isnormal(Gain) || Gain < -100.0 || Gain > +100.0) {
				Success = 0;
			} else {
				Options->WavGainAdjust = Gain;
			}
			if(!Parsed || !Success) {
				if(ErrorLogger) ErrorLogger(ErrorLogger_Userdata, "ERROR: Invalid waveform gain adjustment (-wavgainadj:%s).\n", GainStr);
			}
		} else if(OPT_MATCH("-wavformat:")) {
			const char *FmtStr = OptStr;
			       if(OPT_MATCH_ARG("default")) {
				Options->WavFormat = SGE_OPT_WAVFORMAT_DEFAULT;
			} else if(OPT_MATCH_ARG("pcm8") || OPT_MATCH_ARG("PCM8")) {
				Options->WavFormat = SGE_WAV_FRMT_PCM8;
			} else if(OPT_MATCH_ARG("pcm16") || OPT_MATCH_ARG("PCM16")) {
				Options->WavFormat = SGE_WAV_FRMT_PCM16;
			} else if(OPT_MATCH_ARG("adpcm") || OPT_MATCH_ARG("ADPCM")) {
				Options->WavFormat = SGE_WAV_FRMT_ADPCM4;
			} else {
				Parsed = 0;
			}
			if(!Parsed) {
				if(ErrorLogger) ErrorLogger(ErrorLogger_Userdata, "ERROR: Unknown wave format (-wavformat:%s).\n", FmtStr);
			}
		} else if(OPT_MATCH("-minloopsize:")) {
			uint32_t Length;
			const char *LenStr = OptStr;
			if(!SGE_ReadUInt32(&Length, OptStr, &OptStr)) {
				Parsed = 0;
			} else if(Length > 0x7FFFFFFF) {
				Success = 0;
			} else {
				int32_t Size = (int32_t)Length;
				if(OPT_MATCH_ARG("ms")) Size = -Size;
				Options->WavMinLoopSize = Size;
			}
			if(!Parsed) {
				if(ErrorLogger) ErrorLogger(ErrorLogger_Userdata, "ERROR: Invalid minimum loop size (-minloopsize:%s).\n", LenStr);
			}
		} else if(OPT_MATCH("-forcemono:")) {
			const char *CondStr = OptStr;
			       if(OPT_MATCH_ARG("never")) {
				Options->WavForceMono = 0;
			} else if(OPT_MATCH_ARG("always")) {
				Options->WavForceMono = 1;
			} else {
				Parsed = 0;
			}
			if(!Parsed) {
				if(ErrorLogger) ErrorLogger(ErrorLogger_Userdata, "ERROR: Unknown mono conversion condition (-forcemono:%s).\n", CondStr);
			}
		} else if(OPT_MATCH("-monoconv-blk:")) {
			const char *SizeStr = OptStr;
			uint32_t WindowSize;
			if(!SGE_ReadUInt32(&WindowSize, OptStr, &OptStr)) {
				Parsed = 0;
			} else if(WindowSize < 8 || WindowSize > 65536 || !IS_POWER_OF_2(WindowSize)) {
				Success = 0;
			} else {
				Options->WavMonoConvWindowSize = WindowSize;
			}
			if(!Parsed || !Success) {
				if(ErrorLogger) ErrorLogger(ErrorLogger_Userdata, "ERROR: Invalid window size for mono conversion (-monoconv-blk:%s).\n", SizeStr);
			}
		} else if(OPT_MATCH("-monoconv-wnd:")) {
			const char *WndStr = OptStr;
			       if(OPT_MATCH_ARG("sine")) {
				Options->WavMonoConvWindowType = SGE_OPT_MONOCONV_WINDOW_SINE;
			} else if(OPT_MATCH_ARG("hann")) {
				Options->WavMonoConvWindowType = SGE_OPT_MONOCONV_WINDOW_HANN;
			} else if(OPT_MATCH_ARG("hamming")) {
				Options->WavMonoConvWindowType = SGE_OPT_MONOCONV_WINDOW_HAMMING;
			} else if(OPT_MATCH_ARG("blackman")) {
				Options->WavMonoConvWindowType = SGE_OPT_MONOCONV_WINDOW_BLACKMAN;
			} else if(OPT_MATCH_ARG("nuttall")) {
				Options->WavMonoConvWindowType = SGE_OPT_MONOCONV_WINDOW_NUTTALL;
			} else {
				Parsed = 0;
			}
			if(!Parsed) {
				if(ErrorLogger) ErrorLogger(ErrorLogger_Userdata, "ERROR: Unknown mono conversion window function (-monoconv-wnd:%s).\n", WndStr);
			}
		} else if(OPT_MATCH("-monoconv-hops:")) {
			uint32_t nHops;
			const char *HopsStr = OptStr;
			if(!SGE_ReadUInt32(&nHops, OptStr, &OptStr)) Parsed = 0;
			else if(nHops < 2 || nHops > Options->WavMonoConvWindowSize || !IS_POWER_OF_2(nHops)) {
				Parsed = 0;
			} else {
				Options->WavMonoConvHops = (uint16_t)nHops;
			}
			if(!Parsed) {
				if(ErrorLogger) ErrorLogger(ErrorLogger_Userdata, "ERROR: Invalid number of hops for mono conversion (-monoconv-hops:%s).\n", HopsStr);
			}
		} else if(OPT_MATCH("-resample:")) {
			if(OPT_MATCH_ARG("none")) {
				Options->WavResampleRate = 0;
			} else {
				uint32_t Rate;
				const char *RateStr = OptStr;
				if(!SGE_ReadUInt32(&Rate, OptStr, &OptStr)) {
					Parsed = 0;
				} else if(Rate < SGE_OPT_WAVRESAMPLE_MINRATE || Rate > SGE_OPT_WAVRESAMPLE_MAXRATE) {
					Success = 0;
				} else {
					Options->WavResampleRate = Rate;
				}
				if(!Parsed || !Success) {
					if(ErrorLogger) ErrorLogger(ErrorLogger_Userdata, "ERROR: Invalid rate for resampling (-resample:%s).\n", RateStr);
				}
			}
		} else if(OPT_MATCH("-resampleif:")) {
			const char *CondStr = OptStr;
			       if(OPT_MATCH_ARG("never")) {
				Options->WavResampleCondition = SGE_OPT_WAVRESAMPCOND_NEVER;
			} else if(OPT_MATCH_ARG("always")) {
				Options->WavResampleCondition = SGE_OPT_WAVRESAMPCOND_ALWAYS;
			} else if(OPT_MATCH_ARG("gt")) {
				Options->WavResampleCondition = SGE_OPT_WAVRESAMPCOND_GT;
			} else if(OPT_MATCH_ARG("lt")) {
				Options->WavResampleCondition = SGE_OPT_WAVRESAMPCOND_LT;
			} else {
				Parsed = 0;
			}
			if(!Parsed) {
				if(ErrorLogger) ErrorLogger(ErrorLogger_Userdata, "ERROR: Unknown resampling condition (-resampleif:%s).\n", CondStr);
			}
		} else if(OPT_MATCH("-lowpass:")) {
			if(OPT_MATCH_ARG("none")) {
				Options->WavLowpassCutoff = 0.0;
			} else {
				double CutoffHz;
				const char *CutoffStr = OptStr;
				if(!SGE_ReadDouble(&CutoffHz, OptStr, &OptStr)) {
					Parsed = 0;
				} else if(!isnormal(CutoffHz) || CutoffHz < 20.0 || CutoffHz > 384000.0) {
					Success = 0;
				} else {
					Options->WavLowpassCutoff = CutoffHz;
				}
				if(!Parsed || !Success) {
					if(ErrorLogger) ErrorLogger(ErrorLogger_Userdata, "ERROR: Invalid low-pass cut-off frequency (-lowpass:%s).\n", CutoffStr);
				}
			}
		} else if(OPT_MATCH("-highshelf:")) {
			double Gain;
			const char *GainStr = OptStr;
			if(!SGE_ReadGain(&Gain, OptStr, &OptStr)) {
				Parsed = 0;
			} else if(!isnormal(Gain) || Gain < -100.0 || Gain > +100.0) {
				Success = 0;
			} else {
				Options->WavHighShelfGain = Gain;
			}
			if(!Parsed || !Success) {
				if(ErrorLogger) ErrorLogger(ErrorLogger_Userdata, "ERROR: Invalid high-shelf filter gain (-highshelf:%s).\n", GainStr);
			}
		} else if(OPT_MATCH("-oversample:")) {
			double Rate;
			const char *RateStr = OptStr;
			if(!SGE_ReadDouble(&Rate, OptStr, &OptStr)) {
				Parsed = 0;
			} else if(!isnormal(Rate)) {
				Success = 0;
			} else {
				//! If oversampling was specified as semitones, parse it now
				       if(OPT_MATCH_ARG("c") || OPT_MATCH_ARG("cents")) {
					Rate = pow(2.0, Rate/1200.0);
				} else if(OPT_MATCH_ARG("st")) {
					Rate = pow(2.0, Rate/12.0);
				}
				if(!isnormal(Rate) || Rate <= 0.0 || Rate > 100.0) {
					Success = 0;
				} else Options->WavOversampleRate = Rate;
			}
			if(!Parsed || !Success) {
				if(ErrorLogger) ErrorLogger(ErrorLogger_Userdata, "ERROR: Invalid oversampling rate (-oversample:%s).\n", RateStr);
			}
		} else if(OPT_MATCH("-wavcull:")) {
			const char *CondStr = OptStr;
			       if(OPT_MATCH_ARG("y") || OPT_MATCH_ARG("Y")) {
				Options->WavEnableCull = 1;
			} else if(OPT_MATCH_ARG("n") || OPT_MATCH_ARG("N")) {
				Options->WavEnableCull = 0;
			} else {
				Parsed = 0;
			}
			if(!Parsed) {
				if(ErrorLogger) ErrorLogger(ErrorLogger_Userdata, "ERROR: Unknown culling condition (-wavcull:%s).\n", CondStr);
			}
		} else if(OPT_MATCH("-src-align:")) {
			const char *AlignStr = OptStr;
			       if(OPT_MATCH_ARG("any")) {
				Options->SRCAlign = SGE_OPT_SRCALIGN_ANY;
			} else if(OPT_MATCH_ARG("loops")) {
				Options->SRCAlign = SGE_OPT_SRCALIGN_LOOPS;
			} else if(OPT_MATCH_ARG("loops-nonperc")) {
				Options->SRCAlign = SGE_OPT_SRCALIGN_LOOPS_NONPERC;
			} else {
				Parsed = 0;
			}
			if(!Parsed) {
				if(ErrorLogger) ErrorLogger(ErrorLogger_Userdata, "ERROR: Unknown SRC alignment (-src-align:%s).\n", AlignStr);
			}
		} else if(OPT_MATCH("-src-round:")) {
			const char *RoundStr = OptStr;
			       if(OPT_MATCH_ARG("down")) {
				Options->SRCRound = SGE_OPT_SRCROUND_DOWN;
			} else if(OPT_MATCH_ARG("middle")) {
				Options->SRCRound = SGE_OPT_SRCROUND_MIDDLE;
			} else if(OPT_MATCH_ARG("up")) {
				Options->SRCRound = SGE_OPT_SRCROUND_UP;
			} else {
				Parsed = 0;
			}
			if(!Parsed) {
				if(ErrorLogger) ErrorLogger(ErrorLogger_Userdata, "ERROR: Unknown SRC rounding mode (-src-round:%s).\n", RoundStr);
			}
		} else if(OPT_MATCH("-src-order:")) {
			uint32_t Order;
			const char *OrderStr = OptStr;
			if(!SGE_ReadUInt32(&Order, OptStr, &OptStr)) {
				Parsed = 0;
			} else if((Order&1) == 0 || Order < 3 || Order > 511) {
				Success = 0;
			} else {
				Options->SRCHalfOrder = (uint8_t)((Order-1) / 2);
			}
			if(!Parsed || !Success) {
				if(ErrorLogger) ErrorLogger(ErrorLogger_Userdata, "ERROR: Invalid SRC filter order (-src-order:%s).\n", OrderStr);
			}
		} else if(OPT_MATCH("-src-window:")) {
			const char *WndStr = OptStr;
			       if(OPT_MATCH_ARG("none")) {
				Options->SRCWindow = SGE_OPT_SRCWINDOW_NONE;
			} else if(OPT_MATCH_ARG("sine")) {
				Options->SRCWindow = SGE_OPT_SRCWINDOW_SINE;
			} else if(OPT_MATCH_ARG("hann")) {
				Options->SRCWindow = SGE_OPT_SRCWINDOW_HANN;
			} else if(OPT_MATCH_ARG("hamming")) {
				Options->SRCWindow = SGE_OPT_SRCWINDOW_HAMMING;
			} else if(OPT_MATCH_ARG("blackman")) {
				Options->SRCWindow = SGE_OPT_SRCWINDOW_BLACKMAN;
			} else if(OPT_MATCH_ARG("nuttall")) {
				Options->SRCWindow = SGE_OPT_SRCWINDOW_NUTTALL;
			} else if(OPT_MATCH_ARG("lanczos")) {
				Options->SRCWindow = SGE_OPT_SRCWINDOW_LANCZOS;
			} else if(OPT_MATCH_ARG("lanczos2")) {
				Options->SRCWindow = SGE_OPT_SRCWINDOW_LANCZOS2;
			} else {
				Parsed = 0;
			}
			if(!Parsed) {
				if(ErrorLogger) ErrorLogger(ErrorLogger_Userdata, "ERROR: Unknown SRC window function (-src-window:%s).\n", WndStr);
			}
		} else if(OPT_MATCH("-src-dither:")) {
			double Level;
			const char *LevelStr = OptStr;
			if(!SGE_ReadDouble(&Level, OptStr, &OptStr)) {
				Parsed = 0;
			} else if(Level < 0.0 || Level > 1.0) {
				Success = 0;
			} else {
				Options->SRCDitherLevel = Level;
			}
			if(!Parsed || !Success) {
				if(ErrorLogger) ErrorLogger(ErrorLogger_Userdata, "ERROR: Invalid SRC dither level (-src-dither:%s).\n", LevelStr);
			}
		} else {
			Parsed = 0;

			//! Check the extra options
			if(ExtraOpt) {
				const struct SGE_ParseOptions_ExtraOpt_t *NextOpt = ExtraOpt;
				while(NextOpt->OptStr) {
					if(OPT_MATCH(NextOpt->OptStr)) {
						FoundExtraOpt = 1;
						if(!NextOpt->OptFunc(OptStr, ExtraOpt_Userdata, Options, ErrorLogger, ErrorLogger_Userdata)) {
							Success = 0;
						}
						break;
					}
					NextOpt++;
				}
			}

			//! Otherwise-unknown option
			if(!FoundExtraOpt && ErrorLogger) ErrorLogger(ErrorLogger_Userdata, "ERROR: Unrecognized option (%s).\n", OptStr);
		}

		//! If parsing failed, then success failed too
		if(!Parsed && !FoundExtraOpt) Success = 0;
		if(Success) OptionsParsed++;
		else HadErrors = 1;

		//! Unknown option (or unparsed extra option)? Skip until whitespace
		if(!Parsed) for(;;) {
			char x = *OptStr;
			if(x == '\0' || isspace(x)) break;
			OptStr++;
		}
#undef OPT_MATCH_ARG
#undef OPT_MATCH
	}
EndOptionsParsing:
	return HadErrors ? (-OptionsParsed) : OptionsParsed;
}

/************************************************/
//! EOF
/************************************************/
