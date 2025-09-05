/************************************************/
ASM_MODE_ARM
/************************************************/

//! Accuracy of the interpolation when using SGE_FAST_INTERPOLATE.
//! This is a trade-off between sample precision, and interpolation precision.
//!  0 = Maximum sample accuracy (16-bit samples, 0-bit interpolation)
//!  8 = Maximum interpolation accuracy (8-bit samples, 8-bit interpolation)
//! For obvious reasons, this should always be greater than 0.
#define FAST_INTERPOLATE_BITS 5

/************************************************/

@ Input:
@  r0:  N
@  r1:
@  r2:
@  r3:
@  r4: &Src
@  r5:
@  r6:  VolL | VolR<<16
@  r7:  Position [16.16fxp]
@  r8: &Wav
@  r9: &Dst
@  sl:  Rate [16.16fxp]
@  fp:
@  ip:
@  lr:

/************************************************/
//! Initialization
/************************************************/

.LMixerCore_PCM8_Stereo_RateLT_Lerp_InitBeg:
.LMixerCore_PCM8_Stereo_RateGT_Lerp_InitBeg:
	AND	r2, r6, #0xFF              @ VolL -> r2
	MOV	r3, r6, lsr #0x10          @ VolR -> r3
	MVN	r5, #0x01                  @ AccessMask -> r5

.LMixerCore_PCM8_Stereo_RateLT_Lerp_InitEnd:
.LMixerCore_PCM8_Stereo_RateGT_Lerp_InitEnd:

/************************************************/
//! Rate < 1.0
/************************************************/

@ r0:  nRem
@ r1:  SmpL |  SmpR<<16
@ r2:  VolL
@ r3:  VolR
@ r4: &Src
@ r5: [Temp]
@ r6:
@ r7:  Position<<16 [0.16fxp]
@ r8:  StepL | StepR<<16
@ r9: &Dst
@ sl:  Rate [0.16fxp]
@ fp: [Temp]
@ ip: [Output]
@ lr: [Output]
@ NOTE: Despite what intuition says, it is generally faster to
@ inline conditionals than to branch out on a new sample, and
@ it only becomes better otherwise when Rate < 0.5 or so.

#define RELOC_TARGET(Target) ( \
	(Target - .LMixerCore_WorkArea + .LMixerCore_PCM8_Stereo_RateLT_Lerp_Beg) - \
	(.LMixerCore_PCM8_Stereo_RateLT_Lerp_InitEnd - .LMixerCore_PCM8_Stereo_RateLT_Lerp_InitBeg) \
)

.macro PCM8_Stereo_RateLT_Lerp_FetchAndMix Rd
#if !SGE_FAST_INTERPOLATE
	MOVS	fp, r7, lsr #(32-8)             @ SubPos -> fp
	MULNE	fp, r8, fp                      @ (SmpL|SmpR + (StepL|StepR)*SubPos) << 8 (plus interpolation precision)
	ADD	fp, fp, r1, lsl #0x08
	MOV	r5, fp, lsr #0x10               @ R*VolR -> r5
	BIC	fp, fp, r5, lsl #0x10           @ L*VolL -> fp
	MUL	r5, r5, r3
	MUL	fp, fp, r2
	ADD	\Rd, \Rd, fp, lsr #MIX_SHIFT_DOWN+8
	MOV	r5, r5, lsr #MIX_SHIFT_DOWN+8
	ADD	\Rd, \Rd, r5, lsl #0x10
#else
	MOVS	fp, r7, lsr #(32-FAST_INTERPOLATE_BITS)
	MULNE	fp, r8, fp
	ADD	fp, fp, r1, lsl #FAST_INTERPOLATE_BITS
# if MIX_CLEAR_MASK
	BIC	fp, fp, #MIX_CLEAR_MASK<<16
# endif
	ADD	\Rd, \Rd, fp, lsr #MIX_SHIFT_DOWN
#endif
	ADDS	r7, r7, sl, lsl #0x10           @ Position += Rate?
	LDRCSH	fp, [r4, #0x02]!                @ Advance to next samples
	ADDCS	r1, r1, r8
#if SGE_FAST_INTERPOLATE
	ANDCS	r8, fp, #0xFF
	MULCS	r5, fp, r3
	MULCS	fp, r8, r2
	MOVCS	r5, r5, lsr #(8+FAST_INTERPOLATE_BITS)
	RSBCS	r8, r1, fp, lsr #FAST_INTERPOLATE_BITS
	ADDCS	r8, r8, r5, lsl #0x10
#else
	ORRCS	r8, fp, fp, lsl #0x08
	BICCS	r8, r8, #0xFF00
	SUBCS	r8, r8, r1
#endif
.endm

.LMixerCore_PCM8_Stereo_RateLT_Lerp_Beg:
	AND	ip, r5, r7, lsr #0x10-1
	LDRH	r1, [r4, ip]!                   @ Ra -> r1, and set Src pointer
	MOV	r7, r7, lsl #(32-16)            @ Shift up Position
#if (SGE_PCM8_STEREO_UNROLL > 1)
	SUBS	r0, r0, #SGE_PCM8_STEREO_UNROLL @ Have enough samples for a block iteration?
#endif
#if SGE_FAST_INTERPOLATE
	AND	r8, r1, #0xFF                   @ Scale Ra = RaL*VolL|RaR*VolR
	MUL	ip, r8, r2
	MUL	lr, r1, r3
	LDRH	fp, [r4, #0x02]!                @ Rb -> r8, and pre-advance Src
	MOV	ip, ip, lsr #FAST_INTERPOLATE_BITS
	MOV	lr, lr, lsr #(8+FAST_INTERPOLATE_BITS)
	ORR	r1, ip, lr, lsl #0x10
	AND	r8, fp, #0xFF                   @ Scale Rb likewise
	MUL	ip, r8, r2
	MUL	lr, fp, r3
	RSB	ip, r1, ip, lsr #FAST_INTERPOLATE_BITS @ Get StepL | StepR<<16
	MOV	lr, lr, lsr #(8+FAST_INTERPOLATE_BITS)
	ADD	r8, ip, lr, lsl #0x10
#else
	LDRH	r8, [r4, #0x02]!                @ Rb -> r8, and pre-advance Src
	ORR	r1, r1, r1, lsl #0x08           @ Re-combine into SmpL | SmpR<<16
	BIC	r1, r1, #0xFF00
	ORR	r8, r8, r8, lsl #0x08
	BIC	r8, r8, #0xFF00
	SUB	r8, r8, r1                      @ Get StepL | StepR<<16
#endif
#if (SGE_PCM8_STEREO_UNROLL > 1)
	BCC	2f
1:
.rept (SGE_PCM8_STEREO_UNROLL/2)
	LDMIA	r9, {ip,lr}                     @ Wet -> ip,lr
	PCM8_Stereo_RateLT_Lerp_FetchAndMix ip
	PCM8_Stereo_RateLT_Lerp_FetchAndMix lr
	STMIA	r9!, {ip,lr}
.endr
	SUBS	r0, r0, #SGE_PCM8_STEREO_UNROLL
	BCS	1b
2:	ADDS	r0, r0, #SGE_PCM8_STEREO_UNROLL @ Restore nRem. Have at least 1 sample?
	BEQ	3f
#endif
20:	LDR	ip, [r9]                        @ Wet -> ip
	PCM8_Stereo_RateLT_Lerp_FetchAndMix ip  @ Mix into Wet
	STR	ip, [r9], #0x04
#if (SGE_PCM8_STEREO_UNROLL != 2) //! 2 samples/iter would have 1 sample remainder at most
	SUBS	r0, r0, #0x01                   @ --nRem?
	BNE	20b
#endif
3:	MOV	r7, r7, lsr #(32-16)            @ Restore Position
	SUB	r4, r4, #0x02                   @ Rewind pre-advance
	B	RELOC_TARGET(.LMixer_VoxLoop_MixLoop_MixChunk_Tail)

.LMixerCore_PCM8_Stereo_RateLT_Lerp_End:

.equ SGE_Driver_MixerCore_PCM8_Stereo_RateLT_Lerp_Size, \
	(.LMixerCore_PCM8_Stereo_RateLT_Lerp_InitEnd - .LMixerCore_PCM8_Stereo_RateLT_Lerp_InitBeg) + \
	(.LMixerCore_PCM8_Stereo_RateLT_Lerp_End     - .LMixerCore_PCM8_Stereo_RateLT_Lerp_Beg)
.if SGE_Driver_MixerCore_PCM8_Stereo_RateLT_Lerp_Size > SGE_Driver_MixerCore_WorkAreaSize
	.equ SGE_Driver_MixerCore_WorkAreaSize, SGE_Driver_MixerCore_PCM8_Stereo_RateLT_Lerp_Size
.endif

#undef RELOC_TARGET

/************************************************/
//! Rate >= 1.0
/************************************************/

@ r0:  SampMask(=FFh)
@ r1: [OutputTemp]
@ r2:  VolL
@ r3:  VolR
@ r4: &Src
@ r5:  AccessMask(=1FFFEh) | (-nRem-1)<<17
@ r6:  ClearMask | ClearMask<<16 -> r6
@ r7:  Position [16.16fxp]
@ r8: [OutputTemp]
@ r9: &Dst
@ sl:  Rate [16.16fxp]
@ fp:  00FF00FFh
@ ip: [Output]
@ lr: [Output]

#define RELOC_TARGET(Target) ( \
	(Target - .LMixerCore_WorkArea + .LMixerCore_PCM8_Stereo_RateGT_Lerp_Beg) - \
	(.LMixerCore_PCM8_Stereo_RateGT_Lerp_InitEnd - .LMixerCore_PCM8_Stereo_RateGT_Lerp_InitBeg) \
)

.macro PCM8_Stereo_RateGT_Lerp_Fetch Rd
	AND	\Rd, r5, r7, lsr #0x10-1        @ Get scaled Position offset
	LDRH	ip, [\Rd, r4]!                  @ Ra -> ip
	LDRH	lr, [\Rd, #0x02]                @ Rb -> lr
	ANDS	\Rd, r0, r7, lsr #0x08          @ SubPos -> Rd
	ORR	ip, ip, ip, lsl #0x08           @ RaL | RaR<<16 -> ip
	BIC	ip, ip, #0xFF00
	ORR	lr, lr, lr, lsl #0x08           @ RbL | RbR<<16 -> lr
	BIC	lr, lr, #0xFF00
	SUB	lr, lr, ip                      @ Rb - Ra
	MULNE	\Rd, lr, \Rd                    @ Interpolate to L<<8 | R<<24 (with garbage in other bits)
	ADD	r7, r7, sl                      @ Position += Rate
	ADD	\Rd, \Rd, ip, lsl #0x08
.endm

.LMixerCore_PCM8_Stereo_RateGT_Lerp_Beg:
	MOV	fp, #0xFF                       @ 00FF00FFh -> fp
	ORR	fp, fp, fp, lsl #0x10
#if MIX_CLEAR_MASK
	MOV	r6, #MIX_CLEAR_MASK             @ ClearMask | ClearMask<<16 -> r6
	ORR	r6, r6, r6, lsl #0x10
#endif
#if (SGE_PCM8_STEREO_UNROLL > 1)
	SUBS	r0, r0, #SGE_PCM8_STEREO_UNROLL @ Have enough samples for a block iteration?
#endif
	SUB	r5, r5, r0, lsl #0x11           @ AccessMask | (-nRem-1)<<17 -> r5
	MOV	r0, #0xFF                       @ SampMask -> r0
#if (SGE_PCM8_STEREO_UNROLL > 1)
	BCC	2f
1:
.rept (SGE_PCM8_STEREO_UNROLL/2)
	PCM8_Stereo_RateGT_Lerp_Fetch r1        @ Dry -> r1,r8
	PCM8_Stereo_RateGT_Lerp_Fetch r8
	AND	r1, fp, r1, lsr #0x08           @ Mask samples to L|R<<16
	AND	r8, fp, r8, lsr #0x08
	EOR	r1, r1, r8, lsl #0x10           @ Swap to L1|L2<<16 -> r1, R1|R2<<16 -> r8
	EOR	r8, r8, r1, lsr #0x10
	EOR	r1, r1, r8, lsl #0x10
	MUL	lr, r8, r3                      @ R *= VolR -> lr
	MUL	ip, r1, r2                      @ L *= VolL -> ip
#if MIX_CLEAR_MASK
	BIC	lr, lr, r6
#endif
	EOR	r1, ip, lr, lsl #0x10           @ Swap back to L1|R1<<16 -> r1, L2|R2<<16 -> r8
	EOR	r8, lr, r1, lsr #0x10
	LDMIA	r9, {ip,lr}                     @ Wet -> ip,lr
	EOR	r1, r1, r8, lsl #0x10
	ADD	ip, ip, r1, lsr #MIX_SHIFT_DOWN
	ADD	lr, lr, r8, lsr #MIX_SHIFT_DOWN
	STMIA	r9!, {ip,lr}
.endr
	ADDS	r5, r5, #SGE_PCM8_STEREO_UNROLL<<17
	BLT	1b
2:	SUBS	r5, r5, #(SGE_PCM8_STEREO_UNROLL-1)<<17 @ Restore nRem. Have at least 1 sample?
# if MIX_CLEAR_MASK
	BGE	3f
# else
	BGE	RELOC_TARGET(.LMixer_VoxLoop_MixLoop_MixChunk_Tail)
# endif
#endif
20:	PCM8_Stereo_RateGT_Lerp_Fetch r1        @ Dry -> r1
	AND	r8, r0, r1, lsr #0x18           @ R -> r8
	AND	r1, r0, r1, lsr #0x08           @ L -> r1
	MUL	ip, r1, r2                      @ L *= VolL -> ip
	MUL	lr, r8, r3                      @ R *= VolR -> lr
	LDR	r1, [r9]                        @ Wet -> ip
	ORR	ip, ip, lr, lsl #0x10           @ L | R<<16
#if MIX_CLEAR_MASK
	BIC	ip, ip, #MIX_CLEAR_MASK<<16
#endif
	ADD	r1, r1, ip, lsr #MIX_SHIFT_DOWN
	STR	r1, [r9], #0x04
#if (SGE_PCM8_STEREO_UNROLL != 2) //! 2 samples/iter would have 1 sample remainder at most
	ADDS	r5, r5, #0x01<<17               @ --nRem?
	BCC	20b
#endif
3:
#if MIX_CLEAR_MASK
	ORR	r6, r2, r3, lsl #0x10           @ Restore VolL|VolR<<16 -> r6
#endif
	B	RELOC_TARGET(.LMixer_VoxLoop_MixLoop_MixChunk_Tail)

.LMixerCore_PCM8_Stereo_RateGT_Lerp_End:

.equ SGE_Driver_MixerCore_PCM8_Stereo_RateGT_Lerp_Size, \
	(.LMixerCore_PCM8_Stereo_RateGT_Lerp_InitEnd - .LMixerCore_PCM8_Stereo_RateGT_Lerp_InitBeg) + \
	(.LMixerCore_PCM8_Stereo_RateGT_Lerp_End     - .LMixerCore_PCM8_Stereo_RateGT_Lerp_Beg)
.if SGE_Driver_MixerCore_PCM8_Stereo_RateGT_Lerp_Size > SGE_Driver_MixerCore_WorkAreaSize
	.equ SGE_Driver_MixerCore_WorkAreaSize, SGE_Driver_MixerCore_PCM8_Stereo_RateGT_Lerp_Size
.endif

#undef RELOC_TARGET

/************************************************/
//! EOF
/************************************************/
