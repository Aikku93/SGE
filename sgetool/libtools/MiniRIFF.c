/**************************************/
#include <stdio.h>
#include <stdint.h>
/**************************************/
#include "FileIO_Int.h"
#include "MiniRIFF.h"
/**************************************/

//! Read RIFF chunk
int RIFF_CkRead(FILE *File, void *User, const struct RIFF_CkHdl_t *CkHdl, const struct RIFF_CkListHdl_t *ListHdl, RIFF_CkHdlFunc_t CkDefault) {
	int RetVal = 0;

	//! Read header
	//! NOTE: Size needs to be aligned to 2 bytes as per specification.
	struct RIFF_CkHeader_t Ck;
#if !IS_BIG_ENDIAN
	if(!fread(&Ck, sizeof(Ck), 1, File)) return 0;
#else
	if(
		!FileIO_Get_u32le(&Ck.Size, File) ||
		!FileIO_Get_u32le(&Ck.Type, File)
	) return 0;
#endif
	int CkDataBeg = ftell(File);
	int CkDataEnd = CkDataBeg + ((Ck.Size+1) &~ 1);

	//! RIFF/LIST?
	if(Ck.Type == RIFF_FOURCC("RIFF") || Ck.Type == RIFF_FOURCC("LIST")) {
		if(ListHdl) {
			//! Read list type, find handler
			uint32_t Type;
			if(!FileIO_Get_u32le(&Type, File)) return 0;
			for(;ListHdl->Type;ListHdl++) if(ListHdl->Type == Type) {
				//! Issue begin callback, parse list, then issue end callback
				if(ListHdl->ListCbBeg) {
					RetVal = ListHdl->ListCbBeg(File, User);
					if(RetVal < 0) break;
				}
				while(ftell(File) < CkDataEnd) {
					RetVal = RIFF_CkRead(File, User, ListHdl->CkHdl, ListHdl->ListHdl, CkDefault);
					if(RetVal < 0) break;
				}
				if(ListHdl->ListCbEnd) {
					RetVal = ListHdl->ListCbEnd(File, User);
					if(RetVal < 0) break;
				}
				break;
			}
		}
	} else if(CkHdl) {
		//! Not a RIFF/LIST, so interpret as a normal chunk
		int CkHandlerFound = 0;
		for(;CkHdl->Type;CkHdl++) {
			if(CkHdl->Type == Ck.Type) {
				RetVal = CkHdl->Func(File, User, &Ck);
				CkHandlerFound = 1;
				break;
			}
		}

		//! If no handler found, invoke default handler
		if(!CkHandlerFound && CkDefault) {
			RetVal = CkDefault(File, User, &Ck);
		}
	}

	//! Seek to next chunk
	fseek(File, CkDataEnd, SEEK_SET);
	return RetVal;
}

/**************************************/
//! EOF
/**************************************/
