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

@ r0: [OutL0]
@ r1: [OutL1]
@ r2: [OutR0]
@ r3: [OutR1]
@ r4: &BufL
@ r5: &BufR
@ r6: &Src
@ r7:  N
@ r8:  TapL|TapR<<16
@ r9: &Driver
@ sl:  SgnMask   (=80008000h)
@ fp:  InvSgnMask(=7FFF7FFFh)
@ ip: [Temp]
@ lr:

@ RdL/R = Dst, Rs = Src, Rt = Tap
@ RdR must not be Rt
@ NOTE: Converts Rs to unsigned type
.macro Oversample_LerpAndUnpack RdL,RdR,Rs,Rt
	ADD	\Rs, \Rs, sl                       @ <- Convert to unsigned (NOTE: -Must- use ADD, not EOR)
	EOR	ip, \Rs, \Rt                       @ AVG(L[n-1],L[n]) | AVG(R[n-1],R[n]) -> Rd
	BIC	\RdR, \Rs, ip                      @ (a+b)/2 = (a&b) + ((a^b)>>1) = (a&~(a^b)) + ((a^b)>>1)
	AND	ip, fp, ip, lsr #0x01
	ADD	\RdR, \RdR, ip
	EOR	\RdL, \RdR, \Rs, lsl #0x10         @ AvgL | AvgR^L
	EOR	\RdR, \Rs, \RdL, lsr #0x10         @ AvgR | R
	EOR	\RdL, \RdL, \RdR, lsl #0x10        @ AvgL | L
	EOR	\RdL, \RdL, sl                     @ <- Convert back to signed
	EOR	\RdR, \RdR, sl
.endm

.LMixdown_Oversample_Begin:
	LDR	r9, [sp, #0x08]                    @ Driver -> r9
	MOV	sl, #0x8000                        @ SgnMask -> sl
	ORR	sl, sl, sl, lsl #0x10
	MVN	fp, sl                             @ InvSgnMask -> fp
	LDR	r8, [r9, #0x18+SGE_PLATFORM_OVERSAMPLING_OFFS+0x00] @ Load TapL,TapR -> r8
	SUB	r6, r6, r7, lsl #0x02+1            @ Rewind MixBuf
#if 0 //! We can never be misaligned, because each sample expands into two 16bit quantities
	ADR	lr, 0f
0:	TST	r4, #0x03                          @ Handle single samples until aligned
	BNE	.LMixdown_Oversample_Single
#endif

.LMixdown_Oversample_Block:
1:	SUBS	r7, r7, #0x04                      @ nRem = N-4?
	BCC	12f
11:	LDMIA	r6!, {r2,r3}
	Oversample_LerpAndUnpack r0,r1, r2, r8
	Oversample_LerpAndUnpack r2,r8, r3, r2
	STMIA	r4!, {r0,r2}
	STMIA	r5!, {r1,r8}
	LDMIA	r6!, {r2,r8}
	Oversample_LerpAndUnpack r0,r1, r2, r3
	Oversample_LerpAndUnpack r2,r3, r8, r2
	STMIA	r4!, {r0,r2}
	STMIA	r5!, {r1,r3}
	SUBS	r7, r7, #0x04
	BCS	11b
12:	ADDS	r7, r7, #0x04                      @ Handle remaining samples
	BEQ	.LMixdown_Oversample_Exit
2:	ADR	lr, .LMixdown_Oversample_Single

.LMixdown_Oversample_Single:
	LDR	r2, [r6], #0x04
	Oversample_LerpAndUnpack r0,r1, r2, r8
	STR	r0, [r4], #0x04
	STR	r1, [r5], #0x04
	MOV	r8, r2
	SUBS	r7, r7, #0x01
	BXNE	lr

.LMixdown_Oversample_Exit:
	STR	r8, [r9, #0x18+SGE_PLATFORM_OVERSAMPLING_OFFS] @ Store TapL,TapR

/************************************************/
//! EOF
/************************************************/
