// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>



// TODO: reference additional headers your program requires here
#include <cv.h>
#include <highgui.h>

#define WCPADAPI(type) extern "C" __declspec(dllexport) type __stdcall
