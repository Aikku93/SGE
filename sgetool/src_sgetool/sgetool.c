/************************************************/
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/************************************************/
#include "GlobalHelpers.h"
/************************************************/
#include "DLS.h"
#include "MML.h"
#include "MiniRIFF.h"
#include "SGE-Compiler.h"
/************************************************/

#define DEFAULT_OUTPUT_FILENAME "SoundBank.sge"
#define FILENAME_LENGTH_BUFFER_SIZE 256 //! <- This is used for songs list files

/************************************************/

struct ProgramOptions_t {
	const char *OutputFileName;
};

/************************************************/

static const char *UsageString =
	"sgetool - CLI tool for generating SGE sound banks.\n"
	"Usage:\n"
	" sgetool [Options] Bank.dls/Bank.sf2 [MML.txt/MIDI.mid/List=List.txt ...]";
static const char *HelpHintString =
	"Use `sgetool --help` for detailed help information.";
static const char *HelpString =
	"Supported input waveform formats:\n"
	" PCM8, PCM16, PCM24, PCM32, FLOAT32\n"
	" Bit-depths higher than 16-bit will be internally converted to 16-bit.\n"
	"Song lists must contain one song per line. Comments are started with a\n"
	"`;` symbol, and run until the end of that line.\n"
	"Options:\n"
	" Global Options\n"
	" --------------\n"
	" -o:SoundBank.sge   - Set output file.\n"
	" Waveform Options\n"
	" ----------------\n"
	" -wavgain:0.0dB     - Set the global waveform gain. This will modify the sample\n"
	"                      data directly, rather than adjust the waveform volume.\n"
	"                      Can be any decibel value between -100.0dB and +100.0dB,\n"
	"                      or any linear scale between 0.00001 and 100000.0.\n"
	" -wavgainadj:0.0dB  - Similar to the `-wavgain` option, but this will adjust\n"
	"                      the waveform volume parameter rather than modify the data\n"
	"                      directly. The same parameters apply, but note that the\n"
	"                      final waveform gain will clamp between -42.1dB .. +6.0dB.\n"
	"                      This option and `-wavgain` can be combined.\n"
	" -wavformat:default - Set the sample format for all waveforms. Can be any of:\n"
	"                      * default\n"
	"                        Do not alter the format of any waveform.\n"
	"                      * PCM8\n"
	"                        Unsigned 8-bit PCM.\n"
	"                      * PCM16\n"
	"                        Signed 16-bit PCM.\n"
	"                      * ADPCM\n"
	"                        4-bit custom ADPCM format.\n"
	"                      The Gameboy Advance version of this sound driver can only\n"
	"                      play back PCM8 and ADPCM waveforms. All other platforms\n"
	"                      can use any of the listed formats.\n"
	" -wavinterpolate:y  - Set default state for waveform interpolation. This can be\n"
	"                      overridden on a per-sample basis as required.\n"
	"                      Can be any of:\n"
	"                      * n\n"
	"                        Do not use interpolation.\n"
	"                      * y\n"
	"                        Always use interpolation.\n"
	"                      * lt\n"
	"                        Interpolate only when playback rate is < 1.0.\n"
	"                      * gt\n"
	"                        Interpolate only when playback rate is >= 1.0.\n"
	"                      Note that a driver may be compiled to ignore these flags,\n"
	"                      such as by never interpolating, or always interpolating,\n"
	"                      and so this flag should really only be considered a hint.\n"
	" -minloopsize:0     - Set the minimum size in samples for loops. This is mostly\n"
	"                      useful for platforms such as Gameboy Advance and Nintendo\n"
	"                      DS (where short loops can potentially cause performance\n"
	"                      penalties). This option will repeat a loop until it is at\n"
	"                      least this number of samples in length.\n"
	"                      Can be specified as samples, or milliseconds (eg. 30ms).\n"
	" -forcemono:never   - Force all waveforms to become monophonic. This uses a DFT\n"
	"                      based algorithm rather than directly averaging the data.\n"
	"                      Can be any of:\n"
	"                      * never\n"
	"                        Never force conversion. Note that if a waveform uses\n"
	"                        more than two channels, these will be converted to mono\n"
	"                        regardless of this option.\n"
	"                      * always\n"
	"                        Always convert to monophonic.\n"
	" -monoconv-blk:256  - Set DFT window size of mono conversion algorithm. Can be\n"
	"                      anywhere from 8 samples (4 frequency lines) through 65536\n"
	"                      samples (32768 frequency lines); must be a power of 2.\n"
	" -monoconv-wnd:hann - Set DFT window type for mono conversion. Can be any of:\n"
	"                      * sine     (Sine window;     minimum 2 hops)\n"
	"                      * hann     (Hann window;     minimum 4 hops)\n"
	"                      * hamming  (Hamming window;  minimum 4 hops)\n"
	"                      * blackman (Blackman window; minimum 8 hops)\n"
	"                      * nuttall  (Nuttall window;  minimum 8 hops)\n"
	" -monoconv-hops:8   - Set number of STFT hops for mono conversion analysis. The\n"
	"                      maximum number of hops is limited by the block size, but\n"
	"                      the minimum number of hops depends on the window type.\n"
	" -resample:none     - Resample all waveforms to target sample rate. Can be any\n"
	"                      rate between 5000Hz through to 384kHz, or `none` to avoid\n"
	"                      any resampling.\n"
	" -resampleif:always - Condition for which to use resampling. Can be any of:\n"
	"                      * never\n"
	"                        Generate an error if a waveform needs resampling.\n"
	"                      * always\n"
	"                        Always resample.\n"
	"                      * gt\n"
	"                        Only resample if target rate is higher.\n"
	"                      * lt\n"
	"                        Only resample if target rate is lower.\n"
	" -lowpass:none      - Set low-pass filter cut-off frequency. Note that this is\n"
	"                      applied together with the resampling filter, and is not a\n"
	"                      fast pre-pass step when it becomes needed.\n"
	" -highshelf:0.0dB   - Set high-shelf filter gain. This can be useful when using\n"
	"                      low sampling rates so as to improve perceived brightness,\n"
	"                      or to add a slight muffling to the signal.\n"
	"                      Note that this is applied to the output samples only.\n"
	"                      Can be any decibel value between -100.0dB and +100.0dB,\n"
	"                      or any linear scale between 0.00001 and 100000.0.\n"
	"                      Transfer function:\n"
	"                      * H(z) = (1+Gain)*0.5 + (1-Gain)*0.5*z^-1\n"
	" -oversample:1.0    - Set oversampling level (eg. 2.0 will double the length).\n"
	"                      Can be any value greater than 0, but smaller than 100.0.\n"
	"                      When combined with the `-lowpass` option, then the lower\n"
	"                      of the two (ie. the base sampling rate and the low-pass\n"
	"                      cut-off frequency) will be used as the cut-off.\n"
	"                      Note that the `-highshelf` option does not behave as may\n"
	"                      be expected when using this option, as the former applies\n"
	"                      the effect to the output of the resampled signal, which\n"
	"                      may contain silence in the higher frequencies when using\n"
	"                      this option.\n"
	" -wavcull:y         - Cull unused waveforms. When compiling music, all tones in\n"
	"                      use are scanned, and the waveforms they referenced become\n"
	"                      marked as 'necessary'. When culling is enabled, waveforms\n"
	"                      that aren't referenced by any tone used by any song will\n"
	"                      NOT be exported. Disabling this feature will export all\n"
	"                      waveforms, regardless of whether or not they are used.\n"
	"                      Note that when no music is provided, all waveforms are\n"
	"                      exported regardless.\n"
	"                      Can be set to `y` to enable culling, or `n` to disable.\n"
	" Sample-Rate Conversion (SRC) Options\n"
	" ------------------------------------\n"
	" -src-align:loops   - Sample alignment for resampling. Can be any of:\n"
	"                      * any\n"
	"                        Disregard sample boundaries. This can cause loops to\n"
	"                        audibly 'pop' if the loop size becomes non-integer, but\n"
	"                        will resample to exactly the specified sample rate.\n"
	"                      * loops\n"
	"                        Disregard sample boundaries for one-shot waveforms, but\n"
	"                        resample looped waveforms so that their loop points are\n"
	"                        integer-aligned. This will prevent discontinuities, but\n"
	"                        may result in slightly adjusted sampling rates.\n"
	"                        The loop points are adjusted to the nearest integer; to\n"
	"                        adjust this behaviour, see the `-src-round` option.\n"
	"                      * loops-nonperc\n"
	"                        Behaves like the `loops` option, but only adjusts the\n"
	"                        sampling rate if the waveform is non-percussive (ie. is\n"
	"                        not assigned to a drum kit). This can be useful when a\n"
	"                        target sampling rate has better behaviour, but should\n"
	"                        only be used un-adjusted by percussive waveforms due to\n"
	"                        the adjusted rates possibly causing off-pitch issues.\n"
	" -src-round:middle  - Rounding behaviour for resampled loop size (applies to\n"
	"                      the `-src-align:loops` option). Can be any of:\n"
	"                      * down\n"
	"                        Round down the loop size. (eg. 123.99 -> 123.00)\n"
	"                      * middle\n"
	"                        Round off the loop size. (eg. 123.50 -> 124.00)\n"
	"                      * up\n"
	"                        Round up the loop size (eg. 123.01 -> 124.00)\n"
	" -src-order:33      - Set filter order of the resampling filter. Higher values\n"
	"                      result in closer approximations to an ideal brick-wall\n"
	"                      lowpass filter, but can cause ringing artifacts, as well\n"
	"                      as smearing of transients, and longer looped samples (the\n"
	"                      loop needs to be shifted by N/2 to account for ringing).\n"
	"                      Can be any odd value between 3 and 511.\n"
	" -src-window:hann   - Set the resampling filter window. The resampler always\n"
	"                      uses the sinc function as the resampling kernel, but an\n"
	"                      extra window can be added over the source data at each\n"
	"                      sampling interval, which may produce more favourable\n"
	"                      results regarding sinc truncation. Can be any of:\n"
	"                      * none     (Sinc; unwindowed)\n"
	"                      * sine     (Sinc x Sine)\n"
	"                      * hann     (Sinc x Hann)\n"
	"                      * hamming  (Sinc x Hamming)\n"
	"                      * blackman (Sinc x Blackman)\n"
	"                      * nuttall  (Sinc x Nuttall)\n"
	"                      * lanczos  (Sinc x Lanczos)\n"
	"                      * lanczos2 (Sinc x Lanczos^2)\n"
	" -src-dither:1.0    - Set quantization dithering level. This is used when any\n"
	"                      resampling needs to take place, or the output bit-depth\n"
	"                      is lower than the input bit-depth. A level of 0.0 will\n"
	"                      disable dithering, and 1.0 will enable full dithering.\n"
	"                      This is equivalent to the amplitude of the noise to inject\n"
	"                      below the smallest representable level of a given format.\n"
	"                      For example, a dither level of 1.0 for PCM8 adds noise at\n"
	"                      an absolute amplitude of 1/256.\n"
;

/************************************************/

static void OptionsErrorLogger(void *Userdata, const char *Format, ...) {
	(void)Userdata;
	va_list vl; va_start(vl, Format);
	vprintf(Format, vl);
	va_end(vl);
}

static int Option_SetOutputFile(
	const char *OptArg,
	void *Userdata,
	struct SGE_gOptions_t *Options,
	SGE_ParseOptions_ErrorLogger_t ErrorLogger,
	void *ErrorLogger_Userdata
) {
	(void)Options;
	(void)ErrorLogger;
	(void)ErrorLogger_Userdata;
	struct ProgramOptions_t *ProgramOptions = (struct ProgramOptions_t*)Userdata;
	ProgramOptions->OutputFileName = OptArg;
	return 1;
}

static struct SGE_ParseOptions_ExtraOpt_t ProgramOptionsExtra[] = {
	{"-o:", Option_SetOutputFile},
	{NULL},
};

/************************************************/

//! Parse song file (MIDI or MML)
static int ParseSongFile(FILE *File, const char *Filename, struct SGE_LocalDb_t *LocalDb) {
	//! Check for MIDI
	uint32_t MThd = 0, Size = 0; {
		fread(&MThd, sizeof(MThd), 1, File);
		fread(&Size, sizeof(Size), 1, File);
		rewind(File);
	}
	if(MThd == RIFF_FOURCC("MThd") && Size == 6) {
		//! MIDI file
		printf("ERROR: Unimplemented (MIDI files).\n");
		return 0;
	} else {
		//! Assume MML file
		struct MML_t MML;
		printf("Parsing MML song (%s)... ", Filename);
		int ErrorCode = SGE_LocalDb_LoadMML(LocalDb, File, &MML);
		if(ErrorCode != SGE_LOCALDB_ERROR_NONE) {
			//! MML error?
			if(ErrorCode == SGE_LOCALDB_ERROR_MML) {
				puts("Error parsing MML.");
				MML_DisplayLastError(&MML);
			} else {
				puts(SGE_LocalDb_Export_ErrorCodeToString(ErrorCode));
			}
			MML_Destroy(&MML);
			return 0;
		}

		//! Print warnings
		if(MML.nWarnings != 0) {
			printf("Compiled with %d warnings.\n", MML.nWarnings);
			MML_DisplayWarnings(&MML);
		} else puts("Ok.");

		//! Print out track lengths
		uint32_t TrackIdx;
		uint32_t TotalSize = 0;
		for(TrackIdx=0;TrackIdx<MML.nTracks;TrackIdx++) {
			const struct MML_TrackListing_t *Track = &MML.TracksList[TrackIdx];
			printf(" Track %2u: %4u bytes", TrackIdx+1, Track->Size);
			if(Track->Name) printf(" (%s)\n", Track->Name);
			else putchar('\n');
			TotalSize += Track->Size;
		}
		printf(" Total: %u bytes\n", TotalSize);
		MML_Destroy(&MML);
	}
	return 1;
}

/************************************************/

int main(int argc, const char *argv[]) {
	int ReturnValue = -1;
	int NextArgIdx;
	FILE *OutputFile    = NULL;
	FILE *SoundBankFile = NULL;
	struct SGE_gOptions_t Options;
	struct SGE_LocalDb_t LocalDb;
	struct ProgramOptions_t ProgramOptions = {
		.OutputFileName = DEFAULT_OUTPUT_FILENAME,
	};

	//! Initialize options
	Options.WavFormat             = SGE_OPT_WAVFORMAT_DEFAULT;
	Options.WavInterpolate        = 1;
	Options.WavResampleCondition  = SGE_OPT_WAVRESAMPCOND_ALWAYS;
	Options.WavEnableCull         = 1;
	Options.WavForceMono          = 0;
	Options.SRCAlign              = SGE_OPT_SRCALIGN_LOOPS;
	Options.SRCRound              = SGE_OPT_SRCROUND_MIDDLE;
	Options.SRCHalfOrder          = (33-1) / 2;
	Options.SRCWindow             = SGE_OPT_SRCWINDOW_HANN;
	Options.WavMonoConvWindowSize = 256;
	Options.WavMonoConvWindowType = SGE_OPT_MONOCONV_WINDOW_HANN;
	Options.WavMonoConvHops       = 8;
	Options.WavResampleRate       = 0;
	Options.WavMinLoopSize        = 0;
	Options.WavOversampleRate     = 1.0;
	Options.WavLowpassCutoff      = 0.0;
	Options.WavHighShelfGain      = 1.0;
	Options.WavGlobalGain         = 1.0;
	Options.WavGainAdjust         = 1.0;
	Options.SRCDitherLevel        = 1.0;

	//! Initialize local database
	SGE_LocalDb_Init(&LocalDb);

	//! Not enough arguments?
	if(argc < 2 || !strcmp(argv[1], "--help")) {
		puts(UsageString);
		if(argc >= 2 && !strcmp(argv[1], "--help")) puts(HelpString);
		else puts(HelpHintString);
		return 1;
	}

	//! Parse options
	{
		int nOptsParsed = SGE_ParseOptions(&Options, argv+1, argc-1, 0, OptionsErrorLogger, NULL, ProgramOptionsExtra, &ProgramOptions);
		if(nOptsParsed < 0) goto Error_ParseOptions;
		NextArgIdx = 1 + nOptsParsed;

		//! Make sure we got a sound bank
		if(NextArgIdx == argc) {
			puts("ERROR: No sound bank file specified.");
			goto Error_ParseOptions;
		}
	}

	//! Open the sound bank file
	SoundBankFile = fopen(argv[NextArgIdx], "rb");
	if(!SoundBankFile) {
		printf("ERROR: Unable to open sound bank file (%s).\n", argv[NextArgIdx]);
		goto Error_OpenSoundBankFile;
	} else {
		//! Detect the type of sound bank from the RIFF header and convert to local format
		uint32_t Type = 0;
		fseek(SoundBankFile, 8, SEEK_SET);
		fread(&Type, sizeof(Type), 1, SoundBankFile);
		rewind(SoundBankFile);

		//! We can't use a switch-case here because of weirdness involving constant expressions...
		if(Type == RIFF_FOURCC("DLS ")) {
			printf("Parsing DLS file (%s)... ", argv[NextArgIdx]);
			int ErrorCode = SGE_LocalDb_SoundBankFromDLS(&LocalDb, SoundBankFile, &Options);
			if(ErrorCode != DLS_ERROR_NONE) {
				puts(DLS_ErrorCodeToString(ErrorCode));
				goto Error_ReadSoundBankFile;
			}
			puts("Ok.");
		} else if(Type == RIFF_FOURCC("sfbk")) {
			printf("ERROR: Unimplemented (SoundFont files).\n");
			goto Error_ReadSoundBankFile;
		} else {
			printf("ERROR: Unknown sound bank format.\n");
			goto Error_ReadSoundBankFile;
		}
	}
	NextArgIdx++;

	//! Finally, iterate over all songs
	while(NextArgIdx < argc) {
		const char *Filename = argv[NextArgIdx];

		//! First, check song count
		if(LocalDb.nSongs >= 65535) {
			printf("ERROR: Too many songs added (%s).\n", Filename);
			goto Error_OpenSongFile;
		}

		//! Check for a list
		int IsListFile = 0; {
			//! Convert prefix to lowercase
			int n;
			char PrefixBuffer[8];
			PrefixBuffer[7] = '\0';
			strncpy(PrefixBuffer, Filename, 7);
			for(n=0;n<8;n++) {
				char c = PrefixBuffer[n];
				if(c >= 'A' && c <= 'Z') PrefixBuffer[n] = c - 'A' + 'a';
			}

			//! Match prefix
			if(!memcmp(PrefixBuffer, "list=", strlen("list="))) {
				IsListFile = 1;
				Filename += strlen("list=");
			}
		}

		//! Open file
		FILE *SongFile = fopen(Filename, "rb");
		if(!SongFile) {
			printf("ERROR: Unable to open %s file (%s).\n", IsListFile ? "list" : "song", Filename);
			goto Error_OpenSongFile;
		}

		//! Check for a list file
		if(IsListFile) {
			//! Songs list
			printf("--- Parsing songs from list file (%s) ---\n", Filename);
			while(!feof(SongFile)) {
				//! Read filename from line
				static char FilenameBuffer[FILENAME_LENGTH_BUFFER_SIZE];
				int c, nChars = 0, CommentMode = 0;
				while((c = fgetc(SongFile)) != EOF) {
					//! End of line?
					if(c == '\n') break;

					//! Start comment?
					if(c == ';') CommentMode = 1;

					//! If still parsing, continue
					if(!CommentMode) {
						if(nChars >= FILENAME_LENGTH_BUFFER_SIZE-1) {
							puts("ERROR: Filename in list file is too long.\n");
							fclose(SongFile);
							goto Error_ReadSongFile;
						} else FilenameBuffer[nChars++] = (char)c;
					}
				}

				//! Trim whitespace from start and end of line
				char *FilenameBeg = FilenameBuffer;
				char *FilenameEnd = FilenameBuffer + nChars;
				while(FilenameBeg < FilenameEnd && FilenameBeg[ 0] <= ' ') FilenameBeg++;
				while(FilenameBeg < FilenameEnd && FilenameEnd[-1] <= ' ') FilenameEnd--;
				if(FilenameBeg >= FilenameEnd) continue;
				*FilenameEnd = '\0';

				//! Process file
				FILE *File = fopen(FilenameBeg, "rb");
				if(!File) {
					printf("ERROR: Unable to open song file (%s) from list file.\n", FilenameBeg);
					fclose(SongFile);
					goto Error_ReadSongFile;
				}
				int Result = ParseSongFile(File, FilenameBeg, &LocalDb);
				fclose(File);
				if(!Result) {
					fclose(SongFile);
					goto Error_ReadSongFile;
				}
			}
			puts("--- End of songs list file ---");
		} else if(!ParseSongFile(SongFile, Filename, &LocalDb)) {
			fclose(SongFile);
			goto Error_ReadSongFile;
		}

		//! Next song
		fclose(SongFile);
		NextArgIdx++;
	}

	//! Open the output file
	OutputFile = fopen(ProgramOptions.OutputFileName, "wb");
	if(!OutputFile) {
		printf("ERROR: Unable to open output file (%s)\n", ProgramOptions.OutputFileName);
		goto Error_OpenOutputFile;
	}

	//! Export sound bank
	int ErrorCode = SGE_LocalDb_Export(&LocalDb, OutputFile, SoundBankFile, &Options);
	if(ErrorCode != SGE_LOCALDB_ERROR_NONE) {
		puts(SGE_LocalDb_Export_ErrorCodeToString(ErrorCode));
		goto Error_ExportDatabase;
	}
	puts("Ok.");

	//! All done
	ReturnValue = 0;

	//! Exit points
Error_ExportDatabase:
	fclose(OutputFile);
Error_OpenOutputFile:
Error_ReadSongFile:
Error_OpenSongFile:
	SGE_LocalDb_Destroy(&LocalDb);
Error_ReadSoundBankFile:
	fclose(SoundBankFile);
Error_OpenSoundBankFile:
Error_ParseOptions:
	return ReturnValue;
}

/************************************************/
//! EOF
/************************************************/
