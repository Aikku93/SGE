/************************************************/
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
/************************************************/
#include "MML.h"
#include "MMLParser.h"
#include "MMLParser_Commands.h"
/************************************************/

//! This forces the fixups to stop and error out instead of looping forever.
//! I'm reasonably certain that this is impossible, but...
#define MAX_REFERENCE_FIXUP_ITERATIONS 100

/************************************************/

static int MML_ParseCommand(struct MML_t *MML) {
	//! Read command and check for EOF
	struct MML_InputOffs_t CommandOffs; MML_GetInputOffset(MML, &CommandOffs);
	int Command = MML_PeekNextChar(MML);
	if(Command == MML_EOF) return MML_EOF;

	//! Have global command?
	if(MML->nTracks == 0 && Command == '$') {
		MML_ConsumeChars(MML, 1, 0);
		if(MML_StringMatchAndConsume(MML, "tpq",          1)) return MML_Command_Global_TicksPerBeat(MML);
		if(MML_StringMatchAndConsume(MML, "ticksperbeat", 1)) return MML_Command_Global_TicksPerBeat(MML);
		MML_AppendErrorCurrentOffset(MML, "Expected global command following '$' symbol.");
		return MML_ERROR;
	}

	//! Ensure we have a track to write to
	if(MML->nTracks == 0 && Command != '#') {
		MML_AppendErrorCurrentOffset(MML, "Expected '#' to begin new track.");
		return MML_ERROR;
	}

	//! Begin new track?
	if(Command == '#') {
		//! Store the last track and create a new one
		if(MML_FinalizeTrack (MML) == MML_ERROR) return MML_ERROR;
		if(MML_CreateNewTrack(MML) == MML_ERROR) return MML_ERROR;

		//! Does the track have a name?
		MML_ConsumeChars(MML, 1, 0);
		int NextChar = MML_PeekNextChar(MML);
		if(MML_IS_ALPHANUM_OR_UNDERSCORE(NextChar)) {
			if(MML_ReadLabelString(MML, &MML->TracksList[MML->nTracks-1].Name) == MML_ERROR) return MML_ERROR;
		}
		return MML_OK;
	}

	//! Skip over command letter
	MML_ConsumeChars(MML, 1, 0);

	//! Basic commands
	switch(Command) {
		//! Driver commands
		case 'a': /* Fall through */
		case 'b': /* Fall through */
		case 'c': /* Fall through */
		case 'd': /* Fall through */
		case 'e': /* Fall through */
		case 'f': /* Fall through */
		case 'g': return MML_Command_Note        (MML, Command);
		case 'r': return MML_Command_Rest        (MML);
		case '>': return MML_Command_OctaveUp    (MML);
		case '<': return MML_Command_OctaveDown  (MML);
		case 'o': return MML_Command_OctaveSet   (MML);
		case 'v': return MML_Command_Velocity    (MML);
		case 'V': return MML_Command_Volume      (MML);
		case 'E': return MML_Command_Expression  (MML);
		case 'P': return MML_Command_Panning     (MML);
		case 'T': return MML_Command_PitchBend   (MML);
		case 't': return MML_Command_Tempo       (MML);
		case 'q': return MML_Command_NoteMul     (MML);
		case '@': return MML_Command_Program     (MML);

		//! Pattern/repeat/label commands
		case '(': return MML_Command_Pattern                 (MML);
		case '[': return MML_Command_AnonymousPatternOrRepeat(MML);
		case ']': return MML_Command_EndPatternOrRepeat      (MML);
		case '{': return MML_Command_NoteStackOrTripletMode  (MML);
		case '}': return MML_Command_EndTripletMode          (MML);
		case '*': return MML_Command_PatternRecall           (MML);
		case '|': return MML_Command_Break                   (MML);
		case ':': return MML_Command_Label                   (MML);

		//! Compiler commands
		case 'l': return MML_Command_NoteDuration(MML);
	}

	//! Special commands
	if(Command == '$') {
		if(MML_StringMatchAndConsume(MML, "priority",   1)) return MML_Command_Priority  (MML);
		if(MML_StringMatchAndConsume(MML, "transpose",  1)) return MML_Command_Transpose (MML);
		if(MML_StringMatchAndConsume(MML, "t",          1)) return MML_Command_Transpose (MML);
		if(MML_StringMatchAndConsume(MML, "portamento", 1)) return MML_Command_Portamento(MML);
		if(MML_StringMatchAndConsume(MML, "p",          1)) return MML_Command_Portamento(MML);
		if(MML_StringMatchAndConsume(MML, "gotoif",     1)) return MML_Command_GotoIf    (MML);
		if(MML_StringMatchAndConsume(MML, "signal",     1)) return MML_Command_Signal    (MML);
		if(MML_StringMatchAndConsume(MML, "goto",       1)) return MML_Command_Goto      (MML);
		if(MML_StringMatchAndConsume(MML, "end",        1)) return MML_Command_End       (MML);
		MML_AppendErrorCurrentOffset(MML, "Expected command following '$' symbol.");
		return MML_ERROR;
	}

	//! Unknown command
	MML_AppendError(MML, "Unexpected character.", &CommandOffs);
	return MML_ERROR;
}

/************************************************/

static uint32_t MML_RelOffsSizeInNybbles(ptrdiff_t RelOffs) {
	//! Relative offsets have a bias of 1 nybble in magnitude
	if(RelOffs > 0) {
		     if(RelOffs <= +(1+0x7D    )) return 2; //! 01h..FBh        ->     01h..      FBh -> +  00h..+7Dh
		else if(RelOffs <= +(1+0xFD    )) return 4; //! FDh,XXh         ->     FDh..    01FBh -> +  7Eh..+FDh
		else if(RelOffs <= +(1+0x80FD  )) return 6; //! FEh,XXh,XXh     ->   01FDh..  0101FBh -> +  FEh..+80FDh
		else if(RelOffs <= +(1+0x8080FD)) return 8; //! FFh,XXh,XXh,XXh -> 0101FDh..010101FBh -> +80FEh..+8080FDh
	} else {
		     if(RelOffs >= -(1+0x7E    )) return 2; //! 00h..FCh        ->     00h..      FCh -> -  00h..-7Eh
		else if(RelOffs >= -(1+0xFE    )) return 4; //! FDh,XXh         ->     FEh..    01FCh -> -  7Fh..-FEh
		else if(RelOffs >= -(1+0x80FE  )) return 6; //! FEh,XXh,XXh     ->   01FEh..  0101FCh -> -  FFh..-80FEh
		else if(RelOffs >= -(1+0x8080FE)) return 8; //! FFh,XXh,XXh,XXh -> 0101FEh..010101FCh -> -80FFh..-8080FEh
	}
	return 0;
}
static int MML_ResolveReferences(struct MML_t *MML, uint32_t *RefIdxByDstOffs_Head) {
	uint32_t i, j, nReferences = MML->nReferences;
	for(i=0;i<nReferences;i++) {
		struct MML_Reference_t *Ref = &MML->ReferencesList[i];

		//! Find label being referenced
		struct MML_Label_t *Target;
		if(Ref->ReferenceType == MML_REFERENCE_TYPE_NAMED) {
			for(j=0;j<MML->nLabels;j++) {
				if(MML->LabelsList[j].Name && !strcmp(Ref->Name, MML->LabelsList[j].Name)) {
					Target = &MML->LabelsList[j];
					break;
				}
			}
			if(j >= MML->nLabels) {
				MML_AppendError(MML, "Unresolved reference to label.", &Ref->InputOffs);
				return MML_ERROR;
			}

			//! Don't need the name anymore, so destroy it now
			free(Ref->Name);
			Ref->Idx           = j;
			Ref->ReferenceType = MML_REFERENCE_TYPE_INDEXED;
		} else switch(Ref->ReferenceType) {
			case MML_REFERENCE_TYPE_INDEXED: {
				Target = &MML->LabelsList[Ref->Idx];
			} break;
			case MML_REFERENCE_TYPE_ENDOF: {
				Target = &MML->LabelsList[Ref->Idx];
				if(Target->EndLabelIdx == MML_LABELIDX_NULL) {
					MML_AppendError(MML, "Reference to undefined end of label. Please report this error.", &Ref->InputOffs);
					return MML_ERROR;
				}
				Target = &MML->LabelsList[Target->EndLabelIdx];
			} break;
			default: {
				MML_AppendError(MML, "Got unknown label reference type. Please report this error.", &Ref->InputOffs);
				return MML_ERROR;
			} break;
		}

		//! If we're using a goto-type command, ensure we do not
		//! jump into the middle of a pattern or repeat section
		if(
			Ref->CmdType == MML_REFERENCE_CMDTYPE_GOTO &&
			(Target->NestLevel_Pattern > 0 || Target->NestLevel_Repeat > 0)
		) {
			MML_AppendError(MML, "Cannot jump to inside of pattern/repeat section.", &Ref->InputOffs);
			return MML_ERROR;
		}

		//! Generate a warning when using a goto-type command
		//! to jump into the start of a pattern definition
		if(
			Ref->CmdType == MML_REFERENCE_CMDTYPE_GOTO &&
			Target->Type == MML_LABEL_TYPE_PATTERN
		) {
			if(
				MML_AppendWarning(MML, "Jump to pattern start. It is preferred to jump to a label instead.", &Ref->InputOffs) == MML_ERROR
			) return MML_ERROR;
		}

		//! Ensure that nesting levels aren't exceeded.
		//! This will catch "accidental" nesting overflows
		//! eg. [a [[a]]2 ] [[ * ]]2
		//! NOTE: +1 to the nesting level because we haven't included
		//! the actual nesting into the target until just now.
		if((uint32_t)Ref->NestLevel + 1 + (uint32_t)Target->NestLevel_Max > (uint32_t)MML_MAX_NESTING_LEVELS) {
			switch(Target->Type) {
				case MML_LABEL_TYPE_PATTERN: {
					MML_AppendError(MML, "Nesting level exceeded inside this pattern recall.", &Ref->InputOffs);
				} break;
				case MML_LABEL_TYPE_REPEAT: {
					MML_AppendError(MML, "Nesting level exceeded inside this repeat.", &Target->InputOffs);
				} break;
			}
			return MML_ERROR;
		}

		//! If we're recalling a sub-pattern, ensure that it's actually within a pattern
		//! Note that sub-patterns are defined by labels inside a pattern.
		if(
			Ref->CmdType == MML_REFERENCE_CMDTYPE_PATTERN &&
			Target->Type == MML_LABEL_TYPE_GENERIC
		) {
			uint32_t ParentIdx = Target->ParentIdx;
			struct MML_Label_t *Parent = (ParentIdx != MML_LABELIDX_NULL) ? &MML->LabelsList[ParentIdx] : NULL;
			if(!Parent || Parent->Type != MML_LABEL_TYPE_PATTERN) {
				MML_AppendError(MML, "Sub-pattern recall does not target a pattern.", &Ref->InputOffs);
				return MML_ERROR;
			}
		}

		//! Resolve reference
		//! NOTE: Only mark as referenced when this is NOT a self-reference
		uint32_t DstOffs = Target->DataOffs;
		if(!Ref->SelfRef) Target->IsReferenced = 1;
		Target->IsSelfReferenced = Ref->SelfRef;
		Ref->DstOffs      = DstOffs;
		Ref->nNybblesLast = 0;
		Ref->DstOffsDelta = 0;

		//! Insert reference to sorted-by-destination linked list
		uint32_t Prev = MML_REFERENCEIDX_NULL;
		uint32_t Next = *RefIdxByDstOffs_Head;
		while(Next != MML_REFERENCEIDX_NULL) {
			const struct MML_Reference_t *NextRef = &MML->ReferencesList[Next];
			if(NextRef->DstOffs < DstOffs) break;
			Prev = Next;
			Next = NextRef->NextRefInDstOrder;
		}
		if(Prev != MML_REFERENCEIDX_NULL) MML->ReferencesList[Prev].NextRefInDstOrder = i;
		else *RefIdxByDstOffs_Head = i;
		Ref->NextRefInDstOrder = Next;
	}
	return MML_OK;
}
static int MML_PatchReferences(struct MML_t *MML, int *ExtraNybbles, uint32_t RefIdxByDstOffs_Head) {
	uint32_t i, nReferences = MML->nReferences;

	//! Adjust all offsets to account for the size of the offset data
	int Iteration = 0;
	for(Iteration=0;Iteration<MAX_REFERENCE_FIXUP_ITERATIONS;Iteration++) {
		int Verified = 1;
		uint32_t SrcOffsDelta = 0;
		for(i=0;i<nReferences;i++) {
			struct MML_Reference_t *Ref = &MML->ReferencesList[i];

			//! Get the number of nybbles needed for this reference
			ptrdiff_t RelOffs = (ptrdiff_t)(Ref->DstOffs + Ref->DstOffsDelta) - (ptrdiff_t)(Ref->DataOffs + SrcOffsDelta + Ref->nNybblesLast);
			if(RelOffs == 0) {
				MML_AppendError(MML, "Offset to label is 0; use a padding command if this is intentional.", &Ref->InputOffs);
				return MML_ERROR;
			}
			uint32_t nNybbles = MML_RelOffsSizeInNybbles(RelOffs);
			if(nNybbles == 0) {
				MML_AppendError(MML, "Offset to label is too large.", &Ref->InputOffs);
				return MML_ERROR;
			}
			SrcOffsDelta += nNybbles;

			//! If the size changed from the last iteration, fix things up
			int32_t DeltaNybbles = (int32_t)(nNybbles - Ref->nNybblesLast);
			if(DeltaNybbles != 0) {
				Ref->nNybblesLast = nNybbles;

				//! Patch destination offsets
				struct MML_Reference_t *NextDstRef = &MML->ReferencesList[RefIdxByDstOffs_Head];
				while(NextDstRef->DstOffs >= Ref->DataOffs) {
					NextDstRef->DstOffsDelta += DeltaNybbles;
					uint32_t Next = NextDstRef->NextRefInDstOrder;
					if(Next != MML_REFERENCEIDX_NULL) {
						NextDstRef = &MML->ReferencesList[Next];
					} else break;
				}

				//! If any changes occurred, we need another iteration to verify
				Verified = 0;
			}
		}
		if(Verified) break;
	}
	if(Iteration >= MAX_REFERENCE_FIXUP_ITERATIONS) {
		MML_AppendErrorGlobal(MML, "Too many iterations performed while resolving labels.");
		return MML_ERROR;
	}

	//! Finally, resolve the final relative offsets and perform bounds checking
	uint32_t SrcOffsDelta = 0;
	for(i=0;i<nReferences;i++) {
		struct MML_Reference_t *Ref = &MML->ReferencesList[i];
		uint32_t Value;
		int nNybbles = Ref->nNybblesLast;
		uint32_t SrcOffs = Ref->DataOffs + SrcOffsDelta;
		uint32_t DstOffs = Ref->DstOffs  + (uint32_t)Ref->DstOffsDelta;
		if(DstOffs < SrcOffs) Value = 0 | (((SrcOffs + nNybbles) - DstOffs) << 1);
		else                  Value = 1 | ((DstOffs - (SrcOffs + nNybbles)) << 1);
		Ref->DataOffs = SrcOffs;
		Ref->Value    = Value - (1 << 1);
		Ref->nNybbles = nNybbles;
		SrcOffsDelta += nNybbles;
	}
	*ExtraNybbles = SrcOffsDelta;
	return MML_OK;
}
static int MML_GenerateLabelWarnings(struct MML_t *MML) {
	//! Go through each label and generate a warning if not referenced
	uint32_t i;
	for(i=0;i<MML->nLabels;i++) {
		const struct MML_Label_t *Label = &MML->LabelsList[i];
		if(Label->IsReferenced) continue;

		//! If this is a self-reference, generate a warning for
		//! patterns that could be done as repeats instead
		if(Label->IsSelfReferenced) {
			if(Label->Type == MML_LABEL_TYPE_PATTERN) {
				if(MML_AppendWarning(MML, "Self-referenced pattern only; suggest using `[[]]` instead.", &Label->InputOffs) == MML_ERROR) return MML_ERROR;
			}
			continue;
		}

		//! Otherwise, this label is not needed
		switch(Label->Type) {
			case MML_LABEL_TYPE_GENERIC: {
				if(MML_AppendWarning(MML, "Unreferenced label.", &Label->InputOffs) == MML_ERROR) return MML_ERROR;
			} break;
			case MML_LABEL_TYPE_PATTERN: {
				if(MML_AppendWarning(MML, "Unreferenced pattern.", &Label->InputOffs) == MML_ERROR) return MML_ERROR;
			} break;
			default: break;
		}
	}
	return MML_OK;
}
int MML_Parse(struct MML_t *MML) {
	int Result;
	for(;;) {
		MML_ConsumeWhitespace(MML);
		Result = MML_ParseCommand(MML);
		if(Result != MML_OK) break;
	}

	//! If the result is EOF, then we're fine
	if(Result == MML_EOF) {
		Result = MML_OK;

		//! Make sure to finalize the last track
		if(!MML->nTracks) {
			MML_AppendErrorGlobal(MML, "No tracks defined.");
			return MML_ERROR;
		}
		if(MML_FinalizeTrack(MML) == MML_ERROR) return MML_ERROR;

		//! Resolve references and get the number of nybbles needed to store them
		int nNybblesForReferences;
		uint32_t RefIdxByDstOffs_Head = MML_REFERENCEIDX_NULL;
		if(MML_ResolveReferences(MML, &RefIdxByDstOffs_Head) == MML_ERROR) return MML_ERROR;

		//! Patch all references
		if(MML_PatchReferences(MML, &nNybblesForReferences, RefIdxByDstOffs_Head) == MML_ERROR) return MML_ERROR;

		//! Generate warnings for unused labels
		if(MML_GenerateLabelWarnings(MML) == MML_ERROR) return MML_ERROR;

		//! Begin compactifying into nybbles
		uint32_t InOffs = 0, InSize = MML->Output.Offs;
		uint32_t OutSize = 0;
		uint8_t *Dst = (uint8_t*)malloc((InSize + nNybblesForReferences) / 2);
		if(!Dst) {
			MML_AppendErrorGlobal(MML, "Out of memory while allocating output buffer.");
			return MML_ERROR;
		}
		const struct MML_Reference_t *NextRef = MML->ReferencesList, *EndRefs = NextRef + MML->nReferences;
		struct MML_TrackListing_t *NextTrack = MML->TracksList, *EndTracks = NextTrack + MML->nTracks;
		const uint8_t *Src = MML->Output.Data;
		for(;;) {
#define APPEND_NYBBLE(x) Dst[OutSize/2] = (Dst[OutSize/2] >> 4) | (uint8_t)((x) << 4), OutSize++
			//! Check for references
			if(NextRef && OutSize == NextRef->DataOffs) {
				//! Append reference offset
				uint32_t Value = NextRef->Value;
				if(Value <= 0xFC) {
					if(NextRef->nNybbles != 2) {
						MML_AppendErrorGlobal(MML, "Offset size mismatch (expected 2-nybble form). Please report this error.");
						return MML_ERROR;
					}
					APPEND_NYBBLE(Value >> 4);
					APPEND_NYBBLE(Value >> 0);
				} else if(Value <= 0x01FC) {
					if(NextRef->nNybbles != 4) {
						MML_AppendErrorGlobal(MML, "Offset size mismatch (expected 4-nybble form). Please report this error.");
						return MML_ERROR;
					}
					Value -= 0xFD;
					APPEND_NYBBLE(0xF);
					APPEND_NYBBLE(0xD);
					APPEND_NYBBLE(Value >> 4);
					APPEND_NYBBLE(Value >> 0);
				} else if(Value <= 0x0101FC) {
					if(NextRef->nNybbles != 6) {
						MML_AppendErrorGlobal(MML, "Offset size mismatch (expected 6-nybble form). Please report this error.");
						return MML_ERROR;
					}
					Value -= 0x01FD;
					APPEND_NYBBLE(0xF);
					APPEND_NYBBLE(0xE);
					APPEND_NYBBLE(Value >> 12);
					APPEND_NYBBLE(Value >>  8);
					APPEND_NYBBLE(Value >>  4);
					APPEND_NYBBLE(Value >>  0);
				} else {
					if(NextRef->nNybbles != 8) {
						MML_AppendErrorGlobal(MML, "Offset size mismatch (expected 8-nybble form). Please report this error.");
						return MML_ERROR;
					}
					Value -= 0x0101FD;
					APPEND_NYBBLE(0xF);
					APPEND_NYBBLE(0xF);
					APPEND_NYBBLE(Value >> 20);
					APPEND_NYBBLE(Value >> 16);
					APPEND_NYBBLE(Value >> 12);
					APPEND_NYBBLE(Value >>  8);
					APPEND_NYBBLE(Value >>  4);
					APPEND_NYBBLE(Value >>  0);
				}
				if(++NextRef == EndRefs) NextRef = NULL;
			}

			//! Check for end of commands
			//! We need this janky setup because we might have a reference inserted
			//! right at the end of a track, so we need to handle that before exiting
			if(InOffs >= InSize) {
				if(OutSize != InSize + nNybblesForReferences) {
					MML_AppendErrorGlobal(MML, "Output size mismatch. Please report this error.");
					return MML_ERROR;
				}
				if((OutSize%2) != 0) {
					MML_AppendErrorGlobal(MML, "Output is misaligned. Please report this error.");
					return MML_ERROR;
				}
				break;
			}

			//! Check to see if this is where a track begins, and update previous track's size
			if(NextTrack && InOffs == NextTrack->DataOffs) {
				NextTrack->DataOffs = OutSize / 2;
				if(NextTrack != MML->TracksList) NextTrack[-1].Size = (OutSize / 2) - NextTrack[-1].DataOffs;
				if(++NextTrack == EndTracks) NextTrack = NULL;
			}

			//! Push out the next nybble
			APPEND_NYBBLE(Src[InOffs]);
			InOffs++;
#undef APPEND_NYBBLE
		}
		EndTracks[-1].Size = (OutSize / 2) - EndTracks[-1].DataOffs;

		//! Swap output buffers, and update sizes
		free(MML->Output.Data);
		MML->Output.Data = Dst;
		MML->Output.Size = OutSize / 2;
	}
	return Result;
}

/************************************************/
//! EOF
/************************************************/
