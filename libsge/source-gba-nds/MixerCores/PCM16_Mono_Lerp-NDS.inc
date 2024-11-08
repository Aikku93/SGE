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
@ r1:  Ra
@ r2: [Temp]
@ r3:  ARM7: VolR
@ r4: &Src
@ r5:  ARM7: (Rb - Ra) / ARM9: (Rb - Ra)*2
@ r6:  ARM7: VolL / ARM9: VolL / VolR<<16
@ r7:  ARM7: Position<<17 [0.15fxp] / ARM9: Position<<16 [0.15fxp]
@ r8: [Output]
@ r9: &Dst
@ sl:  Rate [0.15fxp]
@ fp: [Output]
@ ip: [Output]
@ lr: [Output]

.macro PCM16_Mono_RateLT_Lerp_FetchAndMix RdL, RdR
#if (__NDS__ == 7)
	MOVS	r2, r7, lsr #(32-8)              @ Smp = Ra + (Rb-Ra)*SubPos -> r2?
	MULNE	r2, r5, r2
	ADDS	r2, r1, r2, asr #0x08
	MLANE	\RdL, r2, r6, \RdL               @ Wet += Dry*Vol
	MLANE	\RdR, r2, r3, \RdR
	ADDS	r7, r7, sl, lsl #(32-15)         @ Position += Rate?
	LDRCSSH	r2, [r4, #0x02]!                 @  New Rb -> r2
	ADDCS	r1, r1, r5                       @  Ra += (Rb-Ra) -> Ra = Rb -> r1
	SUBCS	r5, r2, r1                       @  Rb-Ra -> r5
#else
	SMLAWT	r2, r5, r7, r1                   @ Smp = Ra + (Rb-Ra)*SubPos -> r2
	SMLABB	\RdL, r2, r6, \RdL
	SMLABT	\RdR, r2, r6, \RdR
	ADDS	r7, r7, sl, lsl #(31-15)         @ Position += Rate?
	LDRMISH	r2, [r4, #0x02]!                 @  New Rb -> r2
	SUBMI	r7, r7, #0x01<<31                @  Position -= 1.0 (needed for SMLAWx)
	ADDMI	r1, r1, r5, asr #0x01            @  Ra += (Rb-Ra) -> Ra = Rb -> r1
	SUBMI	r5, r2, r1                       @  Rb-Ra -> r5
	MOVMI	r5, r5, lsl #0x01                @  Rb-Ra *= 2 (needed for SMLAWx with 15bit SubPos)
#endif
.endm

.LMixerCore_PCM16_Mono_RateLT_Lerp:
#if (__NDS__ == 7)
	MOV	r3, r6, lsr #0x10                @ VolR -> r3
	BIC	r6, r6, r3, lsl #0x10            @ VolL -> r6
#endif
	MOV	r1, r7, lsr #0x0F                @ Ra -> r1, and set Src pointer
	MOV	r1, r1, lsl #0x01
	LDRSH	r1, [r4, r1]!
	LDRSH	r5, [r4, #0x02]!                 @ Rb -> r5, and pre-advance Src
#if (__NDS__ == 9)
	MOV	r7, r7, lsl #(31-15)             @ Shift up Position
#else
	MOV	r7, r7, lsl #(32-15)
#endif
#if (SGE_PCM16_MONO_UNROLL > 1)
	SUBS	r0, r0, #SGE_PCM16_MONO_UNROLL   @ Have enough samples for a block iteration?
#endif
	SUB	r5, r5, r1                       @ Rb - Ra -> r5
#if (__NDS__ == 9)
	MOV	r5, r5, lsl #0x01                @ Rb-Ra *= 2 (needed for SMLAWx with 15bit SubPos)
#endif
#if (SGE_PCM16_MONO_UNROLL > 1)
	BCC	2f
1:
.rept (SGE_PCM16_MONO_UNROLL/2)
	LDMIA	r9, {r8,fp,ip,lr}                @ Wet -> r8,fp,ip,lr
	PCM16_Mono_RateLT_Lerp_FetchAndMix r8,fp @ Mix into Wet
	PCM16_Mono_RateLT_Lerp_FetchAndMix ip,lr
	STMIA	r9!, {r8,fp,ip,lr}
.endr
	SUBS	r0, r0, #SGE_PCM16_MONO_UNROLL
	BCS	1b
2:	ADDS	r0, r0, #SGE_PCM16_MONO_UNROLL   @ Restore nRem. Have at least 1 sample?
	BEQ	3f
#endif
20:	LDMIA	r9, {r8,fp}                      @ Wet -> r8
	PCM16_Mono_RateLT_Lerp_FetchAndMix r8,fp @ Mix into Wet
	STMIA	r9!, {r8,fp}
#if (SGE_PCM16_MONO_UNROLL != 2) //! 2 samples/iter would have 1 sample remainder at most
	SUBS	r0, r0, #0x01
	BNE	20b
#endif
3:
#if (__NDS__ == 9)
	MOV	r7, r7, lsr #(31-15)             @ Restore Position
#else
	MOV	r7, r7, lsr #(32-15)
#endif
	SUB	r4, r4, #0x02                    @ Rewind pre-advance
#if (__NDS__ == 7)
	ORR	r6, r6, r3, lsl #0x10            @ Restore VolL|VolR
#endif
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

.macro PCM16_Mono_RateGT_Lerp_Fetch Rd
	AND	ip, r5, r7, lsr #0x0F-1
	BICS	lr, r7, ip, lsl #0x0F-1
	LDRSH	\Rd, [ip, r4]!                 @ Ra -> Rd
	LDRNESH	ip, [ip, #0x02]                @ Rb -> ip
	ADD	r7, r7, sl                     @ Position += Rate
	SUBNES	ip, ip, \Rd                    @ <- 1c stall on ARM9. TODO: How to remove it?
#if (__NDS__ == 9)
	MOV	ip, ip, lsl #0x01
	SMLAWB	\Rd, ip, lr, \Rd               @ Smp = Ra + (Rb-Ra)*SubPos -> Rd?
#else
	MULNE	ip, lr, ip
	ADDNE	\Rd, \Rd, ip, asr #0x0F
#endif
.endm

.LMixerCore_PCM16_Mono_RateGT_Lerp:
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
	PCM16_Mono_RateGT_Lerp_Fetch r8        @ Dry -> r8,fp
	PCM16_Mono_RateGT_Lerp_Fetch fp
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
20:	PCM16_Mono_RateGT_Lerp_Fetch r8        @ Dry -> r8
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
