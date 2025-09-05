/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
/************************************************/

#define AVOID_MUL_WARNINGS 0 //! Avoid eg. "MUL x, x, y" or "SMULL x, y, x, z" etc.

/************************************************/

//! Precision constants
//! RATE_BITS is fixed in the mixer loops, and is just defined
//! here for reference as well as being referenced in the logic
//! that calculate the mixing rates here.
//! FREQ_BITS is the intermediate precision for the frequency
//! calculations of each voice. Note that this implies that a
//! voice has a maximum playback frequency of 2^(32-FREQ_BITS)-1,
//! so setting FREQ_BITS == 14 gives us a maximum playback
//! frequency of 262.143kHz, which should be plenty for GBA/DS.
//! INVRATE_BITS refers to the precision of 1/RateHz, used for
//! calculating the rate from the voice frequency. Given that
//! RateHz >= 8000 by design, 44bit is the highest we can safely
//! use, as 32-Ceiling[Log2[1/8000]] == 44.
//! MAX_RATE is the maximum /integer/ rate we can use. This is
//! limited by 64 - FREQ_BITS + INVRATE_BITS.
#ifdef __GBA__
# define RATE_BITS 16
#else
# define RATE_BITS 15 //! NDS uses 15bit fractional rates for SMULxy, etc
#endif
#define FREQ_BITS 14
#define INVRATE_BITS 44
#define MAX_RATE_LOG2 4
#define MAX_RATE (1<<MAX_RATE_LOG2)
#define SCALEDRATE_BITS (INVRATE_BITS + FREQ_BITS)
#if (SCALEDRATE_BITS + MAX_RATE_LOG2 > 64)
# warning "MAX_RATE is too high; rate will be clipped, instead."
#endif

//! The VOLSTEP offsets are based at 18h, which is the offset
//! after a voice has pushed r4,r5 for processing inside the
//! voice update loop.
#ifdef __GBA__
# define MIX_SHIFT_DOWN (SGE_MIXER_VOLBITS - SGE_MIXER_VOLFRACBITS)
# define MIX_CLEAR_MASK ((1 << MIX_SHIFT_DOWN)-1)
# if SGE_USE_VOLSUBDIV
#  define VOLSTEP_DIVCOUNT_SP_OFFS (0x18 + 0x04*(1<<SGE_VOLSUBDIV_LOG2MAXSUBDIV)) //! Above the bias levels
#  define VOLSTEP_SP_OFFS          (VOLSTEP_DIVCOUNT_SP_OFFS + 0x04)
#  define VOLCOUNTER_SP_OFFS       (VOLSTEP_DIVCOUNT_SP_OFFS + 0x08)
#  define USEDEG1_SP_OFFS          (VOLSTEP_DIVCOUNT_SP_OFFS + 0x0C)
# endif
#else
# if SGE_USE_VOLSUBDIV
#  define VOLSTEP_DIVCOUNT_SP_OFFS (0x18)
#  define VOLSTEP_SP_OFFS          (VOLSTEP_DIVCOUNT_SP_OFFS + 0x04)
#  define USEDEG1_SP_OFFS          (VOLSTEP_DIVCOUNT_SP_OFFS + 0x08)
# endif
#endif

/************************************************/

@ Clip 9bit value to 8bit (or 17bit value to 16bit)
.macro FastClip x, ClipMsk
#ifdef __GBA__
	TEQ	\x, \x, lsl #0x20-8
#else
	TEQ	\x, \x, lsl #0x20-16
#endif
	EORMI	\x, \ClipMsk, \x, asr #0x20
.endm

/************************************************/
#ifdef __GBA__
/************************************************/

#define DEFINE_MIXER_POINTERS_DUMMY() \
	.word 0,0; \
	.word .LMixerCore_Unsupported_Beg, .LMixerCore_Unsupported_End - .LMixerCore_Unsupported_Beg
#define DEFINE_MIXER_POINTERS_DUMMY_BLOCK() \
	DEFINE_MIXER_POINTERS_DUMMY(); \
	DEFINE_MIXER_POINTERS_DUMMY(); \
	DEFINE_MIXER_POINTERS_DUMMY(); \
	DEFINE_MIXER_POINTERS_DUMMY()
#define DEFINE_MIXER_POINTERS_BLOCK(Format, Lerp) \
	.word .LMixerCore_##Format##_Mono_RateLT_##Lerp##_InitBeg; \
	.word .LMixerCore_##Format##_Mono_RateLT_##Lerp##_InitEnd - .LMixerCore_##Format##_Mono_RateLT_##Lerp##_InitBeg; \
	.word .LMixerCore_##Format##_Mono_RateLT_##Lerp##_Beg; \
	.word .LMixerCore_##Format##_Mono_RateLT_##Lerp##_End     - .LMixerCore_##Format##_Mono_RateLT_##Lerp##_Beg; \
	.word .LMixerCore_##Format##_Mono_RateGT_##Lerp##_InitBeg; \
	.word .LMixerCore_##Format##_Mono_RateGT_##Lerp##_InitEnd - .LMixerCore_##Format##_Mono_RateGT_##Lerp##_InitBeg; \
	.word .LMixerCore_##Format##_Mono_RateGT_##Lerp##_Beg; \
	.word .LMixerCore_##Format##_Mono_RateGT_##Lerp##_End     - .LMixerCore_##Format##_Mono_RateGT_##Lerp##_Beg; \
	.if SGE_STEREO_WAVEFORMS; \
		.word .LMixerCore_##Format##_Stereo_RateLT_##Lerp##_InitBeg; \
		.word .LMixerCore_##Format##_Stereo_RateLT_##Lerp##_InitEnd - .LMixerCore_##Format##_Stereo_RateLT_##Lerp##_InitBeg; \
		.word .LMixerCore_##Format##_Stereo_RateLT_##Lerp##_Beg; \
		.word .LMixerCore_##Format##_Stereo_RateLT_##Lerp##_End     - .LMixerCore_##Format##_Stereo_RateLT_##Lerp##_Beg; \
		.word .LMixerCore_##Format##_Stereo_RateGT_##Lerp##_InitBeg; \
		.word .LMixerCore_##Format##_Stereo_RateGT_##Lerp##_InitEnd - .LMixerCore_##Format##_Stereo_RateGT_##Lerp##_InitBeg; \
		.word .LMixerCore_##Format##_Stereo_RateGT_##Lerp##_Beg; \
		.word .LMixerCore_##Format##_Stereo_RateGT_##Lerp##_End     - .LMixerCore_##Format##_Stereo_RateGT_##Lerp##_Beg; \
	.else; \
		DEFINE_MIXER_POINTERS_DUMMY(); \
		DEFINE_MIXER_POINTERS_DUMMY(); \
	.endif
#define DEFINE_MIXER_POINTERS(Format) \
	.if SGE_SUPPORT_##Format; \
		.if !SGE_ALWAYS_LERP; \
			DEFINE_MIXER_POINTERS_BLOCK(Format, NoLerp); \
		.endif; \
		.if SGE_SUPPORT_LERP; \
			DEFINE_MIXER_POINTERS_BLOCK(Format, Lerp); \
		.endif; \
	.else; \
		.if !SGE_ALWAYS_LERP; \
			DEFINE_MIXER_POINTERS_DUMMY_BLOCK(); \
		.endif; \
		.if SGE_SUPPORT_LERP; \
			DEFINE_MIXER_POINTERS_DUMMY_BLOCK(); \
		.endif; \
	.endif

/************************************************/

ASM_DATA_BEG(SGE_Driver_MixerTables, ASM_DATASECT_RODATA;ASM_ALIGN(4))

SGE_Driver_MixerTables:
	DEFINE_MIXER_POINTERS(ADPCM)
	DEFINE_MIXER_POINTERS(PCM8)
	DEFINE_MIXER_POINTERS(PCM16)

.LMixerCore_UnsupportedFormat:
	DEFINE_MIXER_POINTERS_DUMMY()

@ These are not technically mixers, but we use the same loading mechanism
.LMixerCoreStruct_NoReverb:
	.word 0,0
	.word .LMixdown_NoReverb_Beg, .LMixdown_NoReverb_End - .LMixdown_NoReverb_Beg

#ifdef SGE_PLATFORM_HAVE_REVERB
.LMixerCoreStruct_WithReverb:
	.word 0,0
	.word .LMixdown_WithReverb_Beg, .LMixdown_WithReverb_End - .LMixdown_WithReverb_Beg
#endif

ASM_DATA_END(SGE_Driver_MixerTables)

/************************************************/
#else
/************************************************/

#define DEFINE_MIXER_POINTERS_DUMMY() \
	.word .LMixer_VoxLoop_MixLoop_MixChunk_Tail
#define DEFINE_MIXER_POINTERS_DUMMY_BLOCK() \
	DEFINE_MIXER_POINTERS_DUMMY(); \
	DEFINE_MIXER_POINTERS_DUMMY(); \
	DEFINE_MIXER_POINTERS_DUMMY(); \
	DEFINE_MIXER_POINTERS_DUMMY()
#define DEFINE_MIXER_POINTERS_BLOCK(Format, Lerp) \
	.word .LMixerCore_##Format##_Mono_RateLT_##Lerp##; \
	.word .LMixerCore_##Format##_Mono_RateGT_##Lerp##; \
	.if SGE_STEREO_WAVEFORMS; \
		.word .LMixerCore_##Format##_Stereo_RateLT_##Lerp##; \
		.word .LMixerCore_##Format##_Stereo_RateGT_##Lerp##; \
	.else; \
		DEFINE_MIXER_POINTERS_DUMMY(); \
		DEFINE_MIXER_POINTERS_DUMMY(); \
	.endif
#define DEFINE_MIXER_POINTERS(Format) \
	.if SGE_SUPPORT_##Format##; \
		.if !SGE_ALWAYS_LERP; \
			DEFINE_MIXER_POINTERS_BLOCK(Format, NoLerp); \
		.endif; \
		.if SGE_SUPPORT_LERP; \
			DEFINE_MIXER_POINTERS_BLOCK(Format, Lerp); \
		.endif; \
	.else; \
		.if !SGE_ALWAYS_LERP; \
			DEFINE_MIXER_POINTERS_DUMMY_BLOCK(); \
		.endif; \
		.if SGE_SUPPORT_LERP; \
			DEFINE_MIXER_POINTERS_DUMMY_BLOCK(); \
		.endif; \
	.endif

/************************************************/
#endif
/************************************************/

ASM_FUNC_BEG(SGE_Driver_MixerCores, ASM_FUNCSECT_TEXT;ASM_MODE_ARM)

#ifdef __GBA__
@ Work area size must be updated for every mixer core
.equ SGE_Driver_MixerCore_WorkAreaSize, 0
#endif

SGE_Driver_MixerCores:
#define STRINGIFY(x) #x
#ifdef __GBA__
# define MIXER_FILENAME(Format) STRINGIFY(MixerCores/##Format##.inc)
#else
# define MIXER_FILENAME(Format) STRINGIFY(MixerCores/##Format-NDS##.inc)
#endif

//! ADPCM on NDS9 must go in ITCM
#if (SGE_SUPPORT_ADPCM && !defined(__NDS__) || __NDS__ != 9)
# ifdef __GBA__
#  include "MixerCores/ADPCM_Common.inc"
# else
#  include "MixerCores/ADPCM_Common-NDS.inc"
# endif
#  include MIXER_FILENAME(ADPCM_NoLerp)
# if SGE_SUPPORT_LERP
#  include MIXER_FILENAME(ADPCM_Lerp)
# endif
#endif

#if SGE_SUPPORT_PCM8
# if !SGE_ALWAYS_LERP
#  include MIXER_FILENAME(PCM8_Mono_NoLerp)
#  if SGE_STEREO_WAVEFORMS
#   include MIXER_FILENAME(PCM8_Stereo_NoLerp)
#  endif
# endif
# if SGE_SUPPORT_LERP
#  include MIXER_FILENAME(PCM8_Mono_Lerp)
#  if SGE_STEREO_WAVEFORMS
#   include MIXER_FILENAME(PCM8_Stereo_Lerp)
#  endif
# endif
#endif

#if SGE_SUPPORT_PCM16
# if !SGE_ALWAYS_LERP
#  include MIXER_FILENAME(PCM16_Mono_NoLerp)
#  if SGE_STEREO_WAVEFORMS
#   include MIXER_FILENAME(PCM16_Stereo_NoLerp)
#  endif
# endif
# if SGE_SUPPORT_LERP
#  include MIXER_FILENAME(PCM16_Mono_Lerp)
#  if SGE_STEREO_WAVEFORMS
#   include MIXER_FILENAME(PCM16_Stereo_Lerp)
#  endif
# endif
#endif

.LMixerCore_MixdownLoops:
#if 1
# include MIXER_FILENAME(Mixdown_NoReverb)
#endif
#ifdef SGE_PLATFORM_HAVE_REVERB
# include MIXER_FILENAME(Mixdown_WithReverb)
#endif

#ifdef __GBA__

.LMixerCore_Unsupported_Beg:
# if 0
	BL	.LMixer_VoxLoop_MixLoop_ApplySilence
	B	.LMixer_VoxLoop_MixLoop_MixChunk_Tail
# else
	BL	.LMixer_VoxLoop_MixLoop_ApplySilence  - .LMixerCore_WorkArea + .LMixerCore_Unsupported_Beg
	B	.LMixer_VoxLoop_MixLoop_MixChunk_Tail - .LMixerCore_WorkArea + .LMixerCore_Unsupported_Beg
# endif

#endif

.LMixerCore_Unsupported_End:

ASM_DATA_END(SGE_Driver_MixerCores)

/************************************************/
#if (SGE_SUPPORT_ADPCM && defined(__NDS__) && __NDS__ == 9)
/************************************************/

//! The ADPCM mixer requires self-modifying code

ASM_FUNC_BEG(SGE_Driver_MixerCores_ITCM, ASM_FUNCSECT_ITCM;ASM_MODE_ARM)

SGE_Driver_MixerCores_ITCM:
#include "MixerCores/ADPCM_Common-NDS.inc"
#include MIXER_FILENAME(ADPCM_NoLerp)
#if SGE_SUPPORT_LERP
# include MIXER_FILENAME(ADPCM_Lerp)
#endif

ASM_DATA_END(SGE_Driver_MixerCores_ITCM)

/************************************************/
#endif
/************************************************/

ASM_DATA_BEG(SGE_KeyScale, ASM_DATASECT_RODATA;ASM_ALIGN(4))

@ Round[2^(27 + #/12)] &@ Range[0,12]
SGE_KeyScale:
#if (SGE_NOTELUT_BITS == 0)
 #define KEYSCALEVALS(x0,x1,x2,x3,x4,x5,x6,x7,x8,x9,xA,xB,xC,xD,xE,xF) \
	.word x0
#elif (SGE_NOTELUT_BITS == 1)
 #define KEYSCALEVALS(x0,x1,x2,x3,x4,x5,x6,x7,x8,x9,xA,xB,xC,xD,xE,xF) \
	.word x0,x8
#elif (SGE_NOTELUT_BITS == 2)
 #define KEYSCALEVALS(x0,x1,x2,x3,x4,x5,x6,x7,x8,x9,xA,xB,xC,xD,xE,xF) \
	.word x0,x4,x8,xC
#elif (SGE_NOTELUT_BITS == 3)
 #define KEYSCALEVALS(x0,x1,x2,x3,x4,x5,x6,x7,x8,x9,xA,xB,xC,xD,xE,xF) \
	.word x0,x2,x4,x6,x8,xA,xC,xE
#elif (SGE_NOTELUT_BITS == 4)
 #define KEYSCALEVALS(x0,x1,x2,x3,x4,x5,x6,x7,x8,x9,xA,xB,xC,xD,xE,xF) \
	.word x0,x1,x2,x3,x4,x5,x6,x7,x8,x9,xA,xB,xC,xD,xE,xF
#endif
	KEYSCALEVALS( \
		0x8000000,0x807682D,0x80ED735,0x8164D1F,0x81DC9F2,0x8254DB3,0x82CD86A,0x8346A1C, \
		0x83C02D0,0x843A28C,0x84B4958,0x852F739,0x85AAC36,0x8626857,0x86A2BA0,0x871F619  \
	)
	KEYSCALEVALS( \
		0x879C7C9,0x881A0B7,0x88980E8,0x8916864,0x8995732,0x8A14D57,0x8A94ADC,0x8B14FC7, \
		0x8B95C1E,0x8C16FE9,0x8C98B2F,0x8D1ADF6,0x8D9D845,0x8E20A24,0x8EA4399,0x8F284AB  \
	)
	KEYSCALEVALS( \
		0x8FACD62,0x9031DC4,0x90B75D9,0x913D5A8,0x91C3D37,0x924AC8F,0x92D23B6,0x935A2B3, \
		0x93E298E,0x946B84E,0x94F4EFB,0x957ED9B,0x9609436,0x96942D3,0x971F97B,0x97AB834  \
	)
	KEYSCALEVALS( \
		0x9837F05,0x98C4DF7,0x9952510,0x99E0459,0x9A6EBD9,0x9AFDB97,0x9B8D39C,0x9C1D3EE, \
		0x9CADC96,0x9D3ED9A,0x9DD0704,0x9E628DB,0x9EF5326,0x9F885EE,0xA01C139,0xA0B0511  \
	)
	KEYSCALEVALS( \
		0xA14517D,0xA1DA685,0xA270430,0xA306A88,0xA39D993,0xA43515B,0xA4CD1E7,0xA565B3E, \
		0xA5FED6B,0xA698873,0xA732C61,0xA7CD93B,0xA868F0B,0xA904DD8,0xA9A15AB,0xAA3E68D  \
	)
	KEYSCALEVALS( \
		0xAADC084,0xAB7A39B,0xAC18FDA,0xACB8548,0xAD583EF,0xADF8BD6,0xAE99D08,0xAF3B78B, \
		0xAFDDB69,0xB0808AA,0xB123F58,0xB1C7F7B,0xB26C91B,0xB311C41,0xB3B78F7,0xB45DF45  \
	)
	KEYSCALEVALS( \
		0xB504F33,0xB5AC8CC,0xB654C17,0xB6FD91E,0xB7A6FEA,0xB851084,0xB8FBAF4,0xB9A6F45, \
		0xBA52D7F,0xBAFF5AB,0xBBAC7D3,0xBC5A3FF,0xBD08A3A,0xBDB7A8C,0xBE674FF,0xBF1799B  \
	)
	KEYSCALEVALS( \
		0xBFC886C,0xC07A179,0xC12C4CD,0xC1DF270,0xC292A6D,0xC346CCE,0xC3FB99B,0xC4B10DE, \
		0xC5672A1,0xC61DEEE,0xC6D55CF,0xC78D74D,0xC846372,0xC8FFA48,0xC9B9BD8,0xCA7482E  \
	)
	KEYSCALEVALS( \
		0xCB2FF53,0xCBEC150,0xCCA8E30,0xCD665FD,0xCE248C1,0xCEE3687,0xCFA2F57,0xD06333E, \
		0xD124244,0xD1E5C74,0xD2A81D9,0xD36B27D,0xD42EE6A,0xD4F35AB,0xD5B884A,0xD67E652  \
	)
	KEYSCALEVALS( \
		0xD744FCD,0xD80C4C6,0xD8D4547,0xD99D15C,0xDA6690F,0xDB30C6A,0xDBFBB79,0xDCC7647, \
		0xDD93CDD,0xDE60F48,0xDF2ED92,0xDFFD7C6,0xE0CCDEF,0xE19D018,0xE26DE4C,0xE33F897  \
	)
	KEYSCALEVALS( \
		0xE411F04,0xE4E519D,0xE5B906E,0xE68DB83,0xE7632E7,0xE8396A5,0xE9106C9,0xE9E835D, \
		0xEAC0C6E,0xEB9A208,0xEC74435,0xED4F302,0xEE2AE7A,0xEF076A8,0xEFE4B9A,0xF0C2D59  \
	)
	KEYSCALEVALS( \
		0xF1A1BF4,0xF281774,0xF361FE6,0xF443557,0xF5257D1,0xF608762,0xF6EC415,0xF7D0DF7, \
		0xF8B6514,0xF99C977,0xFA83B2E,0xFB6BA44,0xFC546C6,0xFD3E0C1,0xFE28840,0xFF13D51  \
	)
	.word 0x10000000
#undef KEYSCALEVALS

ASM_DATA_END(SGE_KeyScale)

/************************************************/

#ifdef __GBA__
# define DRIVERUPDATE_SECTION ASM_FUNCSECT_FAST //! Update routine in IWRAM
#else
# define DRIVERUPDATE_SECTION ASM_FUNCSECT_TEXT //! Update routine in RAM (too large for ITCM)
#endif

@ r0: Driver

ASM_FUNC_GLOBAL(SGE_Driver_Update)
ASM_FUNC_BEG   (SGE_Driver_Update, DRIVERUPDATE_SECTION;ASM_MODE_ARM)

SGE_Driver_Update:
	STMFD	sp!, {r4-fp,lr}
#if (!defined(__NDS__) || __NDS__ != 9)
	ADR	ip, 1f+1
	BX	ip
#else
	BLX	1f
#endif
ASM_MODE_THUMB
1:	LDR	r2, [r0, #0x00]           @ State -> r2
	LDR	r5, [r0, #0x04]           @ MixBuf -> r5
	LDR	r3, =SGE_DRIVER_STATE_READY
	MOV	r4, r0                    @ Driver -> r4
	CMP	r2, r3                    @ Invalid state?
	BNE	.LEarlyExit

.LGetAndClearMixBuffer:
1:	LDRH	r2, [r4, #0x0E]           @ memset(MixBuf, 0, BufLen*sizeof(MixSmp_t[2]))
	MOV	r1, #0x00
	MOV	r0, r5
#ifdef __GBA__
	LSL	r2, #0x01+1
#else
	LSL	r2, #0x02+1
#endif
	BL	memset
1:	MOV	r0, #0x00                 @ RateScale=1/RateHz -> r0
	MVN	r0, r0
#if (INVRATE_BITS <= 32)
# if (INVRATE_BITS < 32)
	LSR	r0, #(32-INVRATE_BITS)
# endif
	LDRH	r1, [r4, #0x0C]
	BL	__aeabi_uidiv
#else
	LSR	r1, r0, #(64-INVRATE_BITS)
	LDRH	r2, [r4, #0x0C]
	MOV	r3, #0x00
	BL	__aeabi_uldivmod
#endif
	ADD	r0, #0x01
	LDR	r3, [r4, #0x0C]           @ RateHz | N(=BufLen)<<16 -> r3
#ifdef __GBA__
# if SGE_USE_VOLSUBDIV
	SUB	sp, #0x10                 @ With volume subdivision, we need space to store {SubdivCounter,VolStep,VolCounter,UsedEG1}
# endif
	MOV	r7, #0x00                 @ Push {RateScale,RateHz|N,Driver,MixBuf,Bias=0}
# if SGE_USE_VOLSUBDIV
	MOV	r2, #(1<<SGE_VOLSUBDIV_LOG2MAXSUBDIV)-1
1:	PUSH	{r7}                      @ Push all subdivided bias levels as well
	SUB	r2, #0x01
	BNE	1b
# endif
	PUSH	{r0,r3,r4,r5,r7}
#else
# if SGE_USE_VOLSUBDIV
	SUB	sp, #0x0C                 @ With volume subdivision, we need space to store {SubdivCounter,VolStep,UsedEG1}
# endif
	PUSH	{r0,r3,r4,r5}             @ Push {RateScale,RateHz|N,Driver,MixBuf}
#endif

/************************************************/

.LMixer_Entry:
	LDRB	r5, [r4, #0x0A]           @ nVoxRem(=nVox) -> r5, &Vox[] -> r4
	ADD	r4, #SGE_DRIVER_HEADER_SIZE

@ r4: &Vox
@ r5:  nVoxRem
@ sp+00h:  RateScale
@ sp+04h:  RateHz | N<<16
@ sp+08h: &Driver
@ sp+0Ch: &MixBuf
@ sp+10h:  BiasL | BiasR<<16 (or VolStepScale)

.LMixer_VoxLoop:
	MOV	r0, r4                    @ Stat|AdPos<<8|Key<<16|Vel<<24 -> r6
	LDR	r6, [r4, #0x00]
	ADD	r0, #0x20                 @ &Wav -> r0
	LSL	r2, r6, #(33-8)           @ Stat<<25 -> r2. C=ACTIVE?
	BCS	.LMixer_VoxLoop_Active

/************************************************/

.LMixer_VoxLoop_Tail:
	ADD	r4, #SGE_VOX_SIZE
	SUB	r5, #0x01                 @ --nVoxRem?
	BNE	.LMixer_VoxLoop

/************************************************/

@ r4: &Vox + nVox (=&OutBuf)

.LMixer_Mixdown:
	ADD	r2, sp, #0x04             @ RateHz|N<<16 -> r2, Driver -> r5, MixBuf -> r6
	LDMIA	r2, {r2,r5,r6}
	LDRB	r0, [r5, #0x0B]           @ Driver.BfIdxW -> r0
	LDRB	r1, [r5, #0x09]           @ Driver.BfCnt -> r1
	LSR	r7, r2, #0x10             @ N = Driver.BufLen -> r7
	@MOV	r4, r4                    @ DstL = OutBuf -> r4
	ADD	r3, r0, #0x01             @ Store BfIdxW = WRAP(BfIdxW+1)
	SUB	r3, r1
	BCS	0f
	ADD	r3, r1
0:	STRB	r3, [r5, #0x0B]
#ifdef __GBA__
# if SGE_USE_OVERSAMPLING
	LSL	r3, r7, #0x01             @ N*2 -> r3 (for oversampled buffer size)
	MUL	r0, r3                    @ DstL += BfIdxW * N*2 -> r4
	MUL	r1, r3                    @ DstR  = DstL + BfCnt*BufLen*2*sizeof(s8) -> r5
# else
	MUL	r0, r7                    @ DstL += BfIdxW * N -> r4
	MUL	r1, r7                    @ DstR  = DstL + BfCnt*BufLen*sizeof(s8) -> r5
# endif
#else
# if SGE_USE_OVERSAMPLING
	LSL	r3, r7, #0x01+1
# else
	LSL	r3, r7, #0x01
# endif
	MUL	r0, r3
	MUL	r1, r3
#endif
#ifdef SGE_PLATFORM_HAVE_REVERB
	LDR	r3, [r5, #0x14]           @ ReverbData = Driver.ReverbData -> r3?
#endif
	ADD	r4, r0
	ADD	r5, r4, r1
#ifdef __GBA__
	LDR	r0, =.LMixerCoreStruct_NoReverb
	LDR	r1, =.LMixerCore_WorkArea
#endif
#ifdef SGE_PLATFORM_HAVE_REVERB
	CMP	r3, #0x00
	BEQ	0f
# ifdef __GBA__
	LDR	r0, =.LMixerCoreStruct_WithReverb
# else
#  if (__NDS__ == 7)
	LDR	r0, =.LMixdown_WithReverb
	BX	r0
#  else
	BLX	.LMixdown_WithReverb
#  endif
# endif
#endif
#ifdef __GBA__
ASM_ALIGN(4)
0:	BX	pc
	NOP
ASM_MODE_ARM
	STR	r1, [sp, #-0x04]!
	B	.LMixer_VoxLoop_MixLoop_LoadMixer
#else
# if (__NDS__ == 7)
0:	LDR	r0, =.LMixdown_NoReverb
	BX	r0
# else
0:	BLX	.LMixdown_NoReverb
# endif
#endif

/************************************************/

ASM_MODE_THUMB
.LEarlyExit:
#if (!defined(__NDS__) || __NDS__ != 9)
	LDR	r0, =.LExit
	BX	r0
#else
	BLX	.LExit
#endif

/************************************************/
.pool
/************************************************/

ASM_MODE_THUMB

@ N=KEYON

.LMixer_VoxLoop_Active:
	PUSH	{r4,r5}                   @ Push {Vox,nVoxRem}
	LDMIA	r0, {r0-r1}
	MOV	r8, r0                    @ Wav -> r8, Art -> r9
	MOV	r9, r1
	BMI	.LMixer_VoxLoop_KeyOn     @ Handle KEYON as needed
	LSL	r2, #0x01                 @ Is the voice in KEYOFF mode?
	BMI	.LMixer_VoxLoop_KeyOff
.LMixer_VoxLoop_KeyOn_Done:
.LMixer_VoxLoop_KeyOff_Done:

.LMixer_VoxLoop_GetTrackState:
	LSL	r0, r6, #0x20-4           @ NOPLAYER?
	BCS	.LMixer_VoxLoop_GetSavedTrackState
	LDR	r0, [r4, #0x2C]           @ Ply -> r0
	LDR	r1, [r4, #0x28]           @ Trk -> r1
	LDRB	r2, [r0, #0x01]           @ Vol  = Ply.Vol -> r2 [1.7fxp]
	LDRB	r3, [r1, #0x10+2]         @ Vol *= Trk.Vol [1.7 + 1.7 = 1.14fxp]
	MUL	r3, r2
	LDRB	r2, [r1, #0x14+2]         @ Vol *= Trk.Exp [1.14 + 1.7 = 1.21fxp]
	MUL	r2, r3
	LDRB	r3, [r1, #0x18+2]         @ Pan -> r3
	ADD	r1, #0x1C+2
	LDRH	r1, [r1]                  @ Bnd -> r1
	LSR	r2, #(21-7)               @ Vol -> r2 [1.7fxp]
	LSL	r1, #0x10
	ASR	r1, #0x10
.LMixer_VoxLoop_GetTrackState_Done:

/************************************************/

.LMixer_VoxLoop_UpdateEGs:
	MOV	sl, r4                    @ &Vox -> sl

.LMixer_VoxLoop_UpdateEG1:
	LDRH	r0, [r4, #0x08]           @ EG1 -> r0
	MOV	r4, r9                    @ Art -> r4
0:	LDR	r5, =.LMixer_VoxLoop_UpdateEG1_FuncTables
	LSL	r7, r6, #0x18+2
	BMI	.LMixer_VoxLoop_UpdateEG1_ProcessRelease
	LSL	r7, r6, #0x20-2-SGE_VOX_STAT_EG1_SHIFT
#if (SGE_VOX_STAT_EG1_SHIFT > 0)
	LSR	r7, #0x20-2
	LSL	r7, #0x02
#else
	LSR	r7, #0x20-2-2
#endif
	BEQ	.LMixer_VoxLoop_UpdateEG1_AttackOverride
	CMP	r7, #SGE_VOX_STAT_EG_HLD<<2 @ <- Hold state needs special handling
	LDR	r7, [r5, r7]              @ EGFunc -> r7
	BEQ	.LMixer_VoxLoop_UpdateEG1_ProcessHold

.LMixer_VoxLoop_UpdateEG1_ProcessNormal:
	ADD	r5, r0, #0x01             @ Vol *= EG1 [1.7 + 1.16 = 1.23fxp]
	MUL	r2, r5
# if SGE_USE_VOLSUBDIV
	STR	r5, [sp, #USEDEG1_SP_OFFS]
# endif
	BX	r7

.LMixer_VoxLoop_UpdateEG1_ProcessRelease:
	LDR	r7, [r5, #0x04*4]         @ Call Release function
	ADD	r5, r0, #0x01             @ Vol *= EG1
	MUL	r2, r5
# if SGE_USE_VOLSUBDIV
	STR	r5, [sp, #USEDEG1_SP_OFFS]
# endif
	BX	r7

@ NOTE: Attack advances the envelope immediately rather than on the next update
.LMixer_VoxLoop_UpdateEG1_AttackOverride:
	LDR	r7, [r5, #0x04*0]         @ Call Attack function
	BX	r7

/***********************/

@ These have to be sandwiched in somewhere or we run out of Bxx range

/***********************/

@ r4: &Vox
@ Loads Bnd -> r1, Vol -> r2, Pan -> r3

.LMixer_VoxLoop_GetSavedTrackState:
	LDR	r1, [r4, #0x2C]           @ Restore Vol,Pan,Bnd
	MOV	r2, #0xFF
	LSR	r3, r1, #0x08
	AND	r3, r2
	AND	r2, r1
	ASR	r1, #0x10
	B	.LMixer_VoxLoop_GetTrackState_Done

/***********************/

@ r4: &Vox
@ r5:
@ r6:  Stat | xxx<<8
@ r8: &Wav (also in r0)
@ r9: &Art (also in r1)

.LMixer_VoxLoop_KeyOn:
#if SGE_SUPPORT_ADPCM
	LDRB	r3, [r0, #0x00]           @ Wav.Frmt -> ip
#endif
#if (!SGE_USE_VOLSUBDIV && !SGE_PRECISE_KEYON)
	SUB	r6, #SGE_VOX_STAT_KEYON   @ Only clear KEYON if not ramping. When ramping, we detect this flag
		                          @ and then avoid ramping from the old value if we have no attack time.
		                          @ We also need this flag when using precise key-on timing, since we
		                          @ need to know if Phase contains a sample offset in it.
#endif
	MOV	r2, #0x18                 @ Vox.Data = &Wav.Data[] -> r2
	ADD	r2, r0
#if SGE_SUPPORT_ADPCM
	LSR	r3, #0x04
	CMP	r3, #SGE_WAV_FRMT_ADPCM4  @ ADPCM needs to load the initial samples
	BNE	1f
0:	LDRB	r5, [r0, #0x01]           @ Wav.Chan -> r5
	LDR	r0, [r2, #0x10*0+0x00]    @ Store Vox.ADPCM[] = Wav.ADPCM.Init[]
# if SGE_STEREO_WAVEFORMS
	LDR	r3, [r2, #0x10*1+0x00]
# endif
	LSL	r5, #0x04                 @ Seek to data (past the headers)
	STR	r0, [r4, #0x10]
# if SGE_STEREO_WAVEFORMS
	STR	r3, [r4, #0x14]
# endif
	ADD	r2, r5
	MOV	r3, #0xFF                 @ AdPos = 0
	LSL	r3, #0x08
	BIC	r6, r3
#endif
1:	STR	r2, [r4, #0x1C]
1:	LDRB	r2, [r1, #0x0E+0*5+0]     @ EG1.Attack -> r2
	LDRB	r3, [r1, #0x0E+0*5+1]     @ EG1.Hold -> r3
	SUB	r0, r2, #0x01             @ EG1.Attack?
	LSR	r0, #0x10                 @  Have attack: EG1=AttackStep/2 -> r0 | No attack: EG1=1.0
	BEQ	.LMixer_VoxLoop_KeyOn_Attack
0:	ADD	r6, r6, #0x01<<SGE_VOX_STAT_EG1_SHIFT
	LDRB	r2, [r1, #0x0E+0*5+2]     @ EG1.Decay -> r2
	SUB	r3, #0x01                 @ If we have Hold, then set EG1=0.0 regardless
	LSR	r0, r3, #0x10             @ Else, set EG1=1.0 for Decay phase
	BEQ	1f
0:	ADD	r6, r6, #0x01<<SGE_VOX_STAT_EG1_SHIFT
	LDRB	r3, [r1, #0x0E+0*5+3]     @ EG1.Sustain -> r3
	CMP	r2, #0x00                 @ If we have Decay, then all set
	BNE	1f
0:	ADD	r6, r6, #0x01<<SGE_VOX_STAT_EG1_SHIFT
	LSL	r0, r3, #0x08             @ Set EG1=Sustain
	ADD	r0, r3
.LMixer_VoxLoop_KeyOn_Attack_Return:
1:	STRH	r0, [r4, #0x08]           @ Store EG1
1:	LDRB	r2, [r1, #0x0E+1*5+0]     @ Ditto EG2
	LDRB	r3, [r1, #0x0E+1*5+1]
	SUB	r2, #0x01
	LSR	r0, r2, #0x10
	BEQ	1f
0:	ADD	r6, r6, #0x01<<SGE_VOX_STAT_EG2_SHIFT
	LDRB	r2, [r1, #0x0E+1*5+2]
	SUB	r3, #0x01
	LSR	r0, r3, #0x10
	BEQ	1f
0:	ADD	r6, r6, #0x01<<SGE_VOX_STAT_EG2_SHIFT
	LDRB	r3, [r1, #0x0E+1*5+3]
	CMP	r2, #0x00
	BNE	1f
0:	ADD	r6, r6, #0x01<<SGE_VOX_STAT_EG2_SHIFT
	LSL	r0, r3, #0x08
	ADD	r0, r3
1:	STRH	r0, [r4, #0x0A]           @ Store EG2
1:	MOV	r0, #0x00                 @ LFO = 0, Phase = 0
	STR	r0, [r4, #0x0C]
#if !SGE_PRECISE_KEYON
	STRH	r0, [r4, #0x18]
#endif
	B	.LMixer_VoxLoop_KeyOn_Done

@ Without ramping, we have a trade-off where the EG value is
@ set to half-way between 0 and the next attack level
.LMixer_VoxLoop_KeyOn_Attack:
#if !SGE_USE_VOLSUBDIV
# if SGE_VARIABLE_SYNC_RATE
	MOV	r5, r2
	BL	.LMixer_VoxLoop_GetLinearStep
	LSR	r0, r5, #0x01
# else
	LDR	r0, =SGE_EnvelopeLUT_Linear
	LSL	r2, #0x01
	LDRH	r0, [r0, r2]
# endif
	LSR	r0, #0x01
#else
	STRH	r0, [r4, #0x06]           @ Store "old" volume as 0
	STRH	r0, [r4, #0x1A]
	SUB	r6, #SGE_VOX_STAT_KEYON   @ Clear KEYON so that we ramp from the "old" value of 0
#endif
	B	.LMixer_VoxLoop_KeyOn_Attack_Return

@ NOTE: KEYOFF marks the notes as though in Sustain phase
.LMixer_VoxLoop_KeyOff:
	MOV	r0, #SGE_VOX_STAT_EG_MSK << SGE_VOX_STAT_EG1_SHIFT
	AND	r0, r6                    @ Reset EGs to 1.0 when they are in Hold (EG value is abused as a counter)
	BNE	0f
	@MOV	r1, r9
	LDRB	r0, [r1, #0x0A]           @ Check for curved attack
	LSR	r0, #0x01
	BCC	1f
	LDRH	r0, [r4, #0x08]           @ EG1 Attack is on a curve, so apply it now
	MOV	r1, r0
	MUL	r1, r0
	LSL	r0, #0x01
	LSR	r1, #0x10
	SUB	r0, r1
	LSR	r1, r0, #0x10             @ <- EG1 value can end up as 1.0, so make sure to not overflow
	SUB	r0, r1
	STRH	r0, [r4, #0x08]
	B	1f
0:	BIC	r6, r0                    @ Clear envelope phase
	SUB	r0, #SGE_VOX_STAT_EG_HLD << SGE_VOX_STAT_EG1_SHIFT
	BNE	1f
	MVN	r0, r0
	STRH	r0, [r4, #0x08]
1:	MOV	r0, #SGE_VOX_STAT_EG_MSK << SGE_VOX_STAT_EG2_SHIFT
	AND	r0, r6
	BIC	r6, r0
	SUB	r0, #SGE_VOX_STAT_EG_HLD << SGE_VOX_STAT_EG2_SHIFT
	BNE	1f
	MVN	r0, r0
	STRH	r0, [r4, #0x0A]
1:	ADD	r6, #SGE_VOX_STAT_EG_SUS * ((1 << SGE_VOX_STAT_EG1_SHIFT) | (1 << SGE_VOX_STAT_EG2_SHIFT))
	B	.LMixer_VoxLoop_KeyOff_Done @ We need to reset the envelope phase to something that we do not modify here

/***********************/
.pool
/***********************/

.LMixer_VoxLoop_UpdateEG1_ProcessHold:
	LSL	r2, #0x10                 @ Vol *= EG1(=1.0) (needed because value in EG1 is abused as a counter)
#if SGE_USE_VOLSUBDIV
	MOV	r5, #0x01
	LSL	r5, #0x10
	STR	r5, [sp, #USEDEG1_SP_OFFS]
#endif
	BX	r7

.LMixer_VoxLoop_UpdateEG1_Done:
.LMixer_VoxLoop_ApplyArticulationVolume:
	LSR	r2, #(23-16)              @ [Vol -> 1.16fxp]
	LSR	r5, r6, #0x18             @ Vol *= Vox.Vel -> r5 (1.16 + 1.7 = 1.23fxp]
	MUL	r5, r2
	MOV	r7, #0x01
	LDRSB	r7, [r4, r7]              @ Art.Pan -> r7
	LDRB	r2, [r4, #0x00]           @ Art.Vol -> r2
	SUB	r3, #0x40                 @ Unbias Pan to -3Fh..+3Fh, and scale to .7fxp
	LSL	r3, #0x01
	ADD	r3, r7                    @ Pan += Art.Pan
	MUL	r2, r5                    @ Vol *= Art.Vol -> r2 [1.23 + 1.8 = 1.31fxp]
	MOV	r4, sl                    @ Restore &Vox -> r4
	ADD	r5, r2                    @ Fix Art.Vol bias -> r5
	MOV	r2, r8
	LDRB	r2, [r2, #0x0F]           @ Wav.Gain -> r2
	STRH	r0, [r4, #0x08]           @ Store EG1
	LSR	r5, #(31-24)              @ Vol -> 1.24fxp
	MUL	r2, r5                    @ Vol *= Wav.Gain -> r2 [1.24fxp + 1.7 = 1.31fxp]

@ NOTE: EG2 maxes out at 65535/65536
.LMixer_VoxLoop_UpdateEG2:
	LDRH	r0, [r4, #0x0A]           @ EG2 -> r0
	MOV	r5, #0x0C                 @ offsetof(EG2ToKey) -> r5
0:	LDR	r4, =.LMixer_VoxLoop_UpdateEG2_FuncTables
	LSL	r7, r6, #0x18+2
	BMI	.LMixer_VoxLoop_UpdateEG2_ProcessRelease
	LSL	r7, r6, #0x20-2-SGE_VOX_STAT_EG2_SHIFT
#if (SGE_VOX_STAT_EG2_SHIFT > 0)
	LSR	r7, #0x20-2
	LSL	r7, #0x02
#else
	LSR	r7, #0x20-2-2
#endif
	CMP	r7, #SGE_VOX_STAT_EG_HLD<<2
	LDR	r7, [r4, r7]              @ EGFunc -> ip
.LMixer_VoxLoop_UpdateEG2_EnterProcess:
	MOV	r4, r9                    @ Art -> r4
	LDRSH	r5, [r4, r5]              @ EG2ToKey -> r5
	MOV	ip, r7
	BEQ	.LMixer_VoxLoop_UpdateEG2_ProcessHold_Continue

.LMixer_VoxLoop_UpdateEG2_ProcessNormal:
	MUL	r5, r0                    @ EG2ToKey *= EG2 (and shift to .8fxp)
	ASR	r5, #0x10
.LMixer_VoxLoop_UpdateEG2_ProcessHold_Continue:
	ADD	r1, r5                    @ Key += EG2 * ScaledEG2ToKey
	BX	ip

.LMixer_VoxLoop_UpdateEG2_ProcessRelease:
	LDR	r7, [r4, #0x04*4]         @ Call Release function
	@ Note that we must have Z=0 here
	B	.LMixer_VoxLoop_UpdateEG2_EnterProcess

/***********************/
.pool
/***********************/

.LMixer_VoxLoop_UpdateEG2_Done:
	MOV	r4, sl                    @ Restore &Vox
	STRH	r0, [r4, #0x0A]           @ Store EG2

.LMixer_VoxLoop_UpdateEGs_Done:
	STRH	r6, [r4, #0x00]           @ Store Stat (store halfword to set AdPos, which we might have modified)
	LSL	r6, #0x08
	LSR	r6, #0x18
	LSL	r6, #0x08
	ADD	r1, r6                    @ Key = Key + Bnd -> r1 [.8fxp]
	MOV	r6, r9                    @ And now we can put Art in r6

/************************************************/

.LMixer_VoxLoop_UpdateLFO:
	LDR	r0, [r4, #0x0C]           @ LFO | LFOFade<<16 -> r0
#if !SGE_VARIABLE_SYNC_RATE
	LDRB	r5, [r6, #0x04]           @ LFODelay -> r5
	LDR	r7, =SGE_EnvelopeLUT_Linear
	LSL	r5, #0x01
	LDRH	r5, [r7, r5]              @ FadeStep -> r5
#else
	LDRB	r5, [r6, #0x04]           @ LFODelay -> r5
	ADD	r7, sp, #0x08             @ FadeStep -> r5
	BL	.LMixer_VoxLoop_GetLinearStep_Core
#endif
	LDRB	r7, [r6, #0x05]           @ LFORate -> r7
	LSL	r5, #0x10                 @ PhaseStep -> upper 16 bits of r5
	ADD	r7, #0x01                 @ <- Rate is biased by 1
ASM_ALIGN(4)
	BX	pc
	NOP

ASM_MODE_ARM
.LMixer_VoxLoop_UpdateLFO_UpdateOscillator:
	LDRB	fp, [r6, #0x09]           @ LFOShape|LFORamp -> fp
#if !SGE_VARIABLE_SYNC_RATE
	LDR	ip, =SGE_EnvelopeLUT_Linear
	MOV	sl, r7, lsl #0x0A-4       @ PhaseStep = LFORateHz * (2^16 * AGB_FRAME_CYCLES / AGB_HW_FREQ_HZ) ~= (1 + 2^-4)(1 + 2^-7)*2^10
	ADD	sl, sl, sl, lsr #0x04     @ For this level of precision, the NDS version has the same scaling constant
	ADD	sl, sl, sl, lsr #0x07
#else
	ADD	sl, sp, #0x08             @ RateScale -> sl, RateHz | N<<16 -> ip
	LDMIA	sl, {sl,ip}
	MOV	ip, ip, lsr #0x10         @ PhaseStep = 2^16 * LFORateHz * N / SampRateHz
	MUL	r7, ip, r7
	UMULL	ip, sl, r7, sl
	MOVS	ip, ip, lsr #(INVRATE_BITS+4-16)
	ADC	sl, ip, sl, lsl #0x20-(INVRATE_BITS+4-16)
#endif
	CMP	r0, r0                    @ Set C=1
	TST	fp, #0x10|0x20            @ Ramping at all?
	CMNEQ	r0, #0x01<<16             @  N: Set PhaseStep=0 until LFOFade==1.0
	BCC	.LMixer_VoxLoop_UpdateLFO_UpdateOscillator_Delay
0:	TST	fp, #0x20                 @ LFOFrqRamp?
	MOVNE	r7, r0, lsr #0x10         @  PhaseStep *= (LFOFade+1) (this ensures PhaseStep can be scaled by 1.0)
	MLANE	sl, r7, sl, sl
	MOVNE	sl, sl, lsr #0x10
0:	AND	ip, fp, #0x0F             @ LFOShape -> ip
	RSB	r7, ip, ip, lsl #0x04     @ LFOSign = LFOShape/5 = LFOShape*13/64 -> r7
	SUB	r7, r7, ip, lsl #0x01
	MOV	r7, r7, lsr #0x06
	SUB	ip, ip, r7                @ LFOShape = LFOShape%5 -> ip
	SUB	ip, ip, r7, lsl #0x02
	LDR	pc, [pc, ip, lsl #0x02]   @ LFOKeyMod -> ip, LFOVolMod -> r9
	NOP
	.word .LMixer_VoxLoop_UpdateLFO_UpdateOscillator_Sine
	.word .LMixer_VoxLoop_UpdateLFO_UpdateOscillator_Tri
	.word .LMixer_VoxLoop_UpdateLFO_UpdateOscillator_Saw
	.word .LMixer_VoxLoop_UpdateLFO_UpdateOscillator_Square
	.word .LMixer_VoxLoop_UpdateLFO_UpdateOscillator_Noise
	.pool
.LMixer_VoxLoop_UpdateLFO_UpdateOscillator_ApplySign:
	TST	fp, #0x10                 @ LFOAmpRamp?
	MOVNE	lr, r0, lsr #(16+16-15)   @  Y: LFOKeyMod *= LFOFade, LFOVolMod *= LFOFade
	MLANE	ip, lr, ip, ip            @     Only use the upper 15bits of LFOFade or we may overflow
	MLANE	r9, lr, r9, r9
	MOVNE	ip, ip, asr #0x0F
	MOVNE	r9, r9, lsr #0x0F
	CMP	r7, #0x01                 @ Check sign mode (0: +/-, 1: +, 2: -)
	ANDCS	ip, ip, ip, asr #0x1F     @  +/-: LFOKeyMod = Min[0,LFOKeyMod] (only the negative values)
	RSBEQ	ip, ip, #0x00             @  +: LFOKeyMod = -LFOKeyMod (always positive now)
0:	MOV	sl, sl, lsl #0x10         @ PhaseStep -> upper 16 bits of sl
	ADD	r0, sl, r0, ror #0x10     @ Phase += PhaseStep
	ADDS	r0, r5, r0, ror #0x10     @ Fade += FadeStep?
0:	ORRCS	r0, r0, #0x00FF<<16       @  Clip on overflow
	ORRCS	r0, r0, #0xFF00<<16
#if (!defined(__NDS__) || __NDS__ != 9)
	ADR	lr, .LMixer_VoxLoop_UpdateLFO_Finish+1
	BX	lr
#else
	BLX	.LMixer_VoxLoop_UpdateLFO_Finish
#endif
.LMixer_VoxLoop_UpdateLFO_UpdateOscillator_Delay:
	ADDS	r0, r0, r5                @ Fade += FadeStep?
	MOV	ip, #0x00                 @ Force values for Phase=0
	MOV	r7, #0x00
	B	0b

ASM_MODE_THUMB
.LMixer_VoxLoop_UpdateLFO_Finish:
	STR	r0, [r4, #0x0C]           @ Store LFO
	MOV	r0, #0x06
	LDRSH	r0, [r6, r0]              @ LFOToKey -> r0
	MOV	r5, r9                    @ LFOVolMod -> r5
	MOV	r7, ip                    @ LFOKeyMod -> r7
	MUL	r0, r7                    @ Key += LFOKeyMod*LFOToKey [1.14 + 8.8 = 8.22fxp]
	LDRB	r7, [r6, #0x08]           @ LFOToVol -> r7
	ASR	r0, #(22-8)               @ Key -> 8.8fxp
	ADC	r1, r0
	MUL	r7, r5                    @ VolMod = LFOToVol*LFOVolMod -> r7 [.8 + 1.14 = .22fxp]
	LSR	r5, r2, #(31-16)          @ Vol in 1.16fxp -> r5
	LSR	r7, #(22-15)              @ VolMod -> .15fxp
	MUL	r7, r5                    @ dVol = Vol*VolMod -> r7 [1.16 + .15fxp = .31fxp]
	SUB	r2, r7                    @ Vol -= dVol

.LMixer_VoxLoop_ApplyPortamento:
	LDR	r5, [r4, #0x28]           @ Track -> r5?
	CMP	r5, #0x00
	BEQ	.LMixer_VoxLoop_ApplyPortamento_Finish
	LDRB	r0, [r5, #0x00]           @ Check for portamento enable
	LSR	r0, #0x02
	BCC	.LMixer_VoxLoop_ApplyPortamento_Finish
	ADD	r5, #0x24
	LDRB	r0, [r5, #0x02]           @ Portamento.Value -> r0
	LDRB	r7, [r5, #0x03]           @ Portamento.Target -> r7
	LDRB	r5, [r5, #0x00]           @ Portamento.Phase -> r5
	SUB	r7, r0, r7                @ Delta = Target - Value. Delta < 0?
	LSL	r7, #0x08                 @ Build Key|Fine via (Delta<<8)+Phase
	ADD	r7, r5
	ADD	r1, r7                    @ Key += Key|Fine
.LMixer_VoxLoop_ApplyPortamento_Finish:

/************************************************/

.LMixer_VoxLoop_PrepareToMix:
	MOV	r5, r8
	LDRB	r0, [r5, #0x07]           @ Wav.Root -> r0
	LDRB	r5, [r5, #0x0B]           @ Wav.Fine -> r5
	MOV	r7, #0x02
	LDRSH	r7, [r6, r7]              @ Art.Tune -> r7
	LSL	r0, #0x08                 @ Key = Key - Root*256 + Fine + Art.Tune -> r1
	SUB	r1, r0
	ADD	r1, r5
	ADD	r1, r7
	ADD	r3, #0x7E                 @ Bias Pan to 00h..FCh
	ASR	r7, r3, #0x1F
	BIC	r3, r7
	CMP	r3, #0xFC                 @ Clamp Pan to 00h..FCh
	BCC	0f
	MOV	r3, #0xFC
ASM_ALIGN(4)
0:	BX	pc                        @ PanL = Cos[(Pan+1.0)/2.0 * Pi/2] -> r5, PanR = Sin[(Pan+1.0)/2.0 * Pi/2] -> r7
	NOP
ASM_MODE_ARM
	RSB	r5, r3, #0xFC             @ Flip for cosine -> r5
	MUL	ip, r3, r3                @ zs^2 -> ip
	MUL	lr, r5, r5                @ zc^2 -> lr
	MOV	r7, #0x0180               @ a -> r7 (need to add 1/8 later)
	SUB	ip, r7, ip, lsr #0x09     @ a + b*zs^2 -> ip
	SUB	lr, r7, lr, lsr #0x09     @ a + b*zc^2 -> lr
	MUL	r7, ip, r3                @ zs*(a+b*zs^2) -> r7
	MUL	ip, lr, r5                @ zc*(a+b*zc^2) -> ip
	ADD	r7, r7, r3, lsr #0x04     @ Add 9/128 factor to a to get exact 1.0 as needed
	ADD	r7, r7, r3, lsr #0x07
	ADD	ip, ip, r5, lsr #0x04
	ADD	r5, ip, r5, lsr #0x07
	ADR	ip, .LMixer_VoxLoop_ApplyPan_Finish+1
	BX	ip
ASM_MODE_THUMB
.LMixer_VoxLoop_ApplyPan_Finish:
0:	LSR	r2, #(31-15)              @ Vol -> 1.15fxp
	MUL	r7, r2                    @ VolR -> r7 [1.15 + 1.16 = 1.31fxp]
	MUL	r2, r5                    @ VolL -> r2
	LSR	r3, r7, #(31-15)          @ VolR -> 1.15fxp -> r3
	LSR	r2, #(31-15)              @ VolL -> 1.15fxp -> r2
#if !SGE_USE_VOLSUBDIV
	STRH	r2, [r4, #0x06]           @ Store new volumes to voice
	STRH	r3, [r4, #0x1A]
#endif
	LDRH	r7, [r4, #0x18]           @ Position = Phase -> r7
ASM_ALIGN(4)
	BX	pc
	NOP
ASM_MODE_ARM
	LDR	r9, [sp, #0x14]           @ Dst   = MixBuf -> r9
#if SGE_USE_VOLSUBDIV
	LDRH	r6, [r4, #0x06]           @ Load previous volume
	LDRH	ip, [r4, #0x1A]
	STRH	r2, [r4, #0x06]           @ Store new volumes to voice
	STRH	r3, [r4, #0x1A]
#endif
	MOVS	r2, r2, lsr #(15-SGE_MIXER_VOLBITS) @ Re-scale VolL/R (plus round off when collapsed to 0)
	ADCEQ	r2, r2, #0x00
	MOVS	r3, r3, lsr #(15-SGE_MIXER_VOLBITS)
	ADCEQ	r3, r3, #0x00
	LDRH	r5, [sp, #0x0E]           @ MxCnt = N -> r5
#if (SGE_USE_VOLSUBDIV || SGE_PRECISE_KEYON)
	LDRB	fp, [r4, #0x00]
	TST	fp, #SGE_VOX_STAT_KEYON   @ If we have KEYON, then we must NOT ramp (and must skip samples as needed)
	BICNE	fp, fp, #SGE_VOX_STAT_KEYON
	STRNEB	fp, [r4, #0x00]
# if SGE_USE_VOLSUBDIV
	ORR	r5, r5, r5, lsl #0x10     @ MxCnt | MxCntPerSubdiv<<16 -> r5
	ORRNE	r6, r2, r3, lsl #0x10
# endif
# if !SGE_PRECISE_KEYON
	BNE	1f
# else
	BNE	.LMixer_VoxLoop_KeyOnShift
# endif
0:	MOVS	r6, r6, lsr #(15-SGE_MIXER_VOLBITS) @ Re-scale the old volume in the same way we scaled the new volume
	ADCEQ	r6, r6, #0x00
	MOVS	ip, ip, lsr #(15-SGE_MIXER_VOLBITS)
	ADCEQ	ip, ip, #0x00
	ADD	fp, r6, ip                @ VolOld = VolL_Old + VolR_Old -> fp
	ADD	lr, r2, r3                @ VolNew = VolL_New + VolR_New -> lr
	CMP	fp, lr                    @ VolMin -> fp, VolMax -> lr
	EORCS	fp, fp, lr
	EORCS	lr, lr, fp
	EORCS	fp, fp, lr
	@    (VolMax/VolMin) * (RateHz/1000)/N > MaxRatioPerMs?
	@ -> (VolMax/VolMin) > MaxRatioPerMs * N/(RateHz/1000)?
	@ Not quite correct, but close enough. Real formula should be:
	@  20Log10[VolMax/VolMin] * (RateHz/1000)/N > MaxDecibelChangePerMs?
	@ However... the extra steps of multiplying by N/RateHz, while doable,
	@ adds arguably unnecessary complexity, so we just simplify to:
	@     VolMax/VolMin > MaxRatio?
	@  -> VolMax > VolMin*MaxRatio?
	@  -> VolMax/MaxRatio > VolMin?
	@ And to make things even faster, we use MaxRatio = 1/(1-2^-M).
	@ For example, M=2 sets MaxRatio=1/(1-1/4) = 4/3 = +/-2.5dB.
	SUB	lr, lr, lr, lsr #SGE_VOLSUBDIV_LOG2RATIO
	CMP	lr, fp                    @ Ratio not large enough?
	CMPHI	lr, #0x01                 @ (and also make sure we have VolMax/MaxRatio > 1)
	ORRLS	r6, r2, r3, lsl #0x10     @  Y: Vol = VolL_New | VolR_New<<16 -> r6
	BLS	1f
0:	SUB	r2, r2, r6                @ DeltaVolume = Vol_New - Vol_Old -> r2,r3
	SUB	r3, r3, ip
	ORR	r6, r6, ip, lsl #0x10     @ Vol = VolL_Old | VolR_Old<<16 -> r6
	ORR	fp, fp, lr, lsl #0x10     @ VolMin | VolMax/MaxRatio<<16 -> fp
# ifdef __GBA__
	@ For the GBA mixer, we must test for the maximum subdivision
	@ level because we must store bias levels, and so cannot rely
	@ only on VOLSUBDIV_MINLENGTH.
	@ Additionally, because volume is only 8-bit, we can use the
	@ remaining bits to keep the volume delta exact, which is a
	@ somewhat more pressing concern on GBA than NDS due to the
	@ much lower resolution of the volumes.
	MOV	ip, #(1<<SGE_VOLSUBDIV_LOG2MAXSUBDIV)-1
	MOV	r2, r2, lsl #SGE_VOLSUBDIV_LOG2MAXSUBDIV
	MOV	r3, r3, lsl #SGE_VOLSUBDIV_LOG2MAXSUBDIV
# endif
0:	CMP	r5, #(SGE_VOLSUBDIV_MINLENGTH*2)<<16 @ Reached maximum subdivision level?
# ifdef __GBA__
	MOVCSS	ip, ip, lsr #0x01         @  N: Check for maximum subdivision
	MOVCS	r2, r2, asr #0x01         @     DeltaL /= 2 (rounding not needed, because we already
	MOVCS	r3, r3, asr #0x01         @     DeltaR /= 2  shifted the values up for this to work)
# else
	SUBCS	r2, r2, r2, asr #0x1F     @     DeltaL /= 2 (and round towards 0 for negative values)
	MOVCS	r2, r2, asr #0x01
	SUBCS	r3, r3, r3, asr #0x1F     @     DeltaR /= 2
	MOVCS	r3, r3, asr #0x01
# endif
	MOVCS	lr, r5, lsr #0x10+1       @     MxCntPerSubdiv /= 2 (round UP to avoid stray samples at the end)
	SUBCS	r5, r5, lr, lsl #0x10
	ADDCS	lr, fp, #0x01<<16         @ MaxRatio *= 2 (via (VolMax/MaxRatio) /= 2, rounding DOWN)
	MOVCS	lr, lr, lsr #0x10+1
	SUBCS	fp, fp, lr, lsl #0x10
	CMPCS	fp, fp, lsl #0x10         @ Ratio still too large?
	BHI	0b
0:	ADD	r3, r2, r3, lsl #0x10     @ VolStep = DeltaL|DeltaR<<16 -> r3
#endif
.LMixer_VoxLoop_KeyOnShift_Finish:
#if SGE_USE_VOLSUBDIV
1:	MOV	r2, r5, lsr #0x10         @ SubdivCounter = MxCntPerSubdiv -> r2
	ADD	fp, sp, #VOLSTEP_DIVCOUNT_SP_OFFS
# ifdef __GBA__
	MOV	r6, r6, lsl #SGE_VOLSUBDIV_LOG2MAXSUBDIV
	STMIA	fp, {r2,r3,r6}            @ Store {SubdivCounter,VolStep,VolCounter}
	MOV	sl, #(1<<SGE_VOLSUBDIV_LOG2MAXSUBDIV)
1:	LDR	ip, [fp, -sl, lsl #0x02]
	MOV	lr, r5, lsl #0x10
	SUBS	r2, r2, lr, lsr #0x10+SGE_VOLSUBDIV_LOG2MAXSUBDIV
	ADDLS	r2, r2, r5, lsr #0x10
	BIC	lr, r6, #((1<<SGE_VOLSUBDIV_LOG2MAXSUBDIV)-1)<<16
	ADD	ip, ip, lr, lsr #SGE_VOLSUBDIV_LOG2MAXSUBDIV
	STR	ip, [fp, -sl, lsl #0x02]
	ADDLS	r6, r6, r3
	SUBS	sl, sl, #0x01
	BNE	1b
# else
	STMIA	fp, {r2,r3}               @ Store {SubdivCounter,VolStep}
# endif
#else
	ORR	r6, r2, r3, lsl #0x10     @ VolL | VolR<<16 -> r6
#endif
0:	LDR	ip, =SGE_KeyScale         @ Octave = Key/12 -> r2
	MOV	r0, r1, asr #0x08-SGE_NOTELUT_BITS @ int(Key) -> r0
	ADD	r2, r0, r0, asr #0x02     @ x/12 ~= x*(1+2^-2)(1+2^-4)(1+2^-8)(1+2^-16)*2^-4 (up to rounding)
	ADD	r2, r2, r2, asr #0x04
#if (24+SGE_NOTELUT_BITS >= 28)
	ADD	r2, r2, #0x06             @ Adding 6 here then subtracting 4 later works for x == -2^28..+2^28
#else
	ADD	r2, r2, #0x02             @ Adding 2 here works for x == -2^27..+2^27... fixed-point maths is wack yo
#endif
	ADD	r2, r2, r2, asr #0x08     @ NOTE: We want the quotient here, not the integer part
	ADD	r2, r2, r2, asr #0x10
#if (24+SGE_NOTELUT_BITS > 27)
	SUB	r2, r2, #0x04
#endif
	MOV	r2, r2, asr #0x04+SGE_NOTELUT_BITS
	SUB	r0, r0, r2, lsl #0x02+SGE_NOTELUT_BITS @ Key = Key%12 = Key - Octave*12 -> r0
	SUB	r0, r0, r2, lsl #0x03+SGE_NOTELUT_BITS
	ADD	ip, ip, r0, lsl #0x02                  @ a = Scale[Key] -> ip, b = Scale[Key+1] -> ip [.31fxp]
	LDMIA	ip, {ip,lr}
	LDR	r3, [r8, #0x0C]                        @ WavFreq = Wav.Freq -> r3
	ANDS	r0, r1, #(1<<(8-SGE_NOTELUT_BITS))-1   @ Tune -> r0?
	SUBNE	lr, lr, ip                             @  Interpolate between a and b by Tune
	MULNE	r0, lr, r0
	BIC	r3, r3, #0xFF<<24
	ADDNE	ip, ip, r0, lsr #(8-SGE_NOTELUT_BITS) @ KeyScale -> ip
	RSBS	lr, r2, #0x1B-FREQ_BITS   @ ShiftDown = KEYSCALE_BITS - Octave - FREQ_BITS -> lr?
	UMULL	r0, r1, ip, r3            @ Freq = KeyScale * WavFreq -> r0,r1
	LDR	r2, [sp, #0x08]           @ RateScale -> r2
	LDRH	ip, [sp, #0x0C]           @ RateHz -> ip
	BPL	11f
10:	RSB	lr, lr, #0x00             @ ShiftDown < 0: Need to shift up
	RSB	r3, lr, #0x20
	MOVS	sl, r1, lsr r3
	MVNNE	r0, #0x00
	MVNNE	r1, #0x00
	MOVEQ	r1, r1, lsl lr
	ORREQ	r1, r1, r0, lsr r3
	MOVEQ	r0, r0, lsl lr
	B	0f
11:	CMP	lr, #0x20                 @ Check for ShiftDown >= 32
	MOVCS	r0, r1
	MOVCS	r1, #0x00
	SUBCS	lr, lr, #0x20
	RSB	r3, lr, #0x20
	MOVS	r0, r0, lsr lr            @ Freq *= 2^Octave (with rounding)
	ADC	r0, r0, r1, lsl r3
	MOV	r1, r1, lsr lr
0:
#if (SCALEDRATE_BITS + MAX_RATE_LOG2 < 64)
	CMP	r0, ip, lsl #MAX_RATE_LOG2+FREQ_BITS @ Freq/RateHz >= MAX_RATE? (via Freq >= RateHz*MAX_RATE)
	SBCS	ip, r1, ip, lsr #0x20-(MAX_RATE_LOG2+FREQ_BITS)
	UMULLCC	r0, sl, r2, r0            @   N: Rate = Freq*RateScale -> r0,sl, then cast to final precision
	MOVCS	r0, #0x00                 @   Y: Rate = MAX
	MOVCS	sl, #MAX_RATE<<(SCALEDRATE_BITS-32)
	MLACC	sl, r2, r1, sl
#else
	UMULL	r0, sl, r2, r0
	MLA	sl, r2, r1, sl
#endif
#if (SCALEDRATE_BITS-RATE_BITS <= 32)
	MOVS	r0, r0, lsr #(SCALEDRATE_BITS-RATE_BITS)
	ADC	sl, r0, sl, lsl #0x20-(SCALEDRATE_BITS-RATE_BITS)
#else
	MOVS	sl, sl, lsr #(SCALEDRATE_BITS-RATE_BITS-32)
	ADC	sl, sl, #0x00
#endif

/************************************************/

@ r0:
@ r1:
@ r2:
@ r3:
@ r4: &Src
@ r5:  MxCnt (without SGE_USE_VOLSUBDIV), or MxCnt | MxCntPerSubdiv<<16 (with SGE_USE_VOLSUBDIV, already loaded)
@ r6:  VolL | VolR<<16
@ r7:  Position
@ r8: &Wav
@ r9: &Dst
@ sl:  Rate
@ fp:
@ ip:
@ lr:
@ sp+00h: &Vox
@ sp+04h:  nVoxRem
@ sp+08h:  RateScale
@ sp+0Ch:  RateHz | N<<16
@ sp+10h: &Driver
@ sp+14h: &MixBuf
@ sp+18h:  BiasL | BiasR<<16 (or VolStepScale)

.LMixer_VoxLoop_MixLoop:
	LDRH	r3, [r8, #0x00]            @ Wav.{Interpolate|Frmt,Chan} -> r3,lr
#if (defined(__GBA__) && !SGE_USE_VOLSUBDIV)
	LDR	r2, [sp, #0x18]            @ Bias -> r2
#endif
	MOV	lr, r3, lsr #0x08
	BIC	r3, r3, lr, lsl #0x08
	MOV	ip, r3, lsr #0x04          @ Wav.Frmt -> ip
#if SGE_SUPPORT_ADPCM
	CMP	ip, #SGE_WAV_FRMT_ADPCM4
	LDREQB	r0, [r4, #0x01]            @ Position += AdPos (with ADPCM)
#endif
	LDR	r4, [r4, #0x1C]            @ Src   = Vox.Data -> r4
#if SGE_SUPPORT_ADPCM
	ADDEQ	r7, r7, r0, lsl #RATE_BITS
#endif
#ifdef __GBA__
# if !SGE_USE_VOLSUBDIV
	ADD	r2, r2, r6                 @ Bias += Vol
	STR	r2, [sp, #0x18]
# endif
0:
# if (SGE_SUPPORT_LERP && !SGE_ALWAYS_LERP)
	MOVS	r3, r3, lsr #0x01          @ InterpolateBit | Frmt<<1 -> r0
	ADC	r0, ip, ip
	CMP	lr, #0x02                  @ MixerIdx = RateGT | Stereo<<1 | Interpolate<<2 | Frmt<<3 -> r0
	ADC	r0, r0, r0
	CMP	sl, #0x01<<16              @ (Rate < 1.0) ? (~0) : 0 -> r2
	SBC	r2, r2, r2
	EOR	r2, r2, r0, lsl #0x1F-1    @ (Interpolate == GT && Rate < 1.0) || (Interpolate == LT && Rate < 1.0)?
	AND	r2, r2, r3, lsl #0x1F      @  If not using rate-dependent interpolation flags, disable the adjustment
	EOR	r0, r0, r2, lsr #0x1F-1    @  Toggle interpolation (LT sets Interpolate=0 and GT sets Interpolate=1 by default)
	ADC	r0, r0, r0
# else
	CMP	lr, #0x02                  @ MixerIdx = RateGT | Stereo<<1 | Frmt<<2 -> r0
	ADC	r0, ip, ip
	CMP	sl, #0x01<<16
	ADC	r0, r0, r0
# endif
	STR	r0, .LMixerCore_MixFormat
	CMP	ip, #SGE_WAV_FRMT_CNT      @ Sanity check Wav.Frmt, Wav.Chan
	CMPCC	lr, #SGE_WAV_CHAN_MAX+1
	LDRCC	r2, =SGE_Driver_MixerTables
	LDR	r3, .LMixerCore_WorkAreaType
	LDRCS	r0, =.LMixerCore_UnsupportedFormat
	ADDCC	r0, r2, r0, lsl #0x04
	CMP	r0, r3                     @ Need to load new mixer loop?
	ADRNE	fp, 0f
	STRNE	fp, [sp, #-0x04]!
	BNE	.LMixer_VoxLoop_MixLoop_LoadMixer
0:
#endif

.LMixer_VoxLoop_MixLoop_MixChunk:
#if !SGE_USE_VOLSUBDIV
	MOV	r0, r5                     @ MxCnt = MxRem -> r0
#else
	LDR	r0, [sp, #VOLSTEP_DIVCOUNT_SP_OFFS]
	MOV	r3, r5, lsl #0x10          @ MxCnt = MIN(MxRem, SubdivCounter) -> r0
	CMP	r0, r3, lsr #0x10
	MOVHI	r0, r3, lsr #0x10
#endif
0:	ADD	r2, r8, #0x18              @ DataBeg -> r2
#if SGE_SUPPORT_ADPCM
	CMP	ip, #SGE_WAV_FRMT_ADPCM4   @ Frmt == ADPCM?
	ADDEQ	r2, r2, lr, lsl #0x04      @  Skip ADPCM header[s]
#endif
	SUB	r3, r4, r2                 @ GlobalPosition - Position = (Src - DataBeg)*SamplesPerByte/nChan -> r3
#if SGE_SUPPORT_ADPCM
	MOVEQ	r3, r3, lsr #0x02          @  ADPCM: SamplesPerByte == 7/sizeof(Frame)
	RSBEQ	r3, r3, r3, lsl #0x03
#endif
#if SGE_STEREO_WAVEFORMS
	CMP	lr, #0x02
	MOVEQ	r3, r3, lsr #0x01
#endif
#if SGE_SUPPORT_PCM16
	CMP	ip, #SGE_WAV_FRMT_PCM16    @ Two bytes per sample for PCM16
	MOVEQ	r3, r3, lsr #0x01
#endif
	MLA	fp, sl, r0, r7             @ TargetGlobalPosition = (GlobalPosition-Position) + ((Position|Phase + nRem*Rate) >> BITS) -> fp
	LDR	lr, [r8, #0x04]            @ Wav.Size -> lr
	ADD	fp, r3, fp, lsr #RATE_BITS
	BIC	lr, lr, #0xFF<<24
	SUBS	fp, fp, lr                 @ Overstep = (TargetGlobalPosition - Wav.Size)?
	BCC	1f
0:	ADDS	fp, r3, #0x00              @ Save GlobalPosition-Position -> fp, and set C=0
	ADD	r0, r3, r7, lsr #RATE_BITS @ GlobalPosition -> r0
	RSB	r0, r0, lr                 @ MxCnt = Ceiling[(((Size-GlobalPosition) << BITS) - Phase) / Rate]
	ADD	r0, sl, r0, lsl #RATE_BITS
	MOV	r1, r7, lsl #(32-RATE_BITS)
	SBC	r0, r0, r1, lsr #(32-RATE_BITS)
	MOV	r1, sl
	BL	__aeabi_uidiv
	MLA	r1, sl, r0, r7             @ Recalculate Overstep
	LDR	lr, [r8, #0x04]
	ADD	fp, fp, r1, lsr #RATE_BITS
	BIC	lr, lr, #0xFF<<24
	SUB	fp, fp, lr
1:
#if (defined(__GBA__) && SGE_USE_VOLSUBDIV)
	LDR	r6, [sp, #VOLCOUNTER_SP_OFFS]
	BIC	r6, r6, #((1<<SGE_VOLSUBDIV_LOG2MAXSUBDIV)-1)<<16
	MOV	r6, r6, lsr #SGE_VOLSUBDIV_LOG2MAXSUBDIV
#endif
	STMFD	sp!, {r0,r5,r8,fp}         @ Push {MxCnt,MxRem,Wav,Overstep} and call mixer
#ifdef __GBA__
	B	.LMixerCore_WorkArea
#else
	LDRB	r3, [r8, #0x00]            @ Wav.Interpolate|Frmt -> r3
	LDRB	lr, [r8, #0x01]            @ Wav.Chan -> lr
	MOV	ip, r3, lsr #0x04          @ Wav.Frmt -> ip
# if (SGE_SUPPORT_LERP && !SGE_ALWAYS_LERP)
	MOVS	r3, r3, lsr #0x01          @ InterpolateBit | Frmt<<1 -> r0
	ADC	r1, ip, ip
	CMP	lr, #0x02                  @ MixerIdx = RateGT | Stereo<<1 | Interpolate<<2 | Frmt<<3 -> r1
	ADC	r1, r1, r1
	CMP	sl, #0x01<<15              @ (Rate < 1.0) ? (~0) : 0 -> r2
	SBC	r2, r2, r2
	EOR	r2, r2, r1, lsl #0x1F-1    @ (Interpolate == GT && Rate < 1.0) || (Interpolate == LT && Rate < 1.0)?
	AND	r2, r2, r3, lsl #0x1F      @  If not using rate-dependent interpolation flags, disable the adjustment
	EOR	r1, r1, r2, lsr #0x1F-1    @  Toggle interpolation (LT sets Interpolate=0 and GT sets Interpolate=1 by default)
	ADC	r1, r1, r1
# else
	CMP	lr, #0x02                  @ MixerIdx = RateGT | Stereo<<1 | Frmt<<2 -> r1
	ADC	r1, ip, ip
	CMP	sl, #0x01<<15
	ADC	r1, r1, r1
# endif
	CMP	ip, #SGE_WAV_FRMT_CNT      @ Sanity check Wav.Frmt, Wav.Chan
	CMPCC	lr, #SGE_WAV_CHAN_MAX+1
	LDRCC	pc, [pc, r1, lsl #0x02]
	BCS	.LMixer_VoxLoop_MixLoop_MixChunk_Tail
0:	DEFINE_MIXER_POINTERS(ADPCM)
	DEFINE_MIXER_POINTERS(PCM8)
	DEFINE_MIXER_POINTERS(PCM16)
#endif

.LMixer_VoxLoop_MixLoop_MixChunk_Tail:
	LDMFD	sp!, {r0,r5,r8,fp}         @ Restore {MxCnt,MxRem,Wav,Overstep}
	LDRH	ip, [r8, #0x00]            @ Wav.{Frmt,Chan} -> ip,lr
	CMP	fp, #0x00                  @ Overstep? (ie. need to stop or loop)
	MOV	lr, ip, lsr #0x08
	BIC	ip, ip, lr, lsl #0x08
	MOV	ip, ip, lsr #0x04
	BGE	.LMixer_VoxLoop_MixLoop_HandleLoop
.LMixer_VoxLoop_MixLoop_HandleLoop_Done:
#if SGE_USE_VOLSUBDIV
	ADD	r2, sp, #VOLSTEP_DIVCOUNT_SP_OFFS
# ifdef __GBA__
	LDMIA	r2, {r2,r3,r6}
# else
	LDMIA	r2, {r2,r3}
# endif
	SUBS	r2, r2, r0
	ADDLS	r2, r2, r5, lsr #0x10
	STR	r2, [sp, #VOLSTEP_DIVCOUNT_SP_OFFS]
# ifdef __GBA__
	ADDLS	r6, r6, r3                    @ Finished a subdivision - update volume
	STRLS	r6, [sp, #VOLCOUNTER_SP_OFFS] @ NOTE: GBA is kept "exact" because of the poor precision...
# else
	ADDLS	r6, r6, r3                    @       ... while NDS chops the precision early
# endif
#endif
	SUBS	r5, r5, r0                 @ MxRem -= MxCnt?
#if SGE_USE_VOLSUBDIV
	MOVS	r2, r5, lsl #0x10          @ Upper bits contain MxCntPerSubdiv, so we have to clear them
#endif
	BNE	.LMixer_VoxLoop_MixLoop_MixChunk

.LMixer_VoxLoop_MixLoop_Exit:
	MOV	r0, r7, lsr #RATE_BITS     @ IntPos = int(Position) -> r0
#if (RATE_BITS != 16)
	BIC	r7, r7, r0, lsl #RATE_BITS
#endif
	MOV	r3, r4                     @ Data = Src -> r3
	LDMFD	sp!, {r4-r5}               @ Restore {Vox,nVoxRem}
#if SGE_SUPPORT_PCM16
	CMP	ip, #SGE_WAV_FRMT_PCM16    @ Two bytes per sample for PCM16
	MOVEQ	r0, r0, lsl #0x01
#endif
#if SGE_SUPPORT_ADPCM
	CMP	ip, #SGE_WAV_FRMT_ADPCM4   @ ADPCM?
# if SGE_STEREO_WAVEFORMS
	MLANE	r3, r0, lr, r3             @  N: Data += IntPos*nChan
# else
	ADDNE	r3, r3, r0
# endif
	STREQB	r0, [r4, #0x01]            @  Y: Store Vox.AdPos = Position
#else
# if SGE_STEREO_WAVEFORMS
	MLA	r3, r0, lr, r3
# else
	ADD	r3, r3, r0
# endif
#endif
	LDRB	lr, [r4, #0x00]            @ Stat -> lr
#if SGE_USE_VOLSUBDIV
	LDR	r2, [sp, #USEDEG1_SP_OFFS-0x04*2] @ EG1 -> r2 (NOTE: This is AFTER popping r4 and r5!)
#else
	LDRH	r2, [r4, #0x08]
#endif
	AND	lr, lr, #SGE_VOX_STAT_EG_MSK << SGE_VOX_STAT_EG1_SHIFT
	CMP	lr, #SGE_VOX_STAT_EG_HLD << SGE_VOX_STAT_EG1_SHIFT
	MOVHI	lr, #0x01<<(8 + 16-SGE_EG1_LOG2THRESHOLD)
	CMP	r2, lr, lsr #0x08          @ EG1 below threshold (only apply when past attack and hold)?
	STRCCB	lr, [r4, #0x00]            @  Y: Kill voice
	STRCS	r3, [r4, #0x1C]            @  N: Store Vox.Data
	STRCSH	r7, [r4, #0x18]            @     Store Vox.Phase
#if (!defined(__NDS__) || __NDS__ != 9)
	LDR	ip, =.LMixer_VoxLoop_Tail+1
	BX	ip
#else
	BLX	.LMixer_VoxLoop_Tail
#endif

/************************************************/
#if SGE_PRECISE_KEYON
/************************************************/

.LMixer_VoxLoop_KeyOnShift:
	TST	fp, #SGE_VOX_STAT_NOPLAYER @ If we have no player, skip this whole thing
	BNE	0f
#if SGE_USE_VOLSUBDIV
	CMP	r7, r5, lsr #0x10          @ Clip to M=N-1 samples
	MOVCS	r7, r5, lsr #0x10
	SUBCS	r7, r7, #0x01
#else
	CMP	r7, r5
	SUBCS	r7, r5, #0x01
#endif
#ifdef __GBA__
	MOVS	r0, r7                     @ Skip M samples
	BLNE	.LMixer_VoxLoop_MixLoop_ApplySilence
#else
	ADD	r9, r9, r7, lsl #0x03
#endif
	SUB	r5, r5, r7                 @ MxCnt -= SkippedSamples
0:	MOV	r7, #0x00                  @ Reset Phase=0
	B	.LMixer_VoxLoop_KeyOnShift_Finish

/************************************************/
#endif
/************************************************/
.pool
/************************************************/

.macro EG_GetLinearStep
#if SGE_VARIABLE_SYNC_RATE
	BL	.LMixer_VoxLoop_GetLinearStep
#else
	LDR	r7, =SGE_EnvelopeLUT_Linear
	LSL	r5, #0x01
	LDRH	r5, [r7, r5]
#endif
.endm

.macro EG_GetExpDecStep
#if SGE_VARIABLE_SYNC_RATE
	BL	.LMixer_VoxLoop_GetExpDecStep
#else
	LDR	r7, =SGE_EnvelopeLUT_ExpDec
	LSL	r5, #0x01
	LDRH	r5, [r7, r5]
#endif
.endm

/************************************************/

@ r0:  EG
@ r1:  Bnd
@ r2:  Vol
@ r3:  Pan
@ r4: &Art
@ r5:
@ r6:  Stat | xxx<<8
@ r7:
@ Updates EG and Stat, destroys r5,r7

ASM_MODE_THUMB

.LMixer_VoxLoop_UpdateEG1_Attack:
	LDRB	r5, [r4, #0x0E+0*5+0]     @ EG1c -> r5
	EG_GetLinearStep
	ADD	r0, r5                    @ EG1 += EG1c?
	LSR	r7, r0, #0x10
	BNE	.LMixer_VoxLoop_UpdateEG1_Attack_Done
	LDRB	r7, [r4, #0x0A]           @ EG1Flags -> r5
	ADD	r5, r0                    @ NextEG1 = EG1+EG1c
	LSR	r5, #0x10-1               @ NextEG1 >= 1.5? Snap to next phase
	CMP	r5, #0x03
	BCS	.LMixer_VoxLoop_UpdateEG1_Attack_Done
	LSR	r7, #0x01                 @ Parabolic attack?
	BCS	2f
1:	ADD	r5, r0, #0x01             @  N: Vol *= EG1
	B	0f
2:	MOV	r7, r0                    @  Y: Vol *= 1-(1-EG1)^2 [1.7 + 1.16 = 1.23fxp]
	MUL	r7, r7                    @     Note: 1-(1-x)^2 = 2*x - x^2
	LSL	r5, r0, #0x01
	LSR	r7, #0x10
	SUB	r5, r7
0:	MUL	r2, r5                    @ Vol *= EG1
#if SGE_USE_VOLSUBDIV
	STR	r5, [sp, #USEDEG1_SP_OFFS]
#endif
	B	.LMixer_VoxLoop_UpdateEG1_Done

.LMixer_VoxLoop_UpdateEG1_Attack_Done:
	LSL	r2, #0x10                 @ Assume EG1 = 1.0 now and move to Hold/Decay
#if SGE_USE_VOLSUBDIV
	MOV	r5, #0x01
	LSL	r5, #0x10
	STR	r5, [sp, #USEDEG1_SP_OFFS]
#endif
	LDRB	r5, [r4, #0x0E+0*5+1]     @ Hold time?
	ADD	r6, #0x01 << SGE_VOX_STAT_EG1_SHIFT
	CMP	r5, #0x00
	BEQ	.LMixer_VoxLoop_UpdateEG1_Hold_Done
0:	MOV	r0, #0x01                 @ Force EG1 = 0.0+eps for Hold (must not be 0)
	B	.LMixer_VoxLoop_UpdateEG1_Done

.LMixer_VoxLoop_UpdateEG1_Hold:
	LDRB	r5, [r4, #0x0E+0*5+1]     @ EG1c -> r5
	EG_GetLinearStep
	ADD	r0, r5                    @ EG1 += EG1c?
	LSR	r7, r0, #0x10
	BNE	.LMixer_VoxLoop_UpdateEG1_Hold_Done
	ADD	r5, r0                    @ NextEG1 = EG1+EG1c
	LSR	r5, #0x10-1               @ NextEG1 >= 1.5? Snap to next phase
	CMP	r5, #0x03
	BCS	.LMixer_VoxLoop_UpdateEG1_Hold_Done
	B	.LMixer_VoxLoop_UpdateEG1_Done

.LMixer_VoxLoop_UpdateEG1_Hold_Done:
	MVN	r0, r5                    @ Reset EG1 = 1.0
	LDRB	r5, [r4, #0x0E+0*5+2]     @ Decay time?
	LSR	r0, #0x10
	ADD	r6, #0x01 << SGE_VOX_STAT_EG1_SHIFT
	CMP	r5, #0x00
	BEQ	.LMixer_VoxLoop_UpdateEG1_Decay_Done
	B	.LMixer_VoxLoop_UpdateEG1_Done

.LMixer_VoxLoop_UpdateEG1_Decay:
	LDRB	r5, [r4, #0x0E+0*5+2]     @ EG1c -> r5
	EG_GetExpDecStep
	LDRB	r7, [r4, #0x0E+0*5+3]     @ Sustain -> r7
	MUL	r5, r0                    @ EG1 *= EG1c
	LSL	r0, r7, #0x08
	ADD	r7, r0
	LSR	r0, r5, #0x10
	CMP	r0, r7                    @ EG1 < Sustain?
	BCC	.LMixer_VoxLoop_UpdateEG1_Decay_Done
	B	.LMixer_VoxLoop_UpdateEG1_Done

.LMixer_VoxLoop_UpdateEG1_Decay_Done:
	LDRB	r0, [r4, #0x0E+0*5+3]     @ Force EG1 = Sustain
	ADD	r6, #0x01 << SGE_VOX_STAT_EG1_SHIFT
	LSL	r5, r0, #0x08
	ADD	r0, r5
	B	.LMixer_VoxLoop_UpdateEG1_Done
/*
.LMixer_VoxLoop_UpdateEG1_Sustain:
	B	.LMixer_VoxLoop_UpdateEG1_Done
*/
.LMixer_VoxLoop_UpdateEG1_Release:
	LDRB	r5, [r4, #0x0E+0*5+4]     @ EG1c -> r5
	EG_GetExpDecStep
#if (!defined(SGE_PLATFORM_HAVE_REVERB) && defined(SGE_PLATFORM_HAVE_FAKE_REVERB))
	MOV	ip, r5                    @ EG1c -> ip
	LDR	r7, [sp, #0x10]           @ Driver -> r5
	LDRH	r5, [r7, #0x16]           @ ReverbDecay -> r5
	LDRH	r7, [r7, #0x14]           @ ReverbFb -> r7
	CMP	r5, ip                    @ If ReverbDecay is faster than EG1c, use EG1c to silence
	BLS	2f
	CMP	r0, r7                    @ EG1 <= Fb: Use ReverbDecay
	BLS	3f
1:	MOV	r5, ip                    @ EG1 > Fb: Use EG1c and clip to ReverbFb
	MUL	r5, r0
	LDR	r7, [sp, #0x10]           @ Driver -> r7
	LSR	r0, r5, #0x10
	LDRH	r5, [r7, #0x14]           @ ReverbFb -> r5
	CMP	r0, r5                    @ If EG1 fell below ReverbFb, clip to ReverbFb
	BHI	0f
	MOV	r0, r5
0:	B	.LMixer_VoxLoop_UpdateEG1_Done
2:	MOV	r5, ip                    @ Restore EG1c -> r5 and apply to EG1
3:
#endif
	MUL	r5, r0
	LSR	r0, r5, #0x10
	B	.LMixer_VoxLoop_UpdateEG1_Done

/************************************************/

ASM_MODE_THUMB

.LMixer_VoxLoop_UpdateEG2_Attack:
	LDRB	r5, [r4, #0x0E+1*5+0]     @ EG2c -> r5
	EG_GetLinearStep
	ADD	r0, r5                    @ EG2 += EG2c?
	LSR	r7, r0, #0x10
	BNE	.LMixer_VoxLoop_UpdateEG2_Attack_Done
	ADD	r5, r0                    @ NextEG2 = EG2+EG2c
	LSR	r5, #0x10-1               @ NextEG2 >= 1.5? Snap to next phase
	CMP	r5, #0x03
	BCS	.LMixer_VoxLoop_UpdateEG2_Attack_Done
	B	.LMixer_VoxLoop_UpdateEG2_Done

.LMixer_VoxLoop_UpdateEG2_Attack_Done:
	LDRB	r5, [r4, #0x0E+1*5+1]     @ Hold time?
	ADD	r6, #0x01 << SGE_VOX_STAT_EG2_SHIFT
	CMP	r5, #0x00
	BEQ	.LMixer_VoxLoop_UpdateEG2_Hold_Done
0:	MOV	r0, #0x01                 @ Force EG2 = 0.0+eps for Hold
	B	.LMixer_VoxLoop_UpdateEG2_Done

.LMixer_VoxLoop_UpdateEG2_Hold:
	LDRB	r5, [r4, #0x0E+1*5+1]     @ EG2c -> r5
	EG_GetLinearStep
	ADD	r0, r5                    @ EG2 += EG2c?
	LSR	r7, r0, #0x10
	BNE	.LMixer_VoxLoop_UpdateEG2_Hold_Done
	ADD	r5, r0                    @ NextEG2 = EG2+EG2c
	LSR	r5, #0x10-1               @ NextEG2 >= 1.5? Snap to next phase
	CMP	r5, #0x03
	BCS	.LMixer_VoxLoop_UpdateEG2_Hold_Done
	B	.LMixer_VoxLoop_UpdateEG2_Done

.LMixer_VoxLoop_UpdateEG2_Hold_Done:
	MVN	r0, r5                    @ Reset EG2 = 1.0
	LDRB	r5, [r4, #0x0E+1*5+2]     @ Decay time?
	LSR	r0, #0x10
	ADD	r6, #0x01 << SGE_VOX_STAT_EG2_SHIFT
	CMP	r5, #0x00
	BEQ	.LMixer_VoxLoop_UpdateEG2_Decay_Done
	B	.LMixer_VoxLoop_UpdateEG2_Done

.LMixer_VoxLoop_UpdateEG2_Decay:
	LDRB	r5, [r4, #0x0E+1*5+2]     @ EG2c -> r5
	EG_GetLinearStep
	LDRB	r7, [r4, #0x0E+1*5+3]     @ Sustain -> r7
	SUB	r0, r5                    @ EG2 -= EG2c
	LSL	r5, r7, #0x08
	ADD	r7, r5
	CMP	r0, r7                    @ EG2 < Sustain? (NOTE: Use signed comparison to avoid having to clip)
	BLT	.LMixer_VoxLoop_UpdateEG2_Decay_Done
	B	.LMixer_VoxLoop_UpdateEG2_Done

.LMixer_VoxLoop_UpdateEG2_Decay_Done:
	LDRB	r0, [r4, #0x0E+1*5+3]     @ Force EG2 = Sustain
	ADD	r6, #0x01 << SGE_VOX_STAT_EG2_SHIFT
	LSL	r5, r0, #0x08
	ADD	r0, r5
	B	.LMixer_VoxLoop_UpdateEG2_Done
/*
.LMixer_VoxLoop_UpdateEG2_Sustain:
	B	.LMixer_VoxLoop_UpdateEG2_Done
*/
.LMixer_VoxLoop_UpdateEG2_Release:
#if (!defined(SGE_PLATFORM_HAVE_REVERB) && defined(SGE_PLATFORM_HAVE_FAKE_REVERB))
	LDR	r5, [sp, #0x10]           @ Driver -> r5
	LDR	r7, [sp, #0x00]           @ Vox -> r7
	LDRH	r5, [r5, #0x14]           @ ReverbFb -> r5
	LDRH	r7, [r7, #0x08]           @ EG1 -> r7
	CMP	r7, r5                    @ EG1 <= ReverbFb: Stop release
	BLS	1f
#endif
	LDRB	r5, [r4, #0x0E+1*5+4]     @ EG2c -> r5
	EG_GetLinearStep
	SUB	r0, r5                    @ EG2 -= EG2c
	ASR	r5, r0, #0x1F             @ EG2 = MAX(0, EG2)
	BIC	r0, r5
1:	B	.LMixer_VoxLoop_UpdateEG2_Done

/************************************************/
.pool
/************************************************/

@ r0:  MxCnt
@ r1:
@ r2:
@ r3:
@ r4: &Src
@ r5:  MxRem
@ r6:  VolL | VolR<<16
@ r7:  Position
@ r8: &Wav
@ r9: &Dst
@ sl:  Rate
@ fp:  Overstep
@ ip:  Wav.Frmt
@ lr:  Wav.Chan
@ sp+00h: &Vox

ASM_MODE_ARM

.LMixer_VoxLoop_MixLoop_HandleLoop:
	LDMIB	r8, {r2-r3}               @ Size = Wav.Size -> r2, LoopSize = Wav.Loop -> r3?
	MOV	r7, r7, lsl #(32-RATE_BITS) @ Clear integer part of Position (we modify Src directly)
	MOV	r7, r7, lsr #(32-RATE_BITS)
	BICS	r3, r3, #0xFF<<24
	BEQ	.LMixer_VoxLoop_MixLoop_HandleLoop_NoLoop
0:	SUBS	fp, fp, r3                @ Overstep %= LoopSize
	BHI	0b
	ADDCC	fp, fp, r3
1:	BIC	r2, r2, #0xFF<<24
	SUB	r2, r2, r3                @ GlobalPosition = LoopBeg(=Size-LoopSize) + Overstep -> r2
	ADD	r2, r2, fp
#if SGE_SUPPORT_ADPCM
	CMP	ip, #SGE_WAV_FRMT_ADPCM4  @ ADPCM?
# ifdef __GBA__
	BEQ	.LMixerCore_WorkArea + (.LMixer_VoxLoop_MixLoop_HandleLoop_ADPCM - .LMixerCore_ADPCM_InitBeg)
# else
	BEQ	.LMixer_VoxLoop_MixLoop_HandleLoop_ADPCM
# endif
#endif
#if SGE_SUPPORT_PCM16
	CMP	ip, #SGE_WAV_FRMT_PCM16   @ Two bytes per sample for PCM16
	MOVEQ	r2, r2, lsl #0x01
#endif
	ADD	r4, r8, #0x18             @  N: Src = Wav.PCM8 + GlobalPosition*nChan
#if SGE_STEREO_WAVEFORMS
	MLA	r4, r2, lr, r4
#else
	ADD	r4, r4, r2
#endif
	B	.LMixer_VoxLoop_MixLoop_HandleLoop_Done

.LMixer_VoxLoop_MixLoop_HandleLoop_NoLoop:
#ifdef __GBA__
# if !SGE_USE_VOLSUBDIV
	RSBS	r0, r0, r5                @ MxRem -= MxCnt?
	BLNE	.LMixer_VoxLoop_MixLoop_ApplySilence
# else
1:	RSBS	r0, r0, r5, lsr #0x10     @ Clear remaining samples of this subdivision
	BLNE	.LMixer_VoxLoop_MixLoop_ApplySilence
	ADD	r2, sp, #VOLSTEP_SP_OFFS
	LDMIA	r2, {r3,r6}
	SUB	r5, r5, r5, lsr #0x10     @ MxCnt -= MxCntPerSubdiv?
	ADD	r6, r6, r3                @ Finished a subdivision - update volume
	STR	r6, [sp, #VOLCOUNTER_SP_OFFS]
	BIC	r6, r6, #((1<<SGE_VOLSUBDIV_LOG2MAXSUBDIV)-1)<<16
	MOV	r6, r6, lsr #SGE_VOLSUBDIV_LOG2MAXSUBDIV
	@MOV	r0, #0x00                 @ <- This gets set to 0 in ApplySilence
	MOVS	r2, r5, lsl #0x10
	BNE	1b
# endif
#endif
	LDMFD	sp!, {r4-r5}              @ Restore {Vox,nVoxRem}
#ifdef __GBA__
	STRB	r0, [r4, #0x00]           @ Kill voice
#else
	STRB	r3, [r4, #0x00]
#endif
#if (!defined(__NDS__) || __NDS__ != 9)
	LDR	ip, =.LMixer_VoxLoop_Tail+1
	BX	ip
#else
	BLX	.LMixer_VoxLoop_Tail
#endif

@ r0: nSamplesRem
@ Return r0=0, destroys ip
#ifdef __GBA__
.LMixer_VoxLoop_MixLoop_ApplySilence:
1:	LDR	ip, [r9]                  @ Add bias to remaining samples
	SUBS	r0, r0, #0x01
	ADD	ip, ip, r6, lsl #SGE_MIXER_VOLFRACBITS + (7 - SGE_MIXER_VOLBITS)
	STR	ip, [r9], #0x04
	BNE	1b
0:	BX	lr
#endif

/************************************************/
.pool
/************************************************/

.LMixer_Mixdown_Return:
#if SGE_USE_OVERSAMPLING
# ifdef __GBA__
#  include "SGE_Driver_Mixdown_Oversample.inc"
# else
#  include "SGE_Driver_Mixdown_Oversample-NDS.inc"
# endif
#endif
#if (defined(__NDS__) && __NDS__ == 9)
	MOV	r0, r5                    @ BufR -> r0
#endif
	LDR	r5, [sp, #0x08]           @ Driver -> r5
#ifdef __GBA__
# if !SGE_USE_VOLSUBDIV
	ADD	sp, sp, #0x04*(4+1)       @ Pop {RateScale,RateHz|N,Driver,MixBuf}, and Bias
# else
	@ Need to pop {SubdivCounter,VolStep,VolCounter,UsedEG1} as well as the bias levels
	ADD	sp, sp, #0x04*(4+4+(1<<SGE_VOLSUBDIV_LOG2MAXSUBDIV))
# endif
#else
# if SGE_USE_VOLSUBDIV
	ADD	sp, sp, #0x04*(4+3)       @ <- Need to pop {SubdivCounter,VolStep,UsedEG1}
# else
	ADD	sp, sp, #0x04*4
# endif
# if (__NDS__ == 9)
	LDRH	r6, [r5, #0x0E]           @ Flush the output buffer from the cache
	SUB	r0, r0, r6, lsl #0x01+SGE_USE_OVERSAMPLING
	MOV	r1, r6, lsl #0x01+SGE_USE_OVERSAMPLING
	BL	DC_FlushRange
	SUB	r0, r4, r6, lsl #0x01+SGE_USE_OVERSAMPLING
	MOV	r1, r6, lsl #0x01+SGE_USE_OVERSAMPLING
	BL	DC_FlushRange
# endif
#endif

/************************************************/

@ r5: &Driver

@ Priority formula:
@  p = Trk.Priority*2^27 + Art.Vol*2^19 + (Trk.Vol+Trk.Exp)*2^11 + Vox.Vel*2^4 + Ply.Vol + Vox.EG1*2^-6
@ For EG1 in Attack/Hold, EG1==FFFFh
@ This is pre-calculated before updating the players to avoid a massive
@ performance penalty for every note event that triggers. It is likely
@ excessive to do all voices, but may as well

.LCalculateVoicePriorities:
	LDR	r6, [r5, #0x04]            @ Priorities = Driver.MixBuf -> r6
	LDRB	r3, [r5, #0x0A]            @ nVoiceRem = Driver.VoxCnt -> r3
	ADD	r2, r5, #SGE_DRIVER_HEADER_SIZE - SGE_VOX_SIZE
1:	LDR	r9, [r2, #SGE_VOX_SIZE]!   @ Vox.{Stat,xx,xx,Vel} -> r9. Check for activity
	ANDS	r1, r9, #SGE_VOX_STAT_ACTIVE
	BEQ	2f
	MOV	r7, r9, lsr #0x18
	TST	r9, #SGE_VOX_STAT_KEYOFF   @ Increase priority by 1 for voices NOT in Release phase
	ADDEQ	r7, r7, #0x01<<(27-4)
	AND	r0, r9, #SGE_VOX_STAT_EG_MSK << SGE_VOX_STAT_EG1_SHIFT
	SUBS	r0, r0, #SGE_VOX_STAT_EG_DEC << SGE_VOX_STAT_EG1_SHIFT
	LDRCSH	r0, [r2, #0x08]            @ Decay/Sustain/Release: EG1 = Vox.EG1 -> r0
	MOVCC	r0, r0, lsr #0x10          @ Attack/Hold: EG1 = FFFFh
	MOV	r1, r7, lsl #0x04          @ Add Vox.Vel
	ADD	r7, r2, #0x24              @ Vox.{Art,Trk,Ply} -> r7,r8,r9
	TST	r9, #SGE_VOX_STAT_NOPLAYER @ Have an attached player?
	LDMIA	r7, {r7,r8,r9}
	LDRB	r7, [r7, #0x00]            @ Art.Vol -> r7
	LDREQB	sl, [r9, #0x01]            @  Y: Add Ply.Vol
	LDREQB	fp, [r8, #0x03]            @     Add Trk.Priority
	LDREQB	ip, [r8, #0x10+2]          @     Add Trk.Vol
	LDREQB	lr, [r8, #0x14+2]          @     Add Trk.Exp
	LDRNEB	ip, [r2, #0x2C]            @  N: Add Ply.Vol = Trk.Vol = Trk.Exp = Saved.Vol
	ADDEQ	r1, r1, sl
	ADDEQ	r1, r1, fp, lsl #0x1B
	ADDEQ	r1, r1, ip, lsl #0x0B
	ADDEQ	r1, r1, lr, lsl #0x0B
	ADDNE	r1, r1, ip, lsl #0x01      @ <- Account for 8bit vs 7bit mismatch
	ADDNE	r1, r1, ip, lsl #0x0B+1
	ADD	r1, r1, r7, lsl #0x13      @ Add Art.Vol
	ADD	r1, r1, r0, lsr #0x06      @ Add Vox.EG1
2:	STR	r1, [r6], #0x04            @ Store Priority
	SUBS	r3, r3, #0x01              @ --nVoiceRem?
	BNE	1b

/************************************************/

@ r5: &Driver

.LUpdatePlayers:
#if SGE_VARIABLE_SYNC_RATE
	LDRH	r0, [r5, #0x0C]          @ Get update BPM -> r7
	LDRH	r1, [r5, #0x0E]          @ See SGE_Driver_UpdatePlayer.s for details on this calculation
	LSL	r2, r0, #0x02
	ADD	r0, r2
# if (SGE_BPM_FRACBITS > 0)
	LSL	r0, #SGE_BPM_FRACBITS
# endif
# if (SGE_QUARTERNOTE_TICKS == 48)
	LSL	r1, #0x02
# else
#  error "FIXME: BufLen*QUARTERNOTE_TICKS/12"
# endif
	LSR	r2, r1, #0x01            @ <- Apply rounding
	ADD	r0, r2
	BL	__aeabi_uidiv
	MOV	r7, r0
#endif
	LDR	r4, [r5, #0x10]          @ Player = PlayersList -> r4
#if SGE_PRECISE_KEYON
	SUB	sp, #0x04                @ Make room for SampleOffs in stack
#endif
#if (!defined(__NDS__) || __NDS__ != 9)
	LDR	r6, =SGE_Driver_UpdatePlayer
#endif
1:	CMP	r4, #0x00                @ Player?
	BEQ	2f
#if (!defined(__NDS__) || __NDS__ != 9)
	MOV	lr, pc
	BX	r6
#else
	BLX	SGE_Driver_UpdatePlayer
#endif
	LDR	r4, [r4, #0x1C]          @ Player = Player->Next
	B	1b
2:
#if SGE_PRECISE_KEYON
	ADD	sp, #0x04
#endif

/************************************************/

.LExit:
#if (!defined(__NDS__) || __NDS__ != 9)
	LDMFD	sp!, {r4-fp,lr}
	BX	lr
#else
	LDMFD	sp!, {r4-fp,pc}
#endif

/************************************************/
.pool
/************************************************/

ASM_ALIGN(4)

.LMixer_VoxLoop_UpdateEG1_FuncTables:
	.word .LMixer_VoxLoop_UpdateEG1_Attack+1
	.word .LMixer_VoxLoop_UpdateEG1_Hold+1
	.word .LMixer_VoxLoop_UpdateEG1_Decay+1
	.word /*.LMixer_VoxLoop_UpdateEG1_Sustain*/ .LMixer_VoxLoop_UpdateEG1_Done+1
	.word .LMixer_VoxLoop_UpdateEG1_Release+1

.LMixer_VoxLoop_UpdateEG2_FuncTables:
	.word .LMixer_VoxLoop_UpdateEG2_Attack+1
	.word .LMixer_VoxLoop_UpdateEG2_Hold+1
	.word .LMixer_VoxLoop_UpdateEG2_Decay+1
	.word /*.LMixer_VoxLoop_UpdateEG2_Sustain*/ .LMixer_VoxLoop_UpdateEG2_Done+1
	.word .LMixer_VoxLoop_UpdateEG2_Release+1

/************************************************/
#if SGE_VARIABLE_SYNC_RATE
/************************************************/

@ r5: CompandedEnvTime (EnvTime = CompandedEnvTime^2/1626, in seconds)
@ sp+08h: RateScale
@ sp+0Ch: RateHz | N<<16
@ Returns Step in r5, destroys r7,ip

ASM_MODE_THUMB

@ d = 2^(16 + Log2[-100dB] / (EnvTime / SecPerUpdate))
@   = 2^(16 + Log2[-100dB] / (EnvTime / (N/RateHz)))
@   = 2^(16 + (Log2[-100dB]*N/RateHz) / EnvTime)
@   = 2^(16 + (Log2[-100dB]*N/RateHz) / (CompandedEnvTime^2/1626))
@   = 2^(16 + (Log2[-100dB]*1626*N/RateHz) / CompandedEnvTime^2)
.LMixer_VoxLoop_GetExpDecStep:
	PUSH	{r0-r3,lr}
	ADD	r7, sp, #0x08 + 0x04*5
	BL	.LMixer_VoxLoop_GetLinearStep_Core
	LDR	r1, =34017 @ -Log2[10^(-100/20)]*2^(27 - 16)
	MOV	r0, #0x10
	MUL	r5, r1
	LSL	r0, #0x1B
	SUB	r0, r5
#if (!defined(__NDS__) || __NDS__ != 9)
	LDR	r1, =SGE_Exp2fxp
#endif
	BLS	.LMixer_VoxLoop_GetExpDecStep_ImmediateDecay
#if (!defined(__NDS__) || __NDS__ != 9)
	BL	1f
#else
	BL	SGE_Exp2fxp
#endif
	MOV	r5, r0
	POP	{r0-r3,pc}
#if (!defined(__NDS__) || __NDS__ != 9)
1:	BX	r1
#endif

.LMixer_VoxLoop_GetExpDecStep_ImmediateDecay:
	MOV	r5, #0x00
	POP	{r0-r3,pc}

@ d = 2^16 / (EnvTime / SecPerUpdate)
@   = 2^16 / (EnvTime / (N/RateHz))
@   = (2^16*N/RateHz) / EnvTime
@   = (2^16*N/RateHz) / (CompandedEnvTime^2/1626)
@   = (2^16*1626*N/RateHz) / CompandedEnvTime^2
.LMixer_VoxLoop_GetLinearStep:
	ADD	r7, sp, #0x08 + 0x04*0

@ r5: x
@ r7+00h: RateScale
@ r7+04h: RateHz | N<<16

#if (SGE_RECPLUT_PRECISION <= 16)
# define SHIFT_DOWN (5+INVRATE_BITS-32 + SGE_RECPLUT_PRECISION*2 - 16)
#else
# define SHIFT_DOWN (5+INVRATE_BITS-32 + 32 - 16)
#endif

ASM_ALIGN(4)
.LMixer_VoxLoop_GetLinearStep_Core:
	BX	pc
	NOP
ASM_MODE_ARM
.LMixer_VoxLoop_GetLinearStep_Core_ARM:
#if AVOID_MUL_WARNINGS
	STR	lr, [sp, #-0x04]!
#endif
	LDMIA	r7, {r7,ip}           @ RateScale -> r7, RateHz | N<<16 -> ip
	MOV	ip, ip, lsr #0x10
	ADD	ip, ip, ip, lsl #0x04 @ 2^5*1626*N -> ip
	ADD	ip, ip, ip, lsl #0x0C
	SUB	ip, ip, ip, lsr #0x08
	SUB	ip, ip, ip, lsr #0x02
#if !AVOID_MUL_WARNINGS
	UMULL	ip, r7, r7, ip        @ da = 2^(5+INVRATE_BITS-32)*1626*N/RateHz -> r7
#else
	UMULL	ip, lr, r7, ip
#endif
	LDR	ip, =SGE_RecpLUT      @ dt = 1/CompandedEnvTime^2 -> ip, as MIN(32,.RECPLUTBITS*2) bits precision
#if (SGE_RECPLUT_BITS == 8)
	LDRB	r5, [ip, r5]
#elif (SGE_RECPLUT_BITS == 16)
	MOV	r5, r5, lsl #0x01
	LDRH	r5, [ip, r5]
#elif (SGE_RECPLUT_BITS == 32)
	LDR	r5, [ip, r5, lsl #0x02]
#endif
#if (SGE_RECPLUT_PRECISION > 16)
	MOV	r5, r5, lsr #SGE_RECPLUT_PRECISION - 16
#endif
	MUL	ip, r5, r5
#if !AVOID_MUL_WARNINGS
	UMULL	r7, r5, ip, r7        @ da*dt = 2^(5+INVRATE_BITS-32 + MIN(32,RECPLUTBITS*2))*1626*N/RateHz / CompandedEnvTime^2 -> r7,r5
#else
	UMULL	r7, r5, ip, lr
#endif
#if (SHIFT_DOWN > 32)
	MOV	r5, r5, lsr #(SHIFT_DOWN - 32)
#else
	MOVS	r7, r7, lsr #SHIFT_DOWN
	ADC	r5, r7, r5, lsl #0x20-SHIFT_DOWN
#endif
#if AVOID_MUL_WARNINGS
	LDR	lr, [sp], #0x04
#endif
	BX	lr

/************************************************/
.pool
/************************************************/
#endif
/************************************************/

ASM_MODE_ARM

@ r0: Phase [0.16fxp - lower 16 bits]
@ Outputs LFOKeyMod ~ Sin[x] -> ip, LFOVolMod ~ (1-Cos[x])/2 -> r9 [.14fxp]
@ Must not destroy any register except for lr!

.LMixer_VoxLoop_UpdateLFO_UpdateOscillator_Sine:
	MOVS	lr, r0, lsl #0x11         @ x = Mod[z,1.0], C=(Mod[z,2.0]>=1.0)
	MOV	lr, lr, lsr #0x18         @ y = 4*(x-x^2) -> ip [.14fxp] (for x = 0..1)
	MUL	ip, lr, lr                @  -> 2^14 * 4*(x/256 - (x/256)^2) (for x = 0..255)
	MOV	r9, r0, lsl #0x10         @ (similar idea for LFOVolMod, but z is always in range 0..1)
	RSB	ip, ip, lr, lsl #0x08     @   = 2^16*x/256 - 2^16*(x/256)^2 = x*256 - x^2
	MOV	lr, r9, lsr #0x18
	MUL	r9, lr, lr
	RSBCS	ip, ip, #0x00             @ Negate Mod[z,2.0] >= 1.0
	RSB	r9, r9, lr, lsl #0x08
	MUL	lr, r9, r9                @ ... and a final y^2 for LFOVolMod to make it smoother
	MOV	r9, lr, lsr #0x0E
	B	.LMixer_VoxLoop_UpdateLFO_UpdateOscillator_ApplySign

.LMixer_VoxLoop_UpdateLFO_UpdateOscillator_Tri:
	MOVS	lr, r0, lsl #0x11         @ x = Mod[z,1.0], C=(Mod[z,2.0]>=1.0)
	MOV	ip, lr, lsr #(32-14)      @ y = x -> ip [.14fxp]
	RSBMI	ip, ip, #0x02<<14         @ Reflect odd quadrants
	RSBCS	ip, ip, #0x00             @ Reflect odd half
	MOVS	lr, r0, lsl #0x10         @ Similar for LFOVolMod
	MOV	r9, lr, lsr #(32-14)
	RSBMI	r9, r9, #0x02<<14
	B	.LMixer_VoxLoop_UpdateLFO_UpdateOscillator_ApplySign

.LMixer_VoxLoop_UpdateLFO_UpdateOscillator_Saw:
	MOV	lr, r0, lsl #0x10         @ Just sign-extend the phase for LFOKeyMod
	MOV	ip, lr, asr #0x10
	MOV	r9, lr, lsr #0x10         @ And zero-extend for LFOVolMod
	B	.LMixer_VoxLoop_UpdateLFO_UpdateOscillator_ApplySign

.LMixer_VoxLoop_UpdateLFO_UpdateOscillator_Square:
	TST	r0, #0x8000               @ + for first half, - for second half
	MOV	ip, #0x01<<14
	RSBNE	ip, ip, #0x00
	AND	r9, ip, ip, lsr #(31-14)  @ 0 for first half, + for second half
	B	.LMixer_VoxLoop_UpdateLFO_UpdateOscillator_ApplySign

.LMixer_VoxLoop_UpdateLFO_UpdateOscillator_Noise:
	MOV	ip, r0, lsl #0x10         @ Noise = (Phase*256.0)*BBh, in uppermost bits
	AND	ip, ip, #0xFF<<24
	RSB	r9, ip, ip, lsl #0x04
	SUBS	r9, r9, ip, lsl #0x02
	MOV	r9, r9, lsr #(32-14)
	MOV	ip, r9                    @ ... and negate LFOKeyMod based on shift-out bit
	RSBCS	ip, ip, #0x00
	CMP	sl, #0x0100               @ Clip PhaseStep to 1 noise shift per update, or
	MOVHI	sl, #0x0100               @ we may get no effect in the worst case
	B	.LMixer_VoxLoop_UpdateLFO_UpdateOscillator_ApplySign

/************************************************/
#ifdef __GBA__
/************************************************/

@ r0: &MixerStruct
@ Load mixer into work area
@ Destroys r0,r1,r2,fp, returns via sp+00h
.LMixer_VoxLoop_MixLoop_LoadMixer:
	STR	r0, .LMixerCore_WorkAreaType
	ADR	fp, .LMixerCore_WorkArea
	STMFD	sp!, {r3-sl,lr}
	LDMIA	r0, {r0-r3}               @ InitBeg/Size -> r0,r1, CoreBeg/Size -> r2,r3
1:	SUBS	r1, r1, #0x10             @ The init section is usually small, so use a small loop here
	LDMCSIA	r0!, {r4-r7}
	STMCSIA	fp!, {r4-r7}
	BHI	1b
10:	MOVS	lr, r1, lsl #0x20-3
	LDMCSIA	r0!, {r4-r5}
	LDRMI	r6, [r0], #0x04
	STMCSIA	fp!, {r4-r5}
	STRMI	r6, [fp], #0x04
2:	SUBS	r3, r3, #0x40             @ The core section is usually large, so use a large loop here
	LDMCSIA	r2!, {r4-sl,lr}
	STMCSIA	fp!, {r4-sl,lr}
	LDMCSIA	r2!, {r4-sl,lr}
	STMCSIA	fp!, {r4-sl,lr}
	BHI	2b
20:	MOVS	lr, r3, lsl #0x20-5
	LDMCSIA	r2!, {r4-sl,lr}
	STMCSIA	fp!, {r4-sl,lr}
	LDMMIIA	r2!, {r4-r7}
	STMMIIA	fp!, {r4-r7}
	MOVS	lr, r3, lsl #0x20-3
	LDMCSIA	r2!, {r4-r5}
	LDRMI	r6, [r2], #0x04
	STMCSIA	fp!, {r4-r5}
	STRMI	r6, [fp], #0x04
3:	LDMFD	sp!, {r3-sl,lr,pc}

.LMixerCore_WorkAreaType: .word 0
.LMixerCore_MixFormat:    .word 0
.LMixerCore_WorkArea:
	.space SGE_Driver_MixerCore_WorkAreaSize

/************************************************/
#endif
/************************************************/

ASM_FUNC_END(SGE_Driver_Update)

/************************************************/
//! EOF
/************************************************/
