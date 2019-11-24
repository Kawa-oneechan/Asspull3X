//Kawa made this to allow Asspull3X to compile on his Linux VM.
//NFD is slated for replacement by a custom system, so :shrug:

#include "nfd_common.h"
nfdresult_t NFD_OpenDialog( const nfdchar_t *filterList,
                            const nfdchar_t *defaultPath,
                            nfdchar_t **outPath )
{
	return NFD_CANCEL;
}

nfdresult_t NFD_SaveDialog( const nfdchar_t *filterList,
                            const nfdchar_t *defaultPath,
                            nfdchar_t **outPath )
{
	return NFD_CANCEL;
}
