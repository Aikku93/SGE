/************************************************/
#pragma once
/************************************************/

//! Platform-specific pre-processor definitions:
//!  -SGE_DECLSPEC (must always be defined, even if empty)
//!  -SGE_PLATFORM_DRIVERDATA_SIZE (must always be defined; sizeof(struct SGE_Driver_Platform_t))
//!  -SGE_PLATFORM_IS_64BIT
//!  -SGE_PLATFORM_HAVE_FILEDB
//!  -SGE_PLATFORM_HAVE_REVERB
//!  -SGE_PLATFORM_HAVE_FANCY_REVERB (requires SGE_PLATFORM_HAVE_REVERB)
//!  -SGE_PLATFORM_HAVE_FAKE_REVERB (requires no SGE_PLATFORM_HAVE_REVERB)
//! Platform-specific type definitions:
//!  -SGE_MixSmp_t
//!  -SGE_OutSmp_t
//!  -struct SGE_Driver_Platform_t {}
//! Notes:
//!  -If SGE_Driver_Platform_t is not needed, it must explicitly contain
//!   no data (eg. `int Dummy[0]`), as otherwise g++ may interpret it to
//!   contain `char[1]`, which will break assumptions in C/C++ code.
//!   Note that the structure must be aligned to the size of a pointer;
//!   see the definition of SGE_PTRALIGNED for an example.
//!  -FANCY_REVERB applies a low-pass filter at every delay line to add
//!   absorption to each reflection, giving a much more realistic effect.
//!   This effect adds more CPU load, and the reverb's low-pass cutoff
//!   parameter will need to be increased to account for it.
//!  -To expose internal functions and parameters, define SGE_INTERNALS.

/************************************************/

//! Ensure the platform has been defined
#ifndef SGE_DECLSPEC
# error "Do not include SGE.h directly. Use the platform-specific file."
#endif

//! Declare default stack size if not declared
#ifndef SGE_MAX_STACK_DEPTH
# define SGE_MAX_STACK_DEPTH 4
#endif

/************************************************/

//! SGE_MemDb_t::Magic
#define SGE_DB_MAGIC ('S' | 'G'<<8 | 'd'<<16 | 'b'<<24)

/************************************************/

//! SGE_Wav_t::Interpolate
#define SGE_WAV_INTERPOLATE_OFF    0x00 //! Never interpolate
#define SGE_WAV_INTERPOLATE_ON     0x01 //! Always interpolate
#define SGE_WAV_INTERPOLATE_RATELT 0x02 //! Interpolate when Rate < 1.0
#define SGE_WAV_INTERPOLATE_RATEGT 0x03 //! Interpolate when Rate >= 1.0

//! SGE_Wav_t::Frmt
#define SGE_WAV_FRMT_ADPCM4 0x00
#define SGE_WAV_FRMT_PCM8   0x01
#define SGE_WAV_FRMT_PCM16  0x02
#define SGE_WAV_FRMT_CNT    0x03

//! SGE_Wav_t::Chan
#define SGE_WAV_CHAN_MAX 2

/************************************************/
#ifndef SGE_PLATFORM_IS_COMPILER
/************************************************/

//! SGE_Vox_t::Stat
#define SGE_VOX_STAT_EG_ATK    0x00
#define SGE_VOX_STAT_EG_HLD    0x01
#define SGE_VOX_STAT_EG_DEC    0x02
#define SGE_VOX_STAT_EG_SUS    0x03
#define SGE_VOX_STAT_EG_MSK    0x03
#define SGE_VOX_STAT_EG1_SHIFT 0x00 //! Bit0..1
#define SGE_VOX_STAT_EG2_SHIFT 0x02 //! Bit2..3
#define SGE_VOX_STAT_NOPLAYER  0x10
#define SGE_VOX_STAT_KEYOFF    0x20
#define SGE_VOX_STAT_KEYON     0x40
#define SGE_VOX_STAT_ACTIVE    0x80

//! SGE_Driver_t::State
#ifdef SGE_INTERNALS
# define SGE_DRIVER_STATE_MAGIC  (0x656773) //! "sge"
# define SGE_DRIVER_STATE_READY  (0x00 | SGE_DRIVER_STATE_MAGIC<<8)
# define SGE_DRIVER_STATE_PAUSED (0x80 | SGE_DRIVER_STATE_MAGIC<<8)
# ifndef SGE_PLATFORM_IS_64BIT
#  define SGE_DRIVER_HEADER_SIZE (0x18 + SGE_PLATFORM_DRIVERDATA_SIZE)
#  define SGE_VOX_SIZE           (0x30)
#  define SGE_PLAYER_HEADER_SIZE (0x20)
#  define SGE_TRACK_SIZE         (0x2C + 0x04*SGE_MAX_STACK_DEPTH + ((0x01*SGE_MAX_STACK_DEPTH+3)&~3))
# else
#  define SGE_DRIVER_HEADER_SIZE (0x28 + SGE_PLATFORM_DRIVERDATA_SIZE)
#  define SGE_VOX_SIZE           (0x48)
#  define SGE_PLAYER_HEADER_SIZE (0x38)
#  define SGE_TRACK_SIZE         (0x30 + 0x08*SGE_MAX_STACK_DEPTH + ((0x01*SGE_MAX_STACK_DEPTH+7)&~7))
# endif
# define SGE_WHOLENOTE_TICKS     (192) //! Ticks per whole note
# define SGE_QUARTERNOTE_TICKS   (SGE_WHOLENOTE_TICKS/4)
#endif

//! SGE_Reverb_InitReverbData() and SGE_Reverb_GetLineLengths()
#ifdef SGE_PLATFORM_HAVE_REVERB
# define SGE_REVERB_ERROR_TOO_SHORT  ( 0) //! DecayTime is too short (or nTapLines == 0)
# define SGE_REVERB_ERROR_TOO_LONG   (-1) //! DecayTime is too long
# define SGE_REVERB_ERROR_TOO_SPARSE (-2) //! RoomDensity is too low
# define SGE_REVERB_ERROR_TOO_WIDE   (-3) //! StereoWidth is too large
#endif

/************************************************/
#endif // SGE_PLATFORM_IS_COMPILER
/************************************************/
#ifndef __ASSEMBLER__
/************************************************/
#include <stdint.h>
/************************************************/
#ifdef __cplusplus
extern "C" {
#endif
/************************************************/

#define SGE_FORCE_INLINE  __attribute__((always_inline)) static inline
#define SGE_PACKED        __attribute__((packed))
#define SGE_ALIGNED       __attribute__((aligned(4)))
#define SGE_PTRALIGNED    __attribute__((aligned(__SIZEOF_POINTER__)))

/************************************************/

//! Database linkage structure
#ifdef SGE_PLATFORM_HAVE_FILEDB
struct SGE_Db_t;
#endif
union SGE_ALIGNED SGE_PACKED SGE_DbLink_t {
#ifdef SGE_PLATFORM_HAVE_FILEDB
	//! [00h] Database pointer (when loaded via SGE_FileDb_LoadWave()/SGE_FileDb_LoadSong())
	struct SGE_Db_t *Db;
#endif
	//! [00h] Absolute offset in database (when loaded via SGE_MemDb_LoadSong())
	struct {
		uint32_t isRaw:1;   //! [00h, b0]     Always 1 (0 = FileDb, 1 = Memory)
		uint32_t dbOffs:31; //! [00h, b1..31] Absolute offset in file, divided by 32
		uint32_t r1;        //! [04h]         [Reserved for 64-bit alignment of Db]
	};
};

/************************************************/

//! Waveform ADPCM header structure [10h bytes]
struct SGE_ALIGNED SGE_PACKED SGE_ADPCMHeader_t {
	 int16_t Init[2]; //! [00h] Initial samples
	 int16_t Coef[2]; //! [04h] LPC filter coefficients
	 int16_t Loop[2]; //! [08h] Loop samples
	uint32_t LpPt;    //! [0Ch] Loop point (in frames)
};

//! Waveform structure [18h + var bytes]
//! Notes:
//!  -Stereo ADPCM has two headers, followed by interleaved frames.
struct SGE_ALIGNED SGE_PACKED SGE_Wav_t {
	uint8_t  Interpolate:4; //! [00h, b0..3] Interpolation mode
	uint8_t  Frmt:4;        //! [00h, b4..7] Encoding format
	uint8_t  Chan;          //! [01h] Channel count
	uint16_t dbIdx;         //! [02h] Wavetable index (in database)
	uint32_t Size:24;       //! [04h, b 0..23] Waveform length (in samples)
	uint32_t Root:8;        //! [04h, b24..31] Root key (60 = Middle-C)
	uint32_t Loop:24;       //! [08h, b 0..23] Loop size (in samples; 0 = No loop)
	uint32_t Fine:8;        //! [08h, b24..31] Fine-tuning (.8fxp, semitones)
	uint32_t Freq:24;       //! [0Ch, b 0..23] Natural frequency (in Hz)
	uint32_t Gain:8;        //! [0Ch, b24..31] Volume (00h = 1/256, 7Fh = 100%, FFh = 200%)
	union SGE_DbLink_t dbLink; //! [10h] Database link
	union {                 //! [18h] Sample data
		uint8_t  Data[0];  //! [00h] Generic byte data
		uint8_t  PCM8[0];  //! [00h] Sample data (unsigned 8bit PCM)
		 int16_t PCM16[0]; //! [00h] Sample data (signed 16bit PCM)
		struct {
			struct SGE_ADPCMHeader_t Header; //! [00h] ADPCM header
			uint32_t Data[0];                //! [10h] ADPCM frames
		} ADPCM[0];
	};
};

/************************************************/

//! Waveform articulation structure [18h bytes]
//! Notes:
//!  -Timings are companded in a square root format:
//!     Msecs = Value^2 * 1000/1626
//!   This allows higher accuracy for shorter envelopes, with a total range of
//!   0.0 .. 39.99 seconds
//!   This is a much cheaper approximation to a true logarithmic companding.
//!  -LFO->Key operates using a Sin[x] operator (or similar).
//!  -LFO->Volume operates using a (1+Cos[x])/2 operator (or similar).
//!  -EG1 generates an exponential decay with linear (or parabolic) attack.
//!  -EG2 generates a fully linear curve for all phases.
struct SGE_ALIGNED SGE_PACKED SGE_WavArt_t {
	uint8_t Vol;          //! [00h] Master volume    (00h = 1/256, FFh = 100%)
	 int8_t Pan;          //! [01h] Master panning   (-7Eh = 100% L, 00h = Center, +7Eh = 100% R)
	int16_t Tune;         //! [02h] Master tuning    (+/-0100h = +/-1.0 semitones)
	uint8_t LFODelay;     //! [04h] Delay (or ramp) time
	uint8_t LFORate;      //! [05h] LFO frequency    (00h = 1/16Hz, FFh = 16Hz)
	int16_t LFOToKey;     //! [06h] LFO -> Key       (+/-0100h = +/-1.0 semitones)
	uint8_t LFOToVol;     //! [08h] LFO -> Volume    (00h = 0%, FFh = 255/256)
	uint8_t LFOShape:4;   //! [09h, b0..3] LFO shape (0..4 = +/-, 5..9 = +, 10..14 = -; sine/tri/saw/square/noise)
	uint8_t LFOAmpRamp:1; //! [09h, b4..4] Ramp LFO amplitude over delay time
	uint8_t LFOFrqRamp:1; //! [09h, b5..5] Ramp LFO frequency over delay time
	uint8_t r1:2;         //! [09h, b6..7]
	uint8_t EG1Shape:1;   //! [0Ah, b0..0] EG1 shape (0 = Linear, 1 = Parabolic)
	uint8_t r2:7;         //! [0Ah, b1..7]
	uint8_t r3;           //! [0Bh]
	int16_t EG2ToKey;     //! [0Ch] EG2 -> Key       (+/-10h = +/-1.0 semitones)

	struct SGE_PACKED SGE_WavArt_AHDSR_t { //! [0Eh] Envelope generators
		uint8_t Attack;  //! [00h] Attack time
		uint8_t Hold;    //! [01h] Hold time
		uint8_t Decay;   //! [02h] Decay time
		uint8_t Sustain; //! [03h] Sustain level (00h = 0%, FFh = 100%)
		uint8_t Release; //! [04h] Release time
	} EG1, EG2;
};

/************************************************/

//! Tone region structure [04h bytes]
//! Notes:
//!  -Tone regions may not overlap their key ranges.
struct SGE_ALIGNED SGE_PACKED SGE_ToneRegion_t {
	uint32_t KeyLo:7;  //! [00h,b 0.. 6] Key range start (inclusive)
	uint32_t KeyHi:7;  //! [00h,b 7..13] Key range end (inclusive)
	uint32_t ArtIdx:7; //! [00h,b14..20] Articulation index
	uint32_t Wave:11;  //! [00h,b21..31] Associated waveform
};

//! Tone layer structure [04h + 04h*nReg + 18h*nArt bytes]
//! Notes:
//!  -Velocities are specified as 0..127, corresponding to 1..128.
//!  -Layer velocities are allowed to overlap.
//!  -The articulation structures for this layer are placed immediately after
//!   last region structure, and the next layer is located immediately after
//!   the last articulation of the last layer, and so on.
struct SGE_ALIGNED SGE_PACKED SGE_ToneLayer_t {
	uint8_t VelLo; //! [00h] Velocity range (low,  inclusive)
	uint8_t VelHi; //! [01h] Velocity range (high, inclusive)
	uint8_t nReg;  //! [02h] Number of region structures
	uint8_t nArt;  //! [03h] Number of articulation structures
	struct SGE_ToneRegion_t Regions[0];
};

//! Tone structure [04h + var bytes]
//! Notes:
//!  -Tones are placed sequentially in memory, and may be seeked via either
//!   adding Tone.Size to the current offset, or iterating all layers and their
//!   regions and articulations.
struct SGE_ALIGNED SGE_PACKED SGE_Tone_t {
	uint32_t Size  :24; //! [00h, b0 ..23] Size of tone (in bytes, including this header)
	uint32_t nLayer:8;  //! [00h, b24..31] Number of layers
	struct SGE_ToneLayer_t Layers[0];
};

/************************************************/

//! Song structure [10h + var bytes]
//! Notes:
//!  -If this song is not associated with a database, dbIdx should be 0.
//!  -TrkOffs[] specifies the offset of each track's data relative to the start
//!   offset of this structure. There will be exactly nTrack items in the list.
//!  -The tone structures for this song are located immediately after the last
//!   TrkOffs item.
//!   If nTone == 0, then this song uses the databases's global tone bank.
//!  -Generally, the track data should be placed immediately after all the tone
//!   data. However, it is possible to place data in between the tones and the
//!   track data, provided that the track offsets are still valid.
struct SGE_ALIGNED SGE_PACKED SGE_Song_t {
	uint8_t  nTrack;     //! [00h] Track count
	uint8_t  nTone;      //! [01h] Tone count
	uint16_t dbIdx;      //! [02h] Index in database
	uint32_t r1;         //! [04h] [Reserved]
	union SGE_DbLink_t dbLink; //! [08h] Database link
	uint32_t TrkOffs[0]; //! [10h] Track offsets (in bytes)
};

/************************************************/

//! Memory-mapped database structure [10h bytes]
//! This corresponds to directly reading the database file from memory, with
//! the assumption that the memory is read-only and cannot be relocated.
//! If any song uses a global/shared tone bank, this data comes immediately
//! after this header.
struct SGE_ALIGNED SGE_PACKED SGE_GlobalToneBank_t {
	uint8_t nTones;             //! [00h] Tone count in bank
	uint8_t r1[3];
	struct SGE_Tone_t Tones[0]; //! [04h] Start of tones
};
struct SGE_ALIGNED SGE_PACKED SGE_Db_t {
	uint32_t Magic;       //! [00h] Signature
	uint16_t nWave;       //! [04h] Number of waveforms in database
	uint16_t nSong;       //! [06h] Number of songs in database
	uint32_t WaveTabOffs; //! [08h] Waveform table offset
	uint32_t SongTabOffs; //! [0Ch] Song table offset
};

/************************************************/
#ifndef SGE_PLATFORM_IS_COMPILER
/************************************************/
#ifdef SGE_PLATFORM_HAVE_FILEDB
/************************************************/

//! Database instance structure [32bit: 08h bytes | 64bit: 10h bytes]
//! NOTE: When an item is loaded in Persistent mode, then the lowest
//! bit of the Data pointer is set to 1.
struct SGE_PTRALIGNED SGE_PACKED SGE_FileDbInstance_t {
#ifdef SGE_PLATFORM_IS_64BIT
	uint32_t  Offs;    //! [00h] Data offset in file (in 32-byte chunks)
	uint32_t  Inst;    //! [04h] Instance count
	void     *Data;    //! [08h] Data pointer (when Inst > 0)
#else
	uint32_t  Offs:24; //! [00h] Data offset in database file (in 32-byte chunks)
	uint32_t  Inst:8;  //! [03h] Instance count
	void     *Data;    //! [04h] Data pointer (when Inst > 0)
#endif
};

//! Database function callbacks structure [32bit: 18h bytes | 64bit: 30h bytes]
//! Approximate libc correspondence:
//!   Read  = fseek() + fread()
//!   Alloc = malloc()
//!   Free  = free()
//! Notes;
//!  -AllocFnc() must return a 4-byte aligned pointer to memory (or NULL).
//!  -ReadFnc() must return the number of bytes fetched.
//!  -On ARM9, ReadFnc() must also flush the cache for the associated memory.
struct SGE_PTRALIGNED SGE_PACKED SGE_FileDbFuncCallbacks_t {
	void      *ReadArg;
	uint32_t (*ReadFnc)(void *Dst, uint32_t Offs, uint32_t Size, void *Arg);
	void      *AllocArg;
	void*    (*AllocFnc)(uint32_t Size, void *Arg);
	void      *FreeArg;
	void     (*FreeFnc)(void *Data, void *Arg);
};

//! Database structure [32bit: 10h bytes | 64bit: 20h bytes]
struct SGE_PTRALIGNED SGE_PACKED SGE_FileDb_t {
#ifdef SGE_PLATFORM_IS_64BIT
	uint16_t nWave; //! [00h] Number of waveforms in database
	uint16_t nSong; //! [02h] Number of songs in database
	uint32_t r1;    //! [04h] [Reserved]
	struct SGE_FileDbFuncCallbacks_t *FuncTab; //! [08h] Function callbacks table
	struct SGE_FileDbInstance_t      *WaveTab; //! [10h] Waveform instance table
	struct SGE_FileDbInstance_t      *SongTab; //! [18h] Song instance table
#else
	uint16_t nWave; //! [00h] Number of waveforms in database
	uint16_t nSong; //! [02h] Number of songs in database
	struct SGE_FileDbFuncCallbacks_t *FuncTab; //! [04h] Function callbacks table
	struct SGE_FileDbInstance_t      *WaveTab; //! [08h] Waveform instance table
	struct SGE_FileDbInstance_t      *SongTab; //! [0Ch] Song instance table
#endif
};

/************************************************/
#endif // SGE_PLATFORM_HAVE_FILEDB
/************************************************/
#endif // SGE_PLATFORM_IS_COMPILER
/************************************************/

//! Song-player track structure [32bit: 30h bytes | 64bit: 40h bytes]
//! Notes:
//!  -On GBA and NDS, nybble indices are stored as part of the pointers,
//!   using the uppermost bits of the pointer address. This means that
//!   the values in ::NybbleSrc and ::NybbleStack go unused.
//!   Similarly, ::RepeatType is not used, because the repeat type is
//!   packed in the lowest bit of the stack pointers, which are aligned
//!   to words (4 bytes).
struct SGE_PACKED SGE_Ctrl8_t { //! [04h bytes]
	uint8_t Phase;    //! [00h] Ramp phase
	uint8_t Duration; //! [01h] Ramp duration (ticks)
	uint8_t Value;    //! [02h] Current value
	uint8_t Target;   //! [03h] Ramp target
};
struct SGE_PACKED SGE_Ctrl16_t { //! [06h bytes]
	uint8_t  Phase;    //! [00h] Ramp phase
	uint8_t  Duration; //! [01h] Ramp duration (ticks)
	uint16_t Value;    //! [02h] Current value
	uint16_t Target;   //! [04h] Ramp target
};
union SGE_PACKED SGE_CtrlUnion_t {
	struct { uint8_t Phase, Duration; };
	struct SGE_Ctrl8_t  Ctrl8;
	struct SGE_Ctrl16_t Ctrl16;
};
struct SGE_PTRALIGNED SGE_PACKED SGE_Track_t {
	uint8_t  NybbleSrc:1;          //! [00h, b0..0] Nybble offset for Src pointer
	uint8_t  PortamentoEnable:1;   //! [00h, b1..1] Portamento enable bit
	uint8_t  r1:6;                 //! [00h, b2..7] [Reserved for expansion]
	uint8_t  StackDepth;           //! [01h] Current stack depth
	uint8_t  Program;              //! [02h] Active program
	uint8_t  Priority;             //! [03h] Track priority
	uint8_t  Octave;               //! [04h] Current octave offset (Octave*12)
	 int8_t  Transpose;            //! [05h] Transposition value
	uint16_t Rest;                 //! [06h] Rest time
	uint16_t LastTimeCode;         //! [08h] Last time-coded note duration
	uint8_t  NoteLengthMul;        //! [0Ah] Note value scaling
	 int8_t  NoteLengthAdd;        //! [0Bh] Note value addition/subtraction
	struct SGE_Ctrl8_t  Vel;       //! [0Ch] Velocity controller
	struct SGE_Ctrl8_t  Vol;       //! [10h] Volume controller
	struct SGE_Ctrl8_t  Exp;       //! [14h] Expression controller
	struct SGE_Ctrl8_t  Pan;       //! [18h] Pan controller
	struct SGE_Ctrl16_t Bnd;       //! [1Ch] Pitch bend controller
	uint8_t RepeatType:4;          //! [22h, b0..3] Pattern/repeat type for stack pointers
	uint8_t NybbleStack:4;         //! [22h, b4..7] Nybble offset for stack pointers
	uint8_t PortamentoVoice;       //! [23h] Voice index of last portamento event
	struct SGE_Ctrl8_t Portamento; //! [24h] Portamento controller
	const uint8_t *Src;            //! [28h] Current source pointer
	const uint8_t *Stack  [SGE_MAX_STACK_DEPTH]; //! [32bit: 2Ch | 64bit: 30h] Call stack
	      uint8_t  ReptCnt[SGE_MAX_STACK_DEPTH]; //! [32bit: Var | 64bit: Var] Repeat counters
};

/************************************************/
#ifndef SGE_PLATFORM_IS_COMPILER
/************************************************/

//! Song-player structure [32bit: 20h + 30h*nTracks bytes | 64bit: 38h + 40h*nTracks bytes]
//! Notes:
//!  -ExtCallFunc() is specified as returning uint8_t. This function is used
//!   when a track issues a "GotoIf" command (Fh,Ah) or "Signal" command
//!   (Fh,Bh). For the latter, the return value is ignored, but for "GotoIf",
//!   a return value of 0 indicates "FALSE" and that the condition failed,
//!   whilst any other value indicates "TRUE" and the track will jump to the
//!   "condition passed" location.
//!  -The effective player tempo (ie. BPM times time stretch) must be 1023BPM
//!   or below. Any effective tempo faster than this will be clipped.
//!  -Do NOT directly write a TempoStretch value of 0 to a player, and do NOT
//!   directly change a TempoStretch value from 0. The player modifies its
//!   internal state during calls to SGE_Player_Pause(), and attempting to
//!   call SGE_Player_Resume() when SGE_Player_Pause() has not been called
//!   and TempoStretch == 0 will very likely cause a segfault.
//!   The only exception to this rule is when writing a TempoStretch value of
//!   0 to a player, and later writing a non-zero value, without using the
//!   pause/resume functions directly.
struct SGE_Driver_t;
typedef uint8_t (*SGE_ExtCallFunc_t)(uint8_t PayloadSize, int32_t Payload, struct SGE_Track_t *Track);
struct SGE_PTRALIGNED SGE_PACKED SGE_Player_t {
	uint8_t  nTracks;                //! [00h] Available track slots
	uint8_t  MasterVolume;           //! [01h] Player volume (00h = 0%, 80h = 100%)
	uint16_t TempoStretch;           //! [02h] Tempo scaling (0 = Paused, 0100h = 100%, 0200h = 200%, etc.)
#ifndef SGE_PLATFORM_IS_64BIT
	SGE_ExtCallFunc_t ExtCallFunc;   //! [04h] External call function
#else
	uint32_t r1;                     //! [04h] [Reserved]
#endif
	struct SGE_Ctrl16_t Tempo;       //! [08h] Tempo controller
	int16_t TimePhase;               //! [0Eh] Timing phase
	const struct SGE_Song_t *Song;   //! [32bit: 10h | 64bit: 10h] Linked song
	struct SGE_Driver_t     *Driver; //! [32bit: 14h | 64bit: 18h] Driver attachment
	struct SGE_Player_t     *Prev;   //! [32bit: 18h | 64bit: 20h] Next player in chain
	struct SGE_Player_t     *Next;   //! [32bit: 1Ch | 64bit: 28h] Previous player in chain
#ifdef SGE_PLATFORM_IS_64BIT
	SGE_ExtCallFunc_t ExtCallFunc;   //! [64bit: 30h] External call function
#endif
	struct SGE_Track_t Tracks[0];
};

/************************************************/

//! Voice structure [32bit: 30h bytes | 64bit: 48h bytes]
//! Notes:
//!  -To use a voice without mapping to a player/track, set Vox.Trk=NULL, and
//!   fill the TrkVol/TrkPan/TrkBnd members with desired playback data, then
//!   write SGE_VOX_STAT_NOPLAYER|SGE_VOX_STAT_KEYON|SGE_VOX_STAT_ACTIVE to
//!   Stat.
//!  -On key-on, if SGE_VOX_STAT_NOPLAYER is not used, then the Phase member
//!   is used to store the sample-level synchronization. This allows a sample
//!   to start "partly into" the mixing chunk for better granularity. Note
//!   that envelopes are still aligned to the mix chunk, however.
union SGE_PACKED SGE_Vox_PlayState_t {
	//! With Trk != NULL
	const struct SGE_Player_t *Ply; //! [00h] Linked song player

	//! With Trk == NULL
	//! These are copied from Trk on key-off
	struct SGE_PACKED {
		uint8_t TrkVol; //! [00h] Volume (from Ply.Vol*Trk.Vol*Trk.Exp)
		uint8_t TrkPan; //! [01h] Panning
		int16_t TrkBnd; //! [02h] Pitch bend
	};
};
struct SGE_PTRALIGNED SGE_PACKED SGE_Vox_t {
	uint8_t  Stat;        //! [00h] Channel status
	uint8_t  AdPos;       //! [01h] ADPCM frame position
	uint8_t  Key;         //! [02h] Key index
	uint8_t  Vel;         //! [03h] Key velocity
	uint16_t TicksRem;    //! [04h] Ticks counter
	uint16_t VolL;        //! [06h] Volume at last update (8000h = 100%; left channel)
	uint16_t EG1;         //! [08h] EG1 value
	uint16_t EG2;         //! [0Ah] EG2 value
	uint16_t LFOPhase;    //! [0Ch] LFO phase
	uint16_t LFOFade;     //! [0Eh] LFO fade-in counter
	 int16_t ADPCM[2][2]; //! [10h] ADPCM taps [Left, Right][y[n-2], y[n-1]]
	uint16_t Phase;       //! [18h] Playback phase
	uint16_t VolR;        //! [1Ah] Volume at last update (8000h = 100%; right channel)
#ifdef SGE_PLATFORM_IS_64BIT
	uint8_t  r2[4];
#endif
	const uint8_t             *Data;     //! [32bit: 1Ch | 64bit: 20h] Sample data source
	const struct SGE_Wav_t    *Wav;      //! [32bit: 20h | 64bit: 28h] Linked waveform
	const struct SGE_WavArt_t *Art;      //! [32bit: 24h | 64bit: 30h] Linked waveform articulation
	const struct SGE_Track_t  *Trk;      //! [32bit: 28h | 64bit: 38h] Linked track
	union SGE_Vox_PlayState_t  PlyState; //! [32bit: 2Ch | 64bit: 40h] Player state
};

/**************************************/

//! Driver structure
//! Size:
//!  -32bit: 18h + sizeof(PlatformData) + 30h*VoxCnt + BfCnt*BufLen*sizeof(SGE_OutSmp_t[2]) bytes
//!  -64bit: 28h + sizeof(PlatformData) + 48h*VoxCnt + BfCnt*BufLen*sizeof(SGE_OutSmp_t[2]) bytes
//! Immediately following the voices are the output buffers.
struct SGE_ReverbData_t;
struct SGE_PTRALIGNED SGE_PACKED SGE_Driver_t {
	uint32_t  State;  //! [00h] Driver state flags
#ifdef SGE_PLATFORM_IS_64BIT
	uint32_t r1;      //! [04h] [Reserved]
#else
	SGE_MixSmp_t (*MixBuf)[0][2]; //! [04h] Assigned mixing buffer
#endif
	uint8_t   BfIdxR; //! [08h] Buffer index (currently playing)
	uint8_t   BfCnt;  //! [09h] Buffer count
	uint8_t   VoxCnt; //! [0Ah] Voice count
	uint8_t   BfIdxW; //! [0Bh] Buffer index (next update)
	uint16_t  RateHz; //! [0Ch] Sampling rate (in Hz)
	uint16_t  BufLen; //! [0Eh] Length of each buffer (in samples)
	struct SGE_Player_t     *PlayerList; //! [32bit: 10h | 64bit: 10h] Linked list of music players
#ifdef SGE_PLATFORM_HAVE_REVERB
	struct SGE_ReverbData_t *ReverbData; //! [32bit: 14h | 64bit: 18h] Reverb data structure
#elif (defined(SGE_PLATFORM_HAVE_FAKE_REVERB))
	uint16_t ReverbFb;                   //! [32bit: 14h | 64bit: 18h] Reverb feedback level
	uint16_t ReverbDecay;                //! [32bit: 16h | 64bit: 1Ah] Reverb decay coefficient
# ifdef SGE_PLATFORM_IS_64BIT
	uint32_t ReverbPadding;
# endif
#endif
#ifdef SGE_PLATFORM_IS_64BIT
	SGE_MixSmp_t (*MixBuf)[0][2]; //! [20h] Assigned mixing buffer
#endif
	struct SGE_Driver_Platform_t PlatformData;
	struct SGE_Vox_t Vox[0];
};
SGE_FORCE_INLINE
SGE_OutSmp_t *SGE_GetOutputBuffers(struct SGE_Driver_t *Driver) {
	return (SGE_OutSmp_t*)(Driver->Vox + Driver->VoxCnt);
}

/************************************************/

//! SGE_Driver_GetWorkAreaSize(VoxCnt, BufCnt, BufLen)
//! Description: Get size of SGE_Driver_t structure for given parameters.
//! Arguments:
//!   VoxCnt: Number of voices.
//!   BufCnt: Number of output buffers.
//!   BufLen: Length of each buffer.
//! Returns: Size in bytes of a SGE_Driver_t structure to suit the parameters.
//! Notes:
//!  -Returns 0 if the requested parameters cannot form a valid driver.
SGE_DECLSPEC uint32_t SGE_Driver_GetWorkAreaSize(uint8_t VoxCnt, uint8_t BufCnt, uint16_t BufLen);

//! SGE_Music_GetPlayerSize(TrackCnt)
//! Description: Get size of SGE_Player_t structure for given parameters.
//! Arguments:
//!   TrackCnt: Number of tracks in player.
//! Returns: Size in bytes of a SGE_Player_t to suit the parameters.
//! Notes:
//!  -Returns 0 if TrackCnt is unsupported.
SGE_DECLSPEC uint32_t SGE_Music_GetPlayerSize(uint8_t TrackCnt);

/************************************************/

//! SGE_CriticalSection_Enter(Driver)
//! Description: Enter critical section for specified driver.
//! Arguments:
//!  Driver: Driver work area to lock.
//! Returns: Locking key; driver enters mutex state.
//! Notes:
//!  -This is a weak function and can be overriden.
SGE_DECLSPEC uint32_t SGE_CriticalSection_Enter(struct SGE_Driver_t *Driver);

//! SGE_CriticalSection_Leave(Driver)
//! Description: Leave critical section for specified driver.
//! Arguments:
//!  Driver: Driver work area to unlock.
//!  Key:    Value returned from prior SGE_CriticalSection_Enter() call.
//! Returns: Nothing; driver exits mutex state.
//! Notes:
//!  -This is a weak function and can be overriden.
SGE_DECLSPEC void SGE_CriticalSection_Leave(struct SGE_Driver_t *Driver, uint32_t Key);

/************************************************/

//! SGE_Driver_Init(Driver, VoxCnt, RateHz, BufCnt, BufLen, MixBuf)
//! Description: Initialize SGE driver.
//! Arguments:
//!   Driver: Driver work area.
//!   VoxCnt: Number of voices.
//!   RateHz: Sampling rate (in Hz).
//!   BufCnt: Number of output buffers.
//!   BufLen: Number of samples per buffer.
//!   MixBuf: Assigned mixing buffer.
//! Returns: On success, returns a non-zero value. On failure, returns 0.
//! Notes:
//!  -BufCnt must be between 2 and 255.
//!  -When using reverb, pre-delay is equal to the total buffer size.
//! GBA-specific notes:
//!  -BufLen must be a multiple of 8.
//!  -BufCnt*BufLen must be a multiple of 16.
//!  -This function should only be used if SGE_VARIABLE_SYNC_RATE is used,
//!   or envelope and music timings may be wrong, as it will be assumed
//!   that the driver is operating at the native rate.
//!   See SGE_Driver_InitDefault() for a default setup.
SGE_DECLSPEC uint32_t SGE_Driver_Init(
	struct SGE_Driver_t *Driver,
	uint8_t  VoxCnt,
	uint16_t RateHz,
	uint8_t  BufCnt,
	uint16_t BufLen,
	SGE_MixSmp_t *MixBuf
);

//! SGE_Driver_Close(Driver)
//! Description: Release driver from hardware.
//! Arguments:
//!   Driver: Driver work area.
//! Returns: Nothing; driver state paused.
//! Notes:
//!  -This routine does NOT disable the sound hardware.
//!  -Once this function is called, the driver area may be deleted if it is no
//!   longer needed.
//! PC-specific notes:
//!  -This function MUST be called before exiting the program, and preferably
//!   also when exceptions occur and the program must exit - resource leakage
//!   may occur otherwise.
SGE_DECLSPEC void SGE_Driver_Close(struct SGE_Driver_t *Driver);

//! SGE_Driver_Update(Driver)
//! Description: Update driver state.
//! Arguments:
//!   Driver: Driver work area.
//! Returns: Nothing; driver state updated and audio mixed to output.
//! GBA-specific notes:
//!  -This routine handles updating of song states, as well as voice mixing.
//!   Because of this, this function can take a very long time to complete,
//!   especially if used in conjunction with reverb effects.
//!   For the sake of safety, it's recommended to use the driver with a
//!   VBlank-synchronizing sampling rate, and structure the VBlank interrupt
//!   routine as follows:
//!     nSoundUpdates = 0;
//!     VBlank() {
//!       SGE_Driver_Sync();
//!       nSoundUpdates++;
//!       OtherVBlankCriticalThings();
//!       atomic { CurVBlankIsBusy = VBlankIsBusy, VBlankIsBusy = true; }
//!         if(CurVBlankIsBusy) return;
//!         GraphicsUpdate();
//!         OtherFunctions();
//!         EnableInterrupts();
//!           while(nSoundUpdates > 0) {
//!             SGE_Driver_Update();
//!             atomic { nSoundUpdates--; }
//!           }
//!         DisableInterrupts();
//!       VBlankIsBusy = false;
//!     }
//!   Note that atomicity is guaranteed by IRQ mode and does not need explicit
//!   atomic operations such as SWP/SWPB instructions.
//!   Provided that the sum of CPU usage between all those functions is below
//!   100% per frame, this should help to avoid buffer underrun issues. Note
//!   that some underrun protection can be had by setting BufCnt to something
//!   larger than 2 buffers. However, if CPU usage is consistently over 100%
//!   then this only postpones the issue.
//!  -This routine should generally only be called after SGE_Driver_Sync(). If
//!   called before, this can introduce additional latency into the mixer,
//!   which may result in buffer underrun earlier than expected.
//! GBA/DS-specific notes:
//!  -This function is NOT thread safe, as self-modifying code will be used.
SGE_DECLSPEC void SGE_Driver_Update(struct SGE_Driver_t *Driver);

//! SGE_Driver_UnderrunRecover(Driver)
//! Description: Clear current target buffer to silence and move to next.
//! Arguments:
//!   Driver: Driver work area.
//! Returns: Nothing; next buffer treated as silence.
//! Notes:
//!  -The way this function is intended to work is as follows:
//!   -A thread receives commands to update (based on a sync event), which
//!    increments a "number of updates to process" counter.
//!   -When updates are being processed, then for as long as this "number of
//!    updates to process" counter is greater than or equal to the number of
//!    buffers, this function is called instead of SGE_Driver_Update().
SGE_DECLSPEC void SGE_Driver_UnderrunRecover(struct SGE_Driver_t *Driver);

//! SGE_Driver_Pause(Driver)
//! Description: Pause playback of driver.
//! Arguments:
//!   Driver: Driver work area.
//! Returns: Nothing; driver state paused.
//! GBA-specific notes:
//!  -This function stops DMA transfer and disables the hardware timer.
SGE_DECLSPEC void SGE_Driver_Pause(struct SGE_Driver_t *Driver);

//! SGE_Driver_Resume(Driver)
//! Description: Resume paused driver.
//! Arguments:
//!   Driver: Driver work area.
//! Returns: Nothing; driver is hooked back into the hardware.
//! GBA-specific notes:
//!  -Calling this function will once again take over DMA1+2 and a timer.
SGE_DECLSPEC void SGE_Driver_Resume(struct SGE_Driver_t *Driver);

//! SGE_Driver_Panic(Driver)
//! Description: Kill all voices.
//! Arguments:
//!  Driver: Driver work area.
//! Returns: Nothing; all voices killed.
//! Notes:
//!  -This function is most useful when needing to unload waveforms from memory
//!   and it is unknown if any of those waveforms are currently in use.
SGE_DECLSPEC void SGE_Driver_Panic(struct SGE_Driver_t *Driver);

/************************************************/

//! SGE_Music_InitializePlayer(Player, TrackCnt, ExtCallFunc)
//! Description: Prepare music player area for attachment to driver.
//! Arguments:
//!   Player:      Music player to initialize.
//!   TrackCnt:    Number of tracks to support.
//!   ExtCallFunc: External-call handler function (NULL = None).
//! Returns: Nothing; music player initialized.
SGE_DECLSPEC void SGE_Music_InitializePlayer(struct SGE_Player_t *Player, uint8_t TrackCnt, SGE_ExtCallFunc_t ExtCallFunc);

//! SGE_Music_AttachPlayerToDriver(Driver, Player)
//! Description: Attach music player to driver.
//! Arguments:
//!   Driver: Driver to attach music player to.
//!   Player: Music player to attach.
//! Returns: Nothing; music player attached to driver.
//! Notes:
//!  -Music players are always attached at the end of the list of players.
SGE_DECLSPEC void SGE_Music_AttachPlayerToDriver(struct SGE_Driver_t *Driver, struct SGE_Player_t *Player);

//! SGE_Music_DetachPlayerFromDriver(Player)
//! Description: Detach music player from driver.
//! Arguments:
//!   Player: Music player to detach.
//! Returns: Nothing; music player detached from driver.
//! Notes:
//!  -All voices associated with this player will be killed.
SGE_DECLSPEC void SGE_Music_DetachPlayerFromDriver(struct SGE_Player_t *Player);

//! SGE_Music_Play(Player, Song)
//! Description: Begin playing music.
//! Arguments:
//!   Player: Music player to play song on.
//!   Song:   Song to play.
//! Returns: On success, returns Player. On failure, returns NULL.
//! Notes:
//!  -This function is equivalent to calling SGE_Music_PlayEx() with arguments
//!   {MasterVolume=80h, TempoStretch=0100h}; see SGE_Music_PlayEx().
SGE_DECLSPEC struct SGE_Player_t *SGE_Music_Play(struct SGE_Player_t *Player, const struct SGE_Song_t *Song);

//! SGE_Music_PlayEx(Player, Song, MasterVolume, TempoStretch)
//! Description: Begin playing music (with parameters).
//! Arguments:
//!   Player:       Music player to play song on.
//!   Song:         Song to play.
//!   MasterVolume: Initial music volume (0 = Minimum, 80h = 100%).
//!   TempoStretch: Initial time stretching (0 = Paused, 0100h = 100%, etc.).
//! Returns: On success, returns Player. On failure, returns NULL.
//! Notes:
//!  -This function initially calls SGE_Music_Stop() on this player. If it is
//!   necessary that no voices associated with this player be in use, ensure to
//!   call SGE_Music_Kill() before calling this function.
//!  -If the target player does not have enough track structures to play all
//!   the tracks in a song, it will play only as many as it supports.
//!  -If starting a song with TempoStretch == 0, then SGE_Music_Resume() must
//!   be used to begin playback; do NOT directly write to TempoStretch.
SGE_DECLSPEC struct SGE_Player_t *SGE_Music_PlayEx(struct SGE_Player_t *Player, const struct SGE_Song_t *Song, uint8_t MasterVolume, uint16_t TempoStretch);

//! SGE_Music_Pause(Player)
//! Description: Pause playback of music.
//! Arguments:
//!   Player: Music player to pause.
//! Returns: Nothing; music player paused.
//! Notes:
//!  -Ensure that the player continues existing for at least one more update
//!   (via SGE_Driver_Update()) before destroying it, if this is the intended
//!   purpose; this will allow the voices to capture the state before release.
//!  -This function will re-arrange all data pointers to become relative, in
//!   order to resume playback of a song even after it has gone through an
//!   unload/load cycle (see SGE_Music_ResumeEx()).
SGE_DECLSPEC void SGE_Music_Pause(struct SGE_Player_t *Player);

//! SGE_Music_Resume(Player)
//! Description: Resume playback of music.
//! Arguments:
//!   Player: Music player to resume playback on.
//! Returns: On success, returns a pointer to the song that is now playing. On
//!          failure, returns NULL and playback is not resumed.
//! Notes:
//!  -Only use this function if the music associated with the music player has
//!   NOT undergone unloading and reloading into memory.
//!  -This function is equivalent to calling SGE_Music_ResumeEx() with the song
//!   that is currently held inside the Player structure.
SGE_DECLSPEC const struct SGE_Song_t *SGE_Music_Resume(struct SGE_Player_t *Player);

//! SGE_Music_ResumeEx(Player, Song, TempoStretch)
//! Description: Resume playback of music, with new pointer to song.
//! Arguments:
//!   Player:       Music player to resume playback on.
//!   Song:         New pointer for song data.
//!   TempoStretch: Time stretching on resume(0 = Paused, 0100h = 100%, etc.).
//! Returns: On success, returns a pointer to the song that is now playing. On
//!          failure, returns NULL and playback is not resumed.
//! Notes:
//!  -This function cannot check if the song it is being resumed with is the
//!   song that was playing at the time the player was paused.
SGE_DECLSPEC const struct SGE_Song_t *SGE_Music_ResumeEx(struct SGE_Player_t *Player, const struct SGE_Song_t *Song, uint16_t TempoStretch);

//! SGE_Music_Stop(Player)
//! Description: Stop playback of music.
//! Arguments:
//!   Player: Music player to stop.
//! Returns: Nothing; music player stopped.
SGE_DECLSPEC void SGE_Music_Stop(struct SGE_Player_t *Player);

//! SGE_Music_Kill(Player)
//! Description: Stop playback of music and kill all associated voices.
//! Arguments:
//!   Player: Music player to stop.
//! Returns: Nothing; music player stopped and all associated voices killed.
SGE_DECLSPEC void SGE_Music_Kill(struct SGE_Player_t *Player);

/************************************************/

//! SGE_Db_GetWave(Db, Idx)
//! Description: Get waveform from memory-mapped database.
//! Arguments:
//!   Db:  Pointer to database structure.
//!   Idx: Index of waveform to load.
//! Returns: On success, returns a pointer to the desired waveform. On failure,
//!          returns NULL.
//! Notes:
//!  -This function only fails if the waveform doesn't exist (Idx >= Db.nWave).
SGE_DECLSPEC const struct SGE_Wav_t *SGE_Db_GetWave(const struct SGE_Db_t *Db, uint16_t Idx);

//! SGE_MemDb_GetSong(Db, Idx)
//! Description: Get song from memory-mapped database.
//! Arguments:
//!   Db:  Pointer to database structure.
//!   Idx: Index of song to load.
//! Returns: On success, returns a pointer to the desired song. On failure,
//!          returns NULL.
//! Notes:
//!  -This function only fails if the song doesn't exist (Idx >= Db.nSong).
SGE_DECLSPEC const struct SGE_Song_t *SGE_Db_GetSong(const struct SGE_Db_t *Db, uint16_t Idx);

/************************************************/
#ifdef SGE_PLATFORM_HAVE_FILEDB
/************************************************/

//! SGE_FileDb_Load(Db, Callbacks)
//! Description: Prepare database for access.
//! Arguments:
//!   Db:        Pointer to structure to hold the allocation data.
//!   Callbacks: Pointer to list of callbacks for use in this database.
//! Returns: On success, returns TRUE. On failure, returns FALSE.
//! Notes:
//!  -Db does not need to be cleared prior to calling this function; it will be
//!   filled out entirely by this function call.
//!  -Callbacks must remain in memory for as long as the database exists, and
//!   may only be destroyed after the database has been unloaded.
SGE_DECLSPEC uint8_t SGE_FileDb_Load(struct SGE_FileDb_t *Db, const struct SGE_FileDbFuncCallbacks_t *Callbacks);

//! SGE_Db_Unload(Db)
//! Description: Unload database.
//! Arguments:
//!   Db: Pointer to structure holding the database allocation data.
//! Returns: Nothing; database is unloaded from memory.
//! Notes:
//!  -Unloading a database implies unloading ALL waveforms and songs loaded
//!   from it. Ensure that the driver is not using any of these resources, or
//!   that any resources in use are marked as Persistent.
SGE_DECLSPEC void SGE_FileDb_Unload(struct SGE_FileDb_t *Db);

//! SGE_Db_GetWave(Db, Idx)
//! Description: Get waveform from database.
//! Arguments:
//!   Db:  Pointer to database structure.
//!   Idx: Index of waveform to load.
//! Returns: If the waveform has been previously loaded via SGE_Db_LoadWave(),
//!          returns a pointer to the waveform. Otherwise, returns NULL.
//! Notes:
//!  -This function will also fail if the desired waveform doesn't exist.
SGE_DECLSPEC struct SGE_Wav_t *SGE_FileDb_GetWave(struct SGE_FileDb_t *Db, uint16_t Idx);

//! SGE_Db_LoadWave(Db, Idx, Persistent)
//! Description: Load waveform from database.
//! Arguments:
//!   Db:         Pointer to database structure.
//!   Idx:        Index of waveform to load.
//!   Persistent: Never dissociate waveform from database after loading.
//! Returns: On success, returns a pointer to the desired waveform. On failure,
//!          returns NULL.
//! Notes:
//!  -This function call may fail if memory was not able to be allocated, or if
//!   the instance count of the desired waveform was too large, or the desired
//!   waveform doesn't exist (Idx >= Db.nWave).
//!  -When a waveform is loaded in Persistent mode, it will still exist after
//!   the database that holds it is destroyed. To destroy such a waveform, call
//!   the de-allocation function corresponding to the function that allocated
//!   the waveform (eg. Db.FuncTab.Free()).
//!  -It is possible to load a waveform in Persistent mode even if an existing
//!   instance already exists. In this case, the existing waveform is marked as
//!   Persistent, and its pointer is returned.
//! NDS9-specific notes:
//!  -The data will be automatically cache-cleaned prior to returning.
SGE_DECLSPEC struct SGE_Wav_t *SGE_FileDb_LoadWave(struct SGE_FileDb_t *Db, uint16_t Idx, uint8_t Persistent);

//! SGE_Db_UnloadWave(Wave)
//! Description: Unload waveform from memory.
//! Arguments:
//!   Wave: Waveform pointer received from SGE_Db_LoadWave().
//! Returns: On success, returns TRUE. On failure, returns FALSE.
//! Notes:
//!  -Ensure that no voice is using this waveform before unloading.
//!  -When attempting to unload the last instance of a Persistent waveform, the
//!   function also fails.
SGE_DECLSPEC uint8_t SGE_FileDb_UnloadWave(struct SGE_Wav_t *Wave);

//! SGE_FileDb_UnloadWaveByIndex(WaveIdx, Db)
//! Description: Unload waveform from memory.
//! Arguments:
//!   WaveIdx: Index of waveform to unload.
//!   Db:      Pointer to database structure to unload from.
//! Returns: On success, returns TRUE. On failure, returns FALSE.
//! Notes:
//!  -Ensure that no voice is using this waveform before unloading.
//!  -When attempting to unload the last instance of a Persistent waveform, the
//!   function also fails.
SGE_DECLSPEC uint8_t SGE_FileDb_UnloadWaveByIndex(uint16_t WaveIdx, struct SGE_Wav_t *Wave);

//! SGE_Db_GetSong(Db, Idx)
//! Description: Get song from database.
//! Arguments:
//!   Db:  Pointer to database structure.
//!   Idx: Index of song to load.
//! Returns: If the song has been previously loaded via SGE_Db_LoadSong(),
//!          returns a pointer to the song. Otherwise, returns NULL.
//! Notes:
//!  -This function will also fail if the desired song doesn't exist.
SGE_DECLSPEC struct SGE_Song_t *SGE_FileDb_GetSong(struct SGE_FileDb_t *Db, uint16_t Idx);

//! SGE_Db_LoadSong(Db, Idx)
//! Description: Load song from database.
//! Arguments:
//!   Db:  Pointer to database structure.
//!   Idx: Index of song to load.
//!   Persistent: Never dissociate waveform from database after loading.
//! Returns: On success, returns a pointer to the desired song. On failure,
//!          returns NULL.
//! Notes:
//!  -This function call may fail if memory was not able to be allocated, or if
//!   the instance count of the desired song was too large, or the desired song
//!   doesn't exist (Idx >= Db.nSong).
SGE_DECLSPEC struct SGE_Song_t *SGE_FileDb_LoadSong(struct SGE_FileDb_t *Db, uint16_t Idx, uint8_t Persistent);

//! SGE_Db_UnloadSong(Song)
//! Description: Unload song from memory.
//! Arguments:
//!   Song: Song pointer received from SGE_Db_LoadSong().
//! Returns: On success, returns TRUE. On failure, returns FALSE.
//! Notes:
//!  -Ensure that no music player is using this song, and that no voices are
//!   using any waveform that this song may have unloaded. To be sure of this,
//!   use SGE_Song_Kill() on any player that is using this song.
SGE_DECLSPEC uint8_t SGE_FileDb_UnloadSong(struct SGE_Song_t *Song);

/************************************************/
#endif // SGE_PLATFORM_HAVE_FILEDB
/************************************************/
#ifdef SGE_PLATFORM_HAVE_REVERB
/************************************************/

//! Reverb parameter structure [10h bytes]
/*!
  Explanation of parameters
  -------------------------
  nTapLines:
    Desired number of taps. The higher this value is, the more natural the
    reverb sounds, at the cost of more processing power being required.
    Format: Raw integer.
  RoomDensity:
    Controls the reverb reflection characteristics. The larger the value, the
    more "boxed-in" the sound. Conversely, smaller values give a more
    diffuse-sounding output. Note that if this is set too low, the reverb
    initialization routines may fail as the reflections become too sparse.
    Format: 0.8fxp normalized units (eg. FFh = 99%)
  StereoWidth:
    Alters the reflection times for L/R channels to increase the stereo image.
    Larger values give more exaggerated differences.
    Format: 8.8fxp milliseconds (eg. 1000h = 16.0ms)
  DecayTime:
    Controls the length of the reverb tail. Larger values sound very spacious,
    but can be prone to ringing when combined with a high feedback level.
    This refers to the time it takes for the reverb to decay by 60dB (ie. the
    standardized RT_60 specification).
    Format: 8.8fxp seconds (eg. 2000h = 32.0s)
  Feedback:
    Feedback level for reverb. Higher values result in a more "open" sound, but
    can be prone to ringing when paired with a small number of taps and/or high
    lowpass cutoff frequency.
    Format: 8.8fxp dB attenuation (eg. 600h = -6.0dB)
  LowpassHz:
    -3dB point for the lowpass filters. Lower values give a more "muffled" and
    absorbant reverb, while high values give a more "brilliant" and open sound.
    IMPORTANT: When using "simple" reverb, this lowpass cutoff is accurate.
    However, when using "fancy" reverb, this cutoff needs to be warped, as the
    series lowpass filters will cause a shift in the cutoff frequency.
    Specifically, the cutoff point can be translated via:
      FancyCutoffHz  = RateHz/2 * (SimpleCutoffHz * 2/RateHz)^(1/nTapLines)
      SimpleCutoffHz = RateHz/2 * (FancyCutoffHz  * 2/RateHz)^nTapLines
    Format: Hertz.
  RateHz:
    Sampling rate of processing. This affects the calculations of the reverb
    line sizes and lowpass coefficient, but does not affect the feedback level.
    Format: Hertz.
!*/
struct SGE_ALIGNED SGE_PACKED SGE_ReverbParam_t {
	uint8_t  nTapLines;   //! [00h]
	uint8_t  RoomDensity; //! [01h]
	uint16_t StereoWidth; //! [02h]
	uint16_t DecayTime;   //! [04h]
	uint16_t Feedback;    //! [06h]
	uint32_t LowpassHz;   //! [08h]
	uint32_t RateHz;      //! [0Ch]
};

//! Reverb line structure
//!  Normal: [32bit: 08h bytes | 64bit: 10h bytes]
//!  Fancy:  [32bit: 0Ch bytes | 64bit: 10h bytes]
struct SGE_PTRALIGNED SGE_PACKED SGE_ReverbLine_t {
	SGE_OutSmp_t *Buf; //! [32bit: 00h | 64bit: 00h] Line buffer
	uint16_t Len;      //! [32bit: 04h | 64bit: 08h] Line length (in samples)
	uint16_t Idx;      //! [32bit: 06h | 64bit: 0Ah] Buffer position (counted down from Len-1 by pre-decrement)
#ifdef SGE_PLATFORM_HAVE_FANCY_REVERB
	 int32_t zLp;      //! [32bit: 08h | 64bit: 0Ch] Lowpass z^-1 tap
#else
# ifdef SGE_PLATFORM_IS_64BIT
	uint8_t r1[4];
# endif
#endif
};

//! Reverb work area structure
//!  Normal: [32bit: 0Ch + nTapLines*08h bytes | 64bit: 10h + nTapLines*xxh bytes]
//!  Fancy:  [32bit: 04h + nTapLines*0Ch bytes | 64bit: 08h + nTapLines*xxh bytes]
struct SGE_PTRALIGNED SGE_PACKED SGE_ReverbData_t {
	uint8_t Fb;
	uint8_t Lp;
	uint8_t nTapLines;
	uint8_t r1;
#ifndef SGE_PLATFORM_HAVE_FANCY_REVERB
	int32_t zLp[2];
#endif
#ifdef SGE_PLATFORM_IS_64BIT
	uint8_t r2[4];
#endif
	struct SGE_ReverbLine_t TapLines[0][2];
};

/************************************************/

//! SGE_Reverb_GetReverbDataSize(ReverbParam)
//! Description: Get size of SGE_ReverbData_t structure for given parameters.
//! Arguments:
//!   ReverbParam: Pointer to reverb parameters structure.
//! Returns: Size in bytes of a SGE_ReverbData_t structure to suit ReverbParam.
//! Notes:
//!  -Returns an error code if the requested parameters cannot form a valid
//!   reverb (see the SGE_REVERB_ERROR definitions).
SGE_DECLSPEC int32_t SGE_Reverb_GetReverbDataSize(const struct SGE_ReverbParam_t *ReverbParam);

//! SGE_Reverb_InitReverbData(ReverbParam, ReverbData)
//! Description: Initialize reverb work area using the given parameters.
//! Arguments:
//!   ReverbParam: Pointer to reverb parameters structure.
//!   ReverbData:  Pointer to reverb work area.
//! Returns: On success, returns a positive non-zero value. On failure, returns
//!          an error code (see the SGE_REVERB_ERROR definitions).
SGE_DECLSPEC int32_t SGE_Reverb_InitReverbData(
	const struct SGE_ReverbParam_t *ReverbParam,
	struct SGE_ReverbData_t *ReverbData
);

/************************************************/

//! SGE_Reverb_SetFeedbackLevel(ReverbData, Feedback_dBatten)
//! Description: Set reverb feedback level.
//! Arguments:
//!   ReverbData:       Pointer to reverb work area.
//!   Feedback_dBatten: Feedback level (.8fxp dB attenuation; 0600h = -6dB).
//! Returns: Nothing; feedback level altered.
//! Notes:
//!  -The feedback level is capped at -0.034dB due to hardware limitations.
SGE_DECLSPEC void SGE_Reverb_SetFeedbackLevel(struct SGE_ReverbData_t *ReverbData, uint16_t Feedback_dBatten);

//! SGE_Reverb_SetLowpassCutoff(ReverbData, CutoffHz, RateHz)
//! Description: Set reverb lowpass cutoff frequency.
//! Arguments:
//!   ReverbData: Pointer to reverb work area.
//!   CutoffHz:   -3dB point of lowpass filter (in Hz).
//!   RateHz:     Sampling rate (in Hz).
//! Returns: Nothing; lowpass cutoff altered.
SGE_DECLSPEC void SGE_Reverb_SetLowpassCutoff(struct SGE_ReverbData_t *ReverbData, uint32_t CutoffHz, uint32_t RateHz);

/************************************************/

//! SGE_Reverb_AttachToDriver(Driver, ReverbData)
//! Description: Attach reverb processor to driver.
//! Arguments:
//!   Driver:     Driver work area.
//!   ReverbData: Reverb work area.
//! Returns: Nothing; reverb processor attached to driver.
//! Notes:
//!  -Passing ReverbData == NULL disables reverb processing.
SGE_FORCE_INLINE
SGE_DECLSPEC void SGE_Reverb_AttachToDriver(
	struct SGE_Driver_t     *Driver,
	struct SGE_ReverbData_t *ReverbData
) {
	Driver->ReverbData = ReverbData;
}

/************************************************/
#endif // SGE_PLATFORM_HAVE_REVERB
#if (!defined(SGE_PLATFORM_HAVE_REVERB) && defined(SGE_PLATFORM_HAVE_FAKE_REVERB))
/************************************************/

//! SGE_FakeReverb_SetFeedbackLevel(Driver, Feedback_dBatten)
//! Description: Set reverb feedback level for driver.
//! Arguments:
//!   Driver:           Driver work area.
//!   Feedback_dBatten: Feedback attenuation level (as 8.8fxp decibels).
//! Returns: Nothing; feedback level for reverb effect is set.
void SGE_FakeReverb_SetFeedbackLevel(struct SGE_Driver_t *Driver, uint16_t Feedback_dBatten);

//! SGE_FakeReverb_SetDecayTime(Driver, DecayTimeSecs)
//! Description: Set decay time for reverb effect.
//! Arguments:
//!   Driver:        Driver work area.
//!   DecayTimeSecs: Decay time (as .8fxp seconds from 0dB gain until -60dB gain).
//! Returns: Nothing; decay time for reverb effect is set.
void SGE_FakeReverb_SetDecayTime(struct SGE_Driver_t *Driver, uint16_t DecayTimeSecs);

/************************************************/
#endif // SGE_PLATFORM_HAVE_FAKE_REVERB
/************************************************/
#ifdef SGE_INTERNALS
/************************************************/

//! SGE_Music_StopPlayerVoices(Player)
//! Description: Stop all voices associated with a player.
//! Arguments:
//!   Player: Music player for which to stop associated voices.
//! Returns: Nothing; all voices associated with music player are stopped by
//!          setting key-off.
void SGE_Music_StopPlayerVoices(struct SGE_Player_t *Player);

//! SGE_Music_KillPlayerVoices(Driver, Player)
//! Description: Kill all voices associated with a player.
//! Arguments:
//!   Player: Music player for which to kill associated voices.
//! Returns: Nothing; all voices associated with music player are killed.
void SGE_Music_KillPlayerVoices(struct SGE_Player_t *Player);

#ifdef SGE_PLATFORM_HAVE_REVERB

//! SGE_Reverb_GetLineLengths(ReverbParam, Dst)
//! Description: Get the lengths of the reverb lines.
//! Arguments:
//!   ReverbParam: Pointer to reverb parameters structure.
//!   Dst:         Pointer to storage for the line lengths.
//! Returns: Sum of all line lengths or error code (see SGE_REVERB_ERROR).
int32_t SGE_Reverb_GetLineLengths(const struct SGE_ReverbParam_t *ReverbParam, const uint16_t (*Dst)[][2]);

#endif

/************************************************/
#endif // SGE_INTERNALS
/************************************************/
#endif // SGE_PLATFORM_IS_COMPILER
/************************************************/
#ifdef __cplusplus
}
#endif
/************************************************/
#endif
/************************************************/
//! EOF
/************************************************/
