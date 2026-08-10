// Copyright (C) 2025 Matthew Moran
//
// This file is part of ChartDisplay. See the GPL-3.0 license headers in the other source files.

#pragma once

// Single source of truth for the program version. Used by the title bar (ChartDisplay.cpp) and the
// VERSIONINFO resource (ChartDisplay.rc). Bump these when releasing.
#define CD_VER_MAJOR 3
#define CD_VER_MINOR 0
#define CD_VER_PATCH 0
#define CD_VER_BUILD 0

#define CD_STRINGIZE2(x) #x
#define CD_STRINGIZE(x) CD_STRINGIZE2(x)
#define CD_VERSION_STRING CD_STRINGIZE(CD_VER_MAJOR) "." CD_STRINGIZE(CD_VER_MINOR) "." CD_STRINGIZE(CD_VER_PATCH)
