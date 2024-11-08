/************************************************/
#include <math.h>
#include <stdio.h>
/************************************************/
#include "SGE-Compiler.h"
/************************************************/

//! Read raw 32bit integer
int SGE_ReadUInt32(uint32_t *Target, const char *Str, const char **StrEnd) {
	uint32_t Value;
	int nChars = 0;
	if(sscanf(Str, "%u%n", &Value, &nChars) == 0) return 0;
	if(Target) *Target = Value;
	if(StrEnd) *StrEnd = Str + nChars;
	return 1;
}

/************************************************/

//! Read raw double float
int SGE_ReadDouble(double *Target, const char *Str, const char **StrEnd) {
	double Value;
	int nChars = 0;
	if(sscanf(Str, "%lf%n", &Value, &nChars) == 0) return 0;
	if(Target) *Target = Value;
	if(StrEnd) *StrEnd = Str + nChars;
	return 1;
}

/************************************************/

//! Read gain in linear form, or dB form
int SGE_ReadGain(double *Target, const char *Str, const char **StrEnd) {
	double Gain;
	int nChars = 0;
	int IsDecibel = 0;
	if(!sscanf(Str, "%lf %n%*1[dD]%*1[bB]%n", &Gain, &nChars, &IsDecibel)) return 0;
	if(StrEnd) *StrEnd = IsDecibel ? (Str + IsDecibel) : (Str + nChars);
	if(Target) *Target = IsDecibel ? pow(10.0, Gain/20.0) : Gain;
	return 1;
}

/************************************************/
//! EOF
/************************************************/
