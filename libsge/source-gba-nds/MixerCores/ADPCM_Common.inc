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
@  r7:  Position [16.16fxp]
@  r8: &Wav
@  r9: &Dst
@  sl:  Rate [16.16fxp]
@  fp:
@  ip:
@  lr:

/************************************************/
//! Initialization
/************************************************/

#define RELOC_TARGET(Target) ( \
	(Target - .LMixerCore_WorkArea + .LMixerCore_ADPCM_InitBeg) - \
)

.LMixerCore_ADPCM_InitBeg:
.LMixerCore_ADPCM_Mono_RateLT_NoLerp_InitBeg:
.LMixerCore_ADPCM_Mono_RateGT_NoLerp_InitBeg:
.LMixerCore_ADPCM_Mono_RateLT_Lerp_InitBeg:
.LMixerCore_ADPCM_Mono_RateGT_Lerp_InitBeg:
.LMixerCore_ADPCM_Stereo_RateLT_NoLerp_InitBeg:
.LMixerCore_ADPCM_Stereo_RateGT_NoLerp_InitBeg:
.LMixerCore_ADPCM_Stereo_RateLT_Lerp_InitBeg:
.LMixerCore_ADPCM_Stereo_RateGT_Lerp_InitBeg:
	LDR	ip, [sp, #0x10]             @ Vox -> ip
	MOV	fp, #0x0F<<7                @ ShiftMask | nRem<<16 -> fp
	ORR	fp, fp, r0, lsl #0x10
#if SGE_STEREO_WAVEFORMS
	LDR	r1, RELOC_TARGET(.LMixerCore_MixFormat) @ RateGT | Stereo<<1 | Interpolate<<2
	MOVS	r0, r1, lsr #0x01           @ C=RateGT, r0.Bit0 = Stereo
# if SGE_SUPPORT_LERP
	ANDS	r0, r0, #0x01
# endif
	MOVEQ	r0, #0x04                   @ Set stride (4 bytes per frame per channel)
	MOVNE	r0, #0x08
# if !SGE_SUPPORT_LERP
	STRCCB	r0, .LMixerCore_ADPCM_InitEnd + (.LMixerCore_ADPCM_RateLT_NoLerp_ReadFrameOpcode-.LMixerCore_ADPCM_RateLT_NoLerp_Beg)
	STRCSB	r0, .LMixerCore_ADPCM_InitEnd + (.LMixerCore_ADPCM_RateGT_NoLerp_ReadFrameOpcode-.LMixerCore_ADPCM_RateGT_NoLerp_Beg)
# elif SGE_ALWAYS_LERP
	STRCCB	r0, .LMixerCore_ADPCM_InitEnd + (.LMixerCore_ADPCM_RateLT_Lerp_ReadFrameOpcode-.LMixerCore_ADPCM_RateLT_Lerp_Beg)
	STRCSB	r0, .LMixerCore_ADPCM_InitEnd + (.LMixerCore_ADPCM_RateGT_Lerp_ReadFrameOpcode-.LMixerCore_ADPCM_RateGT_Lerp_Beg)
# else
	MOV	r2, r1, lsr #0x02           @ IsRateGT | Lerp<<1 -> r2
	ADC	r2, r2, r2
	ADR	r3, .LMixerCore_ADPCM_Init_FrameOpcodePtrs
	LDRB	r3, [r3, r2]
.LMixerCore_ADPCM_Init_FrameOpcodePtrsOffs:
	STRB	r0, [pc, r3, lsl #0x02]
# endif
	STR	r6, [sp, #-0x04]!           @ Store Vol (modified by stereo mixer)
	@TST	r1, #0x02                   @ Handle R channel first as needed
	BEQ	2f
1:	STMFD	sp!, {r4,r6-r9,fp}
	LDRSH	r2, [r8, #0x18+0x10*1+0x04] @ C1 -> r2
	LDRSH	r3, [r8, #0x18+0x10*1+0x06] @ C2 -> r3
	LDRSH	r0, [ip, #0x10+0x04*1]      @ YzM1 -> r0
	LDRSH	r1, [ip, #0x12+0x04*1]      @ YzM2 -> r1
	ADD	r4, r4, #0x04               @ Move to R channel
	AND	r6, r6, #0xFF<<16           @ Mask to R channel
	BL	.LMixerCore_ADPCM_Enter
	LDR	ip, [sp, #0x10+0x04*7]      @ Vox -> ip
	LDMFD	sp!, {r4,r6-r9,fp}
	STR	r0, [ip, #0x10+0x04*1]      @ Store Vox.ADPCM[1]
	AND	r6, r6, #0xFF               @ Mask to L channel
#endif
2:	LDRSH	r2, [r8, #0x18+0x10*0+0x04] @ C1 -> r2
	LDRSH	r3, [r8, #0x18+0x10*0+0x06] @ C2 -> r3
	LDRSH	r0, [ip, #0x10+0x04*0]      @ YzM1 -> r0
	LDRSH	r1, [ip, #0x12+0x04*0]      @ YzM2 -> r1
	BL	.LMixerCore_ADPCM_Enter
	LDR	ip, [sp, #0x10+0x04*1]      @ Vox -> ip
#if SGE_STEREO_WAVEFORMS
	LDR	r6, [sp], #0x04             @ Restore Vol
#endif
	STR	r0, [ip, #0x10+0x04*0]      @ Store Vox.ADPCM[0]
0:	MOVS	r2, lr, lsl #0x20-16        @ Set Position=AdPos by testing low bits of FrameData
	MOVEQ	lr, lr, lsr #0x10
	ADDEQ	r7, r7, #(16/4)<<16
	TST	lr, #0xFF
	MOVEQ	lr, lr, lsr #0x08
	ADDEQ	r7, r7, #(8/4)<<16
	TST	lr, #0x0F
	ADDEQ	r7, r7, #(4/4)<<16
	B	RELOC_TARGET(.LMixer_VoxLoop_MixLoop_MixChunk_Tail)

#if (SGE_STEREO_WAVEFORMS && SGE_SUPPORT_LERP && !SGE_ALWAYS_LERP)
//! NOTE: Add 08h to the offset because of pipelining
# define MAKE_OFFS(Mode) \
	.byte ( \
		.LMixerCore_ADPCM_InitEnd + \
		(.LMixerCore_ADPCM_##Mode##_ReadFrameOpcode - .LMixerCore_ADPCM_##Mode##_Beg) - \
		(.LMixerCore_ADPCM_Init_FrameOpcodePtrsOffs + 0x08) \
	) / 4
.LMixerCore_ADPCM_Init_FrameOpcodePtrs:
	MAKE_OFFS(RateLT_NoLerp)
	MAKE_OFFS(RateGT_NoLerp)
	MAKE_OFFS(RateLT_Lerp)
	MAKE_OFFS(RateGT_Lerp)
#endif

/************************************************/
//! Loop handling
//! This is sandwiched here to load it with all ADPCM mixers
//! and allows the main update routine to reference it
/************************************************/

@ r0:  MxCnt
@ r1:
@ r2:  GlobalPosition
@ r3:
@ r4: &Src
@ r5:  MxRem
@ r6:  VolL | VolR<<16
@ r7:  Position [16.16fxp]
@ r8: &Wav
@ r9: &Dst
@ sl:  Rate [16.16fxp]
@ fp:  Overstep
@ ip:  Wav.Frmt
@ lr:  Wav.Chan
@ sp+00h: &Vox

.LMixer_VoxLoop_MixLoop_HandleLoop_ADPCM:
	LDR	r1, [r8, #0x18+0x10*0+0x0C] @ LoopFrame -> r1 (loading from either channel will work)
	ADD	r4, r8, #0x18               @ Src = Wav.ADPCM.Data
	ADD	r4, r4, lr, lsl #0x04
	ADD	r4, r4, r1, lsl #0x02       @ Src += LoopFrame*sizeof(Frame)
#if SGE_STEREO_WAVEFORMS
	CMP	lr, #0x02
	ADDEQ	r4, r4, r1, lsl #0x02
#endif
	RSB	r1, r1, r1, lsl #0x03       @ nSkip = GlobalPosition - LoopFrame*7 -> r1?
	SUBS	r1, r2, r1                  @  nSkip == 0: Exact hit on loop frame - Store samples and exit
	BEQ	.LMixer_VoxLoop_MixLoop_HandleLoop_ADPCM_ExactLoopRestart
0:	ADD	r3, r1, r1, lsl #0x03       @ Position = nSkip%7 (we update Src manually)
	ADD	r3, r3, r3, lsl #0x06       @ x/7 ~= x*(1+2^3)(1+2^6)(1+2^-12)*2^-12 (valid for x=0..65535)
	ADD	r3, r3, r3, lsr #0x0C
	ADD	r3, r3, #0x0200             @ <- This rounding is needed to be exact, though
	MOV	r3, r3, lsr #0x0C
	RSB	r3, r3, r3, lsl #0x03
	RSB	r3, r3, r1
	ADD	r7, r7, r3, lsl #0x10
	MOV	fp, #0x0F<<7                @ ShiftMask | nRem<<16 -> fp
	ORR	fp, fp, r1, lsl #0x10
0:	STMFD	sp!, {r0,r7,r9,ip,lr}
	LDR	r9, [sp, #0x04*5]           @ Vox -> r9
#if SGE_STEREO_WAVEFORMS
	CMP	lr, #0x02                   @ Handle R channel first as needed
	MOV	lr, lr, lsl #0x02
	STRB	lr, .LMixer_VoxLoop_MixLoop_HandleLoop_ADPCM_SkipSamples_ReadFrame
	BCC	2f
1:	STMFD	sp!, {r4,fp}
	ADD	r4, r4, #0x04
	LDRSH	r2, [r8, #0x18+0x10*1+0x04] @ C1 -> r2
	LDRSH	r3, [r8, #0x18+0x10*1+0x06] @ C2 -> r3
	LDRSH	r0, [r8, #0x18+0x10*1+0x08] @ YzM1 -> r0
	LDRSH	r1, [r8, #0x18+0x10*1+0x0A] @ YzM2 -> r1
	BL	.LMixer_VoxLoop_MixLoop_HandleLoop_ADPCM_SkipSamples
	STR	r3, [r9, #0x10+0x04*1]      @ Store Vox.ADPCM[1]
	LDMFD	sp!, {r4,fp}
#endif
2:	LDRSH	r2, [r8, #0x18+0x10*0+0x04] @ C1 -> r2
	LDRSH	r3, [r8, #0x18+0x10*0+0x06] @ C2 -> r3
	LDRSH	r0, [r8, #0x18+0x10*0+0x08] @ YzM1 -> r0
	LDRSH	r1, [r8, #0x18+0x10*0+0x0A] @ YzM2 -> r1
	BL	.LMixer_VoxLoop_MixLoop_HandleLoop_ADPCM_SkipSamples
	STR	r3, [r9, #0x10+0x04*0]      @ Store Vox.ADPCM[0]
0:	LDMFD	sp!, {r0,r7,r9,ip,lr}
	B	RELOC_TARGET(.LMixer_VoxLoop_MixLoop_HandleLoop_Done)

.LMixer_VoxLoop_MixLoop_HandleLoop_ADPCM_ExactLoopRestart:
	LDR	fp, [sp, #0x00]
	LDR	r2, [r8, #0x18+0x10*0+0x08] @ Store Vox.ADPCM[] = Wav.ADPCM.Loop[]
#if SGE_STEREO_WAVEFORMS
	LDR	r3, [r8, #0x18+0x10*1+0x08]
#endif
	STR	r2, [fp, #0x10]
#if SGE_STEREO_WAVEFORMS
	STR	r3, [fp, #0x14]
#endif
	B	RELOC_TARGET(.LMixer_VoxLoop_MixLoop_HandleLoop_Done)

@ Returns YzM1|YzM2<<16 -> r3
.LMixer_VoxLoop_MixLoop_HandleLoop_ADPCM_SkipSamples:
	LDR	r7, [r4]                    @ Load frame
	SUB	r0, r0, r1                  @ 2*(YzM1-YzM2) -> r0
	MOV	r0, r0, lsl #0x01
0:	AND	ip, fp, r7, lsl #0x07       @ Set initial shifter
	STRH	ip, 999f
	B	1f
0:	MOV	r7, r7, lsl #0x04           @ Next nybble
1:	SUBS	fp, fp, #0x01<<16           @ --nSkip?
	BCC	2f
10:	MUL	ip, r1, r3                  @ Decode next sample
	ADDS	r1, r1, r0, asr #0x01
	MLANE	ip, r1, r2, ip
	MOV	r0, r7, asr #0x1C
	MOV	ip, ip, asr #0x06-1
999:	ADD	r0, ip, r0, lsl #0x00       @ <- Self-modifying
	BICS	ip, r7, #0xFF000000         @ More samples in this frame?
	BNE	0b
.LMixer_VoxLoop_MixLoop_HandleLoop_ADPCM_SkipSamples_ReadFrame:
	LDR	r7, [r4, #0x04]!            @ Load next frame
	SUBS	fp, fp, #0x01<<16           @ --nSkip?
	ANDCS	ip, fp, r7, lsl #0x07       @  Update shifter
	STRCSH	ip, 999b
	BCS	10b
2:	ADDS	r3, r1, r0, asr #0x01       @ (YzM1-YzM2)+YzM2 = YzM1 -> r3
	ANDMI	r3, r3, r3, lsr #0x10       @ [mask negative to 16bit]
	ORR	r3, r3, r1, lsl #0x10       @ YzM1|YzM2<<16
	BX	lr

/************************************************/
//! Actually entering mixer
/************************************************/

.LMixerCore_ADPCM_Enter:
	STR	lr, [sp, #-0x04]!
0:	LDR	lr, [r4]                  @ Load frame
	MOV	r5, #0x80                 @ BiasPCM -> r5
	AND	ip, fp, lr, lsl #0x07     @ Get initial shifter -> ip
	MOV	r8, r7, lsr #0x10         @ Move to sample at AdPos
	BIC	r7, r7, r8, lsl #0x10
	MOVS	r8, r8, lsl #0x02
	MOVNE	lr, lr, lsl r8
	SUB	r0, r0, r1                @ 2*(YzM1-YzM2) -> r0
	MOV	r0, r0, lsl #0x01
	@ Mixer code follows

/************************************************/

.LMixerCore_ADPCM_InitEnd:
.LMixerCore_ADPCM_Mono_RateLT_NoLerp_InitEnd:
.LMixerCore_ADPCM_Mono_RateGT_NoLerp_InitEnd:
.LMixerCore_ADPCM_Mono_RateLT_Lerp_InitEnd:
.LMixerCore_ADPCM_Mono_RateGT_Lerp_InitEnd:
.LMixerCore_ADPCM_Stereo_RateLT_NoLerp_InitEnd:
.LMixerCore_ADPCM_Stereo_RateGT_NoLerp_InitEnd:
.LMixerCore_ADPCM_Stereo_RateLT_Lerp_InitEnd:
.LMixerCore_ADPCM_Stereo_RateGT_Lerp_InitEnd:

#undef RELOC_TARGET

/************************************************/
//! EOF
/************************************************/
