// cbe.h : Includedatei f�r Standardsystem-Includedateien
// oder h�ufig verwendete projektspezifische Includedateien,
// die nur in unregelm��igen Abst�nden ge�ndert werden.
//

#pragma once

#ifdef CLEARBLOCKENGINE_EXPORTS
#define CBE_API __declspec(dllexport)
#else
#define CBE_API __declspec(dllimport)
#endif


#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Selten verwendete Teile der Windows-Header nicht einbinden.

// windows
#include <Windows.h>

// stl
#include <vector>
#include <fstream>
#include <string>
#include <algorithm>

// d3d
#define CGL_DEBUG
#include <cgl.h>
#pragma comment(lib, "ClearGraphicsLibrary.lib")

// math
#include <xnamath.h>
#include <d3dx9.h>
#pragma comment(lib, "d3dx9.lib")


// TODO: Hier auf zus�tzliche Header, die das Programm erfordert, verweisen.
