/************************************************/
ASM_MODE_ARM
/************************************************/

#define ALLPASS_BITS 14 //! 14 should be the highest safe value, 15 and 16 can overflow

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

/************************************************/

@ r0:  Fb
@ r1: (1.0-Lp) (on ARM9, this is shifted up by 8 bits for SMLAWx)
@ r2:  nTapLines | -nRem<<8 | (nTapLinesRem-1)<<24
@ r3: &ReverbData
@ r4: &DstL/&WetFbSrcR (s16[N])
@ r5: &DstR/&WetFbSrcL (s16[N])
@ r6: &Dry (s32[N][L/R])
@ Simple reverb:
@  r7:  WetL
@  r8: &Line.Buf/Temp
@  r9:  Line.Len|Idx<<16
@  sl:  zLpL
@  fp:  zLpR
@  ip:  ClipMask(=7FFFh)
@  lr:  WetR
@ Fancy reverb:
@  r7:  WetL
@  r8: [Temp]
@  r9: &Line.Buf
@  sl:  Line.Len|Idx<<16
@  fp:  Line.zLp
@  ip:  ClipMask(=7FFFh)
@  lr:  WetR
@ NOTE: Everything is pretty much the same as the GBA version.
@ NOTE: With oversampling, we run out of registers and end up with:
@  r0:  Fb | (1.0-Lp)<<8
@  r1: &Dst

.macro ProcessFilterLine Wet, Chan
#ifdef SGE_PLATFORM_HAVE_FANCY_REVERB
	/************************************************/
	//! Fancy reverb
	/************************************************/
	LDMIA	r3!, {r9-fp}                       @ Line[i][Left].{Buf,Len|Idx,zLp} -> r9,sl,fp
	SUBS	sl, sl, #0x01<<16                  @ --Line.Idx?
	SUB	\Wet, \Wet, fp                     @ Wet = zLp = zLp + (Wet-zLp)*(1.0-Lp) -> fp
# if (!defined(__NDS__) || __NDS__ != 9)
#  if !SGE_USE_OVERSAMPLING
	MUL	r8, \Wet, r1
#  else
	MOV	r8, r0, asr #0x08
	MUL	r8, \Wet, r8
#  endif
	ADDCC	sl, sl, sl, lsl #0x10              @  End: Wrap around (Line.Idx += Line.Len)
	ADD	fp, fp, r8, asr #0x08
# else
#  if !SGE_USE_OVERSAMPLING
	SMLAWB	fp, r1, \Wet, fp
#  else
	SMLAWB	fp, r0, \Wet, fp
#  endif
	ADDCC	sl, sl, sl, lsl #0x10              @  End: Wrap around (Line.Idx += Line.Len)
# endif
	ADD	\Wet, r9, sl, lsr #0x10-1          @ &Tap = &Buf[Idx] -> Wet
	LDRSH	r8, [\Wet]                         @ Tap -> r8
	STMDB	r3, {sl,fp}                        @ Store Line.{Len|Idx,zLp}
.if \Chan == 0
	SUB	fp, fp, r8, asr #0x01              @ Wet = Wet - Tap*0.5 (all-pass filter)
.else
	ADD	fp, fp, r8, asr #0x01
.endif
	STRH	fp, [\Wet]                         @ Buf[Idx] = Wet
.if \Chan == 0
	ADD	\Wet, r8, fp, asr #0x01            @ Wet = Tap + Wet*0.5
.else
	SUB	\Wet, r8, fp, asr #0x01
.endif
#else
	/************************************************/
	//! Simple reverb
	/************************************************/
	LDMIA	r3!, {r8-r9}                       @ Line[i][Left].{Buf,Len|Idx} -> r8,r9
	SUBS	r9, r9, #0x01<<16                  @ --Line.Idx?
	ADDCC	r9, r9, r9, lsl #0x10              @  End: Wrap around (Line.Idx += Line.Len)
	STR	r9, [r3, #-0x04]
	ADD	r8, r8, r9, lsr #0x10-1            @ &Tap = &Buf[Idx] -> sl
	LDRSH	r9, [r8]                           @ Tap -> r8
.if \Chan == 0
	SUB	\Wet, \Wet, r9, asr #0x01          @ Wet = Wet - Tap*0.5 (all-pass filter)
.else
	ADD	\Wet, \Wet, r9, asr #0x01
.endif
	STRH	\Wet, [r8]                         @ Buf[Idx] = Wet
.if \Chan == 0
	ADD	\Wet, r9, \Wet, asr #0x01          @ Wet = Tap + Wet*0.5
.else
	SUB	\Wet, r9, \Wet, asr #0x01
.endif
#endif
.endm

.LMixdown_WithReverb:
#ifdef SGE_PLATFORM_HAVE_FANCY_REVERB
	LDR	r8, [r3], #0x04                    @ {Fb,Lp,nTapLines} -> r0, &TapLines[] -> r3
#else
	LDMIA	r3!, {r8,sl,fp}                    @ As above, but also zLpL -> sl, zLpR -> fp
#endif
	MOV	lr, #0xFF
	ORR	ip, lr, lr, lsl #0x07              @ ClipMask -> ip
	AND	r0, lr, r8, lsr #0x00              @ Fb -> r0
	AND	r1, lr, r8, lsr #0x08              @ (1.0-Lp) -> r1
	RSB	r1, r1, #0x0100
	AND	lr, lr, r8, lsr #0x10              @ nTapLines -> lr
	SUB	r2, lr, r7, lsl #0x08              @ nTapLines | -N<<8 -> r2
#ifdef SGE_PLATFORM_HAVE_FANCY_REVERB
	ADD	r8, lr, lr, lsl #0x01              @ Modify "SUB r3, r3, #IMM" instruction for nTapLines
	MOV	r8, r8, lsl #0x02+1
#else
	MOV	r8, lr, lsl #0x03+1
#endif
	STRB	r8, 3f
#if SGE_USE_OVERSAMPLING
	STMFD	sp!, {r4,r5,r7}
	ADD	r4, r4, #0x02
	ADD	r5, r5, #0x02
	ORR	r0, r0, r1, lsl #0x08
	MOV	r1, r6
#else
# if (__NDS__ == 9)
	MOV	r1, r1, lsl #0x08                  @ <- Shift up so we can use SMLAWx
# endif
#endif

.LMixdown_WithReverb_Loop:
1:
#if SGE_USE_OVERSAMPLING
	LDRSH	r7, [r5], #0x04                    @ DelayL = WetFbSrcR[i] -> r7
	LDRSH	lr, [r4], #0x04                    @ DelayR = WetFbSrcL[i] -> lr
#else
	LDRSH	r7, [r5]
	LDRSH	lr, [r4]
#endif
#ifdef SGE_PLATFORM_HAVE_FANCY_REVERB
# if (ALLPASS_BITS < 16)
	MOV	r7, r7, asr #0x10-ALLPASS_BITS
	SUB	r7, r7, r7, asr #0x1F
	MOV	lr, lr, asr #0x10-ALLPASS_BITS
	SUB	lr, lr, lr, asr #0x1F
# endif
#else
# if (__NDS__ == 7)
#  if !SGE_USE_OVERSAMPLING
	RSBS	r8, sl, r7, asr #0x10-ALLPASS_BITS @ Apply lowpass filter
	MULNE	r7, r8, r1
	RSBS	r8, fp, lr, asr #0x10-ALLPASS_BITS
	MULNE	lr, r8, r1
#  else
	MOV	r8, r0, asr #0x08
	RSBS	r7, sl, r7, asr #0x10-ALLPASS_BITS
	MULNE	r7, r7, r8
	RSBS	lr, fp, lr, asr #0x10-ALLPASS_BITS
	MULNE	lr, lr, r8
#  endif
	ADD	sl, sl, r7, asr #0x08
	ADD	fp, fp, lr, asr #0x08
# else
	RSB	r7, sl, r7, asr #0x10-ALLPASS_BITS
	RSB	lr, fp, lr, asr #0x10-ALLPASS_BITS
#  if !SGE_USE_OVERSAMPLING
	SMLAWB	sl, r1, r7, sl
	SMLAWB	fp, r1, lr, fp
#  else
	SMLAWB	sl, r0, r7, sl
	SMLAWB	fp, r0, lr, fp
#  endif
# endif
	SUB	r7, sl, sl, asr #0x1F              @ Wet = Delay -> r7,lr (plus rounding to avoid infinite noise)
	SUB	lr, fp, fp, asr #0x1F
#endif
	ADD	r2, r2, r2, lsl #0x18              @ nTapLines | -N<<8 | (nTapLinesRem-1)<<24 -> r2
10:	ProcessFilterLine r7, 0
	ProcessFilterLine lr, 1
	SUBS	r2, r2, #0x01<<24                  @ --nTapLinesRem?
	BCS	10b
11:
#if !SGE_USE_OVERSAMPLING
	MUL	r8, r7, r0                         @ Wet *= Fb (.8fxp)
	MUL	r9, lr, r0
#else
	AND	r9, r0, #0xFF
	MUL	r8, r7, r9
	MUL	r9, lr, r9
#endif
	LDMIA	r6!, {r7,lr}                       @ DryL -> r7, DryR -> lr
	MOV	r8, r8, asr #(ALLPASS_BITS+8) - 16
	MOV	r9, r9, asr #(ALLPASS_BITS+8) - 16
	ADD	r8, r8, r7, asr #SGE_MIXER_VOLBITS
	ADD	r9, r9, lr, asr #SGE_MIXER_VOLBITS
#if SGE_USE_CLIPPING
	FastClip r8, ip
	FastClip r9, ip
#endif
#if SGE_USE_OVERSAMPLING
	ADD	r8, r8, r9, lsl #0x10              @ <- This technically can overflow (eg. -80h + -80h<<16 = -80h | -81h)
	STR	r8, [r1], #0x04                    @    but we later convert to unsigned by adding 8000h, so it works out
#else
	STRH	r8, [r4], #0x02
	STRH	r9, [r5], #0x02
#endif
3:	SUB	r3, r3, #0x00                      @ Rewind lines (NOTE: Self-modifying)
	ADDS	r2, r2, #0x01<<8                   @ --nRem?
	BCC	1b
#ifndef SGE_PLATFORM_HAVE_FANCY_REVERB
	STMDB	r3, {sl,fp}                        @ Store zLp
#endif
#if SGE_USE_OVERSAMPLING
	LDMFD	sp!, {r4,r5,r7}
#endif

.LMixdown_WithReverb_Exit:
	B	.LMixer_Mixdown_Return

/************************************************/
//! EOF
/************************************************/
