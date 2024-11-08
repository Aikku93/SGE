/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
/************************************************/
#ifdef SGE_PLATFORM_HAVE_REVERB
/************************************************/

@ r0: &ReverbData
@ r1:  CutoffHz
@ r2:  RateHz

ASM_FUNC_GLOBAL(SGE_Reverb_SetLowpassCutoff)
ASM_FUNC_BEG   (SGE_Reverb_SetLowpassCutoff, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

@ Lp = 2^(-2Pi*RevLp/RateHz * Log2[E])
SGE_Reverb_SetLowpassCutoff:
	PUSH	{r0,lr}
	LSL	r0, r1, #0x0E
	MOV	r1, r2
	BL	__aeabi_uidiv
	LDR	r1, =74258    @ 74258 = 2^27 * 2Pi*Log2[E] / 2^14
	MUL	r1, r0
	MOV	r0, #0x08     @ Lp in .8fxp
	LSL	r0, #0x1B
	SBC	r0, r1        @ 2^(8 - Cutoff/Rate*2Pi*Log2[E]), but more like 2^(7.9999-x) to avoid needing to clip
	SBC	r1, r1
	BIC	r0, r1        @ <- Clip to 0 when Fb is < 1
	BEQ	0f
	BL	SGE_Exp2fxp
0:	POP	{r2-r3}
	STRB	r0, [r2, #0x01]
	BX	r3

ASM_FUNC_END(SGE_Reverb_SetLowpassCutoff)

/************************************************/
#endif
/************************************************/
//! EOF
/************************************************/
