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

.LMixerCore_PCM8_Stereo_RateLT_NoLerp_InitBeg:
.LMixerCore_PCM8_Stereo_RateLT_NoLerp_InitEnd:
.LMixerCore_PCM8_Stereo_RateGT_NoLerp_InitBeg:
.LMixerCore_PCM8_Stereo_RateGT_NoLerp_InitEnd:

/************************************************/

@ r0:  nRem
@ r1: [OutputTemp]
@ r2:  VolL
@ r3:  VolR
@ r4: &Src
@ r5:  AccessMask(=FFFFFFFEh)
@ r6:  ClearMask | ClearMask<<16 -> r6
@ r7:  Position [16.16fxp]
@ r8: [OutputTemp]
@ r9: &Dst
@ sl:  Rate [16.16fxp]
@ fp:  00FF00FFh
@ ip: [Output]
@ lr: [Output]

#define RELOC_TARGET(Target) ( \
	(Target - .LMixerCore_WorkArea + .LMixerCore_PCM8_Stereo_RateLT_NoLerp_Beg) - \
	(.LMixerCore_PCM8_Stereo_RateLT_NoLerp_InitEnd - .LMixerCore_PCM8_Stereo_RateLT_NoLerp_InitBeg) \
)

.macro PCM8_Stereo_NoLerp_Fetch Rd
	AND	\Rd, r5, r7, lsr #0x10-1        @ Get scaled Position offset
	LDRH	\Rd, [r4, \Rd]                  @ L|R<<8 -> Rd
	ADD	r7, r7, sl                      @ Position += Rate
.endm

.LMixerCore_PCM8_Stereo_RateLT_NoLerp_Beg:
.LMixerCore_PCM8_Stereo_RateGT_NoLerp_Beg:
	AND	r2, r6, #0xFF                   @ VolL -> r2
	MOV	r3, r6, lsr #0x10               @ VolR -> r3
	MVN	r5, #0x01                       @ AccessMask -> r5
	MOV	fp, #0xFF                       @ 00FF00FFh -> fp
	ORR	fp, fp, fp, lsl #0x10
	MOV	r6, #MIX_CLEAR_MASK             @ ClearMask | ClearMask<<16 -> r6
	ORR	r6, r6, r6, lsl #0x10
#if (SGE_PCM8_STEREO_UNROLL > 1)
	SUBS	r0, r0, #SGE_PCM8_STEREO_UNROLL @ Have enough samples for a block iteration?
#endif
#if (SGE_PCM8_STEREO_UNROLL > 1)
	BCC	2f
1:
.rept (SGE_PCM8_STEREO_UNROLL/2)
	PCM8_Stereo_NoLerp_Fetch r1             @ Dry -> r1,r8
	PCM8_Stereo_NoLerp_Fetch r8
	ORR	r1, r1, r8, lsl #0x10           @ L1 | R1<<8 | L2<<16 | R2<<24
	AND	r8, fp, r1, lsr #0x08           @ R1 | R2<<16 -> r8
	AND	r1, fp, r1, lsr #0x00           @ L1 | L2<<16 -> r1
	MUL	lr, r8, r3                      @ R *= VolR -> lr
	MUL	ip, r1, r2                      @ L *= VolL -> ip
	BIC	lr, lr, r6
	EOR	r1, ip, lr, lsl #0x10           @ Swap back to L1|R1<<16 -> r1, L2|R2<<16 -> r8
	EOR	r8, lr, r1, lsr #0x10
	LDMIA	r9, {ip,lr}                     @ Wet -> ip,lr
	EOR	r1, r1, r8, lsl #0x10
	ADD	ip, ip, r1, lsr #MIX_SHIFT_DOWN
	ADD	lr, lr, r8, lsr #MIX_SHIFT_DOWN
	STMIA	r9!, {ip,lr}
.endr
	SUBS	r0, r0, #SGE_PCM8_STEREO_UNROLL
	BCS	1b
2:	ADDS	r0, r0, #SGE_PCM8_STEREO_UNROLL @ Restore nRem. Have at least 1 sample?
	BEQ	3f
#endif
20:	PCM8_Stereo_NoLerp_Fetch r1             @ Dry -> r1
	MOV	r8, r1, lsr #0x08               @ R -> r8
	AND	r1, r1, #0xFF                   @ L -> r1
	MUL	ip, r1, r2                      @ L *= VolL -> ip
	MUL	lr, r8, r3                      @ R *= VolR -> lr
	LDR	r1, [r9]                        @ Wet -> ip
	ORR	ip, ip, lr, lsl #0x10           @ L | R<<16
	BIC	ip, ip, #MIX_CLEAR_MASK<<16
	ADD	r1, r1, ip, lsr #MIX_SHIFT_DOWN
	STR	r1, [r9], #0x04
#if (SGE_PCM8_STEREO_UNROLL != 2) //! 2 samples/iter would have 1 sample remainder at most
	SUBS	r0, r0, #0x01                   @ --nRem?
	BNE	20b
#endif
3:	ORR	r6, r2, r3, lsl #0x10           @ Restore VolL|VolR<<16 -> r6
	B	RELOC_TARGET(.LMixer_VoxLoop_MixLoop_MixChunk_Tail)

.LMixerCore_PCM8_Stereo_RateLT_NoLerp_End:
.LMixerCore_PCM8_Stereo_RateGT_NoLerp_End:

.equ SGE_Driver_MixerCore_PCM8_Stereo_RateLT_Size, \
	(.LMixerCore_PCM8_Stereo_RateLT_NoLerp_InitEnd - .LMixerCore_PCM8_Stereo_RateLT_NoLerp_InitBeg) + \
	(.LMixerCore_PCM8_Stereo_RateLT_NoLerp_End     - .LMixerCore_PCM8_Stereo_RateLT_NoLerp_Beg)
.equ SGE_Driver_MixerCore_PCM8_Stereo_RateGT_Size, SGE_Driver_MixerCore_PCM8_Stereo_RateLT_Size
.if SGE_Driver_MixerCore_PCM8_Stereo_RateLT_Size > SGE_Driver_MixerCore_WorkAreaSize
	.equ SGE_Driver_MixerCore_WorkAreaSize, SGE_Driver_MixerCore_PCM8_Stereo_RateLT_Size
.endif

#undef RELOC_TARGET

/************************************************/
//! EOF
/************************************************/
