#include <stdint.h>
#include <stdio.h>

const struct {
	uint8_t Code;
	uint8_t Ticks;
	uint8_t nNybbles;
} Codes[] = {
	{   0,   0, 0},
	{ 0x0, 192, 1},
	{ 0x1,  96, 1},
	{ 0x2,  48, 1},
	{ 0x3,  24, 1},
	{ 0x4,  12, 1},
	{ 0x5,   6, 1},
	{ 0x6,   3, 1},
	{0x0E, 128, 2},
	{0x1E,  64, 2},
	{0x2E,  32, 2},
	{0x3E,  16, 2},
	{0x4E,   8, 2},
	{0x5E,   4, 2},
	{0x6E,   2, 2},
	{0xEE,   1, 2},
};
const int nCodes = sizeof(Codes) / sizeof(Codes[0]);

//! This code is stupid, but only needs to generate once so don't care
int main(int argc, const char *argv[]) {
	(void)argc;
	(void)argv;

	int nTicks;
	FILE *OutputFile = fopen("TickTimeCodeLUT.txt", "wt");
	for(nTicks=1;nTicks<=768;nTicks++) {
		int      BestCost = 1000;
		uint32_t BestCode = 0;
		int a, b, c, d;
		for(a=0;a<nCodes;a++) for(b=0;b<nCodes;b++) for(c=0;c<nCodes;c++) for(d=0;d<nCodes;d++) {
			int Duration = 0, Cost = 0;
			uint32_t Code = 0;
			uint8_t aCode = Codes[a].Code;
			uint8_t bCode = Codes[b].Code;
			uint8_t cCode = Codes[c].Code;
			uint8_t dCode = Codes[d].Code;
			if(a && (b || c || d)) aCode += aCode == 0xEE ? 1 : aCode >= 0xE ? 0x70 : 7;
			if(b && (     c || d)) bCode += bCode == 0xEE ? 1 : bCode >= 0xE ? 0x70 : 7;
			if(c && (          d)) cCode += cCode == 0xEE ? 1 : cCode >= 0xE ? 0x70 : 7;
#define ADD(x) do { \
		Code |= x##Code << (Cost*4); \
		Duration += Codes[x].Ticks; \
		Cost     += Codes[x].nNybbles; \
	} while(0)
			ADD(a);
			ADD(b);
			ADD(c);
			ADD(d);
			if(Duration == nTicks && Cost < BestCost) {
				BestCost = Cost;
				BestCode = Code;
			}
		}
		if(BestCost > 4) BestCost = 0, BestCode = 0;
		fprintf(
			OutputFile,
			"{%d,0x%02X,0x%02X}",
			BestCost,
			(BestCode >> 0) & 0xFF,
			(BestCode >> 8) & 0xFF
		);
		if((nTicks%4) == 0) fprintf(OutputFile, ",\n\t");
		else fprintf(OutputFile, ", ");
	}
}
