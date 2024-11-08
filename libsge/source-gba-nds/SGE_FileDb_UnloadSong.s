/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
/************************************************/
#ifdef SGE_PLATFORM_HAVE_FILEDB
/************************************************/

@ r0: &Song

ASM_FUNC_GLOBAL(SGE_FileDb_UnloadSong)
ASM_FUNC_BEG   (SGE_FileDb_UnloadSong, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_FileDb_UnloadSong:
	CMP	r0, #0x00
	BEQ	.LExit_Fail
0:	LDR	r3, [r0, #0x08] @ Song.Db -> r3
	LSR	r3, #0x01       @ Check dbLink.isRaw
	BCS	.LExit_Fail

.LUnloadTones:
	MOV	r3, lr
	PUSH	{r3-r6}
0:	LDRB	r3, [r0, #0x00] @ nTracks -> r3
	LDRB	r2, [r0, #0x01] @ nTones -> r2
	MOV	r1, r0          @ Song.TrkOffs[] -> r1
	ADD	r1, #0x10
	LSL	r3, #0x02       @ Song.Tones[] -> r1
	ADD	r1, r3
0:	CMP	r2, #0x00
	BEQ	.LUnloadTones_Done

.LUnloadTones_Loop:
	LDMIA	r1!, {r3}       @ nLayers -> r3?
	LSR	r3, #0x18
	BEQ	.LUnloadTones_LoopTail

.LUnloadTones_Layers_Loop:
	LDRB	r4, [r1, #0x02] @ nRegions -> r4?
	LDRB	r5, [r1, #0x03] @ nArticulations -> r5
	ADD	r1, #0x04
	CMP	r4, #0x00
	BEQ	.LUnloadTones_Layers_LoopTail

.LUnloadTones_Regions_Loop:
	LDMIA	r1!, {r6}       @ Unload waveform for this region
	PUSH	{r0-r3}
	LDR	r1, [r0, #0x08]
	LSR	r0, r6, #(32-11)
	BL	SGE_FileDb_UnloadWaveByIndex
	POP	{r0-r3}

.LUnloadTones_Regions_LoopTail:
	SUB	r4, #0x01       @ --nRegionsRem?
	BNE	.LUnloadTones_Regions_Loop

.LUnloadTones_Layers_LoopTail:
	LSL	r5, #0x03       @ Skip articulations (18h in size)
	ADD	r1, r5
	LSL	r5, #0x01
	ADD	r1, r5
	SUB	r3, #0x01       @ --nLayersRem?
	BNE	.LUnloadTones_Layers_Loop

.LUnloadTones_LoopTail:
	SUB	r2, #0x01       @ --nTonesRem?
	BNE	.LUnloadTones_Loop

.LUnloadTones_Done:
	POP	{r3-r6}

.LUnloadInstance:
	MOV	lr, r3
	LDR	r3, [r0, #0x08] @ Song.Db -> r3
	LDRH	r0, [r0, #0x02] @ Return FreeInstance(SongIdx, nSongs, SongTab, Callbacks)
	LDRH	r1, [r3, #0x02]
	LDR	r2, [r3, #0x0C]
	LDR	r3, [r3, #0x04]
	B	SGE_FileDb_FreeInstance

.LExit_Fail:
	MOV	r0, #0x00       @ Return FALSE
	BX	lr

ASM_FUNC_END(SGE_FileDb_UnloadSong)

/************************************************/
#endif
/************************************************/
//! EOF
/************************************************/
