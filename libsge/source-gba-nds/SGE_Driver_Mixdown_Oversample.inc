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
@  r6: &MixBuf + N
@  r7:  N
@  sp+08h: &Driver

/************************************************/

@ r0:  TapL|TapR<<16
@ r1: [Smp0]
@ r2: [Smp1]
@ r3:
@ r4: &BufL
@ r5: &BufR
@ r6: &Src
@ r7:  N
@ r8: [Smp2]
@ r9: [Smp3]
@ sl:  SmpMask(=00FF00FFh)
@ fp:  SgnMask(=00800080h)
@ ip:  ExtMask(=80808080h)
@ lr: &Driver

@ NOTE: Converts Rs to unsigned type
@ Rd = Dst, Rs = Src, Rt = Tap
.macro Oversample_Lerp Rd,Rs,Rt
	ADD	\Rs, \Rs, fp                       @ <- Convert to unsigned (NOTE: -Must- use ADD, not EOR)
	ADD	\Rd, \Rt, \Rs                      @ L[n-1]+L[n] | 0 | R[n-1]+R[n] | 0
	AND	\Rd, sl, \Rd, lsr #0x01            @ Average
	ORR	\Rd, \Rd, \Rs, lsl #0x08           @ AvgL | L[n] | AvgR | R[n]
	EOR	\Rd, \Rd, ip                       @ <- Convert back to signed
.endm
.macro Oversample_Swap Ra,Rb
	EOR	\Rb, \Rb, \Ra, lsr #0x10           @ (L1^R0) | R1
	EOR	\Ra, \Ra, \Rb, lsl #0x10           @ L0 | L1
	EOR	\Rb, \Rb, \Ra, lsr #0x10           @ R0 | R1
.endm

.LMixdown_Oversample_Begin:
	LDR	lr, [sp, #0x08]                    @ Driver -> lr
	MOV	ip, #0x7F                          @ 007F007Fh -> ip
	ORR	ip, ip, ip, lsl #0x10
	ORR	sl, ip, ip, lsl #0x01              @ SmpMask -> sl
	BIC	fp, sl, ip                         @ SgnMask -> fp
	LDR	r0, [lr, #0x18+SGE_PLATFORM_OVERSAMPLING_OFFS+0x00] @ Load TapL,TapR -> r0
	ORR	ip, fp, fp, lsl #0x08              @ ExtMask -> ip
	SUB	r6, r6, r7, lsl #0x01+1            @ Rewind MixBuf
#if 0
	ADR	lr, 0f
0:	TST	r4, #0x03                          @ Handle single samples until aligned
	BNE	.LMixdown_Oversample_Single
#else //! We can only be misaligned by one sample, because each sample expands into two 8bit quantities
	TST	r4, #0x03
	BLNE	.LMixdown_Oversample_Single
#endif

.LMixdown_Oversample_Block:
1:	SUBS	r7, r7, #0x04                      @ nRem = N-4?
	BCC	12f
11:	LDMIA	r6!, {r1-r2,r8-r9}
	Oversample_Lerp r0, r1, r0
	Oversample_Lerp r1, r2, r1
	Oversample_Lerp r2, r8, r2
	Oversample_Lerp r8, r9, r8
	Oversample_Swap r0, r1
	Oversample_Swap r2, r8
	STMIA	r4!, {r0,r2}
	STMIA	r5!, {r1,r8}
	MOV	r0, r9
	SUBS	r7, r7, #0x04
	BCS	11b
12:	ADDS	r7, r7, #0x04                      @ Handle remaining samples
	BEQ	.LMixdown_Oversample_Exit
2:	ADR	lr, .LMixdown_Oversample_Single

.LMixdown_Oversample_Single:
	LDR	r1, [r6], #0x04
	Oversample_Lerp r0, r1, r0
	STRH	r0, [r4], #0x02
	MOV	r0, r0, asr #0x10
	STRH	r0, [r5], #0x02
	MOV	r0, r1
	SUBS	r7, r7, #0x01
	BXNE	lr

.LMixdown_Oversample_Exit:
	STR	r0, [lr, #0x18+SGE_PLATFORM_OVERSAMPLING_OFFS] @ Store TapL,TapR

/************************************************/
//! EOF
/************************************************/
