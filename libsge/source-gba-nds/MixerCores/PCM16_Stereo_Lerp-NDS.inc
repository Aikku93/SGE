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
@ r1:  RaL
@ r2: [Temp]
@ r3:  ARM7: VolR
@ r4: &Src
@ r5:  ARM7: (RbL - RaL) / ARM9: (RbL - RaL)*2
@ r6:  ARM7: VolL / ARM9: VolL | VolR<<16
@ r7:  ARM7: Position<<17 [0.15fxp] / ARM9: Position<<16 [0.15fxp]
@ r8:  RaR
@ r9: &Dst
@ sl:  Rate [0.15fxp]
@ fp:  ARM7: (RbR - RaR) / ARM9: (RbR - RaR)*2
@ ip: [Output]
@ lr: [Output]

.macro PCM16_Stereo_RateLT_Lerp_FetchAndMix RdL, RdR
#if (__NDS__ == 7)
	MOVS	r2, r7, lsr #(32-8)              @ Smp = Ra + (Rb-Ra)*SubPos -> r2?
	MULNE	r2, r5, r2
	ADDS	r2, r1, r2, asr #0x08
	MLANE	\RdL, r2, r6, \RdL               @ Wet += Dry*Vol
	MOVS	r2, r7, lsr #(32-8)              @ Repeat for R
	MULNE	r2, fp, r2
	ADDS	r2, r8, r2, asr #0x08
	MLANE	\RdR, r2, r3, \RdR
	ADDS	r7, r7, sl, lsl #(32-15)         @ Position += Rate?
	LDRCSSH	r2, [r4, #0x04]!                 @  New Rb -> r2
	ADDCS	r1, r1, r5                       @  Ra += (Rb-Ra) -> Ra = Rb -> r1
	SUBCS	r5, r2, r1                       @  Rb-Ra -> r5
	LDRCSSH	r2, [r4, #0x02]
	ADDCS	r8, r8, fp
	SUBCS	fp, r2, r8
#else
	SMLAWT	r2, r5, r7, r1                   @ Smp = Ra + (Rb-Ra)*SubPos -> r2
	SMLABB	\RdL, r2, r6, \RdL
	SMLAWT	r2, fp, r7, r8
	SMLABT	\RdR, r2, r6, \RdR
	ADDS	r7, r7, sl, lsl #(31-15)         @ Position += Rate?
	LDRMISH	r2, [r4, #0x04]!                 @  New Rb -> r2
	SUBMI	r7, r7, #0x01<<31                @  Position -= 1.0 (needed for SMLAWx)
	ADDMI	r1, r1, r5, asr #0x01            @  Ra += (Rb-Ra) -> Ra = Rb -> r1
	SUBMI	r5, r2, r1                       @  Rb-Ra -> r5
	LDRMISH	r2, [r4, #0x02]
	MOVMI	r5, r5, lsl #0x01                @  Rb-Ra *= 2 (needed for SMLAWx with 15bit SubPos)
	ADDMI	r8, r8, fp, asr #0x01
	SUBMI	fp, r2, r8
	MOVMI	fp, fp, lsl #0x01
#endif
.endm

.LMixerCore_PCM16_Stereo_RateLT_Lerp:
#if (__NDS__ == 7)
	MOV	r3, r6, lsr #0x10                @ VolR -> r3
	BIC	r6, r6, r3, lsl #0x10            @ VolL -> r6
#endif
	MOV	r1, r7, lsr #0x0F                @ Ra -> r1,r8, and set Src pointer
	MOV	r1, r1, lsl #0x02
	LDRSH	r1, [r4, r1]!
	LDRSH	r8, [r4, #0x02]
	LDRSH	r5, [r4, #0x04]!                 @ Rb -> r5,fp, and pre-advance Src
	LDRSH	fp, [r4, #0x02]
#if (__NDS__ == 9)
	MOV	r7, r7, lsl #(31-15)             @ Shift up Position
#else
	MOV	r7, r7, lsl #(32-15)
#endif
#if (SGE_PCM16_STEREO_UNROLL > 1)
	SUBS	r0, r0, #SGE_PCM16_STEREO_UNROLL @ Have enough samples for a block iteration?
#endif
	SUB	r5, r5, r1                       @ Rb - Ra -> r5,fp
	SUB	fp, fp, r8
# if (__NDS__ == 9)
	MOV	r5, r5, lsl #0x01                @ Rb-Ra *= 2 (needed for SMLAWx with 15bit SubPos)
	MOV	fp, fp, lsl #0x01
# endif
#if (SGE_PCM16_STEREO_UNROLL > 1)
	BCC	2f
1:
.rept (SGE_PCM16_STEREO_UNROLL/1)
	LDMIA	r9, {ip,lr}                      @ Wet -> ip,lr
	PCM16_Stereo_RateLT_Lerp_FetchAndMix ip,lr @ Mix into Wet
	STMIA	r9!, {ip,lr}
.endr
	SUBS	r0, r0, #SGE_PCM16_STEREO_UNROLL
	BCS	1b
2:	ADDS	r0, r0, #SGE_PCM16_STEREO_UNROLL @ Restore nRem. Have at least 1 sample?
	BEQ	3f
#endif
20:	LDMIA	r9, {ip,lr}                      @ Wet -> ip,lr
	PCM16_Stereo_RateLT_Lerp_FetchAndMix ip,lr @ Mix into Wet
	STMIA	r9!, {ip,lr}
#if (SGE_PCM16_STEREO_UNROLL != 2) //! 2 samples/iter would have 1 sample remainder at most
	SUBS	r0, r0, #0x01
	BNE	20b
#endif
3:
#if (__NDS__ == 9)
	MOV	r7, r7, lsr #(31-15)             @ Restore Position
#else
	MOV	r7, r7, lsr #(32-15)
#endif
	SUB	r4, r4, #0x04                    @ Rewind pre-advance
#if (__NDS__ == 7)
	ORR	r6, r6, r3, lsl #0x10            @ Restore VolL|VolR
#endif
	B	.LMixer_VoxLoop_MixLoop_MixChunk_Tail

/************************************************/
//! Rate >= 1.0
/************************************************/

@ r0:  nRem
@ r1: [Output/Temp]
@ r2: [Output/Temp]
@ r3: [InputL]
@ r4: &Src
@ r5: [InputR]
@ r6:  ARM7: VolL / ARM9: VolL | VolR<<16
@ r7:  Position [17.15fxp, or 0.15fxp with INTERPOLATE == 1]
@ r8: [InputL]
@ r9: &Dst
@ sl:  Rate [17.15fxp]
@ fp: [InputR]
@ ip: [Output/Temp]
@ lr: [Output/Temp]

.macro PCM16_Stereo_RateGT_Lerp_Fetch RdL, RdR
	LDRSH	\RdL, [r4, #0x00]                @ Ra -> Rd
	LDRSH	\RdR, [r4, #0x02]
	LDRSH	ip, [r4, #0x04]                  @ Rb -> ip,lr
	LDRSH	lr, [r4, #0x06]
#if (__NDS__ == 7)
	SUBS	ip, ip, \RdL                     @ Smp = Ra + (Rb-Ra)*SubPos -> ip,lr
	MULNE	ip, r7, ip
	SUBS	lr, lr, \RdR
	MULNE	lr, r7, lr
	ADD	\RdL, \RdL, ip, asr #0x0F
	ADD	\RdR, \RdR, lr, asr #0x0F
#else
	SUB	ip, ip, \RdL
	SUB	lr, lr, \RdR
	MOV	ip, ip, lsl #0x01
	MOV	lr, lr, lsl #0x01
	SMLAWB	\RdL, ip, r7, \RdL
	SMLAWB	\RdR, lr, r7, \RdR
#endif
	ADD	r7, r7, sl                       @ Position += Rate
	MOV	r1, r7, lsr #0x0F                @ Advance Src
	BIC	r7, r7, r1, lsl #0x0F
	ADD	r4, r4, r1, lsl #0x02
.endm

.LMixerCore_PCM16_Stereo_RateGT_Lerp:
#if (__NDS__ == 7)
	MOV	r3, r6, lsr #0x10                @ VolR -> r3
	BIC	r6, r6, r3, lsl #0x10            @ VolL -> r6
#endif
#if (SGE_PCM16_STEREO_UNROLL > 1)
	SUBS	r0, r0, #SGE_PCM16_STEREO_UNROLL @ Have enough samples for a block iteration?
#endif
#if (SGE_PCM16_STEREO_UNROLL > 1)
# if (__NDS__ == 7)
	STR	r3, [sp, #-0x04]!                @ <- Out of registers to store VolR. Oops.
# endif
	BCC	2f
1:
.rept (SGE_PCM16_STEREO_UNROLL/2)
	PCM16_Stereo_RateGT_Lerp_Fetch r3,r5     @ Dry -> r3,r5, r8,fp
	PCM16_Stereo_RateGT_Lerp_Fetch r8,fp
	LDMIA	r9, {r1,r2,ip,lr}                @ Wet -> r1,r2,ip,lr
# if (__NDS__ == 7)
	MLA	r1, r3, r6, r1                   @ Mix into Wet
	LDR	r3, [sp, #0x00]
	MLA	r2, r5, r3, r2
	MLA	ip, r8, r6, ip
	MLA	lr, fp, r3, lr
# else
	SMLABB	r1, r3, r6, r1
	SMLABT	r2, r5, r6, r2
	SMLABB	ip, r8, r6, ip
	SMLABT	lr, fp, r6, lr
# endif
	STMIA	r9!, {r1,r2,ip,lr}
.endr
	SUBS	r0, r0, #SGE_PCM16_STEREO_UNROLL
	BCS	1b
2:	ADDS	r0, r0, #SGE_PCM16_STEREO_UNROLL
# if (__NDS__ == 7)
	LDR	r3, [sp], #0x04
# endif
	BEQ	3f
#endif
20:	PCM16_Stereo_RateGT_Lerp_Fetch r5,r8     @ Dry -> r5,r8
	LDMIA	r9, {ip,lr}                      @ Wet -> ip,lr
#if (__NDS__ == 7)
	MLA	ip, r5, r6, ip                   @ Mix into Wet
	MLA	lr, r8, r3, lr
#else
	SMLABB	ip, r5, r6, ip
	SMLABT	lr, r8, r6, lr
#endif
	STMIA	r9!, {ip,lr}
#if (SGE_PCM16_STEREO_UNROLL != 2) //! 2 samples/iter would have 1 sample remainder at most
	SUBS	r0, r0, #0x01                    @ --nRem?
	BNE	20b
#endif
3:
#if (__NDS__ == 7)
	ORR	r6, r6, r3, lsl #0x10            @ Restore VolL|VolR
#endif
	B	.LMixer_VoxLoop_MixLoop_MixChunk_Tail

/************************************************/
//! EOF
/************************************************/
