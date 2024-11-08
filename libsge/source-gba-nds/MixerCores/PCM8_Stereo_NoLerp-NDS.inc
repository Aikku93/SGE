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
@ r1:  ARM7: SmpL (as signed 8bit) / ARM9: SmpL (as signed 16bit)
@ r2:  ARM7: VolL << 8 / ARM9: 8000h
@ r3:  ARM7: VolR << 8
@ r4: &Src
@ r5: [Temp]
@ r6:  VolL | VolR<<16
@ r7:  Position<<17 [0.15fxp]
@ r8:  ARM7: SmpR (as signed 8bit) / ARM9: SmpR (as signed 16bit)
@ r9: &Dst
@ sl:  Rate [0.15fxp]
@ fp: [Temp]
@ ip: [Output]
@ lr: [Output]

.macro PCM8_Stereo_RateLT_NoLerp_FetchAndMix RdL, RdR
	ADDS	r7, r7, sl, lsl #(32-15)   @ Position += Rate?
	LDRCSB	r5, [r4, #0x02]!           @  Load next samples
	LDRCSB	fp, [r4, #0x01]
#if (__NDS__ == 7)
	MLA	\RdL, r2, r1, \RdL         @ Wet += Dry*Vol
	MLA	\RdR, r3, r8, \RdR
	SUBCS	r1, r5, #0x80              @ <- Convert to signed 8bit
	SUBCS	r8, fp, #0x80
#else
	SMLABB	\RdL, r6, r1, \RdL
	SMLATB	\RdR, r6, r8, \RdR
	RSBCS	r1, r2, r5, lsl #0x08      @ <- Convert to signed 16bit
	RSBCS	r8, r2, fp, lsl #0x08
#endif
.endm

.LMixerCore_PCM8_Stereo_RateLT_NoLerp:
#if (__NDS__ == 7)
	MOV	r3, r6, lsr #0x10          @ VolR -> r3
	BIC	r2, r6, r3, lsl #0x10      @ VolL -> r2
	MOV	r2, r2, lsl #0x08          @ Pre-scale for adding 8bit data to 16bit accumulator
	MOV	r3, r3, lsl #0x08
#endif
	MOV	r1, r7, lsr #0x11
	MOV	r1, r1, lsl #0x01
	LDRH	r1, [r4, r1]!
	MOV	r7, r7, lsl #(32-15)       @ Shift up Position
#if (SGE_PCM8_STEREO_UNROLL > 1)
	SUBS	r0, r0, #SGE_PCM8_STEREO_UNROLL @ Have enough samples for a block iteration?
#endif
#if (__NDS__ == 9)
	MOV	r2, #0x8000                @ SgnMask -> r2
#endif
	MOV	r8, r1, lsr #0x08          @ SmpR -> r8
	AND	r1, r1, #0xFF              @ SmpL -> r1
#if (__NDS__ == 7)
	SUB	r1, r1, #0x80
	SUB	r8, r8, #0x80
#else
	RSB	r1, r2, r1, lsl #0x08
	RSB	r8, r2, r8, lsl #0x08
#endif
#if (SGE_PCM8_STEREO_UNROLL > 1)
	BCC	2f
1:
.rept (SGE_PCM8_STEREO_UNROLL/1)
	LDMIA	r9, {ip,lr}                @ Wet -> ip,lr
	PCM8_Stereo_RateLT_NoLerp_FetchAndMix ip,lr
	STMIA	r9!, {ip,lr}
.endr
	SUBS	r0, r0, #SGE_PCM8_STEREO_UNROLL
	BCS	1b
2:	ADDS	r0, r0, #SGE_PCM8_STEREO_UNROLL @ Restore nRem. Have at least 1 sample?
	BEQ	3f
#endif
20:	LDMIA	r9, {ip,lr}                @ Wet -> ip
	PCM8_Stereo_RateLT_NoLerp_FetchAndMix ip,lr @ Mix into Wet
	STMIA	r9!, {ip,lr}
#if (SGE_PCM8_STEREO_UNROLL != 2) //! 2 samples/iter would have 1 sample remainder at most
	SUBS	r0, r0, #0x01              @ --nRem?
	BNE	20b
#endif
3:	MOV	r7, r7, lsr #(32-15)       @ Restore Position
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

.macro PCM8_Stereo_RateGT_NoLerp_Fetch Rd
	AND	\Rd, r0, r7, lsr #0x0F-1
	LDRH	\Rd, [r4, \Rd]                     @ L|R<<8 -> Rd
	ADD	r7, r7, sl                         @ Position += Rate
.endm

@ NOTE: On ARM7, the following applies:
@  RdL = 1st mix target (L or R)
@  RdR = 2nd mix target (L or R)
.macro PCM8_Stereo_RateGT_NoLerp_Mix RdL, RdR=0, SrcA, SrcB=0
#if (__NDS__ == 7)
	SUBS	r2, \SrcA, #0x8000                 @ <- This keeps the L sample as "garbage" bits, but
	MLANE	\RdL, r2, r3, \RdL                 @    hopefully is not noticeable :/
.if \SrcB != 0
	SUBS	r2, \SrcB, #0x8000
	MLANE	\RdR, r2, r3, \RdR
.endif
#else
	ORR	\SrcA, \SrcA, \SrcA, lsl #0x08     @ Expand to L | R<<16
	BIC	\SrcA, \SrcA, #0xFF00
	EOR	\SrcA, r2, \SrcA, lsl #0x08        @ Convert to signed 16bit
	SMLABB	\RdL, \SrcA, r6, \RdL
	SMLATT	\RdR, \SrcA, r6, \RdR
#endif
.endm

.LMixerCore_PCM8_Stereo_RateGT_NoLerp:
#if (__NDS__ == 7)
	MOV	r3, r6, lsr #0x10                  @ VolR -> r3
	BIC	r6, r6, r3, lsl #0x10              @ VolL -> r6
#endif
#if (SGE_PCM8_STEREO_UNROLL > 1)
	SUBS	r0, r0, #SGE_PCM8_STEREO_UNROLL    @ Have enough samples for a block iteration?
#endif
	MVN	r1, #0x01
	SUB	r0, r1, r0, lsl #0x11              @ AccessMask | (-nRem-1)<<17 -> r0
#if (__NDS__ == 9)
	MOV	r2, #0x8000
	ORR	r2, r2, r2, lsl #0x10
#endif
#if (SGE_PCM8_STEREO_UNROLL > 1)
	BCC	2f
1:
.rept (SGE_PCM8_STEREO_UNROLL/2)
	PCM8_Stereo_RateGT_NoLerp_Fetch r1         @ Dry -> r1,r8
	PCM8_Stereo_RateGT_NoLerp_Fetch r8
	LDMIA	r9, {r5,fp,ip,lr}                  @ Wet -> r5,fp,ip,lr
# if (__NDS__ == 7)
	PCM8_Stereo_RateGT_NoLerp_Mix fp,lr, r1,r8 @ Mix right samples
	MOV	r2, #0xFF00                        @ Shift up to the left sample
	AND	r1, r2, r1, lsl #0x08
	AND	r8, r2, r8, lsl #0x08
	PCM8_Stereo_RateGT_NoLerp_Mix r5,ip, r1,r8 @ Mix left samples
# else
	PCM8_Stereo_RateGT_NoLerp_Mix r5,fp, r1
	PCM8_Stereo_RateGT_NoLerp_Mix ip,lr, r8
# endif
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
#if (__NDS__ == 7)
	MOV	ip, #0xFF00
#endif
20:	PCM8_Stereo_RateGT_NoLerp_Fetch r1         @ Dry -> r1
	LDMIA	r9, {r5,fp}                        @ Wet -> r5,fp
#if (__NDS__ == 7)
	PCM8_Stereo_RateGT_NoLerp_Mix fp,, r1,     @ Mix right sample
	AND	r1, ip, r1, lsl #0x08              @ Shift up to the left sample
	PCM8_Stereo_RateGT_NoLerp_Mix r5,, r1,     @ Mix left sample
#else
	PCM8_Stereo_RateGT_NoLerp_Mix r5,fp, r1
#endif
	STMIA	r9!, {r5,fp}
#if (SGE_PCM8_STEREO_UNROLL != 2) //! 2 samples/iter would have 1 sample remainder at most
	ADDS	r0, r0, #0x01<<17                  @ --nRem?
	BCC	20b
#endif
3:
#if (__NDS__ == 7)
	ORR	r6, r6, r3, lsl #0x10              @ Restore VolL|VolR<<16 -> r6
#endif
	B	.LMixer_VoxLoop_MixLoop_MixChunk_Tail

/************************************************/
//! EOF
/************************************************/
