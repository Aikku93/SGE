/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
/************************************************/
#if (!defined(SGE_PLATFORM_HAVE_REVERB) && defined(SGE_PLATFORM_HAVE_FAKE_REVERB))
/************************************************/

@ r0: &ReverbData
@ r1:  DecayTimeSecs

ASM_FUNC_GLOBAL(SGE_FakeReverb_SetDecayTime)
ASM_FUNC_BEG   (SGE_FakeReverb_SetDecayTime, ASM_FUNCSECT_TEXT;ASM_MODE_ARM)

@ DecayCoef = 2^(16 + Log2[-60dB] * SecsPerUpdate / DecayTimeSecs)
@           = 2^(16 + Log2[-60dB] * BufLen / (RateHz*DecayTimeSecs))
SGE_FakeReverb_SetDecayTime:
	STMFD	sp!, {r0,lr}
	LDRH	r2, [r0, #0x0C]   @ RateHz -> r2
	LDRH	r0, [r0, #0x0E]   @ BufLen -> r0
	LDR	ip, =2675169849
	UMULL	r2, r3, r1, r2    @ RateHz * DecayTimeSecs -> r2,r3
	UMULL	r0, r1, ip, r0    @ 2^28 * Log2[-60dB]*BufLen -> r0,r1
	ORRS	ip, r2, r3        @ Avoid divide by 0
	MVNEQ	r0, #0x00
	MVNEQ	r1, #0x00
	MOVNE	r1, r1, lsl #0x07 @ Scale numerator to .8fxp to match denominator, and .27fxp output
	ORRNE	r1, r1, r0, lsr #0x20-7
	MOVNE	r0, r0, lsl #0x07
	BLNE	__aeabi_uldivmod
	ADDS	r0, r0, #0x00     @ [C=0]
	RSCS	r0, r0, #0x10<<27 @ DecayRate in .16fxp
	RSCS	r1, r1, #0x00     @ 2^(16-Log2[LogDecay]), but more like 2^(15.9999-x) to avoid needing to clip
	MOVCC	r0, #0x00         @ <- Clip to 0 when Decay is < 1
	BLCS	SGE_Exp2fxp
	LDMFD	sp!, {r2,lr}
	STRH	r0, [r2, #0x16]
	BX	lr

ASM_FUNC_END(SGE_FakeReverb_SetDecayTime)

/************************************************/
#endif
/************************************************/
//! EOF
/************************************************/
