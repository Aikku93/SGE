/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
/************************************************/
#ifdef SGE_PLATFORM_HAVE_FILEDB
/************************************************/

@ r0: &Db
@ r1:  Idx
@ r2:  Persistent

ASM_FUNC_GLOBAL(SGE_FileDb_LoadSong)
ASM_FUNC_BEG   (SGE_FileDb_LoadSong, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_FileDb_LoadSong:
	PUSH	{r0,r2,lr}
	LDRH	r3, [r0, #0x02] @ LoadInstance(Idx, nSongs, SongTab, Callbacks)?
	LDR	r2, [r0, #0x0C]
	MOV	r0, r1
	MOV	r1, r3
	LDR	r3, [sp, #0x00]
	LDR	r3, [r3, #0x04]
	BL	SGE_FileDb_LoadInstance
	POP	{r1,r2,r3}
	CMP	r0, #0x00
	BEQ	.LExit_r3
	STR	r1, [r0, #0x08] @ Store Wave.dbLink.Db = Db
	PUSH	{r2-r6}
0:	CMP	r2, #0x00       @ Need to mark as Persistent?
	BEQ	1f
	LDRH	r2, [r0, #0x02] @ SongIdx -> r2
	LDR	r1, [r1, #0x0C] @ SongTab -> r1
	LSL	r2, #0x03
	ADD	r1, r2          @ Seek to SongTab[SongIdx] -> r1
	MOV	r2, #0x01       @ Set Persistent bit in Data pointer
	ORR	r2, r0
	STR	r2, [r1, #0x04]
1:

.LLoadTones:
	LDRB	r3, [r0, #0x00] @ nTracks -> r3
	LDRB	r2, [r0, #0x01] @ nTones -> r2
	MOV	r1, r0          @ Song.TrkOffs[] -> r1
	ADD	r1, #0x10
	LSL	r3, #0x02       @ Song.Tones[] -> r1
	ADD	r1, r3
0:	CMP	r2, #0x00
	BEQ	.LLoadTones_Done

.LLoadTones_Loop:
	LDMIA	r1!, {r3}       @ nLayers -> r3?
	LSR	r3, #0x18
	BEQ	.LLoadTones_LoopTail

.LLoadTones_Layers_Loop:
	LDRB	r4, [r1, #0x02] @ nRegions -> r4?
	LDRB	r5, [r1, #0x03] @ nArticulations -> r5
	ADD	r1, #0x04
	CMP	r4, #0x00
	BEQ	.LLoadTones_Layers_LoopTail

.LLoadTones_Regions_Loop:
	LDMIA	r1!, {r6}       @ Load waveform for this region
	PUSH	{r0-r3}
	LDR	r0, [r0, #0x08]
	LSR	r1, r6, #(32-11)
	LDR	r2, [sp, #0x10]
	BL	SGE_FileDb_LoadWave
	POP	{r0-r3}

.LLoadTones_Regions_LoopTail:
	SUB	r4, #0x01       @ --nRegionsRem?
	BNE	.LLoadTones_Regions_Loop

.LLoadTones_Layers_LoopTail:
	LSL	r5, #0x03       @ Skip articulations (18h in size)
	ADD	r1, r5
	LSL	r5, #0x01
	ADD	r1, r5
	SUB	r3, #0x01       @ --nLayersRem?
	BNE	.LLoadTones_Layers_Loop

.LLoadTones_LoopTail:
	SUB	r2, #0x01       @ --nTonesRem?
	BNE	.LLoadTones_Loop

.LLoadTones_Done:
	POP	{r2-r6}

.LExit_r3:
	@MOV	r0, r0          @ Return Song
	BX	r3

ASM_FUNC_END(SGE_FileDb_LoadSong)

/************************************************/
#endif
/************************************************/
//! EOF
/************************************************/
