/************************************************/
ASM_MODE_ARM
/************************************************/

@ Rate < 1.0 and Rate >= 1.0 have the same register allocations:
@  r0:  2*(YzM1-YzM2)
@  r1:  YzM2
@  r2:  ARM7: C1 / ARM9: C1 | C2<<16
@  r3:  ARM7: C2
@  r4: &Src
@  r5: [Temp]
@  r6:  VolL | VolR<<16
@  r7:  Position [17.15fxp]
@  r8: [Temp]
@  r9: &Dst
@  sl:  Rate [17.15fxp]
@  fp:  ShiftMask | nRem<<16
@  ip: [Temp]
@  lr:  FrameData
@  sp+00h: &ReturnAddr (after jumping to CommonInit)

/************************************************/
//! Rate < 1.0
/************************************************/

.LMixerCore_ADPCM_Mono_RateLT_Lerp:
.LMixerCore_ADPCM_Stereo_RateLT_Lerp:
	ADR	r2, .LMixerCore_ADPCM_RateLT_Lerp_ReadFrameOpcode
	ADR	r3, 0f
	B	.LMixerCore_ADPCM_CommonInit
0:	STRH	ip, 999f                 @ Store shifter
#if (__NDS__ == 9)
	MOVS	r7, r7, lsl #(31-15)     @ Shift Position to upper 16 bits [C=0]
#else
	MOVS	r7, r7, lsl #(32-15)
#endif
0:	MOVCS	lr, lr, lsl #0x04        @ Next nybble
1:	SUBS	fp, fp, #0x01<<16        @ --nRem?
	BCC	2f
10:
#if (__NDS__ == 9)
	SMLAWT	ip, r0, r7, r1           @ Mix
	LDMIA	r9, {r5,r8}
	SMLABB	r5, ip, r6, r5
	SMLABT	r8, ip, r6, r8
	STMIA	r9!, {r5,r8}
	ADDS	r7, r7, sl, lsl #(31-15) @ Position += Rate?
	BPL	1b
#else
	MOV	r8, r6, lsr #0x10
	BIC	r5, r6, r8, lsl #0x10
	MOVS	ip, r7, lsr #0x20-8
	MULNE	ip, r0, ip
	ADDS	ip, r1, ip, asr #0x08+1
	MULNE	r5, ip, r5
	MULNE	r8, ip, r8
	LDR	ip, [r9], #0x04
	ADD	r5, ip, r5
	LDR	ip, [r9], #0x04
	ADD	r8, ip, r8
	STMDB	r9, {r5,r8}
	ADDS	r7, r7, sl, lsl #(32-15) @ Position += Rate?
	BCC	1b
#endif
11:	ADPCM_DecodeSample lr            @ Decode next sample
#if (__NDS__ == 9)
	SUB	r7, r7, #0x01<<31        @ Clear overflow (Position<<16 must remain 0.15fxp)
#endif
	BICS	ip, lr, #0xFF000000      @ More samples in this frame? [C=1]
	BNE	0b
.LMixerCore_ADPCM_RateLT_Lerp_ReadFrameOpcode:
	LDR	lr, [r4, #0x04]!         @ Load next frame
	SUBS	fp, fp, #0x01<<16        @ --nRem?
	ANDCS	ip, fp, lr, lsl #0x07    @  Update shifter
	STRCSH	ip, 999b
	BCS	10b
2:
#if (__NDS__ == 9)
	MOV	r7, r7, lsr #(31-15)     @ Restore Position|Phase
#else
	MOV	r7, r7, lsr #(32-15)
#endif
	ADDS	r0, r1, r0, asr #0x01    @ (YzM1-YzM2)+YzM2 = YzM1 -> r0
	ANDMI	r0, r0, r0, lsr #0x10    @ [mask negative to 16bit]
	ORR	r0, r0, r1, lsl #0x10    @ YzM1|YzM2<<16
	LDR	pc, [sp], #0x04

/************************************************/
//! Rate >= 1.0
/************************************************/

.LMixerCore_ADPCM_Mono_RateGT_Lerp:
.LMixerCore_ADPCM_Stereo_RateGT_Lerp:
	ADR	r2, .LMixerCore_ADPCM_RateGT_Lerp_ReadFrameOpcode
	ADR	r3, 0f
	B	.LMixerCore_ADPCM_CommonInit
0:	STRH	ip, 999f                 @ Store shifter
	SUB	sl, sl, #0x01<<15        @ Rate -= 1.0
	SUB	fp, fp, #0x01<<16        @ --nRem (we use a n>=0 loop)
1:
#if (__NDS__ == 9)
	SMLAWB	ip, r0, r7, r1           @ Mix
	LDMIA	r9, {r5,r8}
	SMLABB	r5, ip, r6, r5
	SMLABT	r8, ip, r6, r8
	STMIA	r9!, {r5,r8}
#else
	MOV	r8, r6, lsr #0x10        @ Mix
	BIC	r5, r6, r8, lsl #0x10
	MOVS	ip, r7, lsr #(15-8)      @ <- We either have a 16bit Rn (3c MUL) or MOV + 8bit Rn (3c all up).
	MULNE	ip, r0, ip               @    With the latter, we can also set flags, so we use that.
	ADDS	ip, r1, ip, asr #0x08+1
	MULNE	r5, ip, r5
	MULNE	r8, ip, r8
	LDR	ip, [r9], #0x04
	ADD	r5, ip, r5
	LDR	ip, [r9], #0x04
	ADD	r8, ip, r8
	STMDB	r9, {r5,r8}
#endif
	ADD	r7, r7, sl               @ Position += Rate
2:	ADPCM_DecodeSample lr            @ Decode next sample
	BICS	ip, lr, #0xFF000000      @ More samples in this frame?
	BEQ	21f
3:	MOV	lr, lr, lsl #0x04        @ Next nybble
	SUBS	r7, r7, #0x01<<15        @ More samples to decode?
	BCS	2b
31:	ADD	r7, r7, #0x01<<15
	SUBS	fp, fp, #0x01<<16        @ --nRem?
	BCS	1b
32:	B	4f
.LMixerCore_ADPCM_RateGT_Lerp_ReadFrameOpcode:
21:	LDR	lr, [r4, #0x04]!         @ Load next frame
	SUBS	r7, r7, #0x01<<15        @ More samples to decode?
	AND	ip, fp, lr, lsl #0x07    @ Update shifter
	STRH	ip, 999b
	BCS	2b
22:	ADD	r7, r7, #0x01<<15
	SUBS	fp, fp, #0x01<<16        @ --nRem?
	BCS	1b
4:	ADD	sl, sl, #0x01<<15        @ Restore Rate
	ADDS	r0, r1, r0, asr #0x01    @ (YzM1-YzM2)+YzM2 = YzM1 -> r0
	ANDMI	r0, r0, r0, lsr #0x10    @ [mask negative to 16bit]
	ORR	r0, r0, r1, lsl #0x10    @ YzM1|YzM2<<16
	LDR	pc, [sp], #0x04

/************************************************/
//! EOF
/************************************************/
