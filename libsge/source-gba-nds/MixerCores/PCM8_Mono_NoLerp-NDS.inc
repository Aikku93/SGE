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
@  r7:  Position [17.15fxp]
@  r8: &Wav
@  r9: &Dst
@  sl:  Rate [17.15fxp]
@  fp:
@  ip:
@  lr:

/************************************************/

@ r0:  nRem
@ r1: [Output]
@ r2: [Output]
@ r3:  ARM7: VolR
@ r4: &Src
@ r5:  8000h
@ r6:  ARM7: VolL / ARM9: VolL | VolR<<16
@ r7:  Position [17.15fxp]
@ r8: [Input]
@ r9: &Dst
@ sl:  Rate [17.15fxp]
@ fp: [Input]
@ ip: [Output/Temp]
@ lr: [Output/Temp]

.macro PCM8_Mono_NoLerp_Fetch Rd
	LDRB	\Rd, [r4, r7, lsr #0x0F] @ Ra -> Rd
	ADD	r7, r7, sl               @ Position += Rate
.endm

.LMixerCore_PCM8_Mono_RateLT_NoLerp:
.LMixerCore_PCM8_Mono_RateGT_NoLerp:
#if (__NDS__ == 7)
	MOV	r3, r6, lsr #0x10                 @ VolR -> r3
	BIC	r6, r6, r3, lsl #0x10             @ VolL -> r6
#endif
	MOV	r5, #0x8000
#if (SGE_PCM8_MONO_UNROLL > 1)
	SUBS	r0, r0, #SGE_PCM8_MONO_UNROLL     @ Have enough samples for a block iteration?
	BCC	2f
1:
.rept (SGE_PCM8_MONO_UNROLL/2)
	PCM8_Mono_NoLerp_Fetch r8                 @ Dry -> r8,fp
	PCM8_Mono_NoLerp_Fetch fp
	LDMIA	r9, {r1,r2,ip,lr}                 @ Wet -> r1,r2,ip,lr
	RSB	r8, r5, r8, lsl #0x08             @ Expand to signed 16bit
	RSB	fp, r5, fp, lsl #0x08
# if (__NDS__ == 7)
	MLA	r1, r8, r6, r1                    @ Mix into Wet
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
2:	ADDS	r0, r0, #SGE_PCM8_MONO_UNROLL     @ Restore nRem. Have at least 1 sample?
# if (__NDS__ == 7)
	BEQ	3f
# else
	BEQ	.LMixer_VoxLoop_MixLoop_MixChunk_Tail
# endif
#endif
20:	PCM8_Mono_NoLerp_Fetch r8                 @ Dry -> r8
	LDMIA	r9, {ip,lr}                       @ Wet -> ip,lr
	RSB	r8, r5, r8, lsl #0x08             @ Expand to signed 16bit
# if (__NDS__ == 7)
	MLA	ip, r8, r6, ip                    @ Mix into Wet
	MLA	lr, r8, r3, lr
# else
	SMLABB	ip, r8, r6, ip
	SMLABT	lr, r8, r6, lr
# endif
	STMIA	r9!, {ip,lr}
#if (SGE_PCM8_MONO_UNROLL != 2) //! 2 samples/iter would have 1 sample remainder at most
	SUBS	r0, r0, #0x01                     @ --nRem?
	BNE	20b
#endif
3:
#if (__NDS__ == 7)
	ORR	r6, r6, r3, lsl #0x10             @ Restore VolL|VolR
#endif
	B	.LMixer_VoxLoop_MixLoop_MixChunk_Tail

/************************************************/
//! EOF
/************************************************/
