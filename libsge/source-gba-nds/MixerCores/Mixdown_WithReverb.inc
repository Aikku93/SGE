/************************************************/
ASM_MODE_ARM
/************************************************/

@ Input:
@  r0:
@  r1:
@  r2:
@  r3: &ReverbData
@  r4: &DstL
@  r5: &DstR
@  r6: &MixBuf
@  r7:  N
@  sp+08h: &Driver
@  sp+10h:  BiasL|BiasR<<16

/************************************************/

// We modify the offset of the LDR instruction to load the bias level
#if SGE_USE_OVERSAMPLING
# define MIXDOWN_BIAS_OFFSET (0x08*SGE_USE_VOLSUBDIV + 0x0C + 0x10)
#else
# define MIXDOWN_BIAS_OFFSET (0x08*SGE_USE_VOLSUBDIV + 0x10)
#endif

/************************************************/

@ r0:  Fb
@ r1: (1.0-Lp)
@ r2:  nTapLines | -nRem<<8 | (nTapLinesRem-1)<<24
@ r3: &ReverbData
@ r4: &DstL/&WetFbSrcR (s8[N])
@ r5: &DstR/&WetFbSrcL (s8[N])
@ r6: &Dry (u16[N][L/R])
@ Simple reverb:
@  r7:  WetL
@  r8: &Line.Buf/Temp
@  r9:  Line.Len|Idx<<16
@  sl:  zLpL
@  fp:  zLpR
@  ip:  ClipMask(=7Fh)
@  lr:  WetR
@ Fancy reverb:
@  r7:  WetL
@  r8: [Temp]
@  r9: &Line.Buf
@  sl:  Line.Len|Idx<<16
@  fp:  Line.zLp
@  ip:  ClipMask(=7Fh)
@  lr:  WetR

@ Original equations:
@  u[t] = x[t] - w*0.5u[t-N] | w=1 for left channel, w=-1 for right channel
@  y[t] = w*0.5u[t] + u[t-N]
@ Things were re-arranged so that x[t] is scaled by 0.5 prior to entering
@ the filter itself, and is compensated-for in the final output:
@  u[t] = 0.5x[t] - w*0.5u[t-N]
@  y[t] = w*u[t] + 2*u[t-N]
@ The purpose of this is to reduce overflow in the delay lines, at the cost
@ of one bit less of bit-depth in the delay line buffers.
.macro ProcessFilterLine Wet, Chan
#ifdef SGE_PLATFORM_HAVE_FANCY_REVERB
	/************************************************/
	//! Fancy reverb
	/************************************************/
	LDMIA	r3!, {r9-fp}                       @ Line[i][Left].{Buf,Len|Idx,zLp} -> r9,sl,fp
	SUBS	sl, sl, #0x01<<16                  @ --Line.Idx?
# if (SGE_ALLPASS_BITS != 7)
	SUB	\Wet, \Wet, fp                     @ Wet = zLp = zLp + (Wet-zLp)*(1.0-Lp) -> fp
# else
	RSB	\Wet, fp, \Wet, asr #0x01          @ <- As above, but pre-scale x[n] by 0.5
# endif
	MUL	r8, \Wet, r1
	ADDCC	sl, sl, sl, lsl #0x10              @  End: Wrap around (Line.Idx += Line.Len)
	ADD	fp, fp, r8, asr #0x08
# if (SGE_ALLPASS_BITS > 8)
	ADD	\Wet, r9, sl, lsr #0x10-1          @ &Tap = &Buf[Idx] -> Wet
	LDRSH	r8, [\Wet]                         @ Tap -> r8
# else
	ADD	\Wet, r9, sl, lsr #0x10
	LDRSB	r8, [\Wet]
# endif
	STMDB	r3, {sl,fp}                        @ Store Line.{Len|Idx,zLp}
.if \Chan == 0
	SUB	fp, fp, r8, asr #0x01              @ Wet = Wet - Tap*0.5 (all-pass filter)
.else
	ADD	fp, fp, r8, asr #0x01
.endif
# if (SGE_ALLPASS_BITS > 8)
	STRH	fp, [\Wet]                         @ Buf[Idx] = Wet
# else
#  if SGE_CLIP_ALLPASS
	FastClip fp, ip                            @ <- This output is always bounded as an 8+8 bit addition, so fast clipping is fine
#  endif
	STRB	fp, [\Wet]                         @ Buf[Idx] = Wet
# endif
# if (SGE_ALLPASS_BITS != 7)
.if \Chan == 0
	ADD	\Wet, r8, fp, asr #0x01            @ Wet = Tap + Wet*0.5
.else
	SUB	\Wet, r8, fp, asr #0x01
.endif
# else
.if \Chan == 0
	ADD	\Wet, fp, r8, lsl #0x01            @ Wet = Tap + Wet*0.5 (Tap and Wet are already scaled by 0.5, so account for that)
.else
	RSB	\Wet, fp, r8, lsl #0x01
.endif
# endif
#else
	/************************************************/
	//! Simple reverb
	/************************************************/
	LDMIA	r3!, {r8-r9}                       @ Line[i][Left].{Buf,Len|Idx} -> r8,r9
	SUBS	r9, r9, #0x01<<16                  @ --Line.Idx?
	ADDCC	r9, r9, r9, lsl #0x10              @  End: Wrap around (Line.Idx += Line.Len)
	STR	r9, [r3, #-0x04]
# if (SGE_ALLPASS_BITS > 8)
	ADD	r8, r8, r9, lsr #0x10-1            @ &Tap = &Buf[Idx] -> sl
	LDRSH	r9, [r8]                           @ Tap -> r8
# else
	ADD	r8, r8, r9, lsr #0x10
	LDRSB	r9, [r8]
# endif
.if \Chan == 0
	SUB	\Wet, \Wet, r9, asr #0x01          @ Wet = Wet - Tap*0.5 (all-pass filter)
.else
	ADD	\Wet, \Wet, r9, asr #0x01
.endif
# if (SGE_ALLPASS_BITS > 8)
	STRH	\Wet, [r8]                         @ Buf[Idx] = Wet
# else
#  if SGE_CLIP_ALLPASS
	FastClip \Wet, ip                          @ <- This output is always bounded as an 8+8 bit addition, so fast clipping is fine
#  endif
	STRB	\Wet, [r8]
# endif
.if \Chan == 0
	ADD	\Wet, r9, \Wet, asr #0x01          @ Wet = Tap + Wet*0.5
.else
	SUB	\Wet, r9, \Wet, asr #0x01
.endif
#endif
.endm

.LMixdown_WithReverb_Beg:
#ifdef SGE_PLATFORM_HAVE_FANCY_REVERB
	LDR	r8, [r3], #0x04                    @ {Fb,Lp,nTapLines} -> r0, &TapLines[] -> r3
#else
	LDMIA	r3!, {r8,sl,fp}                    @ As above, but also zLpL -> sl, zLpR -> fp
#endif
#if SGE_USE_OVERSAMPLING
	STMFD	sp!, {r4,r5,r7}
	ADD	r4, r4, #0x01
	ADD	r5, r5, #0x01
#endif
	MOV	lr, #0xFF
	MOV	ip, #0x7F                          @ ClipMask -> ip
	AND	r0, lr, r8, lsr #0x00              @ Fb -> r0
	AND	r1, lr, r8, lsr #0x08              @ (1.0-Lp) -> r1
	RSB	r1, r1, #0x0100
	AND	lr, lr, r8, lsr #0x10              @ nTapLines -> lr
#if !SGE_USE_VOLSUBDIV
	SUB	r2, lr, r7, lsl #0x08              @ nTapLines | -N<<8 -> r2
#else
	SUB	r2, lr, r7, lsl #0x08-SGE_VOLSUBDIV_LOG2MAXSUBDIV
#endif
#ifdef SGE_PLATFORM_HAVE_FANCY_REVERB
	ADD	r8, lr, lr, lsl #0x01              @ Modify "SUB r3, r3, #IMM" instruction for nTapLines
	MOV	r8, r8, lsl #0x02+1
#else
	MOV	r8, lr, lsl #0x03+1
#endif
	STRB	r8, 3f
#if SGE_USE_VOLSUBDIV
	MOV	lr, #0xE000                        @ Exxxh = Low 16bits of `LDR lr, [sp, #...]` -> lr
	ORR	lr, lr, #MIXDOWN_BIAS_OFFSET
	STMFD	sp!, {r7,lr}
	STRH	lr, 999f
#endif

.LMixdown_WithReverb_Loop:
1:
#if SGE_USE_OVERSAMPLING
	LDRSB	r7, [r5], #0x02                    @ DelayL = WetFbSrcR[i] -> r7
	LDRSB	lr, [r4], #0x02                    @ DelayR = WetFbSrcL[i] -> lr
#else
	LDRSB	r7, [r5]
	LDRSB	lr, [r4]
#endif
#ifdef SGE_PLATFORM_HAVE_FANCY_REVERB
# if (SGE_ALLPASS_BITS > 8)
	MOV	r7, r7, lsl #SGE_ALLPASS_BITS-8
	MOV	lr, lr, lsl #SGE_ALLPASS_BITS-8
# endif
#else
	RSBS	r8, sl, r7, lsl #0x08              @ Apply lowpass filter
	MULNE	r7, r8, r1
	RSBS	r8, fp, lr, lsl #0x08              @ NOTE: The filter works with 16bit input samples
	MULNE	lr, r8, r1
	ADD	sl, sl, r7, asr #0x08
	ADD	fp, fp, lr, asr #0x08
	MOV	r7, sl, asr #0x10-SGE_ALLPASS_BITS @ Wet = Delay -> r7,lr
	MOV	lr, fp, asr #0x10-SGE_ALLPASS_BITS
#endif
	ADD	r2, r2, r2, lsl #0x18              @ nTapLines | -N<<8 | (nTapLinesRem-1)<<24 -> r2
10:	ProcessFilterLine r7, 0
	ProcessFilterLine lr, 1
	SUBS	r2, r2, #0x01<<24                  @ --nTapLinesRem?
	BCS	10b
11:	MUL	r8, r7, r0                         @ Wet *= Fb (.8fxp)
	MUL	r9, lr, r0
	LDR	r7, [r6], #0x04                    @ DryL|DryR<<16 -> r7
999:	LDR	lr, [sp, #MIXDOWN_BIAS_OFFSET]     @ BiasL|BiasR<<16 -> lr
#if (defined(SGE_PLATFORM_HAVE_FANCY_REVERB) && SGE_ALLPASS_BITS > 8)
	ADD	r8, r8, #0x01<<(SGE_ALLPASS_BITS-1)     @ <- Need to round to avoid infinite noise
	ADD	r9, r9, #0x01<<(SGE_ALLPASS_BITS-1)
#endif
	SUB	r7, r7, lr, lsl #SGE_MIXER_VOLFRACBITS + (7-SGE_MIXER_VOLBITS) @ Dry -= Bias
	MOV	lr, r7, asr #0x10+SGE_MIXER_VOLFRACBITS @ DryR -> lr
	MOV	r7, r7, lsl #0x10
	MOV	r7, r7, asr #0x10+SGE_MIXER_VOLFRACBITS @ DryL -> r7
#if (defined(SGE_PLATFORM_HAVE_FANCY_REVERB) && SGE_ALLPASS_BITS == 7)
	ADD	r8, r7, r8, asr #SGE_ALLPASS_BITS+1     @ Wet = Clip(Dry+Wet)
	ADD	r9, lr, r9, asr #SGE_ALLPASS_BITS+1     @ NOTE: ALLPASS_BITS == 7 gets processed as though it was ALLPASS_BITS == 8
#else
	ADD	r8, r7, r8, asr #SGE_ALLPASS_BITS
	ADD	r9, lr, r9, asr #SGE_ALLPASS_BITS
#endif
#if SGE_USE_CLIPPING
	FastClip r8, ip
	FastClip r9, ip
#endif
#if SGE_USE_OVERSAMPLING
	ADD	r8, r8, r9, lsl #0x10              @ <- This technically can overflow (eg. -80h + -80h<<16 = -80h | -81h)
	STR	r8, [r6, #-0x04]                   @    but we later convert to unsigned by adding 80h, so it works out
#else
	STRB	r8, [r4], #0x01
	STRB	r9, [r5], #0x01
#endif
3:	SUB	r3, r3, #0x00                      @ Rewind lines (NOTE: Self-modifying)
	ADDS	r2, r2, #0x01<<8                   @ --nRem?
	BCC	1b
#if SGE_USE_VOLSUBDIV
31:	LDMFD	sp, {r7,lr}
	SUB	r2, r2, r7, lsl #0x08-SGE_VOLSUBDIV_LOG2MAXSUBDIV
	ADDS	lr, lr, #(1<<(32-SGE_VOLSUBDIV_LOG2MAXSUBDIV)) + 0x04
	STRCC	lr, [sp, #0x04]                    @ Need to do another subdivision?
	STRCCH	lr, 999b
	BCC	1b
	ADD	sp, sp, #0x08
#endif
#ifndef SGE_PLATFORM_HAVE_FANCY_REVERB
	STMDB	r3, {sl,fp}                        @ Store zLp
#endif
#if SGE_USE_OVERSAMPLING
	LDMFD	sp!, {r4,r5,r7}
#endif

.LMixdown_WithReverb_Exit:
# if 0
	B	.LMixer_Mixdown_Return
# else
	B	.LMixer_Mixdown_Return - .LMixerCore_WorkArea + .LMixdown_WithReverb_Beg
# endif

.LMixdown_WithReverb_End:

.equ SGE_Driver_Mixdown_WithReverb_Size, (.LMixdown_WithReverb_End - .LMixdown_WithReverb_Beg)
.if SGE_Driver_Mixdown_WithReverb_Size > SGE_Driver_MixerCore_WorkAreaSize
	.equ SGE_Driver_MixerCore_WorkAreaSize, SGE_Driver_Mixdown_WithReverb_Size
.endif

/************************************************/
//! EOF
/************************************************/
