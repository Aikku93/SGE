/************************************************/
ASM_MODE_ARM
/************************************************/

@ Rate < 1.0 and Rate >= 1.0 have the same register allocations:
@  r0:  2*(YzM1-YzM2)
@  r1:  YzM2
@  r2:  C1
@  r3:  C2
@  r4: &Src
@  r5:  BiasPCM
@  r6:  VolL | VolR<<16
@  r7:  Position [16.16fxp]
@  r8:  Out(=(YzM2+BiasPCM)*Vol)
@  r9: &Dst
@  sl:  Rate [16.16fxp]
@  fp:  ShiftMask | nRem<<16
@  ip: [Temp]
@  lr:  FrameData
@  sp+00h: &ReturnAddr

/************************************************/
//! Rate < 1.0
/************************************************/

.LMixerCore_ADPCM_RateLT_Lerp_Beg:
.LMixerCore_ADPCM_Mono_RateLT_Lerp_Beg:
.LMixerCore_ADPCM_Stereo_RateLT_Lerp_Beg:
	STRH	ip, 999f                  @ Store shifter
	MOVS	r7, r7, lsl #0x10         @ Shift Position to upper 16 bits [C=0]
0:	MOVCS	lr, lr, lsl #0x04         @ Next nybble
1:	SUBS	fp, fp, #0x01<<16         @ --nRem?
	BCC	2f
10:	LDR	ip, [r9]                  @ Mix
	MOVS	r8, r7, lsr #0x20-8
	MULNE	r8, r0, r8
	ADD	r8, r1, r8, asr #0x08+1
	ADDS	r8, r5, r8, asr #0x08
	MULNE	r8, r6, r8
#if MIX_CLEAR_MASK
	BIC	r8, r8, #MIX_CLEAR_MASK<<16
#endif
	ADDS	r7, r7, sl, lsl #0x10     @ Position += Rate?
	ADD	ip, ip, r8, lsr #MIX_SHIFT_DOWN
	STR	ip, [r9], #0x04
	BCC	1b
11:	MUL	ip, r1, r3                @ Decode next sample
	ADDS	r1, r1, r0, asr #0x01
	MLANE	ip, r1, r2, ip
	MOV	r0, lr, asr #0x1C
	MOV	ip, ip, asr #0x06-1
999:	ADD	r0, ip, r0, lsl #0x00     @ <- Self-modifying
	BICS	ip, lr, #0xFF000000       @ More samples in this frame? [C=1]
	BNE	0b
.LMixerCore_ADPCM_RateLT_Lerp_ReadFrameOpcode:
	LDR	lr, [r4, #0x04]!          @ Load next frame
	SUBS	fp, fp, #0x01<<16         @ --nRem?
	ANDCS	ip, fp, lr, lsl #0x07     @  Update shifter
	STRCSH	ip, 999b
	BCS	10b
2:	MOV	r7, r7, lsr #0x10         @ Restore Position|Phase
	ADDS	r0, r1, r0, asr #0x01     @ (YzM1-YzM2)+YzM2 = YzM1 -> r0
	ANDMI	r0, r0, r0, lsr #0x10     @ [mask negative to 16bit]
	ORR	r0, r0, r1, lsl #0x10     @ YzM1|YzM2<<16
	LDR	pc, [sp], #0x04

.LMixerCore_ADPCM_RateLT_Lerp_End:
.LMixerCore_ADPCM_Mono_RateLT_Lerp_End:
.LMixerCore_ADPCM_Stereo_RateLT_Lerp_End:

.equ SGE_Driver_MixerCore_ADPCM_RateLT_Lerp_Size, \
	(.LMixerCore_ADPCM_InitEnd         - .LMixerCore_ADPCM_InitBeg) + \
	(.LMixerCore_ADPCM_RateLT_Lerp_End - .LMixerCore_ADPCM_RateLT_Lerp_Beg)
.if SGE_Driver_MixerCore_ADPCM_RateLT_Lerp_Size > SGE_Driver_MixerCore_WorkAreaSize
	.equ SGE_Driver_MixerCore_WorkAreaSize, SGE_Driver_MixerCore_ADPCM_RateLT_Lerp_Size
.endif

/************************************************/
//! Rate >= 1.0
/************************************************/

.LMixerCore_ADPCM_RateGT_Lerp_Beg:
.LMixerCore_ADPCM_Mono_RateGT_Lerp_Beg:
.LMixerCore_ADPCM_Stereo_RateGT_Lerp_Beg:
	STRH	ip, 999f                  @ Store shifter
	SUB	sl, sl, #0x01<<16         @ Rate -= 1.0
	SUB	fp, fp, #0x01<<16         @ --nRem (we use a n>=0 loop)
1:	LDR	ip, [r9]                  @ Mix
	MOVS	r8, r7, lsr #0x08         @ <- We either have a 16bit Rn (3c MUL) or MOV + 8bit Rn (3c all up).
	MULNE	r8, r0, r8                @    With the latter, we can also set flags, so we use that.
	ADD	r8, r1, r8, asr #0x08+1
	ADDS	r8, r5, r8, asr #0x08
	MULNE	r8, r6, r8
	ADD	r7, r7, sl                @ Position += Rate
#if MIX_CLEAR_MASK
	BIC	r8, r8, #MIX_CLEAR_MASK<<16
#endif
	ADD	ip, ip, r8, lsr #MIX_SHIFT_DOWN
	STR	ip, [r9], #0x04
2:	MUL	ip, r1, r3                @ Decode next sample
	ADDS	r1, r1, r0, asr #0x01
	MLANE	ip, r1, r2, ip
	MOV	r0, lr, asr #0x1C
	MOV	ip, ip, asr #0x06-1
999:	ADD	r0, ip, r0, lsl #0x00     @ <- Self-modifying
	BICS	ip, lr, #0xFF000000       @ More samples in this frame?
	BEQ	21f
3:	MOV	lr, lr, lsl #0x04         @ Next nybble
	SUBS	r7, r7, #0x01<<16         @ More samples to decode?
	BCS	2b
31:	ADD	r7, r7, #0x01<<16
	SUBS	fp, fp, #0x01<<16         @ --nRem?
	BCS	1b
32:	B	4f
.LMixerCore_ADPCM_RateGT_Lerp_ReadFrameOpcode:
21:	LDR	lr, [r4, #0x04]!          @ Load next frame
	SUBS	r7, r7, #0x01<<16         @ More samples to decode?
	AND	ip, fp, lr, lsl #0x07     @ Update shifter
	STRH	ip, 999b
	BCS	2b
22:	ADD	r7, r7, #0x01<<16
	SUBS	fp, fp, #0x01<<16         @ --nRem?
	BCS	1b
4:	ADD	sl, sl, #0x01<<16         @ Restore Rate
	ADDS	r0, r1, r0, asr #0x01     @ (YzM1-YzM2)+YzM2 = YzM1 -> r0
	ANDMI	r0, r0, r0, lsr #0x10     @ [mask negative to 16bit]
	ORR	r0, r0, r1, lsl #0x10     @ YzM1|YzM2<<16
	LDR	pc, [sp], #0x04

.LMixerCore_ADPCM_RateGT_Lerp_End:
.LMixerCore_ADPCM_Mono_RateGT_Lerp_End:
.LMixerCore_ADPCM_Stereo_RateGT_Lerp_End:

.equ SGE_Driver_MixerCore_ADPCM_RateGT_Lerp_Size, \
	(.LMixerCore_ADPCM_InitEnd         - .LMixerCore_ADPCM_InitBeg) + \
	(.LMixerCore_ADPCM_RateGT_Lerp_End - .LMixerCore_ADPCM_RateGT_Lerp_Beg)
.if SGE_Driver_MixerCore_ADPCM_RateGT_Lerp_Size > SGE_Driver_MixerCore_WorkAreaSize
	.equ SGE_Driver_MixerCore_WorkAreaSize, SGE_Driver_MixerCore_ADPCM_RateGT_Lerp_Size
.endif

/************************************************/
//! EOF
/************************************************/
