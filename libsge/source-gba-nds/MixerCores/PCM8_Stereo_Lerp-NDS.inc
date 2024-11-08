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
@ r1:  ARM7: SmpL | SmpR <<16 (as unsigned 8bit) / ARM9: SmpL | SmpR <<16 (as unsigned 8bit)
@ r2:  ARM7: 8000h / ARM9: 80008000h
@ r3:  ARM7: VolR
@ r4: &Src
@ r5: [Temp]
@ r6:  ARM7: VolL / ARM9: VolL | VolR<<16
@ r7:  Position<<17 [0.15fxp]
@ r8:  StepL | StepR<<16 (as signed 8bit)
@ r9: &Dst
@ sl:  Rate [0.15fxp]
@ fp: [Temp]
@ ip: [Output]
@ lr: [Output]

.macro PCM8_Stereo_RateLT_Lerp_FetchAndMix RdL, RdR
	MOVS	fp, r7, lsr #(32-8)               @ SubPos -> fp
	MULNE	fp, r8, fp                        @ (SmpL|SmpR + (StepL|StepR)*SubPos) << 8 (plus interpolation precision)
#if (__NDS__ == 9)
	ADDS	r7, r7, sl, lsl #(32-15)          @ Position += Rate?
	ADD	fp, fp, r1, lsl #0x08
	EOR	fp, fp, r2                        @ <- Convert to signed 16bit
	SMLABB	\RdL, fp, r6, \RdL                @ Wet += Dry*Vol
#else
	ADD	fp, fp, r1, lsl #0x08
	RSBS	r5, r2, fp, lsr #0x10             @ SmpR -> r5? (as signed 16bit)
	MLANE	\RdR, r5, r3, \RdR
	MOV	r5, fp, lsl #0x10                 @ SmpL -> r5?
	RSBS	r5, r2, r5, lsr #0x10
	MLANE	\RdL, r5, r6, \RdL
	ADDS	r7, r7, sl, lsl #(32-15)          @ Position += Rate?
#endif
	LDRCSH	r5, [r4, #0x02]!                  @ Advance to next samples
	ADDCS	r1, r1, r8
#if (__NDS__ == 9)
	SMLATT	\RdR, fp, r6, \RdR                @ <- Placed here to avoid a stall cycle
#endif
	ORRCS	r8, r5, r5, lsl #0x08
	BICCS	r8, r8, #0xFF00
	SUBCS	r8, r8, r1
.endm

.LMixerCore_PCM8_Stereo_RateLT_Lerp:
#if (__NDS__ == 7)
	MOV	r3, r6, lsr #0x10                 @ VolR -> r3
	BIC	r6, r6, r3, lsl #0x10             @ VolL -> r6
#endif
	MOV	r1, r7, lsr #0x11
	MOV	r1, r1, lsl #0x01
	LDRH	r1, [r4, r1]!
	MOV	r7, r7, lsl #(32-15)              @ Shift up Position
#if (SGE_PCM8_STEREO_UNROLL > 1)
	SUBS	r0, r0, #SGE_PCM8_STEREO_UNROLL @ Have enough samples for a block iteration?
#endif
	MOV	r2, #0x8000                       @ SgnMask -> r2
# if (__NDS__ == 9)
	ORR	r2, r2, r2, lsl #0x10
# endif
	LDRH	r8, [r4, #0x02]!                  @ Rb -> r8, and pre-advance Src
	ORR	r1, r1, r1, lsl #0x08             @ Re-combine into SmpL | SmpR<<16
	BIC	r1, r1, #0xFF00
	ORR	r8, r8, r8, lsl #0x08
	BIC	r8, r8, #0xFF00
	SUB	r8, r8, r1                        @ Get StepL | StepR<<16
#if (SGE_PCM8_STEREO_UNROLL > 1)
	BCC	2f
1:
.rept (SGE_PCM8_STEREO_UNROLL/1)
	LDMIA	r9, {ip,lr}                       @ Wet -> ip,lr
	PCM8_Stereo_RateLT_Lerp_FetchAndMix ip,lr
	STMIA	r9!, {ip,lr}
.endr
	SUBS	r0, r0, #SGE_PCM8_STEREO_UNROLL
	BCS	1b
2:	ADDS	r0, r0, #SGE_PCM8_STEREO_UNROLL   @ Restore nRem. Have at least 1 sample?
	BEQ	3f
#endif
20:	LDMIA	r9, {ip,lr}                       @ Wet -> ip
	PCM8_Stereo_RateLT_Lerp_FetchAndMix ip,lr @ Mix into Wet
	STMIA	r9!, {ip,lr}
#if (SGE_PCM8_STEREO_UNROLL != 2) //! 2 samples/iter would have 1 sample remainder at most
	SUBS	r0, r0, #0x01                     @ --nRem?
	BNE	20b
#endif
3:	MOV	r7, r7, lsr #(32-15)              @ Restore Position
	SUB	r4, r4, #0x02                     @ Rewind pre-advance
# if (__NDS__ == 7)
	ORR	r6, r6, r3, lsl #0x10             @ Restore VolL|VolR<<16 -> r6
# endif
	B	.LMixer_VoxLoop_MixLoop_MixChunk_Tail

/************************************************/
//! Rate >= 1.0
/************************************************/

@ r0:  AccessMask(=3FFFEh) | (-nRem-1)<<18
@ r1:  ARM7: SmpL (as signed 16bit) / ARM9: SmpL | SmpR<<16 (as signed 16bit, first sample)
@ r2:  ARM7: [Temp] / ARM9: 80008000h
@ r3:  ARM7: VolR
@ r4: &Src
@ r5: [Output]
@ r6:  ARM7: VolL / ARM9: VolL | VolR<<16
@ r7:  Position [17.15fxp]
@ r8:  ARM7: SmpR (as signed 16bit) / ARM9: SmpL | SmpR<<16 (as signed 16bit, second sample)
@ r9: &Dst
@ sl:  Rate [17.15fxp]
@ fp: [Output]
@ ip: [Output]
@ lr: [Output]

.macro PCM8_Stereo_RateGT_Lerp_Fetch Rd
	AND	\Rd, r0, r7, lsr #0x0F-1
	LDRH	ip, [\Rd, r4]!                  @ Ra -> ip
	LDRH	lr, [\Rd, #0x02]                @ Rb -> lr
	MOV	\Rd, r7, lsr #(15-8)            @ SubPos -> Rd
	ANDS	\Rd, \Rd, #0xFF
	ORR	ip, ip, ip, lsl #0x08           @ RaL | RaR<<16 -> ip
	BIC	ip, ip, #0xFF00
	ORR	lr, lr, lr, lsl #0x08           @ RbL | RbR<<16 -> lr
	BIC	lr, lr, #0xFF00
	SUB	lr, lr, ip                      @ Rb - Ra
	MULNE	\Rd, lr, \Rd                    @ Interpolate to L<<8 | R<<24
	ADD	r7, r7, sl                      @ Position += Rate
	ADD	\Rd, \Rd, ip, lsl #0x08
#if (__NDS__ == 9)
	EOR	\Rd, \Rd, r2                    @ <- Convert to signed 16bit
#endif
.endm

.macro PCM8_Stereo_RateGT_Lerp_Mix RdL, RdR=0, SrcA, SrcB=0
#if (__NDS__ == 7)
	MOV	r2, \SrcA, lsr #0x10            @ R -> r2
	BIC	\SrcA, \SrcA, r2, lsl #0x10     @ L -> r1
	SUBS	r2, r2, #0x8000
	MLANE	\RdR, r2, r3, \RdR
	SUBS	\SrcA, \SrcA, #0x8000
	MLANE	\RdL, \SrcA, r6, \RdL
#else
	SMLABB	\RdL, \SrcA, r6, \RdL
	SMLATT	\RdR, \SrcA, r6, \RdR
#endif
.endm

.LMixerCore_PCM8_Stereo_RateGT_Lerp:
#if (__NDS__ == 7)
	MOV	r3, r6, lsr #0x10               @ VolR -> r3
	BIC	r6, r6, r3, lsl #0x10           @ VolL -> r6
#endif
#if (SGE_PCM8_STEREO_UNROLL > 1)
	SUBS	r0, r0, #SGE_PCM8_STEREO_UNROLL @ Have enough samples for a block iteration?
#endif
	MVN	r1, #0x01
	SUB	r0, r1, r0, lsl #0x11           @ AccessMask | (-nRem-1)<<17 -> r0
#if (__NDS__ == 9)
	MOV	r2, #0x8000
	ORR	r2, r2, r2, lsl #0x10
#endif
#if (SGE_PCM8_STEREO_UNROLL > 1)
	BCC	2f
1:
.rept (SGE_PCM8_STEREO_UNROLL/2)
	PCM8_Stereo_RateGT_Lerp_Fetch r1        @ Dry -> r1,r8
	PCM8_Stereo_RateGT_Lerp_Fetch r8
	LDMIA	r9, {r5,fp,ip,lr}               @ Wet -> r5,fp,ip,lr
	PCM8_Stereo_RateGT_Lerp_Mix r5,fp, r1
	PCM8_Stereo_RateGT_Lerp_Mix ip,lr, r8
	STMIA	r9!, {r5,fp,ip,lr}
.endr
	ADDS	r0, r0, #SGE_PCM8_STEREO_UNROLL<<17
	BLT	1b
2:	SUBS	r0, r0, #(SGE_PCM8_STEREO_UNROLL-1)<<17 @ Restore nRem. Have at least 1 sample?
# if (__NDS__ == 9)
	BGE	.LMixer_VoxLoop_MixLoop_MixChunk_Tail
# else
	BGE	3f
# endif
#endif
20:	PCM8_Stereo_RateGT_Lerp_Fetch r1        @ Dry -> r1
	LDMIA	r9, {r5,fp}                     @ Wet -> r5,fp
	PCM8_Stereo_RateGT_Lerp_Mix r5,fp, r1
	STMIA	r9!, {r5,fp}
#if (SGE_PCM8_STEREO_UNROLL != 2) //! 2 samples/iter would have 1 sample remainder at most
	ADDS	r0, r0, #0x01<<17               @ --nRem?
	BCC	20b
#endif
3:
#if (__NDS__ == 7)
	ORR	r6, r6, r3, lsl #0x10           @ Restore VolL|VolR<<16 -> r6
#endif
	B	.LMixer_VoxLoop_MixLoop_MixChunk_Tail

/************************************************/
//! EOF
/************************************************/
