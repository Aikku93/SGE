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
@  sp+10h:  BiasL|BiasR<<16

/************************************************/

@ r0: [Src0]
@ r1: [Smp1]
@ r2: [Smp2]
@ r3: [Smp3]
@ r4: &DstL (s8[L/R][N])
@ r5: &DstR
@ r6: &Src  (u16[N][L/R])
@ r7:  N
@ r8:  BiasL|BiasR<<16
@ r9:  ClipMask(=7Fh)
@ sl:  nRem
@ fp:  BlkData
@ ip: [Temp]
@ lr: [Temp/Link]

.macro NoReverb_MakeSmp Dst, Src, Chan
.if \Chan == 0
	MOV	\Dst, \Src, lsl #0x10
	MOV	\Dst, \Dst, asr #0x10+SGE_MIXER_VOLFRACBITS
.else
	MOV	\Dst, \Src, asr #0x10+SGE_MIXER_VOLFRACBITS
.endif
#if SGE_USE_CLIPPING
	FastClip \Dst, r9
#endif
.endm

.macro NoReverb_MakeBlk Dst, Tmp, Src, Chan, Idx
	NoReverb_MakeSmp \Tmp, \Src, \Chan
.if \Idx > 0
.if \Idx < 3
	AND	\Tmp, \Tmp, #0xFF
.endif
	ORR	\Dst, \Dst, \Tmp, lsl #0x08*\Idx
.else
	AND	\Dst, \Tmp, #0xFF
.endif
.endm

.macro MakeLR x
	SUB	\x, \x, r8, lsl #SGE_MIXER_VOLFRACBITS @ Remove bias (plus possible -1 overflow error in SrcR from SrcL-BiasL)
#if SGE_USE_OVERSAMPLING
	MOV	ip, \x, lsl #0x10
	MOV	\x, \x, asr #0x10+SGE_MIXER_VOLFRACBITS
	MOV	ip, ip, asr #0x10+SGE_MIXER_VOLFRACBITS
# if SGE_USE_CLIPPING
	FastClip \x, r9
	FastClip ip, r9
# endif
	ADD	\x, ip, \x, lsl #0x10                  @ <- This technically can overflow (eg. -80h + -80h<<16 = -80h | -81h)
		                                       @    but we later convert to unsigned by adding 80h, so it works out
#endif
.endm

.LMixdown_NoReverb_Beg:
#if SGE_USE_VOLSUBDIV
	ADD	r7, r7, #0x10<<16                      @ Set first bias offset
#endif
	MOV	r9, #0x7F                              @ ClipMask -> r9

.LMixdown_ContinueMixdown:
#if !SGE_USE_VOLSUBDIV
	LDR	r8, [sp, #0x10]                        @ Bias -> r8
	MOV	sl, r7
#else
	LDR	r8, [sp, r7, lsr #0x10]                @ Load the next bias
	MOV	sl, r7, lsl #0x10
	MOV	sl, sl, lsr #0x10+SGE_VOLSUBDIV_LOG2MAXSUBDIV
#endif
#if !SGE_USE_OVERSAMPLING
	ADR	lr, 0f
0:	TST	r4, #0x03                              @ Handle single samples until aligned
	BNE	.LMixdown_NoReverb_Single
#endif

.LMixdown_NoReverb_Block:
1:	SUBS	sl, sl, #0x04                          @ nRem = N-4?
	BCC	12f
11:	LDMIA	r6!, {r0-r3}                           @ Load samples
	MakeLR	r0
	MakeLR	r1
	MakeLR	r2
	MakeLR	r3
#if SGE_USE_OVERSAMPLING
	STMDB	r6, {r0-r3}
#else
	NoReverb_MakeBlk fp, ip, r0, 0, 0              @ BlkDataL -> fp
	NoReverb_MakeBlk fp, ip, r1, 0, 1
	NoReverb_MakeBlk fp, ip, r2, 0, 2
	NoReverb_MakeBlk fp, ip, r3, 0, 3
	STR	fp, [r4], #0x04
	NoReverb_MakeBlk fp, ip, r0, 1, 0              @ BlkDataR -> fp
	NoReverb_MakeBlk fp, ip, r1, 1, 1
	NoReverb_MakeBlk fp, ip, r2, 1, 2
	NoReverb_MakeBlk fp, ip, r3, 1, 3
	STR	fp, [r5], #0x04
#endif
	SUBS	sl, sl, #0x04                          @ nRem -= 4?
	BCS	11b
12:	ADDS	sl, sl, #0x04                          @ Handle remaining samples
	BEQ	.LMixdown_NoReverb_Exit
2:	ADR	lr, .LMixdown_NoReverb_Single

.LMixdown_NoReverb_Single:
	LDR	r0, [r6], #0x04
	MakeLR	r0
#if SGE_USE_OVERSAMPLING
	STR	r0, [r6, #-0x04]
#else
	NoReverb_MakeSmp fp, r0, 0
	STRB	fp, [r4], #0x01
	NoReverb_MakeSmp fp, r0, 1
	STRB	fp, [r5], #0x01
#endif
	SUBS	sl, sl, #0x01
	BXNE	lr

.LMixdown_NoReverb_Exit:
#if SGE_USE_VOLSUBDIV
	ADD	r7, r7, #0x04<<16                      @ Move to next bias
	CMP	r7, #(0x10+0x04*(1<<SGE_VOLSUBDIV_LOG2MAXSUBDIV))<<16
	BCC	.LMixdown_ContinueMixdown
	SUB	r7, r7, #(0x10+0x04*(1<<SGE_VOLSUBDIV_LOG2MAXSUBDIV))<<16
#endif
# if 0
	B	.LMixer_Mixdown_Return
# else
	B	.LMixer_Mixdown_Return - .LMixerCore_WorkArea + .LMixdown_NoReverb_Beg
# endif

.LMixdown_NoReverb_End:

.equ SGE_Driver_Mixdown_NoReverb_Size, (.LMixdown_NoReverb_End - .LMixdown_NoReverb_Beg)
.if SGE_Driver_Mixdown_NoReverb_Size > SGE_Driver_MixerCore_WorkAreaSize
	.equ SGE_Driver_MixerCore_WorkAreaSize, SGE_Driver_Mixdown_NoReverb_Size
.endif

/************************************************/
//! EOF
/************************************************/
