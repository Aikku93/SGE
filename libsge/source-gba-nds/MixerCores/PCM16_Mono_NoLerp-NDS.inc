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
@  r7:  Position [17.15fxp]
@  r8: &Wav
@  r9: &Dst
@  sl:  Rate [17.15fxp]
@  fp:
@  ip:
@  lr:

/************************************************/
//! Rate < 1.0
/************************************************/

@ r0:  nRem
@ r1:  ARM7: Ra*VolL / ARM9: Ra
@ r2:  ARM7: Ra*VolR
@ r3:  ARM7: VolR
@ r4: &Src
@ r5:  ARM7: VolL
@ r6:  ARM9: VolL | VolR<<16
@ r7:  Position<<17 [0.15fxp]
@ r8: [Output]
@ r9: &Dst
@ sl:  Rate [0.15fxp]
@ fp: [Output]
@ ip: [Output]
@ lr: [Output]

.macro PCM16_Mono_RateLT_NoLerp_FetchAndMix RdL, RdR
#if (__NDS__ == 7)
	ADD	\RdL, \RdL, r1                 @ Wet += Dry*Vol
	ADD	\RdR, \RdR, r2
#else
	SMLABB	\RdL, r1, r6, \RdL
	SMLABT	\RdR, r1, r6, \RdR
#endif
	ADDS	r7, r7, sl, lsl #(32-15)       @ Position += Rate?
	LDRCSSH	r1, [r4, #0x02]!               @  Fetch next sample
#if (__NDS__ == 7)
	MULCS	r2, r1, r3                     @ Pre-multiply Dry*Vol
# if AVOID_MUL_WARNINGS
	MULCS	r1, r5, r1                     @ <- More likely to take 3c than 2c
# else
	MULCS	r1, r1, r5
# endif
#endif
.endm

.LMixerCore_PCM16_Mono_RateLT_NoLerp:
#if (__NDS__ == 7)
	MOV	r3, r6, lsr #0x10              @ VolR -> r3
	BIC	r5, r6, r3, lsl #0x10          @ VolL -> r5
#endif
	MOV	r1, r7, lsr #0x0F              @ Ra -> r1, and set Src pointer
	MOV	r1, r1, lsl #0x01
	LDRSH	r1, [r4, r1]!
	MOV	r7, r7, lsl #(32-15)           @ Shift up Position
#if (SGE_PCM16_MONO_UNROLL > 1)
	SUBS	r0, r0, #SGE_PCM16_MONO_UNROLL @ Have enough samples for a block iteration?
#endif
#if (__NDS__ == 7)
	MUL	r2, r1, r3                     @ Pre-multiply Dry*Vol
# if AVOID_MUL_WARNINGS
	MUL	r1, r5, r1                     @ <- More likely to take 3c than 2c
# else
	MUL	r1, r1, r5
# endif
#endif
#if (SGE_PCM16_MONO_UNROLL > 1)
	BCC	2f
1:
.rept (SGE_PCM16_MONO_UNROLL/2)
	LDMIA	r9, {r8,fp,ip,lr}              @ Wet -> r8,fp,ip,lr
	PCM16_Mono_RateLT_NoLerp_FetchAndMix r8,fp @ Mix into Wet
	PCM16_Mono_RateLT_NoLerp_FetchAndMix ip,lr
	STMIA	r9!, {r8,fp,ip,lr}
.endr
	SUBS	r0, r0, #SGE_PCM16_MONO_UNROLL
	BCS	1b
2:	ADDS	r0, r0, #SGE_PCM16_MONO_UNROLL @ Restore nRem. Have at least 1 sample?
	BEQ	3f
#endif
20:	LDMIA	r9, {r8,fp}                    @ Wet -> r8
	PCM16_Mono_RateLT_NoLerp_FetchAndMix r8,fp @ Mix into Wet
	STMIA	r9!, {r8,fp}
#if (SGE_PCM16_MONO_UNROLL != 2) //! 2 samples/iter would have 1 sample remainder at most
	SUBS	r0, r0, #0x01
	BNE	20b
#endif
3:	MOV	r7, r7, lsr #(32-15)           @ Restore Position
	B	.LMixer_VoxLoop_MixLoop_MixChunk_Tail

/************************************************/
//! Rate >= 1.0
/************************************************/

@ r0:  nRem
@ r1: [Output]
@ r2: [Output]
@ r3:  ARM7: VolR
@ r4: &Src
@ r5:  AccessMask(=3FFFEh)
@ r6:  ARM7: VolL / ARM9: VolL | VolR<<16
@ r7:  Position [17.15fxp]
@ r8: [Input]
@ r9: &Dst
@ sl:  Rate [17.15fxp]
@ fp: [Input]
@ ip: [Output/Temp]
@ lr: [Output/Temp]

.macro PCM16_Mono_RateGT_NoLerp_Fetch Rd
	AND	\Rd, r5, r7, lsr #0x0F-1       @ Ra -> Rd
	LDRSH	\Rd, [r4, \Rd]
	ADD	r7, r7, sl                     @ Position += Rate
.endm

.LMixerCore_PCM16_Mono_RateGT_NoLerp:
#if (__NDS__ == 7)
	MOV	r3, r6, lsr #0x10              @ VolR -> r3
	BIC	r6, r6, r3, lsl #0x10          @ VolL -> r6
#endif
	MVN	r5, #0x01                      @ AccessMask -> r5
	@AND	r5, r5, r5, lsr #(32-17-1)     @ <- Not actually needed
#if (SGE_PCM16_MONO_UNROLL > 1)
	SUBS	r0, r0, #SGE_PCM16_MONO_UNROLL @ Have enough samples for a block iteration?
	BCC	2f
1:
.rept (SGE_PCM16_MONO_UNROLL/2)
	PCM16_Mono_RateGT_NoLerp_Fetch r8      @ Dry -> r8,fp
	PCM16_Mono_RateGT_NoLerp_Fetch fp
	LDMIA	r9, {r1,r2,ip,lr}              @ Wet -> r1,r2,ip,lr
# if (__NDS__ == 7)
	MLA	r1, r8, r6, r1                 @ Mix into Wet
	MLA	r2, r8, r3, r2
	MLA	ip, fp, r6, ip
	MLA	lr, fp, r3, lr
# else
	SMLABB	r1, r8, r6, r1
	SMLABT	r2, r8, r6, r2
	SMLABB	ip, fp, r6, ip
	SMLABT	lr, fp, r6, lr
# endif
	STMIA	r9!, {r1,r2,ip,lr}
.endr
	SUBS	r0, r0, #SGE_PCM16_MONO_UNROLL
	BCS	1b
2:	ADDS	r0, r0, #SGE_PCM16_MONO_UNROLL @ Restore nRem. Have at least 1 sample?
# if (__NDS__ == 7)
	BEQ	3f
# else
	BEQ	.LMixer_VoxLoop_MixLoop_MixChunk_Tail
# endif
#endif
20:	PCM16_Mono_RateGT_NoLerp_Fetch r8      @ Dry -> r8
	LDMIA	r9, {ip,lr}                    @ Wet -> ip,lr
# if (__NDS__ == 7)
	MLA	ip, r8, r6, ip                 @ Mix into Wet
	MLA	lr, r8, r3, lr
# else
	SMLABB	ip, r8, r6, ip
	SMLABT	lr, r8, r6, lr
# endif
	STMIA	r9!, {ip,lr}
#if (SGE_PCM16_MONO_UNROLL != 2) //! 2 samples/iter would have 1 sample remainder at most
	SUBS	r0, r0, #0x01                  @ --nRem?
	BNE	20b
#endif
3:
#if (__NDS__ == 7)
	ORR	r6, r6, r3, lsl #0x10          @ Restore VolL|VolR
#endif
	B	.LMixer_VoxLoop_MixLoop_MixChunk_Tail

/************************************************/
//! EOF
/************************************************/
