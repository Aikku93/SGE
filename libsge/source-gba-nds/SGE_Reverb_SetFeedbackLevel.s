/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
/************************************************/
#ifdef SGE_PLATFORM_HAVE_REVERB
/************************************************/

@ r0: &ReverbData
@ r1:  Feedback_dBatten

ASM_FUNC_GLOBAL(SGE_Reverb_SetFeedbackLevel)
ASM_FUNC_BEG   (SGE_Reverb_SetFeedbackLevel, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

@ Fb = 2^(-Feedback_dBatten/20 * Log2[10])
SGE_Reverb_SetFeedbackLevel:
	PUSH	{r0,lr}
	LDR	r0, =87082    @ 87082 = 2^27 * Log2[10]/20 /  2^8
	MUL	r1, r0
	MOV	r0, #0x08     @ Fb in .8fxp
	LSL	r0, #0x1B
	SBC	r0, r1        @ 2^(8-Log2[Fb]), but more like 2^(7.9999-x) to avoid needing to clip
	SBC	r1, r1
	BIC	r0, r1        @ <- Clip to 0 when Fb is < 1
	BEQ	0f
	BL	SGE_Exp2fxp
0:	POP	{r2-r3}
	STRB	r0, [r2, #0x00]
	BX	r3

ASM_FUNC_END(SGE_Reverb_SetFeedbackLevel)

/************************************************/
#endif
/************************************************/
//! EOF
/************************************************/
