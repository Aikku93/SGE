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
@ r5: [Output]
@ r6:  ARM7: VolL / ARM9: VolL | VolR<<16
@ r7:  Position<<17 [0.15fxp]
@ r8:  RaR
@ r9: &Dst
@ sl:  Rate [0.15fxp]
@ fp: [Output]
@ ip: [Output]
@ lr: [Output]

.macro PCM16_Stereo_RateLT_NoLerp_FetchAndMix RdL, RdR
#if (__NDS__ == 7)
	ADD	\RdL, \RdL, r1                   @ Wet += Dry*Vol
	ADD	\RdR, \RdR, r8
#else
	SMLABB	\RdL, r1, r6, \RdL
	SMLABT	\RdR, r8, r6, \RdR
#endif
	ADDS	r7, r7, sl, lsl #(32-15)         @ Position += Rate?
	LDRCSSH	r1, [r4, #0x04]!                 @  Fetch next samples
	LDRCSSH	r8, [r4, #0x02]                  @  NOTE: Stall cycle on the SMLABB for the next sample
#if (__NDS__ == 7)
	MULCS	r1, r6, r1                       @ Pre-multiply Dry*Vol
	MULCS	r8, r3, r8
#endif
.endm

.LMixerCore_PCM16_Stereo_RateLT_NoLerp:
#if (__NDS__ == 7)
	MOV	r3, r6, lsr #0x10                @ VolR -> r3
	BIC	r6, r6, r3, lsl #0x10            @ VolL -> r6
#endif
	MOV	r1, r7, lsr #0x0F                @ Ra -> r1,r8, and set Src pointer
	MOV	r1, r1, lsl #0x02
	LDRSH	r1, [r4, r1]!
	LDRSH	r8, [r4, #0x02]
	MOV	r7, r7, lsl #(32-15)             @ Shift up Position
#if (SGE_PCM16_STEREO_UNROLL > 1)
	SUBS	r0, r0, #SGE_PCM16_STEREO_UNROLL @ Have enough samples for a block iteration?
#endif
#if (__NDS__ == 7)
	MUL	r1, r6, r1                       @ Pre-multiply Dry*Vol
	MUL	r8, r3, r8
#endif
#if (SGE_PCM16_STEREO_UNROLL > 1)
	BCC	2f
1:
.rept (SGE_PCM16_STEREO_UNROLL/2)
	LDMIA	r9, {r5,fp,ip,lr}                @ Wet -> r5,fpip,lr
	PCM16_Stereo_RateLT_NoLerp_FetchAndMix r5,fp @ Mix into Wet
	PCM16_Stereo_RateLT_NoLerp_FetchAndMix ip,lr
	STMIA	r9!, {r5,fp,ip,lr}
.endr
	SUBS	r0, r0, #SGE_PCM16_STEREO_UNROLL
	BCS	1b
2:	ADDS	r0, r0, #SGE_PCM16_STEREO_UNROLL @ Restore nRem. Have at least 1 sample?
	BEQ	3f
#endif
20:	LDMIA	r9, {ip,lr}                      @ Wet -> ip,lr
	PCM16_Stereo_RateLT_NoLerp_FetchAndMix ip,lr @ Mix into Wet
	STMIA	r9!, {ip,lr}
#if (SGE_PCM16_STEREO_UNROLL != 2) //! 2 samples/iter would have 1 sample remainder at most
	SUBS	r0, r0, #0x01
	BNE	20b
#endif
3:
	MOV	r7, r7, lsr #(32-15)             @ Restore Position
#if (__NDS__ == 7)
	ORR	r6, r6, r3, lsl #0x10            @ Restore VolL|VolR
#endif
	B	.LMixer_VoxLoop_MixLoop_MixChunk_Tail

/************************************************/
//! Rate >= 1.0
/************************************************/

@ r0:  AccessMask(=7FFFCh) | (-nRem-1)<<19
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

.macro PCM16_Stereo_RateGT_NoLerp_Fetch RdL, RdR
	AND	\RdR, r0, r7, lsr #0x0F-2
#if (__NDS__ == 9)
	LDR	\RdL, [r4, \RdR]
#else
	LDRSH	\RdL, [\RdR, r4]!
	LDRSH	\RdR, [\RdR, #0x02]
#endif
	ADD	r7, r7, sl                       @ Position += Rate
.endm

.LMixerCore_PCM16_Stereo_RateGT_NoLerp:
#if (__NDS__ == 7)
	MOV	r3, r6, lsr #0x10                @ VolR -> r3
	BIC	r6, r6, r3, lsl #0x10            @ VolL -> r6
#endif
#if (SGE_PCM16_STEREO_UNROLL > 1)
	SUBS	r0, r0, #SGE_PCM16_STEREO_UNROLL @ Have enough samples for a block iteration?
#endif
	MVN	ip, #0x03                        @ AccessMask | (-nRem-1)<<19 -> r0
	SUB	r0, ip, r0, lsl #0x13
#if (SGE_PCM16_STEREO_UNROLL > 1)
# if (__NDS__ == 7)
	STR	r3, [sp, #-0x04]!                @ <- Out of registers to store VolR. Oops.
# endif
	BCC	2f
1:
.rept (SGE_PCM16_STEREO_UNROLL/2)
	PCM16_Stereo_RateGT_NoLerp_Fetch r3,r5   @ Dry -> r3,r5, r8,fp
	PCM16_Stereo_RateGT_NoLerp_Fetch r8,fp
	LDMIA	r9, {r1,r2,ip,lr}                @ Wet -> r1,r2,ip,lr
# if (__NDS__ == 7)
	MLA	r1, r3, r6, r1                   @ Mix into Wet
	LDR	r3, [sp, #0x00]
	MLA	r2, r5, r3, r2
	MLA	ip, r8, r6, ip
	MLA	lr, fp, r3, lr
# else
	SMLABB	r1, r3, r6, r1
	SMLATT	r2, r3, r6, r2
	SMLABB	ip, r8, r6, ip
	SMLATT	lr, r8, r6, lr
# endif
	STMIA	r9!, {r1,r2,ip,lr}
.endr
	ADDS	r0, r0, #SGE_PCM16_STEREO_UNROLL<<19
	BLT	1b
2:	SUBS	r0, r0, #(SGE_PCM16_STEREO_UNROLL-1)<<19 @ Restore nRem. Have at least 1 sample?
# if (__NDS__ == 7)
	LDR	r3, [sp], #0x04
# endif
	BGE	3f
#endif
20:	PCM16_Stereo_RateGT_NoLerp_Fetch r5,r8   @ Dry -> r5,r8
	LDMIA	r9, {ip,lr}                      @ Wet -> ip,lr
#if (__NDS__ == 7)
	MLA	ip, r5, r6, ip                   @ Mix into Wet
	MLA	lr, r8, r3, lr
#else
	SMLABB	ip, r5, r6, ip
	SMLATT	lr, r5, r6, lr
#endif
	STMIA	r9!, {ip,lr}
#if (SGE_PCM16_STEREO_UNROLL != 2) //! 2 samples/iter would have 1 sample remainder at most
	ADDS	r0, r0, #0x01<<19                @ --nRem?
	BCC	20b
#endif
3:
#if (__NDS__ == 7)
	ORR	r6, r6, r3, lsl #0x10            @ Restore VolL|VolR
#endif
	B	.LMixer_VoxLoop_MixLoop_MixChunk_Tail

/************************************************/
//! EOF
/************************************************/
