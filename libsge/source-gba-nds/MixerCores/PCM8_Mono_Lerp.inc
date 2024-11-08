/************************************************/
ASM_MODE_ARM
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

.LMixerCore_PCM8_Mono_RateLT_Lerp_InitBeg:
.LMixerCore_PCM8_Mono_RateLT_Lerp_InitEnd:
.LMixerCore_PCM8_Mono_RateGT_Lerp_InitBeg:
.LMixerCore_PCM8_Mono_RateGT_Lerp_InitEnd:

/************************************************/
//! Rate < 1.0
/************************************************/

@ r0:  nRem
@ r1:  Ra
@ r2:  Rb - Ra
@ r3: [Temp]
@ r4: &Src
@ r5:
@ r6:  VolL | VolR<<16
@ r7:  Position<<16 [0.16fxp]
@ r8: [Output]
@ r9: &Dst
@ sl:  Rate [0.16fxp]
@ fp: [Output]
@ ip: [Output]
@ lr: [Output]

#define RELOC_TARGET(Target) ( \
	(Target - .LMixerCore_WorkArea + .LMixerCore_PCM8_Mono_RateLT_Lerp_Beg) - \
	(.LMixerCore_PCM8_Mono_RateLT_Lerp_InitEnd - .LMixerCore_PCM8_Mono_RateLT_Lerp_InitBeg) \
)

.macro PCM8_Mono_RateLT_Lerp_FetchAndMix Rd
	MOVS	r3, r7, lsr #(32-8)           @ Smp = Ra + (Rb-Ra)*SubPos -> r3?
	MULNE	r3, r2, r3
	ADDS	r3, r1, r3, asr #0x08
	MULNE	r3, r6, r3                    @  Wet += Dry*Vol
	ADDS	r7, r7, sl, lsl #0x10         @ Position += Rate?
	BIC	r3, r3, #MIX_CLEAR_MASK<<16
	ADD	\Rd, \Rd, r3, lsr #MIX_SHIFT_DOWN
	LDRCSB	r3, [r4, #0x01]!              @  New Rb -> r3
	ADDCS	r1, r1, r2                    @  Ra += (Rb-Ra) -> Ra = Rb -> r1
	SUBCS	r2, r3, r1                    @  Rb-Ra -> r2
.endm

.LMixerCore_PCM8_Mono_RateLT_Lerp_Beg:
	LDRB	r1, [r4, r7, lsr #0x10]!      @ Ra -> r1, and set Src pointer
	LDRB	r2, [r4, #0x01]!              @ Rb -> r2, and pre-advance Src
	MOV	r7, r7, lsl #0x10             @ Shift up Position
#if (SGE_PCM8_MONO_UNROLL > 1)
	SUBS	r0, r0, #SGE_PCM8_MONO_UNROLL @ Have enough samples for a block iteration?
#endif
	SUB	r2, r2, r1                    @ Rb - Ra -> r2
#if (SGE_PCM8_MONO_UNROLL > 1)
	BCC	2f
1:
.rept (SGE_PCM8_MONO_UNROLL/4)
	LDMIA	r9, {r8,fp,ip,lr}             @ Wet -> r8,fp,ip,lr
	PCM8_Mono_RateLT_Lerp_FetchAndMix r8  @ Mix into Wet
	PCM8_Mono_RateLT_Lerp_FetchAndMix fp
	PCM8_Mono_RateLT_Lerp_FetchAndMix ip
	PCM8_Mono_RateLT_Lerp_FetchAndMix lr
	STMIA	r9!, {r8,fp,ip,lr}
.endr
	SUBS	r0, r0, #SGE_PCM8_MONO_UNROLL
	BCS	1b
2:	ADDS	r0, r0, #SGE_PCM8_MONO_UNROLL @ Restore nRem. Have at least 1 sample?
	BEQ	3f
#endif
20:	LDR	r8, [r9]                      @ Wet -> r8
	PCM8_Mono_RateLT_Lerp_FetchAndMix r8  @ Mix into Wet
	STR	r8, [r9], #0x04
	SUBS	r0, r0, #0x01
	BNE	20b
3:	MOV	r7, r7, lsr #0x10             @ Restore Position
	SUB	r4, r4, #0x01                 @ Rewind pre-advance
	B	RELOC_TARGET(.LMixer_VoxLoop_MixLoop_MixChunk_Tail)

.LMixerCore_PCM8_Mono_RateLT_Lerp_End:

.equ SGE_Driver_MixerCore_PCM8_Mono_RateLT_Lerp_Size, \
	(.LMixerCore_PCM8_Mono_RateLT_Lerp_InitEnd - .LMixerCore_PCM8_Mono_RateLT_Lerp_InitBeg) + \
	(.LMixerCore_PCM8_Mono_RateLT_Lerp_End     - .LMixerCore_PCM8_Mono_RateLT_Lerp_Beg)
.if SGE_Driver_MixerCore_PCM8_Mono_RateLT_Lerp_Size > SGE_Driver_MixerCore_WorkAreaSize
	.equ SGE_Driver_MixerCore_WorkAreaSize, SGE_Driver_MixerCore_PCM8_Mono_RateLT_Lerp_Size
.endif

#undef RELOC_TARGET

/************************************************/
//! Rate >= 1.0
/************************************************/

@ r0:  nRem
@ r1:  Smp0
@ r2:  Smp1
@ r3:  FracPosMask(=FFh)
@ r4: &Src
@ r5:  &Src+1
@ r6:  VolL | VolR<<16
@ r7:  Position [16.16fxp]
@ r8: [Output]
@ r9: &Dst
@ sl:  Rate [16.16fxp]
@ fp: [Output]
@ ip: [Temp]
@ lr: [Temp]

#define RELOC_TARGET(Target) ( \
	(Target - .LMixerCore_WorkArea + .LMixerCore_PCM8_Mono_RateGT_Lerp_Beg) - \
	(.LMixerCore_PCM8_Mono_RateGT_Lerp_InitEnd - .LMixerCore_PCM8_Mono_RateGT_Lerp_InitBeg) \
)

.macro PCM8_Mono_RateGT_Lerp_Fetch Rd
	LDRB	\Rd, [r4, r7, lsr #0x10]          @ Ra -> Rd
	LDRB	ip, [r5, r7, lsr #0x10]           @ Rb -> ip
	ANDS	lr, r3, r7, lsr #0x08             @ Smp = Ra + (Rb-Ra)*SubPos -> Rd?
	SUBNE	ip, ip, \Rd
	MULNE	lr, ip, lr
	ADD	r7, r7, sl                        @ Position += Rate
	ADDS	\Rd, \Rd, lr, asr #0x08
	MULNE	\Rd, r6, \Rd                      @ Smp *= Vol
.endm
.macro PCM8_Mono_RateGT_Lerp_Mix Rd, Rs
	BIC	\Rs, \Rs, #MIX_CLEAR_MASK<<16
	ADD	\Rd, \Rd, \Rs, lsr #MIX_SHIFT_DOWN
.endm

.LMixerCore_PCM8_Mono_RateGT_Lerp_Beg:
	MOV	r3, #0xFF                         @ FracPosMask -> r3
	ADD	r5, r4, #0x01                     @ Src+1 -> r5
#if (SGE_PCM8_MONO_UNROLL > 1)
	SUBS	r0, r0, #SGE_PCM8_MONO_UNROLL     @ Have enough samples for a block iteration?
	BCC	2f
1:
.rept (SGE_PCM8_MONO_UNROLL/2)
	PCM8_Mono_RateGT_Lerp_Fetch r1            @ Dry*Vol -> r1,r2
	PCM8_Mono_RateGT_Lerp_Fetch r2
	LDMIA	r9, {r8,fp}                       @ Wet -> r8,fp
	PCM8_Mono_RateGT_Lerp_Mix r8, r1          @ Mix into Wet
	PCM8_Mono_RateGT_Lerp_Mix fp, r2
	STMIA	r9!, {r8,fp}
.endr
	SUBS	r0, r0, #SGE_PCM8_MONO_UNROLL
	BCS	1b
2:	ADDS	r0, r0, #SGE_PCM8_MONO_UNROLL     @ Restore nRem. Have at least 1 sample?
	BEQ	RELOC_TARGET(.LMixer_VoxLoop_MixLoop_MixChunk_Tail)
#endif
20:	PCM8_Mono_RateGT_Lerp_Fetch r1            @ Dry*Vol -> r1
	LDR	r8, [r9]                          @ Wet -> r8
	PCM8_Mono_RateGT_Lerp_Mix r8, r1          @ Mix into Wet
	STR	r8, [r9], #0x04
#if (SGE_PCM8_MONO_UNROLL != 2) //! 2 samples/iter would have 1 sample remainder at most
	SUBS	r0, r0, #0x01                     @ --nRem?
	BNE	20b
#endif
3:
	B	RELOC_TARGET(.LMixer_VoxLoop_MixLoop_MixChunk_Tail)

.LMixerCore_PCM8_Mono_RateGT_Lerp_End:

.equ SGE_Driver_MixerCore_PCM8_Mono_RateGT_Lerp_Size, \
	(.LMixerCore_PCM8_Mono_RateGT_Lerp_InitEnd - .LMixerCore_PCM8_Mono_RateGT_Lerp_InitBeg) + \
	(.LMixerCore_PCM8_Mono_RateGT_Lerp_End     - .LMixerCore_PCM8_Mono_RateGT_Lerp_Beg)
.if SGE_Driver_MixerCore_PCM8_Mono_RateGT_Lerp_Size > SGE_Driver_MixerCore_WorkAreaSize
	.equ SGE_Driver_MixerCore_WorkAreaSize, SGE_Driver_MixerCore_PCM8_Mono_RateGT_Lerp_Size
.endif

#undef RELOC_TARGET

/************************************************/
//! EOF
/************************************************/
