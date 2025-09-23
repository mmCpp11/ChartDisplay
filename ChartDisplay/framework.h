// Copyright (C) 2025 Matthew Moran
//
// This file is part of ChartDisplay.  This program is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 3, or (at your option)
// any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License 
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#ifdef _MSVC_LANG
#if _MSVC_LANG < 202302L
#error "Requires C++23"
#endif
#elif __cplusplus < 202302L
#error "Requires C++23"
#endif

#include "targetver.h"

// Windows Header Files
#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <objbase.h>      // For COM headers
#include <comdef.h>
#include <shobjidl.h>     // for IFileDialogEvents and IFileDialogControlEvents
