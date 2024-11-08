/************************************************/
ASM_MODE_ARM
/************************************************/
//! NOTE: We use the same mixing loop regardless of Rate < 1.0 or Rate >= 1.0,
//! as this is generally faster than a specialized loop for Rate < 1.0.
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

.LMixerCore_PCM8_Mono_RateLT_NoLerp_InitBeg:
.LMixerCore_PCM8_Mono_RateLT_NoLerp_InitEnd:
.LMixerCore_PCM8_Mono_RateGT_NoLerp_InitBeg:
.LMixerCore_PCM8_Mono_RateGT_NoLerp_InitEnd:

/************************************************/

@ r0:  nRem
@ r1:  Smp0
@ r2:  Smp1
@ r3:  Smp2
@ r4: &Src
@ r5:  Smp3
@ r6:  VolL | VolR<<16
@ r7:  Position [16.16fxp]
@ r8: [Output]
@ r9: &Dst
@ sl:  Rate [16.16fxp]
@ fp: [Output]
@ ip: [Output]
@ lr: [Output]

#define RELOC_TARGET(Target) ( \
	(Target - .LMixerCore_WorkArea + .LMixerCore_PCM8_Mono_RateLT_NoLerp_Beg) - \
	(.LMixerCore_PCM8_Mono_RateLT_NoLerp_InitEnd - .LMixerCore_PCM8_Mono_RateLT_NoLerp_InitBeg) \
)

.macro PCM8_Mono_NoLerp_Fetch Rd
	LDRB	\Rd, [r4, r7, lsr #0x10]          @ Ra -> Rd
	ADD	r7, r7, sl                        @ Position += Rate
.endm
.macro PCM8_Mono_NoLerp_Mix Rd, Rs
	BIC	\Rs, \Rs, #MIX_CLEAR_MASK<<16
	ADD	\Rd, \Rd, \Rs, lsr #MIX_SHIFT_DOWN
.endm

.LMixerCore_PCM8_Mono_RateLT_NoLerp_Beg:
.LMixerCore_PCM8_Mono_RateGT_NoLerp_Beg:
#if (SGE_PCM8_MONO_UNROLL > 1)
	SUBS	r0, r0, #SGE_PCM8_MONO_UNROLL     @ Have enough samples for a block iteration?
	BCC	2f
1:
.rept (SGE_PCM8_MONO_UNROLL/4)
	PCM8_Mono_NoLerp_Fetch r1                 @ Dry -> r1,r2,r3,r5
	PCM8_Mono_NoLerp_Fetch r2
	PCM8_Mono_NoLerp_Fetch r3
	PCM8_Mono_NoLerp_Fetch r5
	MUL	r1, r6, r1
	MUL	r2, r6, r2
	MUL	r3, r6, r3
	MUL	r5, r6, r5
	LDMIA	r9, {r8,fp,ip,lr}                 @ Wet -> r8,fp,ip,lr
	PCM8_Mono_NoLerp_Mix r8, r1               @ Mix into Wet
	PCM8_Mono_NoLerp_Mix fp, r2
	PCM8_Mono_NoLerp_Mix ip, r3
	PCM8_Mono_NoLerp_Mix lr, r5
	STMIA	r9!, {r8,fp,ip,lr}
.endr
	SUBS	r0, r0, #SGE_PCM8_MONO_UNROLL
	BCS	1b
2:	ADDS	r0, r0, #SGE_PCM8_MONO_UNROLL     @ Restore nRem. Have at least 1 sample?
	BEQ	RELOC_TARGET(.LMixer_VoxLoop_MixLoop_MixChunk_Tail)
#endif
20:	PCM8_Mono_NoLerp_Fetch r1                 @ Dry*Vol -> r1
	MUL	r1, r6, r1
	LDR	r8, [r9]                          @ Wet -> r8
	PCM8_Mono_NoLerp_Mix r8, r1               @ Mix into Wet
	STR	r8, [r9], #0x04
#if (SGE_PCM8_MONO_UNROLL != 2) //! 2 samples/iter would have 1 sample remainder at most
	SUBS	r0, r0, #0x01                     @ --nRem?
	BNE	20b
#endif
3:	B	RELOC_TARGET(.LMixer_VoxLoop_MixLoop_MixChunk_Tail)

.LMixerCore_PCM8_Mono_RateLT_NoLerp_End:
.LMixerCore_PCM8_Mono_RateGT_NoLerp_End:

.equ SGE_Driver_MixerCore_PCM8_Mono_RateLT_Size, \
	(.LMixerCore_PCM8_Mono_RateLT_NoLerp_InitEnd - .LMixerCore_PCM8_Mono_RateLT_NoLerp_InitBeg) + \
	(.LMixerCore_PCM8_Mono_RateLT_NoLerp_End     - .LMixerCore_PCM8_Mono_RateLT_NoLerp_Beg)
.equ SGE_Driver_MixerCore_PCM8_Mono_RateGT_Size, SGE_Driver_MixerCore_PCM8_Mono_RateLT_Size
.if SGE_Driver_MixerCore_PCM8_Mono_RateLT_Size > SGE_Driver_MixerCore_WorkAreaSize
	.equ SGE_Driver_MixerCore_WorkAreaSize, SGE_Driver_MixerCore_PCM8_Mono_RateLT_Size
.endif

#undef RELOC_TARGET

/************************************************/
//! EOF
/************************************************/
