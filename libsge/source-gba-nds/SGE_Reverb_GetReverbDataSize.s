/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
/************************************************/
#ifdef SGE_PLATFORM_HAVE_REVERB
/************************************************/

@ r0: &ReverbParam

ASM_FUNC_GLOBAL(SGE_Reverb_GetReverbDataSize)
ASM_FUNC_BEG   (SGE_Reverb_GetReverbDataSize, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Reverb_GetReverbDataSize:
	MOV	r3, lr
	PUSH	{r3-r4}
	MOV	r4, r0
1:	@MOV	r0, r4          @ DataSize = SumOfAllLineLengths*sizeof(s8) -> r0?
	MOV	r1, #0x00
	BL	SGE_Reverb_GetLineLengths
#if (defined(__NDS__) || SGE_ALLPASS_BITS > 8)
	LSL	r3, r0, #0x01   @ DataSize *= sizeof(s16) -> r3
#else
	MOV	r3, r0          @ DataSize *= sizeof(s8) -> r3
#endif
	BMI	2f
	LDRB	r1, [r4, #0x00] @ nTaps -> r1
#ifdef SGE_PLATFORM_HAVE_FANCY_REVERB
	MOV	r2, #0x0C*2     @ DataSize += nTaps*2*sizeof(SGE_ReverbLine_t)
	ADD	r3, #0x04       @ DataSize += sizeof(SGE_ReverbData_t)
	MUL	r1, r2
#else
	ADD	r3, #0x0C       @ DataSize += sizeof(SGE_ReverbData_t)
	LSL	r1, #0x03+1     @ DataSize += nTaps*2*sizeof(SGE_ReverbLine_t)
#endif
	ADD	r0, r3, r1      @ Return DataSize
2:	POP	{r3-r4}
	BX	r3

ASM_FUNC_END(SGE_Reverb_GetReverbDataSize)

/************************************************/
#endif
/************************************************/
//! EOF
/************************************************/
