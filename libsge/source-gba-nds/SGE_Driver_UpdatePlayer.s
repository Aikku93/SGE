/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
#include "SGE-GBANDSHw.h"
/************************************************/

#define WHOLENOTE_TICKS 192 //! Ticks per whole note
#define QUARTERNOTE_TICKS (WHOLENOTE_TICKS/4)

/************************************************/

.macro FETCH_NYBBLE
#if (defined(__NDS__) && __NDS__ == 9)
	BLX	.LFetchNybbleARM
#else
	BL	.LFetchNybble
#endif
.endm

/************************************************/

/*!

The GBA formula is:
 UpdateRate_BPM = (GBA_HW_FREQ_HZ/GBA_FRAME_CYCLES) * 60 / QUARTERNOTE_TICKS
                = GBA_HW_FREQ_HZ*5 / (GBA_FRAME_CYCLES*QUARTERNOTE_TICKS/12)
                = RateHz*5 / (BufLen*QUARTERNOTE_TICKS/12)

This formula relies on GBA_FRAME_CYCLES having 1,2,4,8,16,32,64 as
divisors, and that QUARTERNOTE_TICKS has at least 12 as a divisor,
so we can just shift out the required precision from the denominator.

On NDS, we just hardcode the maths:
 UpdateRate_BPM = (NDS_HW_FREQ_HZ/NDS_FRAME_CYCLES) * 60 / QUARTERNOTE_TICKS
                = 67027964 / (QUARTERNOTE_TICKS * 18673)
                = RateHz*5 / (BufLen*QUARTERNOTE_TICKS/12)

The maximum fractional precision we can use is 5 bits:
  10bit BPM
 +5bit fraction
 +1bit sign
 =16bit

!*/
#if !SGE_VARIABLE_SYNC_RATE
# ifdef __GBA__
#  define UPDATERATE_NUM (GBA_HW_FREQ_HZ * 5)
#  define UPDATERATE_DEN ((GBA_FRAME_CYCLES >> SGE_BPM_FRACBITS) * (QUARTERNOTE_TICKS/12))
# else
#  define UPDATERATE_NUM (67027964 << SGE_BPM_FRACBITS)
#  define UPDATERATE_DEN (18673 * QUARTERNOTE_TICKS)
# endif
# define UPDATERATE_BPM ((UPDATERATE_NUM + (UPDATERATE_DEN/2)) / UPDATERATE_DEN)
#endif

/************************************************/

@ Input:
@  r4: &Player (must be preserved)
@  r5: &Driver (must be preserved)
@  r6:         (must be preserved)
@  r7-fp: Saved

ASM_FUNC_GLOBAL(SGE_Driver_UpdatePlayer)
ASM_FUNC_BEG   (SGE_Driver_UpdatePlayer, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Driver_UpdatePlayer:
#if SGE_VARIABLE_SYNC_RATE
	PUSH	{lr}
	LDRH	r0, [r5, #0x0C]     @ Get update BPM
	LDRH	r1, [r5, #0x0E]
	LSL	r2, r0, #0x02
	ADD	r0, r2
# if (SGE_BPM_FRACBITS > 0)
	LSL	r0, #SGE_BPM_FRACBITS
# endif
# if (QUARTERNOTE_TICKS == 48)
	LSL	r1, #0x02
# else
#  error "FIXME: BufLen*QUARTERNOTE_TICKS/12"
# endif
	LSR	r2, r1, #0x01       @ <- Apply rounding
	ADD	r0, r2
	BL	__aeabi_uidiv
	MOV	r7, r0
	PUSH	{r4-r7}
#else
	PUSH	{r4-r6,lr}
#endif
0:	LDR	r6, [r4, #0x10]     @ Song -> r6?
	LDRH	r0, [r4, #0x08+2]   @ Tempo -> r0
	CMP	r6, #0x00
#if (SGE_VARIABLE_SYNC_RATE || SGE_BPM_FRACBITS > 0)
	BEQ	.LExitLocalTrampoline
#else
	BEQ	.LExit
#endif
0:	LDRH	r1, [r4, #0x02]     @ StretchedTempo = TempoStretch * Tempo [.8fxp]
	MOV	r2, #0x0E
	LDRSH	r3, [r4, r2]        @ Phase -> r3
	MUL	r0, r1
	LSR	r0, #0x08-SGE_BPM_FRACBITS     @ int(StretchedTempo) -> r0
	LSR	r2, r0, #(10+SGE_BPM_FRACBITS) @ Clip StretchedTempo to max 1023BPM
	BEQ	0f
	MVN	r0, r2
	LSR	r0, #0x20-(10+SGE_BPM_FRACBITS)
0:	SUB	r2, r3, r0          @ Phase -= StretchedTempo?
	BGT	.LProcessPlayer_End @  <- On break-even (Phase == 0), process tick now

@ r4: &Player
@ r5: &Driver

.LProcessTick:
.LUpdateVoices:
	LDRB	r3, [r5, #0x0A]     @ nVoicesRem = Driver.VoxCnt -> r3
	LDR	r0, [r5, #0x04]     @ &Priorities[nVoices] -> r8 (pointing to highest byte)
	ADD	r5, #SGE_DRIVER_HEADER_SIZE
	LSL	r1, r3, #0x02
	ADD	r0, r1
	ADD	r0, #0x03
	MOV	r8, r0
	NEG	r3, r3              @ Pre-multiply for offsetting into Priorities[]
	LSL	r3, #0x02
1:	LDRB	r0, [r5, #0x00]     @ Check for NOPLAYER
	LSL	r1, r0, #0x20-4     @  If NOPLAYER is set: TicksRem = 0
	BCS	0f                  @  This is needed for portamento checks against TicksRem = -1
	LDR	r1, [r5, #0x2C]     @ Ply -> r0
	CMP	r1, r4              @ Ply == Player?
	BNE	2f
	LDRH	r1, [r5, #0x04]     @ TicksRem -> r1
	SUB	r1, #0x01           @ --TicksRem?
0:	STRH	r1, [r5, #0x04]
	BCS	2f
10:	ADD	r0, #SGE_VOX_STAT_KEYOFF|SGE_VOX_STAT_NOPLAYER
	STRB	r0, [r5, #0x00]     @ Vox.Stat |= KEYOFF | NOPLAYER
	LDR	r0, [r5, #0x28]     @ Trk -> r0
	LDRB	r1, [r4, #0x01]     @ Ply.Vol -> r1
	LDRB	r2, [r0, #0x10+2]   @ Trk.Vol -> r2
	LDRB	r6, [r0, #0x14+2]   @ Trk.Exp -> r6
	MUL	r1, r2
	LDRB	r7, [r0, #0x18+2]   @ Trk.Pan -> r7
	MUL	r1, r6
	LDRB	r6, [r0, #0x03]     @ Trk.Priority -> r6
	LDRH	r0, [r0, #0x1C+2]   @ Trk.Bnd -> r0
	MOV	r2, r8              @ Set Priority=0 for this voice (all voices in Release have Priority=0)
	LDRB	r2, [r2, r3]        @  This is done by just subtracting the track priority from the calculated
	LSL	r6, #0x1B-24        @  voice priority. Theoretically, the values could get messed up if the
	SUB	r6, r2, r6          @  priority changes between key-on and now, but this should be ok still.
	MOV	r2, r8
	STRB	r6, [r2, r3]
	LSR	r1, #(21-7)         @ Vol = Ply.Vol * Trk.Vol * Trk.Exp -> 1.7fxp
	MOV	r2, #0x7F           @ Unbias the bend value
	LSL	r2, #0x07           @ Vol | Pan<<8 | Bnd<<16
	SUB	r0, r2
	ADD	r0, r0              @ Convert Bnd to 8.8 from 7.8
	LSL	r0, #0x08
	ORR	r0, r7
	LSL	r0, #0x08
	ORR	r0, r1
	STR	r0, [r5, #0x2C]     @ Store Vox.PlyState
2:	ADD	r5, #SGE_VOX_SIZE
	ADD	r3, #0x04           @ --nVoicesRem?
	BCC	1b

.LTrackLoop_Enter:
	LDRB	r5, [r4, #0x00]     @ nTracksRem -> r5
	ADD	r4, #0x20           @ Track = &Tracks[] -> r4

@ r4: &Track
@ r5:  nTracksRem

.LTrackLoop:
	LDR	r6, [r4, #0x28]     @ &Src -> r6
	LSL	r0, r6, #0x03       @ Remove NybbleIdx. Have a pointer?
	BEQ	.LTrackLoopTail
	LSR	r0, #0x03           @ Load Data -> r7
	LDR	r7, [r0]
	LSR	r1, r6, #(32-3)     @ Data >>= NybbleIdx*4 to get next command
	LSL	r1, #0x02
	LSR	r7, r1
1:	MOV	r0, r4              @ Update controllers:
	ADD	r0, #0x0C           @  Vel
	BL	.LUpdateCtrl8
	ADD	r0, #0x04           @  Vol
	BL	.LUpdateCtrl8
	ADD	r0, #0x04           @  Exp
	BL	.LUpdateCtrl8
	ADD	r0, #0x04           @  Pan
	BL	.LUpdateCtrl8
	ADD	r0, #0x04           @  Bnd (NOTE: 16bit controller!)
	BL	.LUpdateCtrl16
	ADD	r0, #0x08           @  Portamento
	BL	.LUpdateCtrl8
1:	LDRH	r0, [r4, #0x06]     @ --Rest?
	SUB	r0, #0x01
	BCS	.LTrackLoopTail_StoreTicks
0:	BL	.LGetToneFromProgram

.LTrackReadLoop:
	FETCH_NYBBLE                @ Call appropriate command handler
	LDR	r1, =SGE_Driver_SongCommandJumpTable
	LSL	r0, #0x02
	LDR	r0, [r1, r0]
	BX	r0

#if (SGE_VARIABLE_SYNC_RATE || SGE_BPM_FRACBITS > 0)
@ This is needed because otherwise the jump is out of range, oops...
.LExitLocalTrampoline:
	B	.LExit
#endif

.LTrackReadLoop_End:
	STR	r6, [r4, #0x28]     @ Store next Src

.LTrackLoopTail_StoreTicks:
	STRH	r0, [r4, #0x06]     @ Store Rest = NewRestCounter

.LTrackLoopTail:
	ADD	r4, #SGE_TRACK_SIZE @ Track++
	SUB	r5, #0x01           @ --nTracksRem?
	BNE	.LTrackLoop

.LProcessTick_End:
	ADD	r4, sp, #0x00       @ Restore Player -> r4, Driver -> r5
	LDMIA	r4, {r4-r5}
0:	MOV	r0, #0x08           @ Update Player.Tempo
	ADD	r0, r4
	BL	.LUpdateCtrl16
#if SGE_VARIABLE_SYNC_RATE
	LDR	r3, [sp, #0x0C]
#elif (UPDATERATE_BPM > 255+255)
	LDR	r3, =UPDATERATE_BPM
#endif
	MOV	r2, #0x0E
	LDRSH	r2, [r4, r2]        @ Phase -> r2
	LDRH	r0, [r4, #0x08+2]   @ Tempo -> r0
	LDRH	r1, [r4, #0x02]     @ StretchedTempo = TempoStretch * Tempo [.8fxp]
#if (SGE_VARIABLE_SYNC_RATE || UPDATERATE_BPM > 255+255)
	ADD	r2, r3              @ Phase += UPDATE_BPM
#elif (UPDATERATE_BPM <= 255)
	ADD	r2, #UPDATERATE_BPM
#else
	ADD	r2, #0xFF
	ADD	r2, #UPDATERATE_BPM - 255
#endif
	MUL	r0, r1
	STRH	r2, [r4, #0x0E]
	LSR	r0, #0x08-SGE_BPM_FRACBITS     @ int(StretchedTempo) -> r0
	LSR	r3, r0, #(10+SGE_BPM_FRACBITS) @ Clip StretchedTempo to max 1023BPM
	BEQ	0f
	MVN	r0, r3
	LSR	r0, #0x20-(10+SGE_BPM_FRACBITS)
0:	SUB	r2, r0              @ Phase -= StretchedTempo?
#if (SGE_BPM_FRACBITS == 0)
	BLT	.LProcessTick       @  <- On break even (Phase == 0), stop
#else
	MOV	r3, #0x01 << SGE_BPM_FRACBITS
	CMN	r2, r3              @  <- Same as normal case, but need to check for
	BLE	.LProcessTick       @     Phase <= -1.0 rather than just Phase < 0.
#endif

.LProcessPlayer_End:
	STRH	r2, [r4, #0x0E]     @ Store final Phase

.LExit:
#if (!defined(__NDS__) || __NDS__ == 7)
	POP	{r4-r7}
# if SGE_VARIABLE_SYNC_RATE
	POP	{r7}
# endif
	BX	r7
#else
# if SGE_VARIABLE_SYNC_RATE
	POP	{r4-r7,pc}
# else
	POP	{r4-r6,pc}
# endif
#endif

/************************************************/

@ r0: &Ctrl (preserved on return)
@ Returns new Value in r2

.LUpdateCtrl8:
	PUSH	{lr}
	LDRB	r2, [r0, #0x02] @ Value -> r2
	LDRB	r3, [r0, #0x03] @ Target -> r3
	BL	.LUpdateCtrlGeneric
	STRB	r2, [r0, #0x02] @ Store new Value
	POP	{pc}

@ r0: &Ctrl (preserved on return)
@ Returns new Value in r2

.LUpdateCtrl16:
	PUSH	{lr}
	LDRH	r2, [r0, #0x02] @ Value -> r2
	LDRH	r3, [r0, #0x04] @ Target -> r3
	BL	.LUpdateCtrlGeneric
	STRH	r2, [r0, #0x02] @ Store new Value
	POP	{pc}

@ r0: &Ctrl
@ r2:  Value
@ r3:  Target
@ sp+00h: ReturnAddr (shortcut taken when no update is needed)
@ Updates Ctrl.{Phase,Duration}
@ Returns Ctrl -> r0, NewValue -> r2
@ NOTE: The control will update until Target==Value, regardless of Duration.
@ This allows Duration==FFh to be interpreted as Duration=256 ticks.

.LUpdateCtrlGeneric:
	SUB	r3, r2          @ Delta = Target - Value -> r3?
	BEQ	.LUpdateCtrlGeneric_Done
1:	LDRB	r1, [r0, #0x01] @ Duration -> r1
	PUSH	{r0,r2,lr}
	SUB	r2, r1, #0x01   @ Ctrl.Duration -= 1
	STRB	r2, [r0, #0x01]
	ADD	r1, #0x01       @ Unbias duration
0:	LSL	r0, r3, #0x08   @ Step = (Delta << 8) / Duration + Phase -> ip [.8fxp]
	@MOV	r1, r1
	BL	__aeabi_idivmod
	MOV	ip, r0
	POP	{r0,r2,r3}
	LDRB	r1, [r0, #0x00] @ Step += Phase
	ADD	r1, ip          @ Phase = Step (lower 8 bits)
	STRB	r1, [r0, #0x00]
	ASR	r1, #0x08       @ Return Value + int(Step) -> r2
	ADD	r2, r1
	BX	r3
.LUpdateCtrlGeneric_Done:
	STRB	r3, [r0, #0x00] @ Phase = 0 (this is mostly needed because of portamento...)
	POP	{pc}

/************************************************/

@ Puts &Tone -> r8
@ Clobbers r1,r2,r3

.LGetToneFromProgram:
	LDR	r1, [sp, #0x00]    @ Player.Song -> r1
	LDR	r1, [r1, #0x10]
	LDRB	r2, [r1, #0x00]    @ nTracks -> r2
	LDRB	r3, [r1, #0x01]    @ nTones -> r3
	ADD	r1, #0x10          @ Seek to Song.TrkOffs[]
	LSL	r2, #0x02          @ Seek to Song.Tones[]
	ADD	r1, r2
	LDRB	r2, [r4, #0x02]    @ Program -> r2
	CMP	r2, r3             @ Invalid program?
	BCS	3f
0:	CMP	r2, #0x00          @ Seek to Tone
	BEQ	2f
1:	LDR	r3, [r1]           @ ToneAddr += Tone.Size
	LSL	r3, #0x08
	LSR	r3, #0x08
	ADD	r1, r3
	SUB	r2, #0x01          @ --Program?
	BNE	1b
2:	MOV	r8, r1             @ Tone -> r8
.Lbxlr:	BX	lr
3:	MOV	r1, #0x00          @ Invalid program - Tone = NULL
	B	2b

/************************************************/

@ Puts Nybble -> r0
@ This call only impacts r0, but updates r6,r7

#if (!defined(__NDS__) || __NDS__ != 9)
.LFetchNybble:
	BX	pc
#endif

ASM_MODE_ARM
.LFetchNybbleARM:
	AND	r0, r7, #0x0F         @ Data -> r0
	ADDS	r6, r6, #0x01<<(32-3) @ ++NybbleIdx?
	LDRCS	r7, [r6, #0x04]!      @  C=1: Data = *Src++
	MOVCC	r7, r7, lsr #0x04     @  C=0: Data >>= 4
	BX	lr

/************************************************/

.pool

ASM_FUNC_END(SGE_Driver_UpdatePlayer)

/************************************************/

#include "SGE_Driver_SongCommands.inc"

/************************************************/
//! EOF
/************************************************/
