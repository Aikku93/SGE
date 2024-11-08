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
@ r1:  Ra (ARM7: uint8_t / ARM9: int16_t)
@ r2: [Temp]
@ r3:  ARM7: VolR
@ r4: &Src
@ r5:  Rb - Ra (as signed 8bit)
@ r6:  ARM7: VolL / ARM9: VolL | VolR<<16
@ r7:  Position<<17 [0.15fxp]
@ r8: [Output]
@ r9: &Dst
@ sl:  Rate [0.15fxp]
@ fp: [Output]
@ ip: [Output]
@ lr: [Output]

.macro PCM8_Mono_RateLT_Lerp_FetchAndMix RdL, RdR
	MOVS	r2, r7, lsr #0x20-8             @ Smp = Ra + (Rb-Ra)*SubPos -> r2?
#if (__NDS__ == 7)
	MULNE	r2, r5, r2
	ADD	r2, r2, r1, lsl #0x08           @ <- Expand to signed 16bit
	SUBS	r2, r2, #0x8000
	MLANE	\RdL, r2, r6, \RdL              @ Wet += Dry*Vol
	MLANE	\RdR, r2, r3, \RdR
#else
	SMLABB	r2, r5, r2, r1
	SMLABB	\RdL, r2, r6, \RdL
	SMLABT	\RdR, r2, r6, \RdR
#endif
	ADDS	r7, r7, sl, lsl #(32-15)        @ Position += Rate?
	LDRCSB	r2, [r4, #0x01]!                @  New Rb -> r2
#if (__NDS__ == 7)
	ADDCS	r1, r1, r5                      @  Ra += (Rb-Ra) -> Ra = Rb -> r1
	SUBCS	r5, r2, r1                      @  Rb-Ra -> r5
#else
	ADDCS	r1, r1, r5, lsl #0x08           @ <- Pre-expand to signed 16bit
	SUBCS	r2, r2, #0x80
	SUBCS	r5, r2, r1, asr #0x08           @  Rb-Ra -> r5
#endif
.endm

.LMixerCore_PCM8_Mono_RateLT_Lerp:
#if (__NDS__ == 7)
	MOV	r3, r6, lsr #0x10               @ VolR -> r3
	BIC	r6, r6, r3, lsl #0x10           @ VolL -> r6
#endif
	LDRB	r1, [r4, r7, lsr #0x0F]!        @ Ra -> r1, and set Src pointer
	LDRB	r5, [r4, #0x01]!                @ Rb -> r5, and pre-advance Src
	MOV	r7, r7, lsl #(32-15)            @ Shift up Position
#if (SGE_PCM8_MONO_UNROLL > 1)
	SUBS	r0, r0, #SGE_PCM8_MONO_UNROLL   @ Have enough samples for a block iteration?
#endif
	SUB	r5, r5, r1                      @ Rb - Ra -> r5
# if (__NDS__ == 9)
	SUB	r1, r1, #0x80                   @ <- Pre-expand to signed 16bit
	MOV	r1, r1, lsl #0x08
# endif
#if (SGE_PCM8_MONO_UNROLL > 1)
	BCC	2f
1:
.rept (SGE_PCM8_MONO_UNROLL/2)
	LDMIA	r9, {r8,fp,ip,lr}               @ Wet -> r8,fp,ip,lr
	PCM8_Mono_RateLT_Lerp_FetchAndMix r8,fp @ Mix into Wet
	PCM8_Mono_RateLT_Lerp_FetchAndMix ip,lr
	STMIA	r9!, {r8,fp,ip,lr}
.endr
	SUBS	r0, r0, #SGE_PCM8_MONO_UNROLL
	BCS	1b
2:	ADDS	r0, r0, #SGE_PCM8_MONO_UNROLL   @ Restore nRem. Have at least 1 sample?
	BEQ	3f
#endif
20:	LDMIA	r9, {r8,fp}                     @ Wet -> r8
	PCM8_Mono_RateLT_Lerp_FetchAndMix r8,fp @ Mix into Wet
	STMIA	r9!, {r8,fp}
#if (SGE_PCM8_MONO_UNROLL != 2) //! 2 samples/iter would have 1 sample remainder at most
	SUBS	r0, r0, #0x01
	BNE	20b
#endif
3:	MOV	r7, r7, lsr #(32-15)            @ Restore Position
	SUB	r4, r4, #0x01                   @ Rewind pre-advance
#if (__NDS__ == 7)
	ORR	r6, r6, r3, lsl #0x10           @ Restore VolL|VolR
#endif
	B	.LMixer_VoxLoop_MixLoop_MixChunk_Tail

/************************************************/
//! Rate >= 1.0
/************************************************/

@ r0:  nRem
@ r1: [Output/Temp]
@ r2: [Output]
@ r3:  ARM7: VolR
@ r4: &Src
@ r5: &Src+1
@ r6:  ARM7: VolL / ARM9: VolL | VolR<<16
@ r7:  Position [17.15fxp]
@ r8: [Input]
@ r9: &Dst
@ sl:  Rate [17.15fxp]
@ fp: [Input]
@ ip: [Output/Temp]
@ lr: [Output/Temp]

.macro PCM8_Mono_RateGT_Fetch Rd
	LDRB	\Rd, [r4, r7, lsr #0x0F]      @ Ra -> Rd
	LDRB	ip, [r5, r7, lsr #0x0F]       @ Rb -> ip
	ANDS	lr, r1, r7, lsr #(15-8)       @ Smp = Ra + (Rb-Ra)*SubPos -> Rd?
	ADD	r7, r7, sl                    @ Position += Rate
	SUBNE	ip, ip, \Rd
#if (__NDS__ == 7)
	MULNE	lr, ip, lr
#else
	SMULBB	lr, ip, lr
#endif
	SUB	\Rd, \Rd, #0x80               @ <- Expand to signed 16bit
	ADD	\Rd, lr, \Rd, lsl #0x08
.endm

.LMixerCore_PCM8_Mono_RateGT_Lerp:
#if (__NDS__ == 7)
	MOV	r3, r6, lsr #0x10             @ VolR -> r3
	BIC	r6, r6, r3, lsl #0x10         @ VolL -> r6
#endif
	ADD	r5, r4, #0x01                 @ Src+1 -> r5
#if (SGE_PCM8_MONO_UNROLL > 1)
	SUBS	r0, r0, #SGE_PCM8_MONO_UNROLL @ Have enough samples for a block iteration?
	BCC	2f
1:
.rept (SGE_PCM8_MONO_UNROLL/2)
	MOV	r1, #0xFF
	PCM8_Mono_RateGT_Fetch r8             @ Dry -> r8,fp
	PCM8_Mono_RateGT_Fetch fp
	LDMIA	r9, {r1,r2,ip,lr}             @ Wet -> r1,r2,ip,lr
# if (__NDS__ == 7)
	MLA	r1, r8, r6, r1                @ Mix into Wet
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
	SUBS	r0, r0, #SGE_PCM8_MONO_UNROLL
	BCS	1b
2:	ADDS	r0, r0, #SGE_PCM8_MONO_UNROLL @ Restore nRem. Have at least 1 sample?
# if (__NDS__ == 7)
	BEQ	3f
# else
	BEQ	.LMixer_VoxLoop_MixLoop_MixChunk_Tail
# endif
#endif
	MOV	r1, #0xFF
20:	PCM8_Mono_RateGT_Fetch r8             @ Dry -> r8
	LDMIA	r9, {ip,lr}                   @ Wet -> ip,lr
# if (__NDS__ == 7)
	MLA	ip, r8, r6, ip                @ Mix into Wet
	MLA	lr, r8, r3, lr
# else
	SMLABB	ip, r8, r6, ip
	SMLABT	lr, r8, r6, lr
# endif
	STMIA	r9!, {ip,lr}
#if (SGE_PCM8_MONO_UNROLL != 2) //! 2 samples/iter would have 1 sample remainder at most
	SUBS	r0, r0, #0x01                 @ --nRem?
	BNE	20b
#endif
3:
#if (__NDS__ == 7)
	ORR	r6, r6, r3, lsl #0x10         @ Restore VolL|VolR
#endif
	B	.LMixer_VoxLoop_MixLoop_MixChunk_Tail

/************************************************/
//! EOF
/************************************************/
