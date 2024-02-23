#ifndef _DDS_API_H_
#define _DDS_API_H_

#include "dll.h"

struct DDS_C_API {
    DLLEXPORT void (*pErrorMessage)(int code, char line[80]);
    DLLEXPORT int STDCALL (*pSolveAllBoardsBin)(struct boards* bop, struct solvedBoards* solvedp);
};

#endif // _DDS_API_H_
