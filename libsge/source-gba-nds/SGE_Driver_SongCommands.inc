/************************************************/

@ All commands enter with:
@  r0:  Command*04h
@  r1: &JumpTable
@  r2:
@  r3:
@  r4: &Track
@  r5:  nTracksRem
@  r6: &Src
@  r7:  Data
@  r8: &Tone
@  r9-fp,lr: Free to use
@  sp+00h: &Player
@  sp+04h: &Driver
@  sp+08h: [Reserved]
@  sp+0Ch: [Reserved] (SGE_VARIABLE_SYNC_RATE == 0) | UpdateBPM  (SGE_VARIABLE_SYNC_RATE == 1)
@  sp+10h: SampleOffs (SGE_VARIABLE_SYNC_RATE == 0) | [Reserved] (SGE_VARIABLE_SYNC_RATE == 1)
@  sp+14h:                                            SampleOffs (SGE_VARIABLE_SYNC_RATE == 1)
@ Handlers must return to either .LTrackReadLoop or .LTrackReadLoop_End.
@ On returning to .LTrackReadLoop_End, r0 must be NewRestCounter.
@ Return address on returning will be ignored, so BL is fine if needed.

/************************************************/

ASM_FUNC_BEG(SGE_Driver_SongCommands, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Driver_SongCommands:

/************************************************/

@ 0h: Note (time-coded)
.LSongCommand0h:
	BL	.LFetchTimeCode
	STRH	r0, [r4, #0x08] @ Track.LastTimeCode = nTicks
	B	.LReadNoteCommand

/************************************************/

@ 1h: Note (last time-coded)
.LSongCommand1h:
	LDRH	r0, [r4, #0x08]
	B	.LReadNoteCommand

/************************************************/

@ 2h: Note (1/4)
.LSongCommand2h:
	MOV	r0, #SGE_WHOLENOTE_TICKS/4-1
	B	.LReadNoteCommand

/************************************************/

@ 3h: Note (1/8)
.LSongCommand3h:
	MOV	r0, #SGE_WHOLENOTE_TICKS/8-1
	B	.LReadNoteCommand

/************************************************/

@ 4h: Note (1/16)
.LSongCommand4h:
	MOV	r0, #SGE_WHOLENOTE_TICKS/16-1
	B	.LReadNoteCommand

/************************************************/

@ 5h: Note (1/32)
.LSongCommand5h:
	MOV	r0, #SGE_WHOLENOTE_TICKS/32-1
	B	.LReadNoteCommand

/************************************************/

@ 6h: Note (1/64)
.LSongCommand6h:
	MOV	r0, #SGE_WHOLENOTE_TICKS/64-1
	B	.LReadNoteCommand

/************************************************/
#if 0 //! This command is moved elsewhere for Bxx range
/************************************************/

@ 7h: Rest
.LSongCommand7h:

/************************************************/
#endif
/************************************************/

@ 8h: Octave
.LSongCommand8h:
	FETCH_NYBBLE            @ 0h..Ah: Set octave
	LSL	r2, r0, #0x02
	LSL	r3, r0, #0x03
	CMP	r0, #0x0B
	BCC	1f
0:	LDRB	r2, [r4, #0x04] @ Ch..Fh: Change octave
	BEQ	2f              @ Bh: Relative change
	ADR	r3, .LSetOctave_DeltaTable - 0x01*0x0C
	LDRSB	r3, [r3, r0]
1:	ADD	r2, r3
	STRB	r2, [r4, #0x04]
	B	.LTrackReadLoop
2:	FETCH_NYBBLE
	ADD	r2, #(3*12)
	LSL	r3, r0, #0x20-4 @ Sign expand and multiply by 12
	ASR	r3, #0x20-4-2
	LSL	r0, r3, #0x01
	ADD	r3, r0
	BPL	1b              @  X >= 0: Octave += X+3
	SUB	r2, #(3+2)*12   @  X <  0: Octave += X-2
	B	1b

ASM_ALIGN(4)
.LSetOctave_DeltaTable:
	.byte -12,+12,-24,+24 @ Ch = Down, Dh = Up, Eh = Down*2, Fh = Up*2

/************************************************/

@ 9h: Velocity
.LSongCommand9h:
	MOV	r2, #0x0C       @ ReadCtrl8(Vel)
	B	.LReadCtrl8

/************************************************/

@ Ah: Volume
.LSongCommandAh:
	MOV	r2, #0x10       @ ReadCtrl8(Vol)
	B	.LReadCtrl8

/************************************************/

@ Bh: Expression
.LSongCommandBh:
	MOV	r2, #0x14       @ ReadCtrl8(Exp)
	B	.LReadCtrl8

/************************************************/

@ Ch: Panning
.LSongCommandCh:
	MOV	r2, #0x18       @ ReadCtrl8(Pan)
	B	.LReadCtrl8

/************************************************/

@ Dh: PitchBend
.LSongCommandDh:
	BL	.LFetchWord     @ Ramp | Value<<1 -> r1
	LSR	r0, r1, #0x01   @ Expand Value to 8.8fxp
	LSL	r0, #0x01
	ADD	r0, r1
	MOV	r1, #0x80       @ Unbias Value
	LSL	r1, #0x08+1
	SUB	r0, r1
	MOV	r2, #0x1C       @ ReadCtrl16(Bnd)
	B	.LStoreCtrlMost16

/************************************************/

@ Eh,0h: Portamento enable
.LSongCommandEh0h:
	LDRB	r2, [r4, #0x00] @ PortamentoEnable = 1
	LSR	r3, r2, #0x01+1
	BCS	0f
	ADD	r2, #0x01<<1
	STRB	r2, [r4, #0x00]
1:	MOV	r2, #0xFF       @ PortamentoVoice = INVALID
	MOV	r3, #0x23
	STRB	r2, [r4, r3]
0:	B	.LTrackReadLoop

/************************************************/

@ Eh,1h: Portamento disable
.LSongCommandEh1h:
	LDRB	r2, [r4, #0x00] @ PortamentoEnable = 0
	MOV	r3, #0x01<<1
	BIC	r2, r3
	STRB	r2, [r4, #0x00]
	B	.LTrackReadLoop

/************************************************/

@ Eh,2h: RepeatStart
.LSongCommandEh2h:
	LDRB	r0, [r4, #0x01] @ StackDepth -> r0
	MOV	r1, #0x00
	LSL	r2, r0, #0x02
	ADD	r3, r0, #0x01
	CMP	r3, #SGE_MAX_STACK_DEPTH
	BHI	0f
	STRB	r3, [r4, #0x01] @ StackDepth++
	ADD	r2, #0x2C
	STR	r1, [r4, r2]    @ Stack[StackDepth] = NULL | REPEAT
	MOV	r1, #0x2C + 0x04*SGE_MAX_STACK_DEPTH
	ADD	r1, r4
	STRB	r3, [r1, r0]    @ ReptCnt[StackDepth] = [anything non-zero]
	B	.LTrackReadLoop
0:	B	.LSongCommandFhFh

/************************************************/

@ Eh,3h: PitchBendSt1

.LSongCommandEh3h:
	FETCH_NYBBLE
	SUB	r0, #0x08       @ Unbias, and scale up to 8.8fxp
	LSL	r0, #0x08
	MOV	r2, #0x1C       @ StoreCtrl16(Bnd)
	B	.LStoreCtrlMost16_NoRamp

/************************************************/

@ Eh,4h: PitchBendSt1Ramp

.LSongCommandEh4h:
	FETCH_NYBBLE
	SUB	r0, #0x08       @ Unbias, and scale up to 8.8fxp
	LSL	r0, #0x08
	MOV	r2, #0x1C       @ StoreCtrl16(Bnd)
	B	.LStoreCtrlMost16_Ramp

/************************************************/

@ Eh,5h: PitchBendSt2

.LSongCommandEh5h:
	BL	.LFetchByte
	SUB	r0, #0x80       @ Unbias, and scale up to 8.8fxp
	LSL	r0, #0x08
	MOV	r2, #0x1C       @ StoreCtrl16(Bnd)
	B	.LStoreCtrlMost16_NoRamp

/************************************************/

@ Eh,6h: PitchBendSt2Ramp

.LSongCommandEh6h:
	BL	.LFetchByte
	SUB	r0, #0x80       @ Unbias, and scale up to 8.8fxp
	LSL	r0, #0x08
	MOV	r2, #0x1C       @ StoreCtrl16(Bnd)
	B	.LStoreCtrlMost16_Ramp

/************************************************/

@ Eh,7h: RelPitchBend
.LSongCommandEh7h:
	BL	.LFetchWord     @ Ramp | Value<<1 -> r1
	LSR	r0, r1, #0x01   @ Expand Value to 8.8fxp
	LSL	r0, #0x01
	ADD	r0, r1
	MOV	r1, #0x80       @ Unbias Value
	LSL	r1, #0x08+1
	SUB	r0, r1

.LSongCommand_RelPitchBend:
	MOV	r2, #0x1C+0x02  @ Value = Bnd.Value + Delta
	LDRSH	r1, [r4, r2]
	MOV	r2, #0x1C       @ ReadCtrl16(Bnd)
	LSL	r1, #0x01       @ <- Make sure to account for the Ramp bit!
	ADD	r0, r1
	B	.LStoreCtrlMost16

/************************************************/

@ Eh,8h: RelPitchBendSt1

.LSongCommandEh8h:
	FETCH_NYBBLE
	SUB	r0, #0x08       @ Unbias, and scale up to 8.8fxp
	LSL	r0, #0x08+1
	@ADD	r0, #0x00       @ Ramp = 0
	B	.LSongCommand_RelPitchBend

/************************************************/

@ Eh,9h: RelPitchBendSt1Ramp

.LSongCommandEh9h:
	FETCH_NYBBLE
	SUB	r0, #0x08       @ Unbias, and scale up to 8.8fxp
	LSL	r0, #0x08+1
	ADD	r0, #0x01       @ Ramp = 1
	B	.LSongCommand_RelPitchBend

/************************************************/

@ Eh,Ah: RelPitchBendSt2

.LSongCommandEhAh:
	BL	.LFetchByte
	SUB	r0, #0x80       @ Unbias, and scale up to 8.8fxp
	LSL	r0, #0x08+1
	@ADD	r0, #0x00       @ Ramp = 0
	B	.LSongCommand_RelPitchBend

/************************************************/

@ Eh,Bh: RelPitchBendSt2Ramp

.LSongCommandEhBh:
	BL	.LFetchByte
	SUB	r0, #0x80       @ Unbias, and scale up to 8.8fxp
	LSL	r0, #0x08+1
	ADD	r0, #0x01       @ Ramp = 1
	B	.LSongCommand_RelPitchBend

/************************************************/

@ Fh,0h: Transpose
.LSongCommandFh0h:
	BL	.LFetchByte
	STRB	r0, [r4, #0x05] @ Transpose = ReadByte()
	B	.LTrackReadLoop

/************************************************/

@ Fh,1h: TransposeAdd
.LSongCommandFh1h:
	LDRB	r2, [r4, #0x05] @ Transpose += ReadByte()
	BL	.LFetchByte     @ This is meant to be a signed add, but it will wrap around
	ADD	r0, r2
	STRB	r0, [r4, #0x05]
	B	.LTrackReadLoop

/************************************************/

@ Fh,2h: NoteDurationMulAdd
.LSongCommandFh2h:
	BL	.LFetchByte
	LSR	r0, #0x01       @ C=HaveAdder?
	STRB	r0, [r4, #0x0A] @ NoteLengthMul = ReadByte()
	EOR	r0, r0
	BCC	0f              @  No adder: Set to 0
	BL	.LFetchByte     @ NoteLengthAdd = ReadByte()
0:	STRB	r0, [r4, #0x0B]
	B	.LTrackReadLoop

/************************************************/

@ Fh,3h: Priority
.LSongCommandFh3h:
	FETCH_NYBBLE
	STRB	r0, [r4, #0x03] @ Priority = ReadNybble()
	B	.LTrackReadLoop

/************************************************/

@ Fh,4h: Program
.LSongCommandFh4h:
	BL	.LFetchByte
	MOV	r2, r0
	STRB	r2, [r4, #0x02]      @ Program = ReadByte()
	BL	.LGetToneFromProgram @ Update &Tone
	B	.LTrackReadLoop

/************************************************/

@ Fh,5h: Tempo
.LSongCommandFh5h:
	MOV	r3, #0x03        @ TempoVal -> r1
	BL	.LFetchMulti
	LDR	r3, [sp, #0x00]  @ Player -> r3
	ADD	r0, r1, #0x01<<1 @ Unbias the tempo -> r0
	SUB	r2, r3, r4       @ Make relative to Player rather than Track
	ADD	r2, #0x08        @ ReadCtrl(Player->Tempo)
	B	.LStoreCtrlMost16

/************************************************/

@ Fh,6h: Jump
.LSongCommandFh6h:
	BL	.LFetchSeekAddr
	BL	.LSetNewSrcAddr @ Src = JumpAddr
	B	.LTrackReadLoop

/************************************************/

@ Fh,7h: Repeat
.LSongCommandFh7h:
	BL	.LFetchByte
	MOV	ip, r0          @ nRepeats -> ip
	BL	.LFetchSeekAddr @ ReptAddr -> r0
	LDRB	r3, [r4, #0x01] @ StackDepth-1 -> r3?
	SUB	r3, #0x01       @  StackDepth == 0: New repeat
	BCC	.LSongCommand_Repeat_BeginRepeatSection
	LSL	r2, r3, #0x02   @ Stack[StackDepth-1] -> r2
	ADD	r2, #0x2C
	LDR	r1, [r4, r2]
	CMP	r1, r6          @ If the pointer is not the same, then this is a new repeat
	BNE	.LSongCommand_Repeat_BeginRepeatSection_Checked

.LSongCommand_Repeat_ContinueRepeatSection:
	MOV	r1, #0x2C + 0x04*SGE_MAX_STACK_DEPTH
	ADD	r1, r4
	LDRB	r2, [r1, r3]    @ --RepeatCnt[StackDepth-1]?
	SUB	r2, #0x01
	BCC	.LSongCommand_Repeat_EndRepeatSection
	STRB	r2, [r1, r3]
0:	@MOV	r0, r0          @ Src = ReptAddr and keep looping
	BL	.LSetNewSrcAddr
	B	.LTrackReadLoop

.LSongCommand_Repeat_EndRepeatSection:
	STRB	r3, [r4, #0x01] @ StackDepth--
	B	.LTrackReadLoop

.LSongCommand_Repeat_BeginRepeatSection_Checked:
	CMP	r1, #0x00       @ If this was part of a RepeatStart command, pop the stack
	BNE	0f
	SUB	r3, #0x01
.LSongCommand_Repeat_BeginRepeatSection:
0:	ADD	r3, #(1+1)      @ Restore StackDepth, then StackDepth++
	CMP	r3, #SGE_MAX_STACK_DEPTH
	BHI	.LSongCommandFhFh
	STRB	r3, [r4, #0x01] @ Store StackDepth+1
	LSL	r2, r3, #0x02   @ NOTE: We must offset by -1 when using StackDepth, now
	ADD	r2, #0x2C-0x04
	STR	r6, [r4, r2]    @ Stack[StackDepth] = Src
	MOV	r1, #0x2C + 0x04*SGE_MAX_STACK_DEPTH - 0x01
	ADD	r1, r4
	MOV	r2, ip
	STRB	r2, [r1, r3]    @ RepeatCnt[StackDepth] = nRepeats
0:	@MOV	r0, r0          @ Src = ReptAddr and keep looping
	BL	.LSetNewSrcAddr
	B	.LTrackReadLoop

/************************************************/

@ Fh,8h: Call
.LSongCommandFh8h:
	MOV	r0, #0x01-1     @ nRepeats = 1 (minus bias)
	B	.LSongCommand_Call

/************************************************/

@ Fh,9h: CallWithCounter
.LSongCommandFh9h:
	BL	.LFetchByte      @ nRepeats -> r0
	@B	.LSongCommand_Call

@ r0: nRepeats
.LSongCommand_Call:
	LDRB	r3, [r4, #0x01]  @ StackDepth -> r3
	MOV	r2, #0x2C + 0x04*SGE_MAX_STACK_DEPTH
	ADD	r1, r3, #0x01    @ StackDepth++
	CMP	r1, #SGE_MAX_STACK_DEPTH
	BHI	.LSongCommandFhFh
	STRB	r1, [r4, #0x01]
	ADD	r2, r4           @ &Trk.RepeatCnt[] -> r2
	STRB	r0, [r2, r3]     @ CallCnt = nRepeats
	SUB	r2, #0x04*SGE_MAX_STACK_DEPTH
	LSL	r3, #0x02        @ Stack[StackDepth] = Src
	ADD	r1, r6, #0x01    @ Tag with 'Pattern'
	STR	r1, [r2, r3]
0:	BL	.LFetchSeekAddr
	BL	.LSetNewSrcAddr  @ Src = CallAddr
	B	.LTrackReadLoop

/************************************************/

@ Fh,Ah: GotoIf
.LSongCommandFhAh:
	BL	.LProcessExternalCall
	LSL	r0, #0x18       @ <- Return value is uint8_t, so mask to lower 8 bits
	BEQ	1f
0:	BL	.LFetchSeekAddr @ True:  Src = TargetTrue
	BL	.LSetNewSrcAddr
	B	.LTrackReadLoop
1:	BL	.LFetchSeekAddr @ False: Read TargetTrue, and then discard it
	B	.LTrackReadLoop

/************************************************/

@ Fh,Bh: Signal
.LSongCommandFhBh:
	BL	.LProcessExternalCall
	B	.LTrackReadLoop

/************************************************/

@ Fh,Ch: Break
.LSongCommandFhCh:
	BL	.LFetchSeekAddr @ ReptEndAddr -> r0
	LDRB	r1, [r4, #0x01] @ StackDepth -> r1
	MOV	r2, #0x2C + 0x04*SGE_MAX_STACK_DEPTH
	ADD	r2, r4          @ &RepeatCnt[] -> r2
	SUB	r1, #0x01       @ --StackDepth?
	BCC	0f              @  Not currently nesting: Ignore the command
	LDRB	r3, [r2, r1]    @ ReptCnt > 0? Not the last iteration - ignore command
	CMP	r3, #0x00
	BNE	0f
	LSL	r3, r1, #0x02   @ Stack[StackDepth-1] -> r3
	SUB	r2, #0x04*SGE_MAX_STACK_DEPTH
	LDR	r3, [r2, r3]
	LSR	r2, r3, #0x01   @ Tagged with 'Pattern'? Need to return to caller
	STRB	r1, [r4, #0x01] @ Last iteration - StackDepth--, and go to end of repeat
	BCS	.LSongCommand_Break_Pattern
	BL	.LSetNewSrcAddr @ Move to end of repeat
0:	B	.LTrackReadLoop

/************************************************/

@ Fh,Dh: Return
.LSongCommandFhDh:
	LDRB	r0, [r4, #0x01] @ StackDepth -> r0
	MOV	r3, #0x2C + 0x04*SGE_MAX_STACK_DEPTH
	ADD	r3, r4          @ &RepeatCnt[] -> r3
	SUB	r0, #0x01       @ --StackDepth?
	BCC	.LSongCommand_Return_Exit
	LSL	r1, r0, #0x02   @ Stack[StackDepth-1] -> r2
	ADD	r1, #0x2C
	LDR	r2, [r4, r1]
	LSR	r2, #0x01       @ If not tagged with 'Pattern', then just ignore the command
	BCC	.LSongCommand_Return_Exit
	BEQ	.LSongCommand_Return_InlinePattern @ <- An InlinePattern stores NULL|PATTERN as the stack pointer
0:	LDRB	r1, [r3, r0]    @ ReptCnt -> r1
	SUB	r1, #0x01       @ --ReptCnt?
	BCC	.LSongCommand_Return_Finish
	STRB	r1, [r3, r0]    @ Store ReptCnt
	LSL	r0, r2, #0x01   @ Restore Src = StackPtr
	BL	.LSetNewSrcAddr
	BL	.LFetchSeekAddr @ Return to the start of the pattern
	BL	.LSetNewSrcAddr
	B	.LTrackReadLoop

.LSongCommand_Return_Finish:
	STRB	r0, [r4, #0x01] @ Store --StackDepth
.LSongCommand_Break_Pattern:
	LSL	r0, r2, #0x01   @ Restore Src = StackPtr
	BEQ	0f              @  If StackPtr == NULL, we came from a Break command inside an inline pattern.
		                @  In this case, StackDepth is already updated, and we just continue
	BL	.LSetNewSrcAddr
	BL	.LFetchSeekAddr @ Get offset for start of pattern and discard it
0:	B	.LTrackReadLoop @ Continue reading after the Call command

.LSongCommand_Return_InlinePattern:
	STRB	r0, [r4, #0x01] @ Store corrected StackDepth and exit
.LSongCommand_Return_Exit:
	B	.LTrackReadLoop

/************************************************/

@ Fh,Eh: InlinePattern
.LSongCommandFhEh:
	LDRB	r0, [r4, #0x01] @ StackDepth -> r0
	MOV	r1, #0x01
	LSL	r2, r0, #0x02
	ADD	r0, #0x01
	CMP	r0, #SGE_MAX_STACK_DEPTH
	BHI	.LSongCommandFhFh
	STRB	r0, [r4, #0x01] @ StackDepth++
	ADD	r2, #0x2C
	STR	r1, [r4, r2]    @ Stack[StackDepth] = NULL | PATTERN
	B	.LTrackReadLoop

/************************************************/

@ Fh,Fh: EndOfTrack
.LSongCommandEhCh:
.LSongCommandEhDh:
.LSongCommandEhEh:
.LSongCommandEhFh:
.LSongCommandFhFh:
.LSongCommandFhFh:
	MOV	r6, #0x00       @ Src = NULL
	@MOV	r0, #0x00       @ NewRestCounter = Not used
	B	.LTrackReadLoop_End

/************************************************/

@ Eh,Xh: Extended command
@ r1: &JumpTable
.LSongCommandEh:
	FETCH_NYBBLE
	ADD	r0, #0x10
	LSL	r0, #0x02
	LDR	r0, [r1, r0]
	BX	r0

/************************************************/

@ Fh,Xh: Extended command
@ r1: &JumpTable
.LSongCommandFh:
	FETCH_NYBBLE
	ADD	r0, #0x20
	LSL	r0, #0x02
	LDR	r0, [r1, r0]
	BX	r0

/************************************************/
.pool
/************************************************/

@ r2: CtrlOffs (relative to &Track)

.LReadCtrl8:
	ADD	r2, r4           @ &Ctrl -> r2
	BL	.LFetchByte      @ Ramp | Value<<1 -> r0
	ADD	r0, #0x01<<1     @ Unbias Value
	LSR	r0, #0x01        @ Value -> r0, Ramp -> C?
	STRB	r0, [r2, #0x03]  @ Ctrl.Target = Value
	BCC	2f
1:	PUSH	{r2}
	BL	.LFetchTimeCode  @ Duration -> r0
	POP	{r2}
	CMP	r0, #0xFF        @ Ensure no overflow
	BCC	0f
	MOV	r0, #0xFF
0:	LSL	r0, #0x08
	STRH	r0, [r2, #0x00]  @ Store Duration, Phase = 0
	B	.LTrackReadLoop
2:	STRB	r0, [r2, #0x02]  @ Ctrl.Value = Value (no ramp)
	B	.LTrackReadLoop

/************************************************/

@ r0: Ramp | Value<<1 (at most 16bit)
@ r2: CtrlOffs (relative to &Track)

.LStoreCtrlMost16:
	ASR	r0, #0x01        @ Value -> r0, Ramp -> C?
	BCC	.LStoreCtrlMost16_NoRamp

.LStoreCtrlMost16_Ramp:
	ADD	r2, r4           @ &Ctrl -> r2
	STRH	r0, [r2, #0x04]  @ Ctrl.Target = Value
	PUSH	{r2}
	BL	.LFetchTimeCode  @ Duration -> r0
	POP	{r2}
	CMP	r0, #0xFF        @ Ensure no overflow
	BCC	0f
	MOV	r0, #0xFF
0:	LSL	r0, #0x08
	STRH	r0, [r2, #0x00]  @ Store Duration, Phase = 0
	B	.LTrackReadLoop

.LStoreCtrlMost16_NoRamp:
	ADD	r2, r4           @ &Ctrl -> r2
	STRH	r0, [r2, #0x02]  @ Ctrl.Value  = Value (no ramp)
	STRH	r0, [r2, #0x04]  @ Ctrl.Target = Value
	B	.LTrackReadLoop

/************************************************/

@ This command has been moved here so that NoteOn can reach it

@ 7h: Rest
.LSongCommand7h:
	BL	.LFetchTimeCode

@ r0: NewRestCounter
.LSongCommand_Rest:
	B	.LTrackReadLoop_End

/************************************************/

@ r4: &Track
@ r5:  nNotesRem
@ r6: &Src
@ r7:  Data
@ r8: &Tone
@ r9:  RestAfterNote
@ sl:  Vel
@ fp:  DefaultVelocity
@ sp+00h:  nTicks-1
@ sp+04h:  nTracksRem
@ sp+08h: &Player
@ sp+0Ch: &Driver

@ Input:
@  r0: nTicks-1

.LReadNoteCommand:
	PUSH	{r0,r5}
	LDRB	r0, [r4, #0x0C+2] @ DefaultVelocity = Track.Vel -> fp
	MOV	r5, #0x01         @ nNotesRem = 1
	MOV	r9, r5            @ RestAfterNote = TRUE
	MOV	fp, r0

.LReadNoteCommandLoop_ResetVelocity:
	MOV	sl, fp            @ Vel = DefaultVelocity -> sl

.LReadNoteCommandLoop:
	FETCH_NYBBLE
	CMP	r0, #0x0C
	BCS	.LReadNoteCommand_ControlCode
0:	MOV	r2, #0x05
	LDRSB	r2, [r4, r2]      @ Transpose -> r2
	LDRB	r3, [r4, #0x04]   @ Octave -> r3
	ADD	r0, r2            @ Note += Transpose
	ADD	r0, r3            @ Note += Octave?
	CMP	r0, #0x7F
	BHI	.LReadNoteCommandLoop_NoteOn_Finish
	MOV	r1, r8            @ Tone -> r1?
	CMP	r1, #0x00
	BEQ	.LReadNoteCommandLoop_NoteOn_Finish
0:	LDRB	r2, [r1, #0x03]   @ nLayersRem = Tone.nLayer -> r2
	ADD	r1, #0x04         @ CurLayer = &Tone.Layers[] -> r1

/************************************************/

@ r0:  Note
@ r1: &CurLayer
@ r2:  nLayersRem
@ r3:
@ r4: &Track
@ r5:  nNotesRem
@ r6: &Src
@ r7:  Data
@ r8: &Tone
@ r9:  RestAfterNote
@ sl:  Vel
@ sp+00h:  nTicks-1
@ sp+04h:  nTracksRem
@ sp+08h: &Player
@ sp+0Ch: &Driver

.LReadNoteCommandLoop_NoteOn_LayerLoop:
	PUSH	{r2,r4-r7}
0:	LDRB	r4, [r1, #0x00]    @ Vel >= Layer.VelLo && Vel <= Layer.VelHi?
	LDRB	r5, [r1, #0x01]
	CMP	sl, r4
	LDRB	r4, [r1, #0x02]    @ nRegions -> r4
	BCC	.LReadNoteCommandLoop_NoteOn_LayerLoop_Tail
	CMP	sl, r5
	BHI	.LReadNoteCommandLoop_NoteOn_LayerLoop_Tail

.LReadNoteCommandLoop_NoteOn_LayerLoop_FindRegion:
	ADD	r5, r1, #0x04      @ &Layer.Regions[] -> r5
	LSL	r7, r4, #0x02      @ &Layer.Art[] -> r7
	ADD	r7, r5
1:	LSR	r2, r4, #0x01      @ N/2 * sizeof(Region) -> r2
	LSL	r2, #0x02
	LDR	r6, [r5, r2]       @ Regions[N/2] -> r6
	LSL	r3, r6, #(32-7)    @ Key < Region.KeyLo?
	LSR	r3, #(32-7)
	CMP	r0, r3
	BCC	2f                 @  Y: N = N/2?
10:	LSL	r3, r6, #(32-7-7)  @ Key > Region.KeyHi?
	LSR	r3, #(32-7)
	CMP	r0, r3
	BLS	.LReadNoteCommandLoop_NoteOn_LayerLoop_ParseRegion
0:	ADD	r5, r2             @  Y: Regions += N/2+1, N = (N-1)/2?
	ADD	r5, #0x04
	SUB	r4, #0x01
2:	LSR	r4, #0x01          @ <- Also follows from Key < KeyLo above
	BNE	1b

.LReadNoteCommandLoop_NoteOn_LayerLoop_Tail:
	LDRB	r4, [r1, #0x02]    @ nReg -> r4
	LDRB	r5, [r1, #0x03]    @ nArt -> r5
	ADD	r1, #0x04          @ Seek past layer header
	LSL	r4, #0x02          @ Seek past regions
	ADD	r1, r4
	LSL	r5, #0x03          @ Seek past articulations
	ADD	r1, r5
	ADD	r1, r5
	ADD	r1, r5
	POP	{r2,r4-r7}
	SUB	r2, #0x01          @ --nLayersRem?
	BHI	.LReadNoteCommandLoop_NoteOn_LayerLoop

/************************************************/

.LReadNoteCommandLoop_NoteOn_Finish:
	SUB	r5, #0x01          @ --nNotesRem?
	BNE	.LReadNoteCommandLoop_ResetVelocity

.LReadNoteCommandLoop_Exit:
	CMP	r5, r9             @ RestAfterNote?
	POP	{r0,r5}            @ [Restore nTicks -> r0, nTracksRem -> r5]
	BNE	.LSongCommand_Rest @  Y: Rest for nTicks
	B	.LTrackReadLoop    @  N: Continue reading

/************************************************/

@ r0: ControlCode
.LReadNoteCommand_ControlCode:
	CMP	r0, #0x0E          @ Ch,Dh: Octave change, Eh: Overlay, Fh: Special
	BEQ	.LReadNoteCommand_ControlCode_Overlay
	BHI	.LReadNoteCommand_ControlCode_FhXh
1:	LDRB	r3, [r4, #0x04]    @ Ch,Dh: Octave change: Octave -> r3
	SUB	r0, #0x0D          @  Expand OctaveChange = (OctaveDown = -1, OctaveUp = +1)
	LSL	r0, #0x01+2        @  Octave += OctaveChange*12
	ADD	r0, #0x01*4
	LSL	r1, r0, #0x01
	ADD	r0, r1
	ADD	r3, r0
	STRB	r3, [r4, #0x04]
	B	.LReadNoteCommandLoop

.LReadNoteCommand_ControlCode_Overlay:
	MOV	r0, #0x00
	MOV	r9, r0             @ RestAfterNote = FALSE
	B	.LReadNoteCommandLoop

.LReadNoteCommand_ControlCode_FhXh:
	FETCH_NYBBLE
	SUB	r1, r0, #0x01      @ 0h: Velocity change
	BCC	.LReadNoteCommand_ControlCode_Velocity
	SUB	r0, #0x0B          @ 1h..Bh: Octave set
	BLS	.LReadNoteCommand_ControlCode_OctaveSet

@ r0: PushSize
.LReadNoteCommand_ControlCode_StackPush:
	ADD	r5, r0             @ nNotesRem += PushSize
	B	.LReadNoteCommandLoop

.LReadNoteCommand_ControlCode_Velocity:
	BL	.LFetchByte
	ADD	r0, #0x01<<1       @ Unbias value
	LSR	r0, #0x01          @ Vel = ReadByte() >> 1
	MOV	sl, r0
	BCC	.LReadNoteCommandLoop
1:	MOV	fp, r0             @ Sticky: DefaultVelocity = Vel
	B	.LReadNoteCommandLoop

@ r1: Octave
.LReadNoteCommand_ControlCode_OctaveSet:
	LSL	r0, r1, #0x02      @ Octave *= 12
	LSL	r1, r0, #0x01
	ADD	r0, r1
	STRB	r0, [r4, #0x04]    @ Store Octave
	B	.LReadNoteCommandLoop

/************************************************/

.LReadNoteCommandLoop_NoteOn_LayerLoop_ParseRegion:
	LDR	r2, [sp, #0x1C]    @ Player.Song -> r2
	LSL	r3, r6, #(32-7-7-7) @ Art = &Layer.Art[Region.ArtIdx] -> r7
	LDR	r2, [r2, #0x10]
	LSR	r3, #(32-7)
	LSL	r3, #0x03
	LDR	r5, [r2, #0x08]    @ dbLink -> r5
	ADD	r7, r3
	ADD	r7, r3
	ADD	r7, r3
	LSR	r3, r6, #(32-11)   @ WaveIdx -> r3
	LSR	r5, #0x01          @ File database?
#if defined(SGE_PLATFORM_HAVE_FILEDB)
	BCS	1f
0:	LSL	r5, #0x01          @  Y: Load waveform from instance table
	LDR	r5, [r5, #0x08]    @     WaveTab -> r5
	LSL	r3, #0x03
	ADD	r3, #0x04
	LDR	r3, [r5, r3]       @     Wave = WaveTab[WaveIdx].Data -> r3?
	MOV	r2, #0x01          @     <- Clear Persistent flag
	BIC	r3, r2
	BNE	2f
	B	.LReadNoteCommandLoop_NoteOn_LayerLoop_Tail
#endif
1:	LSL	r5, #0x05          @  N: Memory-mapped database
	SUB	r2, r5             @     Database -> r2
	LDR	r5, [r2, #0x08]    @     WaveTabOffs -> r5
	LSL	r3, #0x02
	ADD	r5, r2             @     &Database.WaveTab[] -> r5
	LDR	r3, [r5, r3]       @     WaveOffs = WaveTab[WaveIdx].Offs -> r3
	ADD	r3, r2             @     Wave = Database.Wave[WaveIdx] -> r3
2:

@ r0:  Note
@ r1: &CurLayer
@ r2:
@ r3: &Wave
@ r4:
@ r5:
@ r6:
@ r7: &Art
@ r8: &Tone
@ r9:  RestAfterNote
@ sl:  Vel
@ sp+00h:  nLayersRem
@ sp+04h: &Track
@ sp+08h:  nNotesRem
@ sp+0Ch: &Src
@ sp+10h:  Data
@ sp+14h:  nTicks
@ sp+18h:  nTracksRem
@ sp+1Ch: &Player
@ sp+20h: &Driver

@ Priority formula:
@  p = Trk.Priority*2^27 + Art.Vol*2^19 + (Trk.Vol+Trk.Exp)*2^11 + Vox.Vel*2^4 + Ply.Vol + Vox.EG1*2^-6
@ Trk.Priority is increased by 1 for each key-on per update to ensure notes get through

.LReadNoteCommandLoop_NoteOn_LayerLoop_FindVoice:
	LDR	r2, [sp, #0x04]    @ Track -> r2
	MOV	r5, #0x24
	ADD	r5, r2
	LDRB	r4, [r5, #0x03]    @ Swap Portamento.Value = Portamento.Target so we sweep from the last note
	STRB	r0, [r5, #0x03]    @ ... and set Portamento.Target = Note
	STRB	r4, [r5, #0x02]
	LDR	r4, [sp, #0x1C]    @ Player -> r4
	LDRB	r5, [r2, #0x00]    @ Check for portamento mode
	LSR	r5, #0x02
	BCS	.LReadNoteCommandLoop_NoteOn_LayerLoop_FindVoice_Portamento
.LReadNoteCommandLoop_NoteOn_LayerLoop_FindVoice_NewPortamento:
	LDRB	r5, [r2, #0x03]    @ Track.Priority -> r5
	LDRB	r6, [r7, #0x02]    @ Art.Vol -> r6
	ADD	r5, #0x01+1        @ Add Trk.Priority (+1 to match "not in Release phase", +1 again for "key on")
	LSL	r5, #0x08
	ADD	r5, r6             @ Add Art.Vol
	LDRB	r6, [r2, #0x10+2]  @ Track.Vol -> r6
	LSL	r5, #0x08
	ADD	r5, r6             @ Add Trk.Vol
	LDRB	r6, [r2, #0x14+2]  @ Track.Exp -> r6
	LDRB	r4, [r4, #0x01]    @ Player.Vol -> r4
	ADD	r5, r6             @ Add Trk.Exp
	LSL	r5, #0x07
	ADD	r5, sl             @ Add Vel
	LSL	r5, #0x04
	ADD	r5, r4             @ Add Ply.Vol
	MOV	r4, #0x00          @ TargetVoice = NULL -> r4
	MVN	r2, r4
	LSR	r2, #(32-10)       @ Add EG1*2^-6
	ADD	r5, r2
	LDR	r2, [sp, #0x20]    @ CurVoice = &Driver.Vox[] -> r2
	PUSH	{r1,r3,r7}
	LDR	r7, [r2, #0x04]    @ Priorities -> r7
	LDRB	r3, [r2, #0x0A]    @ nVoiceRem = Driver.VoxCnt -> r3
	ADD	r2, #SGE_DRIVER_HEADER_SIZE
	MOV	r1, r5             @ TargetPriority = ThisPriority
	PUSH	{r3}               @ Push VoxCnt
1:	LDMIA	r7!, {r6}          @ Vox.Priority -> r6
	CMP	r6, r1             @ Lower priority?
	BHI	2f
	MOV	r4, r2             @  Y: TargetVoice    = Vox
	MOV	r1, r6             @     TargetPriority = Vox.Priority?
	MOV	ip, r3             @     TargetVoiceRem = nVoiceRem
	BEQ	.LReadNoteCommandLoop_NoteOn_LayerLoop_UseVoice
	MOV	lr, r7             @     TargetPriorityPtr -> lr
2:	ADD	r2, #SGE_VOX_SIZE  @ Vox++
	SUB	r3, #0x01          @ --nVoiceRem?
	BNE	1b
0:	CMP	r4, #0x00          @ Got a voice to steal?
	BNE	0f
	ADD	sp, #0x04
	POP	{r1,r3,r7}         @  N: Skip this region
	B	.LReadNoteCommandLoop_NoteOn_LayerLoop_Tail
0:	MOV	r7, lr             @ Restore &Priority[VoiceIdx+1] -> r7
	MOV	r3, ip             @ Restore nVoiceRem -> r3

@ r0:  Note
@ r1: &CurLayer
@ r2:
@ r3: &Wave
@ r4: &Voice
@ r5:  ThisPriority
@ r6:
@ r7: &VoicePriority

.LReadNoteCommandLoop_NoteOn_LayerLoop_UseVoice:
	POP	{r6}               @ VoxCnt -> r6
	SUB	r7, #0x04          @ Rewind from LDMIA
	STR	r5, [r7]           @ Store updated voice priority
	SUB	r6, r3             @ VoxIdx = VoxCnt - nVoiceRem -> r6
	POP	{r1,r3,r7}
#if SGE_PRECISE_KEYON
# if !SGE_VARIABLE_SYNC_RATE
	LDR	r5, [sp, #0x2C]    @ SampleOffs -> r5
# else
	LDR	r5, [sp, #0x30]
# endif
#endif
0:	MOV	r2, #SGE_VOX_STAT_ACTIVE | SGE_VOX_STAT_KEYON
	STRB	r2, [r4, #0x00]    @ Stat = ACTIVE|KEYON
#if SGE_PRECISE_KEYON
	STRH	r5, [r4, #0x18]    @ Phase = SampleOffs
#endif
	ADD	r4, #0x20
	STMIA	r4!, {r3,r7}       @ Store Vox.{Wav,Art}
	LDR	r5, [sp, #0x04]    @ Track -> r5
	LDR	r7, [sp, #0x1C]    @ Player -> r7
	STMIA	r4!, {r5,r7}       @ Store Vox.{Ply,Trk}
	SUB	r4, #0x30          @ Rewind to start of &Vox
	MOV	r2, #0x23          @ Track.PortamentoVoice = VoxIdx
	STRB	r6, [r5, r2]

.LReadNoteCommandLoop_NoteOn_LayerLoop_UseVoice_StoreNote:
	MOV	ip, r5             @ Track -> ip
	STRB	r0, [r4, #0x02]    @ Key = Note
	MOV	r2, sl
	STRB	r2, [r4, #0x03]    @ Vel = Vel
	MOV	r2, #0x0B          @ TicksRem = nTicks*Track.NoteLenMul + Track.NoteLenAdd -> r5
	LDRSB	r2, [r5, r2]
	LDRB	r5, [r5, #0x0A]
	LDR	r6, [sp, #0x14]
	ADD	r6, #0x01          @ <- Restore nTicks
	MUL	r5, r6
	SUB	r2, #0x01          @ <- Restore nTicks-1 by subtracting 1 from NoteLenAdd
	LSR	r5, #0x05
	ADD	r5, r2
	BGT	0f
	MOV	r5, #0x00          @ Clip TicksRem to at least 1 tick
0:	LSR	r2, r5, #0x10
	BEQ	0f
	MVN	r5, r2             @ Clip TicksRem to at most 65536 ticks
	LSR	r5, #0x10
0:	STRH	r5, [r4, #0x04]    @ Store TicksRem
	MOV	r6, ip
	ADD	r6, #0x24
	LDRB	r2, [r6, #0x01]    @ Clip PortamentoDuration to TicksRem
	CMP	r2, r5
	BCC	0f
	STRB	r5, [r6, #0x01]
0:	B	.LReadNoteCommandLoop_NoteOn_LayerLoop_Tail

.LReadNoteCommandLoop_NoteOn_LayerLoop_FindVoice_Portamento:
	MOV	r5, r2
	MOV	r5, #0x23
	LDRB	r5, [r2, r5]       @ PortamentoVoice -> r5
	MOV	r6, #SGE_VOX_SIZE
	CMP	r5, #0xFF          @ If no voice is allocated, restart the effect
	BEQ	.LReadNoteCommandLoop_NoteOn_LayerLoop_FindVoice_PortamentoRestart
	MUL	r5, r6
	LDR	r6, [sp, #0x20]
	ADD	r5, #SGE_DRIVER_HEADER_SIZE
	ADD	r5, r6
	LDRB	r6, [r5, #0x00]    @ Ensure the voice is still active
	LSL	r6, #0x20-7
	BCC	.LReadNoteCommandLoop_NoteOn_LayerLoop_FindVoice_PortamentoRestart
	LDR	r6, [r5, #0x28]    @ Ensure the voice still belongs to this track
	CMP	r6, r2
	BNE	.LReadNoteCommandLoop_NoteOn_LayerLoop_FindVoice_PortamentoRestart
	LDRB	r6, [r5, #0x00]    @ If we JUST did a key-off earlier (ie. TicksRem == FFFFh with KEYOFF), we can still use this voice
	LSL	r6, #0x20-5
	BCS	1f
0:	LDRH	r6, [r5, #0x04]    @ Portamento.Duration = TicksRem
	CMP	r6, #0xFF
	BCC	0f
	MOV	r6, #0xFF
0:	MOV	r4, #0x24+0x01
	STRB	r6, [r2, r4]
0:	MOV	r4, r5             @ <- We expect &Voice in r4
	MOV	r5, r2             @    ... and we expected &Track in r5
	B	.LReadNoteCommandLoop_NoteOn_LayerLoop_UseVoice_StoreNote
1:	MOV	r6, #0x04          @ Ensure TicksRem == FFFFh before using the edge case handler
	LDRSH	r6, [r5, r6]
	ADD	r6, #0x01
	BNE	.LReadNoteCommandLoop_NoteOn_LayerLoop_FindVoice_PortamentoRestart

.LReadNoteCommandLoop_NoteOn_LayerLoop_FindVoice_PortamentoEdgeCase:
	LDRB	r7, [r5, #0x00]    @ Clear KEYOFF and NOPLAYER
	STR	r2, [r5, #0x28]    @ Restore Player,Trk to voice
	STR	r4, [r5, #0x2C]
	SUB	r7, #SGE_VOX_STAT_KEYOFF|SGE_VOX_STAT_NOPLAYER
	STRB	r7, [r5, #0x00]
	MOV	r7, #0x24
	ADD	r7, r2
	STRB	r0, [r7, #0x02]    @ Portamento.Value = Note to resynchronize at new pitch
	STRB	r6, [r7, #0x00]    @ Portamento.Phase = 0
	B	0b

.LReadNoteCommandLoop_NoteOn_LayerLoop_FindVoice_PortamentoRestart:
	MOV	r5, #0x24
	ADD	r5, r2
	MOV	r6, #0x00
	STRB	r0, [r5, #0x02]    @ Portamento.Value = Note because we lost track of the overlap
	STRB	r6, [r5, #0x00]    @ Portamento.Phase = 0
	B	.LReadNoteCommandLoop_NoteOn_LayerLoop_FindVoice_NewPortamento

/************************************************/

@ Returns Byte in r0
@ Clobbers r1

.LFetchByte:
	PUSH	{lr}
	FETCH_NYBBLE
	LSL	r1, r0, #0x04
	FETCH_NYBBLE
	ORR	r0, r1
	POP	{pc}

/************************************************/

@ Returns Word in r1(!)
@ Clobbers r1,r3

.LFetchWord:
	MOV	r3, #0x04
	@B	.LFetchMulti

@ r3: nNybbles
@ Returns Data in r1(!)
@ Clobbers r1,r3

.LFetchMulti:
	PUSH	{lr}
	FETCH_NYBBLE
	MOV	r1, r0
	SUB	r3, #0x01
	BLS	0f
1:	FETCH_NYBBLE
	LSL	r1, #0x04
	ORR	r1, r0
	SUB	r3, #0x01
	BNE	1b
0:	POP	{pc}

/************************************************/

@ Returns SeekAddr (modified with Src) in r0
@ Clobbers r1,r2,r3

.LFetchSeekAddr:
	PUSH	{lr}
	BL	.LFetchByte
	CMP	r0, #0xFC       @ 1-byte address?
	BLS	.LFetchSeekAddr_1byte
	CMP	r0, #0xFE
	BEQ	.LFetchSeekAddr_3byte
	BHI	.LFetchSeekAddr_4byte
	@BCC	.LFetchSeekAddr_2byte

.LFetchSeekAddr_2byte:
	BL	.LFetchByte
	ADD	r0, #0xFD
	B	.LFetchSeekAddr_Unpack

.LFetchSeekAddr_3byte:
	BL	.LFetchWord
	MOV	r0, r1
	ADD	r0, #0xFF
	ADD	r0, #0x01FD-0xFF
	B	.LFetchSeekAddr_Unpack

.LFetchSeekAddr_4byte:
	BL	.LFetchWord
	LSL	r2, r1, #0x08
	BL	.LFetchByte
	LDR	r1, =0x0101FD
	ORR	r0, r2
	ADD	r0, r1
	@B	.LFetchSeekAddr_Unpack

.LFetchSeekAddr_1byte:
.LFetchSeekAddr_Unpack:
	ADD	r0, #0x01<<1    @ Add 1 to magnitude
	LSR	r0, #0x01       @ Extract magnitude and apply sign
	BCS	0f
	NEG	r0, r0
0:	LSL	r1, r0, #(32-3) @ Extract delta nybble
	ASR	r0, #0x03       @ Extract delta 32bit address
	LSL	r0, #0x02
	ADD	r0, r6          @ Combine with current Src
	ADD	r0, r1
	BCC	0f
	ADD	r0, #0x04       @ On overflowing NybbleIdx, increment Src by one word
0:	POP	{pc}

/************************************************/

@ r0: &NewSrc | NewNybbleIdx<<(32-3)
@ Loads Data -> r7, and puts NewSrc -> r6

.LSetNewSrcAddr:
	MOV	r6, r0          @ Src = NewSrc
	LSR	r0, #(32-3)     @ Shift = NybbleIdx*4 -> r0
	LSL	r7, r6, #0x03   @ Clear NybbleIdx
	LSR	r7, #0x03
	LDR	r7, [r7]        @ Reload word
	LSL	r0, #0x02
	LSR	r7, r0
	BX	lr

/************************************************/

@ Returns nTicks-1(!) in r0
@ Clobbers r1,r2,r3

.LFetchTimeCode:
	PUSH	{lr}
	MOV	r2, #0x00                      @ nTicks = 0 -> r0

.LFetchTimeCode_DecodeLoop:
	FETCH_NYBBLE
	CMP	r0, #0x0E                      @ Eh = Triplet
	BHI	.LFetchTimeCode_Ticks          @ Fh = Tick-coded word
	BEQ	.LFetchTimeCode_Triplet
	MOV	r3, #SGE_WHOLENOTE_TICKS       @ Base = 1/1
.LFetchTimeCode_DecodeNormal:
	SUB	r1, r0, #0x07                  @ Shift -> r1. Tied?
	BCS	1f
0:	ADD	r1, #0x07                      @  N: Restore Shift
1:	LSR	r3, r1                         @ nTicks += Base >> Shift
	ADD	r2, r3
	CMP	r0, #0x07                      @ Tied?
	BCS	.LFetchTimeCode_DecodeLoop     @  Y: Continue decoding

.LFetchTimeCode_Exit:
	SUB	r0, r2, #0x01                  @ Return nTicks-1
	POP	{pc}

.LFetchTimeCode_Triplet:
	FETCH_NYBBLE
	CMP	r0, #0x0E                      @ Eh,Eh..Fh = 1/128 (triplet)
	BCS	2f
1:	MOV	r3, #SGE_WHOLENOTE_TICKS * 2/3 @ Base = 1/1 triplet
	B	.LFetchTimeCode_DecodeNormal
2:	ADD	r2, #0x01                      @ nTicks += Base >> Shift (=1 tick)
	CMP	r0, #0x0F                      @ Eh,Fh = 1/128 (tied triplet)
	BNE	.LFetchTimeCode_Exit
	B	.LFetchTimeCode_DecodeLoop

.LFetchTimeCode_Ticks:
	BL	.LFetchWord                    @ Return (nTicks-1)(=ReadWord())
	MOV	r0, r1
	POP	{pc}

/************************************************/

@ Returns result of call in r0
@ Clobbers r1,r2,r3

.LProcessExternalCall:
	PUSH	{lr}
	FETCH_NYBBLE              @ PayloadSize -> r0?
	MOV	r1, #0x00         @ Set default Payload = 0 -> r1
	MOV	r3, r0            @ <- FetchMulti needs nNybbles in r3
	BEQ	0f
	BL	.LFetchMulti      @ Payload -> r1
	LSR	r1, #0x01
	BCC	0f
	NEG	r1, r1
0:	LDR	r3, [sp, #0x04]   @ Player -> r3
	POP	{r2}
	LDR	r3, [r3, #0x04]   @ ExtCallFunc -> r3?
	MOV	lr, r2
	MOV	r2, r4
	CMP	r3, #0x00
	BEQ	0f
	BX	r3                @  Y: Return ExtCallFunc(PayloadSize, Payload, Track)
0:	MOV	r0, #0x00         @  N: Return FALSE
	BX	lr

/************************************************/

ASM_FUNC_END(SGE_Driver_SongCommands)

/************************************************/

ASM_DATA_BEG(SGE_Driver_SongCommandJumpTable, ASM_DATASECT_RODATA;ASM_ALIGN(4))

SGE_Driver_SongCommandJumpTable:
	.word .LSongCommand0h  +1, .LSongCommand1h  +1, .LSongCommand2h  +1, .LSongCommand3h  +1
	.word .LSongCommand4h  +1, .LSongCommand5h  +1, .LSongCommand6h  +1, .LSongCommand7h  +1
	.word .LSongCommand8h  +1, .LSongCommand9h  +1, .LSongCommandAh  +1, .LSongCommandBh  +1
	.word .LSongCommandCh  +1, .LSongCommandDh  +1, .LSongCommandEh  +1, .LSongCommandFh  +1

.LEhCommandTable:
	.word .LSongCommandEh0h+1, .LSongCommandEh1h+1, .LSongCommandEh2h+1, .LSongCommandEh3h+1
	.word .LSongCommandEh4h+1, .LSongCommandEh5h+1, .LSongCommandEh6h+1, .LSongCommandEh7h+1
	.word .LSongCommandEh8h+1, .LSongCommandEh9h+1, .LSongCommandEhAh+1, .LSongCommandEhBh+1
	.word .LSongCommandEhCh+1, .LSongCommandEhDh+1, .LSongCommandEhEh+1, .LSongCommandEhFh+1

.LFhCommandTable:
	.word .LSongCommandFh0h+1, .LSongCommandFh1h+1, .LSongCommandFh2h+1, .LSongCommandFh3h+1
	.word .LSongCommandFh4h+1, .LSongCommandFh5h+1, .LSongCommandFh6h+1, .LSongCommandFh7h+1
	.word .LSongCommandFh8h+1, .LSongCommandFh9h+1, .LSongCommandFhAh+1, .LSongCommandFhBh+1
	.word .LSongCommandFhCh+1, .LSongCommandFhDh+1, .LSongCommandFhEh+1, .LSongCommandFhFh+1

ASM_DATA_END(SGE_Driver_SongCommandJumpTable)

/************************************************/
//! EOF
/************************************************/
