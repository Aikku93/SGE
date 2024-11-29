/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
/************************************************/
#if (!defined(SGE_PLATFORM_HAVE_REVERB) && defined(SGE_PLATFORM_HAVE_FAKE_REVERB))
/************************************************/

@ r0: &ReverbData
@ r1:  Feedback_dBatten

ASM_FUNC_GLOBAL(SGE_FakeReverb_SetFeedbackLevel)
ASM_FUNC_BEG   (SGE_FakeReverb_SetFeedbackLevel, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

@ Fb = 2^(-Feedback_dBatten/20 * Log2[10])
SGE_FakeReverb_SetFeedbackLevel:
	PUSH	{r0,lr}
	LDR	r0, =87082    @ 87082 = 2^27 * Log2[10]/20 /  2^8
	MUL	r1, r0
	MOV	r0, #0x10     @ Fb in .16fxp
	LSL	r0, #0x1B
	SBC	r0, r1        @ 2^(16-Log2[Fb]), but more like 2^(15.9999-x) to avoid needing to clip
	SBC	r1, r1
	BIC	r0, r1        @ <- Clip to 0 when Fb is < 1
	BEQ	0f
	BL	SGE_Exp2fxp
0:	POP	{r2-r3}
	STRH	r0, [r2, #0x14]
	BX	r3

ASM_FUNC_END(SGE_FakeReverb_SetFeedbackLevel)

/************************************************/
#endif
/************************************************/
//! EOF
/************************************************/
