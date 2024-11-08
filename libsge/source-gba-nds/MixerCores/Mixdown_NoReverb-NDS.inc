/************************************************/
ASM_MODE_ARM
/************************************************/

@ Input:
@  r0:
@  r1:
@  r2:
@  r3:
@  r4: &DstL
@  r5: &DstR
@  r6: &MixBuf
@  r7:  N
@  sp+08h: &Driver

/************************************************/

@ r0: [Src0]
@ r1: [Smp1]
@ r2: [Smp2]
@ r3: [Smp3]
@ r4: &DstL (s16[L/R][N])
@ r5: &DstR
@ r6: &Src  (s32[N][L/R])
@ r7:  nRem
@ r8:  ClipMask(=FFFF7FFFh without oversampling, or 7FFFh with oversampling)
@ r9: &Dst (with oversampling)
@ sl: [Smp4]
@ fp: [Smp5]
@ ip: [Smp6]
@ lr: [Smp7/Link]

.macro NoReverb_MakeSmp x
#if (SGE_MIXER_VOLBITS > 0)
	MOV	\x, \x, asr #SGE_MIXER_VOLBITS
#endif
	FastClip \x, r8
.endm

.macro NoReverb_MakeBlk Smp0, Smp1
	NoReverb_MakeSmp \Smp0
	NoReverb_MakeSmp \Smp1
#if SGE_USE_OVERSAMPLING
	ADD	\Smp0, \Smp0, \Smp1, lsl #0x10
#else
	AND	\Smp0, \Smp0, r8, lsr #0x10
	ORR	\Smp0, \Smp0, \Smp1, lsl #0x10
#endif
.endm

.LMixdown_NoReverb:
	MVN	r8, #0x8000           @ ClipMask -> r8
#if SGE_USE_OVERSAMPLING
	AND	r8, r8, r8, lsr #0x10
	MOV	r9, r6
	STR	r7, [sp, #-0x04]!
#else
# if 0 //! We can only be misaligned by one sample, because each sample is 16bit
	ADR	lr, 0b
0:	TST	r4, #0x03             @ Handle single samples until aligned
	BNE	.LMixdown_NoReverb_Single
# else
	TST	r4, #0x03
	BLNE	.LMixdown_NoReverb_Single
# endif
#endif

.LMixdown_NoReverb_Block:
1:	SUBS	r7, r7, #0x04         @ nRem = N-4?
	BCC	12f
11:	LDMIA	r6!, {r0-r3,sl-ip,lr}
#if SGE_USE_OVERSAMPLING
	NoReverb_MakeBlk r0, r1       @ Pack {L0|R0,L1|R1} and {L2|R2,L3|R3}
	NoReverb_MakeBlk r2, r3
	NoReverb_MakeBlk sl, fp
	NoReverb_MakeBlk ip, lr
	STMIA	r9!, {r0,r2,sl,ip}
#else
	NoReverb_MakeBlk r0, r2       @ Pack {L0|L1,L2|L3} and {R0|R1,R2|R3}
	NoReverb_MakeBlk r1, r3
	NoReverb_MakeBlk sl, ip
	NoReverb_MakeBlk fp, lr
	STMIA	r4!, {r0,sl}
	STMIA	r5!, {r1,fp}
#endif
	SUBS	r7, r7, #0x04         @ nRem -= 4?
	BCS	11b
12:	ADDS	r7, r7, #0x04         @ Handle remaining samples
#if SGE_USE_OVERSAMPLING
	BEQ	.LMixdown_NoReverb_Exit
#else
	BEQ	.LMixer_Mixdown_Return
#endif
2:	ADR	lr, .LMixdown_NoReverb_Single

.LMixdown_NoReverb_Single:
	LDMIA	r6!, {r0-r1}
#if SGE_USE_OVERSAMPLING
	NoReverb_MakeBlk r0, r1
	STR	r0, [r9], #0x04
#else
	NoReverb_MakeSmp r0
	NoReverb_MakeSmp r1
	STRH	r0, [r4], #0x02
	STRH	r1, [r5], #0x02
#endif
	SUBS	r7, r7, #0x01
	BXNE	lr

.LMixdown_NoReverb_Exit:
#if SGE_USE_OVERSAMPLING
	LDR	r7, [sp], #0x04
#endif
	B	.LMixer_Mixdown_Return

/************************************************/
//! EOF
/************************************************/
