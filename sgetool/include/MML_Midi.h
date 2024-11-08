/************************************************/
#pragma once
/************************************************/
#include <stdint.h>
/************************************************/
#include "MML.h"
/************************************************/

#define COMBINE_MSB_LSB_16(MSB, LSB) ((uint16_t)MSB << 7 | (uint16_t)LSB)

/************************************************/

//! Maximum size of a SysEx message, including terminating F7h
#define MAX_SYSEX_SIZE 256

//! Buffer used to store SysEx messages
//! This avoids putting it on the stack as part of MidiState_t
uint8_t MML_Midi_SysExBuffer[MAX_SYSEX_SIZE];

/************************************************/

enum {
	RPN_BENDRANGE  = 0,
	RPN_FINETUNE   = 1,
	RPN_COARSETUNE = 2,
};
enum {
	CC_BANK_MSB     =   0,
	CC_DATA_MSB     =   6,
	CC_VOLUME       =   7,
	CC_PAN          =  10,
	CC_EXPRESSION   =  11,
	CC_BANK_LSB     =  32,
	CC_DATA_LSB     =  38,
	CC_PEDAL_DAMPER =  64, //! x < 64: Off. x >= 64: On
	CC_DATA_INC     =  96,
	CC_DATA_DEC     =  97,
	CC_NRPN_LSB     =  98,
	CC_NRPN_MSB     =  99,
	CC_RPN_LSB      = 100,
	CC_RPN_MSB      = 101,
};
struct MML_MidiChan_t {
	uint8_t  IsPopulated:1;
	uint8_t  IsDrumsChannel:1;
	uint8_t  Vol:7;
	uint8_t  VolPending:1;
	uint8_t  Pan:7;
	uint8_t  PanPending:1;
	uint8_t  Exp:7;
	uint8_t  ExpPending:1;
	uint16_t Bend:15;
	uint16_t BendPending:1;
	uint16_t Tempo:15;
	uint16_t TempoPending:1;
	uint8_t  DamperPedal;     //! >= 64: Pedal down
	uint8_t  BankLSB;
	uint8_t  BankMSB;
	uint8_t  Program;
	uint16_t RawBendValue;    //! From MIDI command. We need to buffer this for master transpose/tune updates
	uint16_t BendRange;       //! From RPN 0 (128 = 1.0st)
	uint16_t ParamNumber;     //! From RPN/NRPN commands
	uint16_t ParamData;       //! From RPN/NRPN data commands
	 int16_t MasterTranspose; //! From RPN 2 (in semitones)
	 int32_t MasterTune;      //! From RPN 1 (8192 = 1.0st)

	uint8_t *DataBuffer;
	uint32_t DataBufferOffs;
	uint32_t DataBufferSize;
	uint64_t LastCommandTick;
	uint8_t  NoteOnVel[128];
	uint32_t NoteOnMask[128/32];
	uint32_t NoteOffMask[128/32];
	uint32_t DampersMask[128/32];
	uint32_t NoteStartTick[128];  //! Initial tick of each note (hopefully no note lasts longer than 2^32-1 ticks)
	uint32_t NoteBufferOffs[128]; //! Offset of each active note in the data buffer
};

/************************************************/

struct MML_MidiTrack_t {
	uint8_t LastCommand;
	const uint8_t *SrcDataBuffer;
	const uint8_t *SrcDataBufferEnd;
};

/************************************************/

enum {
	MIDI_SYSMODE_GM,
	MIDI_SYSMODE_GS,
	MIDI_SYSMODE_XG,
};
struct MML_MidiState_t {
	uint16_t Format;
	uint16_t nTracks;
	uint16_t nChannels;
	uint16_t Tempo;
	uint16_t TicksPerBeat;
	uint32_t SysExSize;
	uint64_t CurrentTick;
	uint8_t  MetaChannelPrefix;
	uint8_t *DataBuffer;
	uint8_t *DataBufferEnd;
	struct MML_MidiTrack_t   *Tracks;
	struct MML_MidiChannel_t *Channels;

	uint8_t SysMode;
	uint8_t ResampleTimings;
	uint8_t Ch9Drums:1;      //! GS only
	uint8_t Ch11Drums:1;     //! GS only
	 int8_t MasterTranspose; //! "Coarse" tuning (in semitones)
	int32_t MasterTune;      //! "Fine" tuning (8192000 = 1.0st)
};

/************************************************/

//! TODO: MidiState init etc

//! SysEx handling
void MML_Midi_InterpretSysEx(struct MML_MidiState_t *State);

/************************************************/

//! TODO: MidiTrack init etc

//! Parse next track command
int MML_MidiTrack_ParseNextCommand(struct MML_MidiState_t *State, struct MML_MidiTrack_t *Track);

/************************************************/

//! Initialize channel
void MML_MidiChan_Init(struct MML_MidiChan_t *x, uint8_t IsDrumsChannel);

//! Reset channel
void MML_MidiChan_Reset(struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel);

//! Release all channel notes
int MML_MidiChan_ReleaseAllNotes(struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel);

//! Finalize channel data
int MML_MidiChan_Finalize(struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel);

//! Convert state into MML text
int MML_MidiChan_ConvertToText(
	struct MML_MidiChan_t *Channel,
	char **BufPtr,
	uint32_t *BufOffsPtr,
	uint32_t *BufSizePtr
);

//! Destroy channel
void MML_MidiChan_Destroy(struct MML_MidiChan_t *x);

//! Command handling
//! Notes:
//!  -MML_MidiChan_SetProgram() does NOT store bank MSB/LSB or IsDrums.
int MML_MidiChan_SetVolume     (struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel, uint8_t Vol);
int MML_MidiChan_SetPan        (struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel, uint8_t Pan);
int MML_MidiChan_SetExpression (struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel, uint8_t Exp);
int MML_MidiChan_SetPitchBend  (struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel, uint16_t RawBend);
int MML_MidiChan_SetParameter  (struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel);
int MML_MidiChan_SetTempo      (struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel, uint32_t BPM);
int MML_MidiChan_ProcessNoteOff(struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel, uint8_t Key);
int MML_MidiChan_ProcessNoteOn (struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel, uint8_t Key, uint8_t Vel);
int MML_MidiChan_DamperPedalUp (struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel);
int MML_MidiChan_SetProgram    (struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel, uint8_t Program, uint8_t BankMSB, uint8_t BankLSB, uint8_t IsDrums);
int MML_MidiChan_InsertComment (struct MML_MidiState_t *State, struct MML_MidiChan_t *Channel, const char *Comment);

/************************************************/
//! EOF
/************************************************/
