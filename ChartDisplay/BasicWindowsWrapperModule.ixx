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
module; //insert windows.h into the global module fragment
#if !defined(_WIN64) || !defined(_UNICODE)
#error MMWindowsModule supports only Win64 with unicode support
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <CommCtrl.h>
#include <Richedit.h>
#include <shlobj.h>
#include <objbase.h>
#include <comdef.h>

export module BasicWindowsWrapperModule;

import std;

#ifndef CW_USEDEFAULT
const auto CW_USEDEFAULT = static_cast<int>(0x80000000);
#endif

namespace Win64Wrapper {
    //necessary forward declarations
    export class Window;
    export class ModelessDiagBox;
    export enum class KnownFolderID {
        LocalAppData,
        ProgramData,
        ProgramFiles,
        RoamingAppData,
        TemporaryData
    };
    //type of data contained in the data member of ExtraWindowData
    export enum class ExtraDataType {
        NoData,
        StackData,
        AllocatedDataNew,
        AllocatedDataMalloc,
        AllocatedDataNewArray,
        AllocatedDataCustom, //use of a custom allocator
        ManagedData, //anything that has RAII where the wndproc is not resposible for freeing it, equivalent to StackData essentially
        LPARAM_NonPtrData //specifically to indicate a non-pointer LPARAM for ModelessDialogBox data
    };
    namespace {
        std::unordered_map<HWND, Window*> windowreg;
        std::mutex winreg_mutex;
        //taken from https://stackoverflow.com/questions/8824255/getting-a-windows-message-name, credit to Sebasitian Axe
        inline const std::map<UINT, const wchar_t*> wmsg_Translation = {
    {0, L"WM_NULL" },
    {1, L"WM_CREATE" },
    {2, L"WM_DESTROY" },
    {3, L"WM_MOVE" },
    {5, L"WM_SIZE" },
    {6, L"WM_ACTIVATE" },
    {7, L"WM_SETFOCUS" },
    {8, L"WM_KILLFOCUS" },
    {10, L"WM_ENABLE" },
    {11, L"WM_SETREDRAW" },
    {12, L"WM_SETTEXT" },
    {13, L"WM_GETTEXT" },
    {14, L"WM_GETTEXTLENGTH" },
    {15, L"WM_PAINT" },
    {16, L"WM_CLOSE" },
    {17, L"WM_QUERYENDSESSION" },
    {18, L"WM_QUIT" },
    {19, L"WM_QUERYOPEN" },
    {20, L"WM_ERASEBKGND" },
    {21, L"WM_SYSCOLORCHANGE" },
    {22, L"WM_ENDSESSION" },
    {24, L"WM_SHOWWINDOW" },
    {25, L"WM_CTLCOLOR" },
    {26, L"WM_WININICHANGE" },
    {27, L"WM_DEVMODECHANGE" },
    {28, L"WM_ACTIVATEAPP" },
    {29, L"WM_FONTCHANGE" },
    {30, L"WM_TIMECHANGE" },
    {31, L"WM_CANCELMODE" },
    {32, L"WM_SETCURSOR" },
    {33, L"WM_MOUSEACTIVATE" },
    {34, L"WM_CHILDACTIVATE" },
    {35, L"WM_QUEUESYNC" },
    {36, L"WM_GETMINMAXINFO" },
    {38, L"WM_PAINTICON" },
    {39, L"WM_ICONERASEBKGND" },
    {40, L"WM_NEXTDLGCTL" },
    {42, L"WM_SPOOLERSTATUS" },
    {43, L"WM_DRAWITEM" },
    {44, L"WM_MEASUREITEM" },
    {45, L"WM_DELETEITEM" },
    {46, L"WM_VKEYTOITEM" },
    {47, L"WM_CHARTOITEM" },
    {48, L"WM_SETFONT" },
    {49, L"WM_GETFONT" },
    {50, L"WM_SETHOTKEY" },
    {51, L"WM_GETHOTKEY" },
    {55, L"WM_QUERYDRAGICON" },
    {57, L"WM_COMPAREITEM" },
    {61, L"WM_GETOBJECT" },
    {65, L"WM_COMPACTING" },
    {68, L"WM_COMMNOTIFY" },
    {70, L"WM_WINDOWPOSCHANGING" },
    {71, L"WM_WINDOWPOSCHANGED" },
    {72, L"WM_POWER" },
    {73, L"WM_COPYGLOBALDATA" },
    {74, L"WM_COPYDATA" },
    {75, L"WM_CANCELJOURNAL" },
    {78, L"WM_NOTIFY" },
    {80, L"WM_INPUTLANGCHANGEREQUEST" },
    {81, L"WM_INPUTLANGCHANGE" },
    {82, L"WM_TCARD" },
    {83, L"WM_HELP" },
    {84, L"WM_USERCHANGED" },
    {85, L"WM_NOTIFYFORMAT" },
    {123, L"WM_CONTEXTMENU" },
    {124, L"WM_STYLECHANGING" },
    {125, L"WM_STYLECHANGED" },
    {126, L"WM_DISPLAYCHANGE" },
    {127, L"WM_GETICON" },
    {128, L"WM_SETICON" },
    {129, L"WM_NCCREATE" },
    {130, L"WM_NCDESTROY" },
    {131, L"WM_NCCALCSIZE" },
    {132, L"WM_NCHITTEST" },
    {133, L"WM_NCPAINT" },
    {134, L"WM_NCACTIVATE" },
    {135, L"WM_GETDLGCODE" },
    {136, L"WM_SYNCPAINT" },
    {160, L"WM_NCMOUSEMOVE" },
    {161, L"WM_NCLBUTTONDOWN" },
    {162, L"WM_NCLBUTTONUP" },
    {163, L"WM_NCLBUTTONDBLCLK" },
    {164, L"WM_NCRBUTTONDOWN" },
    {165, L"WM_NCRBUTTONUP" },
    {166, L"WM_NCRBUTTONDBLCLK" },
    {167, L"WM_NCMBUTTONDOWN" },
    {168, L"WM_NCMBUTTONUP" },
    {169, L"WM_NCMBUTTONDBLCLK" },
    {171, L"WM_NCXBUTTONDOWN" },
    {172, L"WM_NCXBUTTONUP" },
    {173, L"WM_NCXBUTTONDBLCLK" },
    {176, L"EM_GETSEL" },
    {177, L"EM_SETSEL" },
    {178, L"EM_GETRECT" },
    {179, L"EM_SETRECT" },
    {180, L"EM_SETRECTNP" },
    {181, L"EM_SCROLL" },
    {182, L"EM_LINESCROLL" },
    {183, L"EM_SCROLLCARET" },
    {185, L"EM_GETMODIFY" },
    {187, L"EM_SETMODIFY" },
    {188, L"EM_GETLINECOUNT" },
    {189, L"EM_LINEINDEX" },
    {190, L"EM_SETHANDLE" },
    {191, L"EM_GETHANDLE" },
    {192, L"EM_GETTHUMB" },
    {193, L"EM_LINELENGTH" },
    {194, L"EM_REPLACESEL" },
    {195, L"EM_SETFONT" },
    {196, L"EM_GETLINE" },
    {197, L"EM_LIMITTEXT" },
    {197, L"EM_SETLIMITTEXT" },
    {198, L"EM_CANUNDO" },
    {199, L"EM_UNDO" },
    {200, L"EM_FMTLINES" },
    {201, L"EM_LINEFROMCHAR" },
    {202, L"EM_SETWORDBREAK" },
    {203, L"EM_SETTABSTOPS" },
    {204, L"EM_SETPASSWORDCHAR" },
    {205, L"EM_EMPTYUNDOBUFFER" },
    {206, L"EM_GETFIRSTVISIBLELINE" },
    {207, L"EM_SETREADONLY" },
    {209, L"EM_SETWORDBREAKPROC" },
    {209, L"EM_GETWORDBREAKPROC" },
    {210, L"EM_GETPASSWORDCHAR" },
    {211, L"EM_SETMARGINS" },
    {212, L"EM_GETMARGINS" },
    {213, L"EM_GETLIMITTEXT" },
    {214, L"EM_POSFROMCHAR" },
    {215, L"EM_CHARFROMPOS" },
    {216, L"EM_SETIMESTATUS" },
    {217, L"EM_GETIMESTATUS" },
    {224, L"SBM_SETPOS" },
    {225, L"SBM_GETPOS" },
    {226, L"SBM_SETRANGE" },
    {227, L"SBM_GETRANGE" },
    {228, L"SBM_ENABLE_ARROWS" },
    {230, L"SBM_SETRANGEREDRAW" },
    {233, L"SBM_SETSCROLLINFO" },
    {234, L"SBM_GETSCROLLINFO" },
    {235, L"SBM_GETSCROLLBARINFO" },
    {240, L"BM_GETCHECK" },
    {241, L"BM_SETCHECK" },
    {242, L"BM_GETSTATE" },
    {243, L"BM_SETSTATE" },
    {244, L"BM_SETSTYLE" },
    {245, L"BM_CLICK" },
    {246, L"BM_GETIMAGE" },
    {247, L"BM_SETIMAGE" },
    {248, L"BM_SETDONTCLICK" },
    {255, L"WM_INPUT" },
    {256, L"WM_KEYDOWN" },
    {256, L"WM_KEYFIRST" },
    {257, L"WM_KEYUP" },
    {258, L"WM_CHAR" },
    {259, L"WM_DEADCHAR" },
    {260, L"WM_SYSKEYDOWN" },
    {261, L"WM_SYSKEYUP" },
    {262, L"WM_SYSCHAR" },
    {263, L"WM_SYSDEADCHAR" },
    {264, L"WM_KEYLAST" },
    {265, L"WM_UNICHAR" },
    {265, L"WM_WNT_CONVERTREQUESTEX" },
    {266, L"WM_CONVERTREQUEST" },
    {267, L"WM_CONVERTRESULT" },
    {268, L"WM_INTERIM" },
    {269, L"WM_IME_STARTCOMPOSITION" },
    {270, L"WM_IME_ENDCOMPOSITION" },
    {271, L"WM_IME_COMPOSITION" },
    {271, L"WM_IME_KEYLAST" },
    {272, L"WM_INITDIALOG" },
    {273, L"WM_COMMAND" },
    {274, L"WM_SYSCOMMAND" },
    {275, L"WM_TIMER" },
    {276, L"WM_HSCROLL" },
    {277, L"WM_VSCROLL" },
    {278, L"WM_INITMENU" },
    {279, L"WM_INITMENUPOPUP" },
    {280, L"WM_SYSTIMER" },
    {287, L"WM_MENUSELECT" },
    {288, L"WM_MENUCHAR" },
    {289, L"WM_ENTERIDLE" },
    {290, L"WM_MENURBUTTONUP" },
    {291, L"WM_MENUDRAG" },
    {292, L"WM_MENUGETOBJECT" },
    {293, L"WM_UNINITMENUPOPUP" },
    {294, L"WM_MENUCOMMAND" },
    {295, L"WM_CHANGEUISTATE" },
    {296, L"WM_UPDATEUISTATE" },
    {297, L"WM_QUERYUISTATE" },
    {306, L"WM_CTLCOLORMSGBOX" },
    {307, L"WM_CTLCOLOREDIT" },
    {308, L"WM_CTLCOLORLISTBOX" },
    {309, L"WM_CTLCOLORBTN" },
    {310, L"WM_CTLCOLORDLG" },
    {311, L"WM_CTLCOLORSCROLLBAR" },
    {312, L"WM_CTLCOLORSTATIC" },
    {512, L"WM_MOUSEFIRST" },
    {512, L"WM_MOUSEMOVE" },
    {513, L"WM_LBUTTONDOWN" },
    {514, L"WM_LBUTTONUP" },
    {515, L"WM_LBUTTONDBLCLK" },
    {516, L"WM_RBUTTONDOWN" },
    {517, L"WM_RBUTTONUP" },
    {518, L"WM_RBUTTONDBLCLK" },
    {519, L"WM_MBUTTONDOWN" },
    {520, L"WM_MBUTTONUP" },
    {521, L"WM_MBUTTONDBLCLK" },
    {521, L"WM_MOUSELAST" },
    {522, L"WM_MOUSEWHEEL" },
    {523, L"WM_XBUTTONDOWN" },
    {524, L"WM_XBUTTONUP" },
    {525, L"WM_XBUTTONDBLCLK" },
    {528, L"WM_PARENTNOTIFY" },
    {529, L"WM_ENTERMENULOOP" },
    {530, L"WM_EXITMENULOOP" },
    {531, L"WM_NEXTMENU" },
    {532, L"WM_SIZING" },
    {533, L"WM_CAPTURECHANGED" },
    {534, L"WM_MOVING" },
    {536, L"WM_POWERBROADCAST" },
    {537, L"WM_DEVICECHANGE" },
    {544, L"WM_MDICREATE" },
    {545, L"WM_MDIDESTROY" },
    {546, L"WM_MDIACTIVATE" },
    {547, L"WM_MDIRESTORE" },
    {548, L"WM_MDINEXT" },
    {549, L"WM_MDIMAXIMIZE" },
    {550, L"WM_MDITILE" },
    {551, L"WM_MDICASCADE" },
    {552, L"WM_MDIICONARRANGE" },
    {553, L"WM_MDIGETACTIVE" },
    {560, L"WM_MDISETMENU" },
    {561, L"WM_ENTERSIZEMOVE" },
    {562, L"WM_EXITSIZEMOVE" },
    {563, L"WM_DROPFILES" },
    {564, L"WM_MDIREFRESHMENU" },
    {640, L"WM_IME_REPORT" },
    {641, L"WM_IME_SETCONTEXT" },
    {642, L"WM_IME_NOTIFY" },
    {643, L"WM_IME_CONTROL" },
    {644, L"WM_IME_COMPOSITIONFULL" },
    {645, L"WM_IME_SELECT" },
    {646, L"WM_IME_CHAR" },
    {648, L"WM_IME_REQUEST" },
    {656, L"WM_IMEKEYDOWN" },
    {656, L"WM_IME_KEYDOWN" },
    {657, L"WM_IMEKEYUP" },
    {657, L"WM_IME_KEYUP" },
    {672, L"WM_NCMOUSEHOVER" },
    {673, L"WM_MOUSEHOVER" },
    {674, L"WM_NCMOUSELEAVE" },
    {675, L"WM_MOUSELEAVE" },
    {768, L"WM_CUT" },
    {769, L"WM_COPY" },
    {770, L"WM_PASTE" },
    {771, L"WM_CLEAR" },
    {772, L"WM_UNDO" },
    {773, L"WM_RENDERFORMAT" },
    {774, L"WM_RENDERALLFORMATS" },
    {775, L"WM_DESTROYCLIPBOARD" },
    {776, L"WM_DRAWCLIPBOARD" },
    {777, L"WM_PAINTCLIPBOARD" },
    {778, L"WM_VSCROLLCLIPBOARD" },
    {779, L"WM_SIZECLIPBOARD" },
    {780, L"WM_ASKCBFORMATNAME" },
    {781, L"WM_CHANGECBCHAIN" },
    {782, L"WM_HSCROLLCLIPBOARD" },
    {783, L"WM_QUERYNEWPALETTE" },
    {784, L"WM_PALETTEISCHANGING" },
    {785, L"WM_PALETTECHANGED" },
    {786, L"WM_HOTKEY" },
    {791, L"WM_PRINT" },
    {792, L"WM_PRINTCLIENT" },
    {793, L"WM_APPCOMMAND" },
    {856, L"WM_HANDHELDFIRST" },
    {863, L"WM_HANDHELDLAST" },
    {864, L"WM_AFXFIRST" },
    {895, L"WM_AFXLAST" },
    {896, L"WM_PENWINFIRST" },
    {897, L"WM_RCRESULT" },
    {898, L"WM_HOOKRCRESULT" },
    {899, L"WM_GLOBALRCCHANGE" },
    {899, L"WM_PENMISCINFO" },
    {900, L"WM_SKB" },
    {901, L"WM_HEDITCTL" },
    {901, L"WM_PENCTL" },
    {902, L"WM_PENMISC" },
    {903, L"WM_CTLINIT" },
    {904, L"WM_PENEVENT" },
    {911, L"WM_PENWINLAST" },
    {1024, L"DDM_SETFMT" },
    {1024, L"DM_GETDEFID" },
    {1024, L"NIN_SELECT" },
    {1024, L"TBM_GETPOS" },
    {1024, L"WM_PSD_PAGESETUPDLG" },
    {1024, L"WM_USER" },
    {1025, L"CBEM_INSERTITEMA" },
    {1025, L"DDM_DRAW" },
    {1025, L"DM_SETDEFID" },
    {1025, L"HKM_SETHOTKEY" },
    {1025, L"PBM_SETRANGE" },
    {1025, L"RB_INSERTBANDA" },
    {1025, L"SB_SETTEXTA" },
    {1025, L"TB_ENABLEBUTTON" },
    {1025, L"TBM_GETRANGEMIN" },
    {1025, L"TTM_ACTIVATE" },
    {1025, L"WM_CHOOSEFONT_GETLOGFONT" },
    {1025, L"WM_PSD_FULLPAGERECT" },
    {1026, L"CBEM_SETIMAGELIST" },
    {1026, L"DDM_CLOSE" },
    {1026, L"DM_REPOSITION" },
    {1026, L"HKM_GETHOTKEY" },
    {1026, L"PBM_SETPOS" },
    {1026, L"RB_DELETEBAND" },
    {1026, L"SB_GETTEXTA" },
    {1026, L"TB_CHECKBUTTON" },
    {1026, L"TBM_GETRANGEMAX" },
    {1026, L"WM_PSD_MINMARGINRECT" },
    {1027, L"CBEM_GETIMAGELIST" },
    {1027, L"DDM_BEGIN" },
    {1027, L"HKM_SETRULES" },
    {1027, L"PBM_DELTAPOS" },
    {1027, L"RB_GETBARINFO" },
    {1027, L"SB_GETTEXTLENGTHA" },
    {1027, L"TBM_GETTIC" },
    {1027, L"TB_PRESSBUTTON" },
    {1027, L"TTM_SETDELAYTIME" },
    {1027, L"WM_PSD_MARGINRECT" },
    {1028, L"CBEM_GETITEMA" },
    {1028, L"DDM_END" },
    {1028, L"PBM_SETSTEP" },
    {1028, L"RB_SETBARINFO" },
    {1028, L"SB_SETPARTS" },
    {1028, L"TB_HIDEBUTTON" },
    {1028, L"TBM_SETTIC" },
    {1028, L"TTM_ADDTOOLA" },
    {1028, L"WM_PSD_GREEKTEXTRECT" },
    {1029, L"CBEM_SETITEMA" },
    {1029, L"PBM_STEPIT" },
    {1029, L"TB_INDETERMINATE" },
    {1029, L"TBM_SETPOS" },
    {1029, L"TTM_DELTOOLA" },
    {1029, L"WM_PSD_ENVSTAMPRECT" },
    {1030, L"CBEM_GETCOMBOCONTROL" },
    {1030, L"PBM_SETRANGE32" },
    {1030, L"RB_SETBANDINFOA" },
    {1030, L"SB_GETPARTS" },
    {1030, L"TB_MARKBUTTON" },
    {1030, L"TBM_SETRANGE" },
    {1030, L"TTM_NEWTOOLRECTA" },
    {1030, L"WM_PSD_YAFULLPAGERECT" },
    {1031, L"CBEM_GETEDITCONTROL" },
    {1031, L"PBM_GETRANGE" },
    {1031, L"RB_SETPARENT" },
    {1031, L"SB_GETBORDERS" },
    {1031, L"TBM_SETRANGEMIN" },
    {1031, L"TTM_RELAYEVENT" },
    {1032, L"CBEM_SETEXSTYLE" },
    {1032, L"PBM_GETPOS" },
    {1032, L"RB_HITTEST" },
    {1032, L"SB_SETMINHEIGHT" },
    {1032, L"TBM_SETRANGEMAX" },
    {1032, L"TTM_GETTOOLINFOA" },
    {1033, L"CBEM_GETEXSTYLE" },
    {1033, L"CBEM_GETEXTENDEDSTYLE" },
    {1033, L"PBM_SETBARCOLOR" },
    {1033, L"RB_GETRECT" },
    {1033, L"SB_SIMPLE" },
    {1033, L"TB_ISBUTTONENABLED" },
    {1033, L"TBM_CLEARTICS" },
    {1033, L"TTM_SETTOOLINFOA" },
    {1034, L"CBEM_HASEDITCHANGED" },
    {1034, L"RB_INSERTBANDW" },
    {1034, L"SB_GETRECT" },
    {1034, L"TB_ISBUTTONCHECKED" },
    {1034, L"TBM_SETSEL" },
    {1034, L"TTM_HITTESTA" },
    {1034, L"WIZ_QUERYNUMPAGES" },
    {1035, L"CBEM_INSERTITEMW" },
    {1035, L"RB_SETBANDINFOW" },
    {1035, L"SB_SETTEXTW" },
    {1035, L"TB_ISBUTTONPRESSED" },
    {1035, L"TBM_SETSELSTART" },
    {1035, L"TTM_GETTEXTA" },
    {1035, L"WIZ_NEXT" },
    {1036, L"CBEM_SETITEMW" },
    {1036, L"RB_GETBANDCOUNT" },
    {1036, L"SB_GETTEXTLENGTHW" },
    {1036, L"TB_ISBUTTONHIDDEN" },
    {1036, L"TBM_SETSELEND" },
    {1036, L"TTM_UPDATETIPTEXTA" },
    {1036, L"WIZ_PREV" },
    {1037, L"CBEM_GETITEMW" },
    {1037, L"RB_GETROWCOUNT" },
    {1037, L"SB_GETTEXTW" },
    {1037, L"TB_ISBUTTONINDETERMINATE" },
    {1037, L"TTM_GETTOOLCOUNT" },
    {1038, L"CBEM_SETEXTENDEDSTYLE" },
    {1038, L"RB_GETROWHEIGHT" },
    {1038, L"SB_ISSIMPLE" },
    {1038, L"TB_ISBUTTONHIGHLIGHTED" },
    {1038, L"TBM_GETPTICS" },
    {1038, L"TTM_ENUMTOOLSA" },
    {1039, L"SB_SETICON" },
    {1039, L"TBM_GETTICPOS" },
    {1039, L"TTM_GETCURRENTTOOLA" },
    {1040, L"RB_IDTOINDEX" },
    {1040, L"SB_SETTIPTEXTA" },
    {1040, L"TBM_GETNUMTICS" },
    {1040, L"TTM_WINDOWFROMPOINT" },
    {1041, L"RB_GETTOOLTIPS" },
    {1041, L"SB_SETTIPTEXTW" },
    {1041, L"TBM_GETSELSTART" },
    {1041, L"TB_SETSTATE" },
    {1041, L"TTM_TRACKACTIVATE" },
    {1042, L"RB_SETTOOLTIPS" },
    {1042, L"SB_GETTIPTEXTA" },
    {1042, L"TB_GETSTATE" },
    {1042, L"TBM_GETSELEND" },
    {1042, L"TTM_TRACKPOSITION" },
    {1043, L"RB_SETBKCOLOR" },
    {1043, L"SB_GETTIPTEXTW" },
    {1043, L"TB_ADDBITMAP" },
    {1043, L"TBM_CLEARSEL" },
    {1043, L"TTM_SETTIPBKCOLOR" },
    {1044, L"RB_GETBKCOLOR" },
    {1044, L"SB_GETICON" },
    {1044, L"TB_ADDBUTTONSA" },
    {1044, L"TBM_SETTICFREQ" },
    {1044, L"TTM_SETTIPTEXTCOLOR" },
    {1045, L"RB_SETTEXTCOLOR" },
    {1045, L"TB_INSERTBUTTONA" },
    {1045, L"TBM_SETPAGESIZE" },
    {1045, L"TTM_GETDELAYTIME" },
    {1046, L"RB_GETTEXTCOLOR" },
    {1046, L"TB_DELETEBUTTON" },
    {1046, L"TBM_GETPAGESIZE" },
    {1046, L"TTM_GETTIPBKCOLOR" },
    {1047, L"RB_SIZETORECT" },
    {1047, L"TB_GETBUTTON" },
    {1047, L"TBM_SETLINESIZE" },
    {1047, L"TTM_GETTIPTEXTCOLOR" },
    {1048, L"RB_BEGINDRAG" },
    {1048, L"TB_BUTTONCOUNT" },
    {1048, L"TBM_GETLINESIZE" },
    {1048, L"TTM_SETMAXTIPWIDTH" },
    {1049, L"RB_ENDDRAG" },
    {1049, L"TB_COMMANDTOINDEX" },
    {1049, L"TBM_GETTHUMBRECT" },
    {1049, L"TTM_GETMAXTIPWIDTH" },
    {1050, L"RB_DRAGMOVE" },
    {1050, L"TBM_GETCHANNELRECT" },
    {1050, L"TB_SAVERESTOREA" },
    {1050, L"TTM_SETMARGIN" },
    {1051, L"RB_GETBARHEIGHT" },
    {1051, L"TB_CUSTOMIZE" },
    {1051, L"TBM_SETTHUMBLENGTH" },
    {1051, L"TTM_GETMARGIN" },
    {1052, L"RB_GETBANDINFOW" },
    {1052, L"TB_ADDSTRINGA" },
    {1052, L"TBM_GETTHUMBLENGTH" },
    {1052, L"TTM_POP" },
    {1053, L"RB_GETBANDINFOA" },
    {1053, L"TB_GETITEMRECT" },
    {1053, L"TBM_SETTOOLTIPS" },
    {1053, L"TTM_UPDATE" },
    {1054, L"RB_MINIMIZEBAND" },
    {1054, L"TB_BUTTONSTRUCTSIZE" },
    {1054, L"TBM_GETTOOLTIPS" },
    {1054, L"TTM_GETBUBBLESIZE" },
    {1055, L"RB_MAXIMIZEBAND" },
    {1055, L"TBM_SETTIPSIDE" },
    {1055, L"TB_SETBUTTONSIZE" },
    {1055, L"TTM_ADJUSTRECT" },
    {1056, L"TBM_SETBUDDY" },
    {1056, L"TB_SETBITMAPSIZE" },
    {1056, L"TTM_SETTITLEA" },
    {1057, L"MSG_FTS_JUMP_VA" },
    {1057, L"TB_AUTOSIZE" },
    {1057, L"TBM_GETBUDDY" },
    {1057, L"TTM_SETTITLEW" },
    {1058, L"RB_GETBANDBORDERS" },
    {1059, L"MSG_FTS_JUMP_QWORD" },
    {1059, L"RB_SHOWBAND" },
    {1059, L"TB_GETTOOLTIPS" },
    {1060, L"MSG_REINDEX_REQUEST" },
    {1060, L"TB_SETTOOLTIPS" },
    {1061, L"MSG_FTS_WHERE_IS_IT" },
    {1061, L"RB_SETPALETTE" },
    {1061, L"TB_SETPARENT" },
    {1062, L"RB_GETPALETTE" },
    {1063, L"RB_MOVEBAND" },
    {1063, L"TB_SETROWS" },
    {1064, L"TB_GETROWS" },
    {1065, L"TB_GETBITMAPFLAGS" },
    {1066, L"TB_SETCMDID" },
    {1067, L"RB_PUSHCHEVRON" },
    {1067, L"TB_CHANGEBITMAP" },
    {1068, L"TB_GETBITMAP" },
    {1069, L"MSG_GET_DEFFONT" },
    {1069, L"TB_GETBUTTONTEXTA" },
    {1070, L"TB_REPLACEBITMAP" },
    {1071, L"TB_SETINDENT" },
    {1072, L"TB_SETIMAGELIST" },
    {1073, L"TB_GETIMAGELIST" },
    {1074, L"TB_LOADIMAGES" },
    {1074, L"EM_CANPASTE" },
    {1074, L"TTM_ADDTOOLW" },
    {1075, L"EM_DISPLAYBAND" },
    {1075, L"TB_GETRECT" },
    {1075, L"TTM_DELTOOLW" },
    {1076, L"EM_EXGETSEL" },
    {1076, L"TB_SETHOTIMAGELIST" },
    {1076, L"TTM_NEWTOOLRECTW" },
    {1077, L"EM_EXLIMITTEXT" },
    {1077, L"TB_GETHOTIMAGELIST" },
    {1077, L"TTM_GETTOOLINFOW" },
    {1078, L"EM_EXLINEFROMCHAR" },
    {1078, L"TB_SETDISABLEDIMAGELIST" },
    {1078, L"TTM_SETTOOLINFOW" },
    {1079, L"EM_EXSETSEL" },
    {1079, L"TB_GETDISABLEDIMAGELIST" },
    {1079, L"TTM_HITTESTW" },
    {1080, L"EM_FINDTEXT" },
    {1080, L"TB_SETSTYLE" },
    {1080, L"TTM_GETTEXTW" },
    {1081, L"EM_FORMATRANGE" },
    {1081, L"TB_GETSTYLE" },
    {1081, L"TTM_UPDATETIPTEXTW" },
    {1082, L"EM_GETCHARFORMAT" },
    {1082, L"TB_GETBUTTONSIZE" },
    {1082, L"TTM_ENUMTOOLSW" },
    {1083, L"EM_GETEVENTMASK" },
    {1083, L"TB_SETBUTTONWIDTH" },
    {1083, L"TTM_GETCURRENTTOOLW" },
    {1084, L"EM_GETOLEINTERFACE" },
    {1084, L"TB_SETMAXTEXTROWS" },
    {1085, L"EM_GETPARAFORMAT" },
    {1085, L"TB_GETTEXTROWS" },
    {1086, L"EM_GETSELTEXT" },
    {1086, L"TB_GETOBJECT" },
    {1087, L"EM_HIDESELECTION" },
    {1087, L"TB_GETBUTTONINFOW" },
    {1088, L"EM_PASTESPECIAL" },
    {1088, L"TB_SETBUTTONINFOW" },
    {1089, L"EM_REQUESTRESIZE" },
    {1089, L"TB_GETBUTTONINFOA" },
    {1090, L"EM_SELECTIONTYPE" },
    {1090, L"TB_SETBUTTONINFOA" },
    {1091, L"EM_SETBKGNDCOLOR" },
    {1091, L"TB_INSERTBUTTONW" },
    {1092, L"EM_SETCHARFORMAT" },
    {1092, L"TB_ADDBUTTONSW" },
    {1093, L"EM_SETEVENTMASK" },
    {1093, L"TB_HITTEST" },
    {1094, L"EM_SETOLECALLBACK" },
    {1094, L"TB_SETDRAWTEXTFLAGS" },
    {1095, L"EM_SETPARAFORMAT" },
    {1095, L"TB_GETHOTITEM" },
    {1096, L"EM_SETTARGETDEVICE" },
    {1096, L"TB_SETHOTITEM" },
    {1097, L"EM_STREAMIN" },
    {1097, L"TB_SETANCHORHIGHLIGHT" },
    {1098, L"EM_STREAMOUT" },
    {1098, L"TB_GETANCHORHIGHLIGHT" },
    {1099, L"EM_GETTEXTRANGE" },
    {1099, L"TB_GETBUTTONTEXTW" },
    {1100, L"EM_FINDWORDBREAK" },
    {1100, L"TB_SAVERESTOREW" },
    {1101, L"EM_SETOPTIONS" },
    {1101, L"TB_ADDSTRINGW" },
    {1102, L"EM_GETOPTIONS" },
    {1102, L"TB_MAPACCELERATORA" },
    {1103, L"EM_FINDTEXTEX" },
    {1103, L"TB_GETINSERTMARK" },
    {1104, L"EM_GETWORDBREAKPROCEX" },
    {1104, L"TB_SETINSERTMARK" },
    {1105, L"EM_SETWORDBREAKPROCEX" },
    {1105, L"TB_INSERTMARKHITTEST" },
    {1106, L"EM_SETUNDOLIMIT" },
    {1106, L"TB_MOVEBUTTON" },
    {1107, L"TB_GETMAXSIZE" },
    {1108, L"EM_REDO" },
    {1108, L"TB_SETEXTENDEDSTYLE" },
    {1109, L"EM_CANREDO" },
    {1109, L"TB_GETEXTENDEDSTYLE" },
    {1110, L"EM_GETUNDONAME" },
    {1110, L"TB_GETPADDING" },
    {1111, L"EM_GETREDONAME" },
    {1111, L"TB_SETPADDING" },
    {1112, L"EM_STOPGROUPTYPING" },
    {1112, L"TB_SETINSERTMARKCOLOR" },
    {1113, L"EM_SETTEXTMODE" },
    {1113, L"TB_GETINSERTMARKCOLOR" },
    {1114, L"EM_GETTEXTMODE" },
    {1114, L"TB_MAPACCELERATORW" },
    {1115, L"EM_AUTOURLDETECT" },
    {1115, L"TB_GETSTRINGW" },
    {1116, L"EM_GETAUTOURLDETECT" },
    {1116, L"TB_GETSTRINGA" },
    {1117, L"EM_SETPALETTE" },
    {1118, L"EM_GETTEXTEX" },
    {1119, L"EM_GETTEXTLENGTHEX" },
    {1120, L"EM_SHOWSCROLLBAR" },
    {1121, L"EM_SETTEXTEX" },
    {1123, L"TAPI_REPLY" },
    {1124, L"ACM_OPENA" },
    {1124, L"BFFM_SETSTATUSTEXTA" },
    {1124, L"CDM_FIRST" },
    {1124, L"CDM_GETSPEC" },
    {1124, L"EM_SETPUNCTUATION" },
    {1124, L"IPM_CLEARADDRESS" },
    {1124, L"WM_CAP_UNICODE_START" },
    {1125, L"ACM_PLAY" },
    {1125, L"BFFM_ENABLEOK" },
    {1125, L"CDM_GETFILEPATH" },
    {1125, L"EM_GETPUNCTUATION" },
    {1125, L"IPM_SETADDRESS" },
    {1125, L"PSM_SETCURSEL" },
    {1125, L"UDM_SETRANGE" },
    {1125, L"WM_CHOOSEFONT_SETLOGFONT" },
    {1126, L"ACM_STOP" },
    {1126, L"BFFM_SETSELECTIONA" },
    {1126, L"CDM_GETFOLDERPATH" },
    {1126, L"EM_SETWORDWRAPMODE" },
    {1126, L"IPM_GETADDRESS" },
    {1126, L"PSM_REMOVEPAGE" },
    {1126, L"UDM_GETRANGE" },
    {1126, L"WM_CAP_SET_CALLBACK_ERRORW" },
    {1126, L"WM_CHOOSEFONT_SETFLAGS" },
    {1127, L"ACM_OPENW" },
    {1127, L"BFFM_SETSELECTIONW" },
    {1127, L"CDM_GETFOLDERIDLIST" },
    {1127, L"EM_GETWORDWRAPMODE" },
    {1127, L"IPM_SETRANGE" },
    {1127, L"PSM_ADDPAGE" },
    {1127, L"UDM_SETPOS" },
    {1127, L"WM_CAP_SET_CALLBACK_STATUSW" },
    {1128, L"BFFM_SETSTATUSTEXTW" },
    {1128, L"CDM_SETCONTROLTEXT" },
    {1128, L"EM_SETIMECOLOR" },
    {1128, L"IPM_SETFOCUS" },
    {1128, L"PSM_CHANGED" },
    {1128, L"UDM_GETPOS" },
    {1129, L"CDM_HIDECONTROL" },
    {1129, L"EM_GETIMECOLOR" },
    {1129, L"IPM_ISBLANK" },
    {1129, L"PSM_RESTARTWINDOWS" },
    {1129, L"UDM_SETBUDDY" },
    {1130, L"CDM_SETDEFEXT" },
    {1130, L"EM_SETIMEOPTIONS" },
    {1130, L"PSM_REBOOTSYSTEM" },
    {1130, L"UDM_GETBUDDY" },
    {1131, L"EM_GETIMEOPTIONS" },
    {1131, L"PSM_CANCELTOCLOSE" },
    {1131, L"UDM_SETACCEL" },
    {1132, L"EM_CONVPOSITION" },
    {1132, L"EM_CONVPOSITION" },
    {1132, L"PSM_QUERYSIBLINGS" },
    {1132, L"UDM_GETACCEL" },
    {1133, L"MCIWNDM_GETZOOM" },
    {1133, L"PSM_UNCHANGED" },
    {1133, L"UDM_SETBASE" },
    {1134, L"PSM_APPLY" },
    {1134, L"UDM_GETBASE" },
    {1135, L"PSM_SETTITLEA" },
    {1135, L"UDM_SETRANGE32" },
    {1136, L"PSM_SETWIZBUTTONS" },
    {1136, L"UDM_GETRANGE32" },
    {1136, L"WM_CAP_DRIVER_GET_NAMEW" },
    {1137, L"PSM_PRESSBUTTON" },
    {1137, L"UDM_SETPOS32" },
    {1137, L"WM_CAP_DRIVER_GET_VERSIONW" },
    {1138, L"PSM_SETCURSELID" },
    {1138, L"UDM_GETPOS32" },
    {1139, L"PSM_SETFINISHTEXTA" },
    {1140, L"PSM_GETTABCONTROL" },
    {1141, L"PSM_ISDIALOGMESSAGE" },
    {1142, L"MCIWNDM_REALIZE" },
    {1142, L"PSM_GETCURRENTPAGEHWND" },
    {1143, L"MCIWNDM_SETTIMEFORMATA" },
    {1143, L"PSM_INSERTPAGE" },
    {1144, L"EM_SETLANGOPTIONS" },
    {1144, L"MCIWNDM_GETTIMEFORMATA" },
    {1144, L"PSM_SETTITLEW" },
    {1144, L"WM_CAP_FILE_SET_CAPTURE_FILEW" },
    {1145, L"EM_GETLANGOPTIONS" },
    {1145, L"MCIWNDM_VALIDATEMEDIA" },
    {1145, L"PSM_SETFINISHTEXTW" },
    {1145, L"WM_CAP_FILE_GET_CAPTURE_FILEW" },
    {1146, L"EM_GETIMECOMPMODE" },
    {1147, L"EM_FINDTEXTW" },
    {1147, L"MCIWNDM_PLAYTO" },
    {1147, L"WM_CAP_FILE_SAVEASW" },
    {1148, L"EM_FINDTEXTEXW" },
    {1148, L"MCIWNDM_GETFILENAMEA" },
    {1149, L"EM_RECONVERSION" },
    {1149, L"MCIWNDM_GETDEVICEA" },
    {1149, L"PSM_SETHEADERTITLEA" },
    {1149, L"WM_CAP_FILE_SAVEDIBW" },
    {1150, L"EM_SETIMEMODEBIAS" },
    {1150, L"MCIWNDM_GETPALETTE" },
    {1150, L"PSM_SETHEADERTITLEW" },
    {1151, L"EM_GETIMEMODEBIAS" },
    {1151, L"MCIWNDM_SETPALETTE" },
    {1151, L"PSM_SETHEADERSUBTITLEA" },
    {1152, L"MCIWNDM_GETERRORA" },
    {1152, L"PSM_SETHEADERSUBTITLEW" },
    {1153, L"PSM_HWNDTOINDEX" },
    {1154, L"PSM_INDEXTOHWND" },
    {1155, L"MCIWNDM_SETINACTIVETIMER" },
    {1155, L"PSM_PAGETOINDEX" },
    {1156, L"PSM_INDEXTOPAGE" },
    {1157, L"DL_BEGINDRAG" },
    {1157, L"MCIWNDM_GETINACTIVETIMER" },
    {1157, L"PSM_IDTOINDEX" },
    {1158, L"DL_DRAGGING" },
    {1158, L"PSM_INDEXTOID" },
    {1159, L"DL_DROPPED" },
    {1159, L"PSM_GETRESULT" },
    {1160, L"DL_CANCELDRAG" },
    {1160, L"PSM_RECALCPAGESIZES" },
    {1164, L"MCIWNDM_GET_SOURCE" },
    {1165, L"MCIWNDM_PUT_SOURCE" },
    {1166, L"MCIWNDM_GET_DEST" },
    {1167, L"MCIWNDM_PUT_DEST" },
    {1168, L"MCIWNDM_CAN_PLAY" },
    {1169, L"MCIWNDM_CAN_WINDOW" },
    {1170, L"MCIWNDM_CAN_RECORD" },
    {1171, L"MCIWNDM_CAN_SAVE" },
    {1172, L"MCIWNDM_CAN_EJECT" },
    {1173, L"MCIWNDM_CAN_CONFIG" },
    {1174, L"IE_GETINK" },
    {1174, L"IE_MSGFIRST" },
    {1174, L"MCIWNDM_PALETTEKICK" },
    {1175, L"IE_SETINK" },
    {1176, L"IE_GETPENTIP" },
    {1177, L"IE_SETPENTIP" },
    {1178, L"IE_GETERASERTIP" },
    {1179, L"IE_SETERASERTIP" },
    {1180, L"IE_GETBKGND" },
    {1181, L"IE_SETBKGND" },
    {1182, L"IE_GETGRIDORIGIN" },
    {1183, L"IE_SETGRIDORIGIN" },
    {1184, L"IE_GETGRIDPEN" },
    {1185, L"IE_SETGRIDPEN" },
    {1186, L"IE_GETGRIDSIZE" },
    {1187, L"IE_SETGRIDSIZE" },
    {1188, L"IE_GETMODE" },
    {1189, L"IE_SETMODE" },
    {1190, L"IE_GETINKRECT" },
    {1190, L"WM_CAP_SET_MCI_DEVICEW" },
    {1191, L"WM_CAP_GET_MCI_DEVICEW" },
    {1204, L"WM_CAP_PAL_OPENW" },
    {1205, L"WM_CAP_PAL_SAVEW" },
    {1208, L"IE_GETAPPDATA" },
    {1209, L"IE_SETAPPDATA" },
    {1210, L"IE_GETDRAWOPTS" },
    {1211, L"IE_SETDRAWOPTS" },
    {1212, L"IE_GETFORMAT" },
    {1213, L"IE_SETFORMAT" },
    {1214, L"IE_GETINKINPUT" },
    {1215, L"IE_SETINKINPUT" },
    {1216, L"IE_GETNOTIFY" },
    {1217, L"IE_SETNOTIFY" },
    {1218, L"IE_GETRECOG" },
    {1219, L"IE_SETRECOG" },
    {1220, L"IE_GETSECURITY" },
    {1221, L"IE_SETSECURITY" },
    {1222, L"IE_GETSEL" },
    {1223, L"IE_SETSEL" },
    {1224, L"CDM_LAST" },
    {1224, L"EM_SETBIDIOPTIONS" },
    {1224, L"IE_DOCOMMAND" },
    {1224, L"MCIWNDM_NOTIFYMODE" },
    {1225, L"EM_GETBIDIOPTIONS" },
    {1225, L"IE_GETCOMMAND" },
    {1226, L"EM_SETTYPOGRAPHYOPTIONS" },
    {1226, L"IE_GETCOUNT" },
    {1227, L"EM_GETTYPOGRAPHYOPTIONS" },
    {1227, L"IE_GETGESTURE" },
    {1227, L"MCIWNDM_NOTIFYMEDIA" },
    {1228, L"EM_SETEDITSTYLE" },
    {1228, L"IE_GETMENU" },
    {1229, L"EM_GETEDITSTYLE" },
    {1229, L"IE_GETPAINTDC" },
    {1229, L"MCIWNDM_NOTIFYERROR" },
    {1230, L"IE_GETPDEVENT" },
    {1231, L"IE_GETSELCOUNT" },
    {1232, L"IE_GETSELITEMS" },
    {1233, L"IE_GETSTYLE" },
    {1243, L"MCIWNDM_SETTIMEFORMATW" },
    {1244, L"EM_OUTLINE" },
    {1244, L"EM_OUTLINE" },
    {1244, L"MCIWNDM_GETTIMEFORMATW" },
    {1245, L"EM_GETSCROLLPOS" },
    {1245, L"EM_GETSCROLLPOS" },
    {1246, L"EM_SETSCROLLPOS" },
    {1246, L"EM_SETSCROLLPOS" },
    {1247, L"EM_SETFONTSIZE" },
    {1247, L"EM_SETFONTSIZE" },
    {1248, L"EM_GETZOOM" },
    {1248, L"MCIWNDM_GETFILENAMEW" },
    {1249, L"EM_SETZOOM" },
    {1249, L"MCIWNDM_GETDEVICEW" },
    {1250, L"EM_GETVIEWKIND" },
    {1251, L"EM_SETVIEWKIND" },
    {1252, L"EM_GETPAGE" },
    {1252, L"MCIWNDM_GETERRORW" },
    {1253, L"EM_SETPAGE" },
    {1254, L"EM_GETHYPHENATEINFO" },
    {1255, L"EM_SETHYPHENATEINFO" },
    {1259, L"EM_GETPAGEROTATE" },
    {1260, L"EM_SETPAGEROTATE" },
    {1261, L"EM_GETCTFMODEBIAS" },
    {1262, L"EM_SETCTFMODEBIAS" },
    {1264, L"EM_GETCTFOPENSTATUS" },
    {1265, L"EM_SETCTFOPENSTATUS" },
    {1266, L"EM_GETIMECOMPTEXT" },
    {1267, L"EM_ISIME" },
    {1268, L"EM_GETIMEPROPERTY" },
    {1293, L"EM_GETQUERYRTFOBJ" },
    {1294, L"EM_SETQUERYRTFOBJ" },
    {1536, L"FM_GETFOCUS" },
    {1537, L"FM_GETDRIVEINFOA" },
    {1538, L"FM_GETSELCOUNT" },
    {1539, L"FM_GETSELCOUNTLFN" },
    {1540, L"FM_GETFILESELA" },
    {1541, L"FM_GETFILESELLFNA" },
    {1542, L"FM_REFRESH_WINDOWS" },
    {1543, L"FM_RELOAD_EXTENSIONS" },
    {1553, L"FM_GETDRIVEINFOW" },
    {1556, L"FM_GETFILESELW" },
    {1557, L"FM_GETFILESELLFNW" },
    {1625, L"WLX_WM_SAS" },
    {2024, L"SM_GETSELCOUNT" },
    {2024, L"UM_GETSELCOUNT" },
    {2024, L"WM_CPL_LAUNCH" },
    {2025, L"SM_GETSERVERSELA" },
    {2025, L"UM_GETUSERSELA" },
    {2025, L"WM_CPL_LAUNCHED" },
    {2026, L"SM_GETSERVERSELW" },
    {2026, L"UM_GETUSERSELW" },
    {2027, L"SM_GETCURFOCUSA" },
    {2027, L"UM_GETGROUPSELA" },
    {2028, L"SM_GETCURFOCUSW" },
    {2028, L"UM_GETGROUPSELW" },
    {2029, L"SM_GETOPTIONS" },
    {2029, L"UM_GETCURFOCUSA" },
    {2030, L"UM_GETCURFOCUSW" },
    {2031, L"UM_GETOPTIONS" },
    {2032, L"UM_GETOPTIONS2" },
    {4096, L"LVM_FIRST" },
    {4096, L"LVM_GETBKCOLOR" },
    {4097, L"LVM_SETBKCOLOR" },
    {4098, L"LVM_GETIMAGELIST" },
    {4099, L"LVM_SETIMAGELIST" },
    {4100, L"LVM_GETITEMCOUNT" },
    {4101, L"LVM_GETITEMA" },
    {4102, L"LVM_SETITEMA" },
    {4103, L"LVM_INSERTITEMA" },
    {4104, L"LVM_DELETEITEM" },
    {4105, L"LVM_DELETEALLITEMS" },
    {4106, L"LVM_GETCALLBACKMASK" },
    {4107, L"LVM_SETCALLBACKMASK" },
    {4108, L"LVM_GETNEXTITEM" },
    {4109, L"LVM_FINDITEMA" },
    {4110, L"LVM_GETITEMRECT" },
    {4111, L"LVM_SETITEMPOSITION" },
    {4112, L"LVM_GETITEMPOSITION" },
    {4113, L"LVM_GETSTRINGWIDTHA" },
    {4114, L"LVM_HITTEST" },
    {4115, L"LVM_ENSUREVISIBLE" },
    {4116, L"LVM_SCROLL" },
    {4117, L"LVM_REDRAWITEMS" },
    {4118, L"LVM_ARRANGE" },
    {4119, L"LVM_EDITLABELA" },
    {4120, L"LVM_GETEDITCONTROL" },
    {4121, L"LVM_GETCOLUMNA" },
    {4122, L"LVM_SETCOLUMNA" },
    {4123, L"LVM_INSERTCOLUMNA" },
    {4124, L"LVM_DELETECOLUMN" },
    {4125, L"LVM_GETCOLUMNWIDTH" },
    {4126, L"LVM_SETCOLUMNWIDTH" },
    {4127, L"LVM_GETHEADER" },
    {4129, L"LVM_CREATEDRAGIMAGE" },
    {4130, L"LVM_GETVIEWRECT" },
    {4131, L"LVM_GETTEXTCOLOR" },
    {4132, L"LVM_SETTEXTCOLOR" },
    {4133, L"LVM_GETTEXTBKCOLOR" },
    {4134, L"LVM_SETTEXTBKCOLOR" },
    {4135, L"LVM_GETTOPINDEX" },
    {4136, L"LVM_GETCOUNTPERPAGE" },
    {4137, L"LVM_GETORIGIN" },
    {4138, L"LVM_UPDATE" },
    {4139, L"LVM_SETITEMSTATE" },
    {4140, L"LVM_GETITEMSTATE" },
    {4141, L"LVM_GETITEMTEXTA" },
    {4142, L"LVM_SETITEMTEXTA" },
    {4143, L"LVM_SETITEMCOUNT" },
    {4144, L"LVM_SORTITEMS" },
    {4145, L"LVM_SETITEMPOSITION32" },
    {4146, L"LVM_GETSELECTEDCOUNT" },
    {4147, L"LVM_GETITEMSPACING" },
    {4148, L"LVM_GETISEARCHSTRINGA" },
    {4149, L"LVM_SETICONSPACING" },
    {4150, L"LVM_SETEXTENDEDLISTVIEWSTYLE" },
    {4151, L"LVM_GETEXTENDEDLISTVIEWSTYLE" },
    {4152, L"LVM_GETSUBITEMRECT" },
    {4153, L"LVM_SUBITEMHITTEST" },
    {4154, L"LVM_SETCOLUMNORDERARRAY" },
    {4155, L"LVM_GETCOLUMNORDERARRAY" },
    {4156, L"LVM_SETHOTITEM" },
    {4157, L"LVM_GETHOTITEM" },
    {4158, L"LVM_SETHOTCURSOR" },
    {4159, L"LVM_GETHOTCURSOR" },
    {4160, L"LVM_APPROXIMATEVIEWRECT" },
    {4161, L"LVM_SETWORKAREAS" },
    {4162, L"LVM_GETSELECTIONMARK" },
    {4163, L"LVM_SETSELECTIONMARK" },
    {4164, L"LVM_SETBKIMAGEA" },
    {4165, L"LVM_GETBKIMAGEA" },
    {4166, L"LVM_GETWORKAREAS" },
    {4167, L"LVM_SETHOVERTIME" },
    {4168, L"LVM_GETHOVERTIME" },
    {4169, L"LVM_GETNUMBEROFWORKAREAS" },
    {4170, L"LVM_SETTOOLTIPS" },
    {4171, L"LVM_GETITEMW" },
    {4172, L"LVM_SETITEMW" },
    {4173, L"LVM_INSERTITEMW" },
    {4174, L"LVM_GETTOOLTIPS" },
    {4179, L"LVM_FINDITEMW" },
    {4183, L"LVM_GETSTRINGWIDTHW" },
    {4191, L"LVM_GETCOLUMNW" },
    {4192, L"LVM_SETCOLUMNW" },
    {4193, L"LVM_INSERTCOLUMNW" },
    {4211, L"LVM_GETITEMTEXTW" },
    {4212, L"LVM_SETITEMTEXTW" },
    {4213, L"LVM_GETISEARCHSTRINGW" },
    {4214, L"LVM_EDITLABELW" },
    {4235, L"LVM_GETBKIMAGEW" },
    {4236, L"LVM_SETSELECTEDCOLUMN" },
    {4237, L"LVM_SETTILEWIDTH" },
    {4238, L"LVM_SETVIEW" },
    {4239, L"LVM_GETVIEW" },
    {4241, L"LVM_INSERTGROUP" },
    {4243, L"LVM_SETGROUPINFO" },
    {4245, L"LVM_GETGROUPINFO" },
    {4246, L"LVM_REMOVEGROUP" },
    {4247, L"LVM_MOVEGROUP" },
    {4250, L"LVM_MOVEITEMTOGROUP" },
    {4251, L"LVM_SETGROUPMETRICS" },
    {4252, L"LVM_GETGROUPMETRICS" },
    {4253, L"LVM_ENABLEGROUPVIEW" },
    {4254, L"LVM_SORTGROUPS" },
    {4255, L"LVM_INSERTGROUPSORTED" },
    {4256, L"LVM_REMOVEALLGROUPS" },
    {4257, L"LVM_HASGROUP" },
    {4258, L"LVM_SETTILEVIEWINFO" },
    {4259, L"LVM_GETTILEVIEWINFO" },
    {4260, L"LVM_SETTILEINFO" },
    {4261, L"LVM_GETTILEINFO" },
    {4262, L"LVM_SETINSERTMARK" },
    {4263, L"LVM_GETINSERTMARK" },
    {4264, L"LVM_INSERTMARKHITTEST" },
    {4265, L"LVM_GETINSERTMARKRECT" },
    {4266, L"LVM_SETINSERTMARKCOLOR" },
    {4267, L"LVM_GETINSERTMARKCOLOR" },
    {4269, L"LVM_SETINFOTIP" },
    {4270, L"LVM_GETSELECTEDCOLUMN" },
    {4271, L"LVM_ISGROUPVIEWENABLED" },
    {4272, L"LVM_GETOUTLINECOLOR" },
    {4273, L"LVM_SETOUTLINECOLOR" },
    {4275, L"LVM_CANCELEDITLABEL" },
    {4276, L"LVM_MAPINDEXTOID" },
    {4277, L"LVM_MAPIDTOINDEX" },
    {4278, L"LVM_ISITEMVISIBLE" },
    {8192, L"OCM__BASE" },
    {8197, L"LVM_SETUNICODEFORMAT" },
    {8198, L"LVM_GETUNICODEFORMAT" },
    {8217, L"OCM_CTLCOLOR" },
    {8235, L"OCM_DRAWITEM" },
    {8236, L"OCM_MEASUREITEM" },
    {8237, L"OCM_DELETEITEM" },
    {8238, L"OCM_VKEYTOITEM" },
    {8239, L"OCM_CHARTOITEM" },
    {8249, L"OCM_COMPAREITEM" },
    {8270, L"OCM_NOTIFY" },
    {8465, L"OCM_COMMAND" },
    {8468, L"OCM_HSCROLL" },
    {8469, L"OCM_VSCROLL" },
    {8498, L"OCM_CTLCOLORMSGBOX" },
    {8499, L"OCM_CTLCOLOREDIT" },
    {8500, L"OCM_CTLCOLORLISTBOX" },
    {8501, L"OCM_CTLCOLORBTN" },
    {8502, L"OCM_CTLCOLORDLG" },
    {8503, L"OCM_CTLCOLORSCROLLBAR" },
    {8504, L"OCM_CTLCOLORSTATIC" },
    {8720, L"OCM_PARENTNOTIFY" },
    {32768, L"WM_APP" },
    {52429, L"WM_RASDIALEVENT" },
        };
        struct RegisteredWindowDeleter {
            void operator()(HWND windowptr) const noexcept {
                if (windowptr) {
                    try {
                        //scoped lock guarantees unlock even if erase throws; the registry bookkeeping
                        //failing must not prevent the window itself from being destroyed below.
                        std::lock_guard<std::mutex> guard(winreg_mutex);
                        windowreg.erase(windowptr);
                    }
                    catch (...) {}
                    DestroyWindow(windowptr);
                }
            }
        };
        using RegisteredWindow = std::unique_ptr<std::remove_pointer_t<HWND>, RegisteredWindowDeleter>;
        using GenericWindow = std::unique_ptr < std::remove_pointer_t<HWND>, decltype([](HWND ptr) {DestroyWindow(ptr);}) > ;
        const DWORD NO_STYLES = 0;
        const ATOM DIALOGATOM = 32770;
        struct ExtraWindowData {
            Window* winclass;
            void* data;
            ExtraDataType type;
            ExtraWindowData(Window* w=nullptr, void* d=nullptr,ExtraDataType t=ExtraDataType::NoData) :winclass(w), data(d),type(t) {}
        };
        struct ExtraDialogData {
            ModelessDiagBox* winclass;
            LPARAM data;
            ExtraDataType type;
            ExtraDialogData(ModelessDiagBox* w = nullptr, LPARAM d = 0, ExtraDataType t = ExtraDataType::NoData) 
                :winclass(w), data(d), type(t) {}
        };

        const std::unordered_map<KnownFolderID, GUID> known_folder_guid = {
            {KnownFolderID::LocalAppData,FOLDERID_LocalAppData},
            {KnownFolderID::ProgramData,FOLDERID_ProgramData},
            {KnownFolderID::RoamingAppData,FOLDERID_RoamingAppData}
        };
    }
    export {
        //COM Items
        template<typename T>
        concept COMInterface = std::derived_from<T, IUnknown>;
        template<COMInterface T>
        struct COMDeleter {
            void operator()(T* ptr) {
                ptr->Release();
            }
        };
        template<COMInterface T>
        using GenericCOMPtr = std::unique_ptr<T, COMDeleter<T>>;

        struct COMInit {
            const HRESULT hr;
            COMInit(DWORD initflags = COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)
                :hr(CoInitializeEx(nullptr,initflags)) {}
            ~COMInit() {
                CoUninitialize();
            }
            COMInit(const COMInit&) = delete;
            COMInit(COMInit&&) = delete;
            COMInit& operator=(const COMInit&) = delete;
            COMInit& operator=(COMInit&&) = delete;
        };
        //throw _com_error if FAILED(hr) is true
        void CheckCOMResult(HRESULT hr) {
            if (FAILED(hr)) {
                throw _com_error(hr);
            }
        }
        /////////////////////////////////////////
        //convert a Windows message number to a const wchar_t* containing the name of the message. Useful for debugging
        const wchar_t* ConvertMessage(UINT msg) {
            try {
                auto txtmsg = wmsg_Translation.at(msg);
                return txtmsg;
            }
            catch (std::out_of_range&) {
                return L"Unknown Message";
            }
        }
        std::wstring convert_string(const std::string& str) {
            if (str.empty()) return {};
            const int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
            if (size == 0) return {};
            std::wstring wstr;
            wstr.resize_and_overwrite(size, [&](wchar_t* buf, size_t) -> size_t {
                int written = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), buf, size);
                return written > 0 ? static_cast<size_t>(written) : 0;
            });
            return wstr;
        }

        std::string convert_string(const std::wstring& wstr) {
            if (wstr.empty()) return {};
            const int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
            if (size == 0) return {};
            std::string str;
            str.resize_and_overwrite(size, [&](char* buf, size_t) -> size_t {
                int written = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), buf, size, nullptr, nullptr);
                return written > 0 ? static_cast<size_t>(written) : 0;
            });
            return str;
        }
        //casts pointer to LPARAM, less typing
        template<typename T>
        LPARAM PtrToLP(T* ptr) {
            return std::bit_cast<LPARAM>(ptr);
        }
        inline LPARAM PtrToLP(std::nullptr_t) noexcept
        {
            return 0;
        }
        //associated windows, store a HWND to the class it is associated with in the window proc of the associated class
        //WPARAM: either 1 for store or 2 for remove
        //LPARAM on store: HWND of the window that the window of the proc has been associated to. On remove: not used
        const UINT WM_ASSOCIATEDWINDOW = (WM_USER + 0x0700);
        enum class HookTypes {
            InputMessage = WH_MSGFILTER,
            //        SysInputMessage = WH_SYSMSGFILTER,
            Keyboard = WH_KEYBOARD,
            //        LowKeyboard = WH_KEYBOARD_LL,
            Mouse = WH_MOUSE,
            //        LowMouse = WH_MOUSE_LL,
            Message = WH_GETMESSAGE,
            PreWindowProc = WH_CALLWNDPROC,
            PostWindowProc = WH_CALLWNDPROCRET,
            CBT = WH_CBT,
            Debug = WH_DEBUG,
            ForegroundIdle = WH_FOREGROUNDIDLE,
            Shell = WH_SHELL
        };
        //Set Hook Handle with a call to SetWindowsHookEx
        //workaround for MSVC bug (can't export with lambda in unevaluated context or with anonymous struct
        //Bug: https://developercommunity.visualstudio.com/t/Modules-export-anonymous-struct-variab/10177181
        struct HookDeleter {
            void operator()(HHOOK hook) {
                UnhookWindowsHookEx(hook);
            }
        };
        using HookHandle = std::unique_ptr<std::remove_pointer_t<HHOOK>, HookDeleter>;
        //    export using HookHandle = std::unique_ptr < std::remove_pointer_t<HHOOK>, decltype([](HHOOK hook) {UnhookWindowsHookEx(hook);}) > ; //should work
        enum class MessageBoxResponse {
            Error=0,
            Ok = IDOK,
            Cancel,
            Abort,
            Retry,
            Ignore,
            Yes,
            No,
            Again = 10
        };
        enum class MessageBoxStyles : UINT {
            AbortRetryIgnore = MB_ABORTRETRYIGNORE,
            CancelTryContinue = MB_CANCELTRYCONTINUE,
            Help = MB_HELP,
            Ok = MB_OK,
            OkCancel = MB_OKCANCEL,
            RetryCancel = MB_RETRYCANCEL,
            YesNo = MB_YESNO,
            YesNoCancel = MB_YESNOCANCEL,
            IconExclamation = MB_ICONEXCLAMATION,
            IconWarning = MB_ICONWARNING,
            IconInformation = MB_ICONINFORMATION,
            IconAsterisk = MB_ICONASTERISK,
            IconQuestion = MB_ICONQUESTION,
            IconStop = MB_ICONSTOP,
            IconError = MB_ICONERROR,
            IconHand = MB_ICONHAND,
            DefaultButton1 = MB_DEFBUTTON1,
            DefaultButton2 = MB_DEFBUTTON2,
            DefaultButton3 = MB_DEFBUTTON3,
            DefaultButton4 = MB_DEFBUTTON4,
            AppModal = MB_APPLMODAL,
            SystemModal = MB_SYSTEMMODAL,
            TaskModal = MB_TASKMODAL,
            DefaultDesktop = MB_DEFAULT_DESKTOP_ONLY,
            RightJustified = MB_RIGHT,
            RightToLeft = MB_RTLREADING,
            Foreground = MB_SETFOREGROUND,
            Topmost = MB_TOPMOST,
            Service = MB_SERVICE_NOTIFICATION
        };
        //styles requires 0+ arguments of type MessageBoxStyles
        template<typename... Styles>
        MessageBoxResponse CreateMessageBox(std::wstring_view text, std::wstring_view title, HWND parent = nullptr, Styles... styles)
            requires (std::convertible_to<Styles, MessageBoxStyles> && ...) {
            UINT mbstyles = (0 | ... | std::to_underlying(styles));
            //no styles given, use a sensible default
            if (mbstyles == 0) {
                mbstyles = MB_OK | MB_ICONINFORMATION | MB_DEFBUTTON1 | MB_APPLMODAL;
            }
            //thread_local: the CBT hook is installed for the calling thread (GetCurrentThreadId), so the
            //handle must be per-thread too, otherwise concurrent message boxes on different threads race.
            thread_local HookHandle mbhook;
            auto hookcallback = [](int nCode, WPARAM wParam, LPARAM lParam)->LRESULT {
                if (nCode < 0) {
                    return CallNextHookEx(mbhook.get(), nCode, wParam, lParam);
                }
                if (nCode == HCBT_CREATEWND) {
                    auto mbcs = std::bit_cast<CBT_CREATEWND*>(lParam);
                    if (!(mbcs->lpcs->lpszClass == MAKEINTATOM(DIALOGATOM))) {
                        return CallNextHookEx(mbhook.get(), nCode, wParam, lParam);
                    }
                    RECT winrect;
                    auto ck = GetWindowRect(mbcs->lpcs->hwndParent, &winrect);
                    if (ck == 0) {
                        return 1;
                    }
                    mbcs->lpcs->x = static_cast<int>(winrect.left) +
                        ((winrect.right - winrect.left) - mbcs->lpcs->cx) / 2;
                    mbcs->lpcs->y = static_cast<int>(winrect.top) +
                        ((winrect.bottom - winrect.top) - mbcs->lpcs->cy) / 2;
                    return 0;
                }
                else {
                    return CallNextHookEx(mbhook.get(), nCode, wParam, lParam);
                }
                };
            if (parent) {
                mbhook.reset(SetWindowsHookEx(std::to_underlying(HookTypes::CBT), hookcallback, nullptr, GetCurrentThreadId()));
            }
            auto check = MessageBox(parent, text.data(), title.data(), mbstyles);
            mbhook.reset();
            if (check == 0) {
                return MessageBoxResponse::Error;
            }
            else {
                return static_cast<MessageBoxResponse>(check);
            }
        }
        //Task Dialog
        namespace TaskDialog
        {
            enum class Flags : DWORD
            {
                Default = 0,
                EnableHyperlinks = TDF_ENABLE_HYPERLINKS,
                HiconMain = TDF_USE_HICON_MAIN,
                HiconFooter = TDF_USE_HICON_FOOTER,
                AllowDialogCancellation = TDF_ALLOW_DIALOG_CANCELLATION,
                CommandLinks = TDF_USE_COMMAND_LINKS,
                CommandLinksNoIcon = TDF_USE_COMMAND_LINKS_NO_ICON,
                ExpandFooterArea = TDF_EXPAND_FOOTER_AREA,
                ExpandedByDefault = TDF_EXPANDED_BY_DEFAULT,
                VerificationFlagChecked = TDF_VERIFICATION_FLAG_CHECKED,
                ShowProgressBar = TDF_SHOW_PROGRESS_BAR,
                ShowMarqueeProgressBar = TDF_SHOW_MARQUEE_PROGRESS_BAR,
                CallbackTimer = TDF_CALLBACK_TIMER,
                PositionRelativeToWindow = TDF_POSITION_RELATIVE_TO_WINDOW,
                RTLLayout = TDF_RTL_LAYOUT,
                NoDefaultRadioButton = TDF_NO_DEFAULT_RADIO_BUTTON,
                AllowMinimize = TDF_CAN_BE_MINIMIZED,
                SizeToContent = TDF_SIZE_TO_CONTENT
            };
            enum class CommonButtons : DWORD
            {
                None = 0,
                Ok = TDCBF_OK_BUTTON,
                Yes = TDCBF_YES_BUTTON,
                No = TDCBF_NO_BUTTON,
                Cancel = TDCBF_CANCEL_BUTTON,
                Close = TDCBF_CLOSE_BUTTON,
                Retry = TDCBF_RETRY_BUTTON
            };
        }
    }
    //Task dialog internals: module linkage, so the exported templates below can use these while an importer
    //cannot name them. Not an anonymous namespace - internal linkage would make them TU-local, and a TU-local
    //entity cannot be referenced by the definition of an exported template.
    namespace TaskDialog
    {
        //enable_bitmask helper
        template<class E> struct enable_bitmask : std::false_type {};
        template<> struct enable_bitmask<Flags> : std::true_type {};
        template<> struct enable_bitmask<CommonButtons> : std::true_type {};
        //Common buttons go in as a bitmask but come back as a plain control id, so Create() needs both ways.
        constexpr int CommonButtonToId(CommonButtons b) noexcept
        {
            switch (b) {
            case CommonButtons::Ok:     return IDOK;
            case CommonButtons::Yes:    return IDYES;
            case CommonButtons::No:     return IDNO;
            case CommonButtons::Cancel: return IDCANCEL;
            case CommonButtons::Close:  return IDCLOSE;
            case CommonButtons::Retry:  return IDRETRY;
            default:                    return 0;
            }
        }
        constexpr std::optional<CommonButtons> IdToCommonButton(int id) noexcept
        {
            switch (id) {
            case IDOK:     return CommonButtons::Ok;
            case IDYES:    return CommonButtons::Yes;
            case IDNO:     return CommonButtons::No;
            case IDCANCEL: return CommonButtons::Cancel;
            case IDCLOSE:  return CommonButtons::Close;
            case IDRETRY:  return CommonButtons::Retry;
            default:       return std::nullopt;
            }
        }
    }//end namespace TaskDialog internal
    export {
        namespace TaskDialog //start namespace task dialog export
        {
            //exported: callers write CommonButtons::Yes | CommonButtons::Cancel for themselves
            template<class E> requires enable_bitmask<E>::value
            constexpr E operator|(E lhs, E rhs) noexcept
            {
                return static_cast<E>(std::to_underlying(lhs) | std::to_underlying(rhs));
            }
            template<class E> requires enable_bitmask<E>::value
            constexpr E& operator|=(E& lhs, E rhs) noexcept { return lhs = lhs | rhs; }

            enum class NoCustomButtons : int {};
            //An enum of control ids for custom buttons or radio buttons. CommonButtons and Flags are excluded:
            //they are TDCBF_/TDF_ bitmasks rather than ids, and CommonButtons in particular would make
            //std::variant<CommonButtons, T> degenerate. Values must be >= 100 to stay clear of IDOK..IDCONTINUE,
            //which is on the caller to honor.
            template<class T>
            concept ControlIds = std::is_scoped_enum_v<T> && !std::same_as<T, CommonButtons> && !std::same_as<T, Flags>;
            //TD_*_ICON are MAKEINTRESOURCEW values (pointers), so only the numeric part is kept; Create()
            //rebuilds the pointer with MAKEINTRESOURCEW.
            enum class StandardIcon : WORD
            {
                None = 0,
                Warning = static_cast<WORD>(-1),
                Error = static_cast<WORD>(-2),
                Information = static_cast<WORD>(-3),
                Shield = static_cast<WORD>(-4)
            };
            template<ControlIds TaskDialogButtons, ControlIds TaskDialogRadios = NoCustomButtons>
            struct TaskDialogResponse
            {
                HRESULT res=E_FAIL;
                std::optional<std::variant<CommonButtons,TaskDialogButtons>> button;
                //no common radio buttons exist, so this is never a CommonButtons
                std::optional<TaskDialogRadios> radio;
                std::optional<bool> verification;
                //true when the dialog ran; says nothing about which button was pressed
                explicit operator bool() const noexcept { return SUCCEEDED(res); }
            };
            template <class Id>
            struct TaskButtonData
            {
                Id id;
                std::wstring text;
            };
            struct ExpandedInfo
            {
                std::wstring info;
                std::optional<std::wstring> expanded_label;
                std::optional<std::wstring> collapsed_label;
            };
            //Text that can be replaced while the dialog is up. An element that was left empty at creation does
            //not exist and cannot be filled in later - the dialog only reserves space for what it was given.
            enum class TextElement : int
            {
                Content = TDE_CONTENT,
                ExpandedInformation = TDE_EXPANDED_INFORMATION,
                Footer = TDE_FOOTER,
                MainInstruction = TDE_MAIN_INSTRUCTION
            };
            enum class IconElement : int
            {
                Main = TDIE_ICON_MAIN,
                Footer = TDIE_ICON_FOOTER
            };
            enum class ProgressState : int
            {
                Normal = PBST_NORMAL,
                Error = PBST_ERROR,
                Paused = PBST_PAUSED
            };
            enum class ButtonAction { Close, KeepOpen };
            //Whether the elapsed time keeps counting up or starts over.
            enum class TimerAction { Continue, ResetTickCount };
            //Non-owning view of a running task dialog, handed to every callback.
            template<ControlIds TaskDialogButtons, ControlIds TaskDialogRadios = NoCustomButtons>
            class TaskDialogView
            {
            public:
                explicit TaskDialogView(HWND dialog) noexcept : hwnd(dialog) {}
                HWND Handle() const noexcept { return hwnd; }
                void ClickButton(TaskDialogButtons id) const noexcept { Send(TDM_CLICK_BUTTON, IdOf(id)); }
                void ClickButton(CommonButtons id) const noexcept { Send(TDM_CLICK_BUTTON, CommonButtonToId(id)); }
                void Close() const noexcept { Send(TDM_CLICK_BUTTON, IDCANCEL); }
                void EnableButton(TaskDialogButtons id, bool enabled) const noexcept { Send(TDM_ENABLE_BUTTON, IdOf(id), enabled); }
                void EnableButton(CommonButtons id, bool enabled) const noexcept { Send(TDM_ENABLE_BUTTON, CommonButtonToId(id), enabled); }
                //Puts the UAC shield on the button; the caller still has to do the elevating.
                void SetButtonElevationRequired(TaskDialogButtons id, bool required) const noexcept
                {
                    Send(TDM_SET_BUTTON_ELEVATION_REQUIRED_STATE, IdOf(id), required);
                }
                void ClickRadioButton(TaskDialogRadios id) const noexcept { Send(TDM_CLICK_RADIO_BUTTON, IdOf(id)); }
                void EnableRadioButton(TaskDialogRadios id, bool enabled) const noexcept { Send(TDM_ENABLE_RADIO_BUTTON, IdOf(id), enabled); }
                void ClickVerification(bool checked, bool set_focus = false) const noexcept
                {
                    Send(TDM_CLICK_VERIFICATION, checked, set_focus);
                }
                void SetText(TextElement element, const std::wstring& text, bool resize = true) const noexcept
                {
                    Send(resize ? TDM_SET_ELEMENT_TEXT : TDM_UPDATE_ELEMENT_TEXT,
                        static_cast<WPARAM>(std::to_underlying(element)), reinterpret_cast<LPARAM>(text.c_str()));
                }
                //An icon can only be swapped for one of its own kind
                void SetIcon(IconElement element, StandardIcon icon) const noexcept
                {
                    Send(TDM_UPDATE_ICON, static_cast<WPARAM>(std::to_underlying(element)),
                        reinterpret_cast<LPARAM>(MAKEINTRESOURCEW(std::to_underlying(icon))));
                }
                void SetIcon(IconElement element, HICON icon) const noexcept
                {
                    Send(TDM_UPDATE_ICON, static_cast<WPARAM>(std::to_underlying(element)), reinterpret_cast<LPARAM>(icon));
                }
                //Switches an existing progress bar between the two styles
                void SetMarqueeProgressBar(bool marquee) const noexcept { Send(TDM_SET_MARQUEE_PROGRESS_BAR, marquee); }
                //Starts or stops the marquee animation, sweeping once per interval.
                void SetMarqueeAnimation(bool running, std::chrono::milliseconds interval = std::chrono::milliseconds{ 0 }) const noexcept
                {
                    Send(TDM_SET_PROGRESS_BAR_MARQUEE, running, static_cast<LPARAM>(interval.count()));
                }
                void SetProgressBarState(ProgressState state) const noexcept
                {
                    Send(TDM_SET_PROGRESS_BAR_STATE, static_cast<WPARAM>(std::to_underlying(state)));
                }
                //The range is a pair of WORDs, so both ends are capped at 65535.
                void SetProgressBarRange(WORD min, WORD max) const noexcept
                {
                    Send(TDM_SET_PROGRESS_BAR_RANGE, 0, MAKELPARAM(min, max));
                }
                //returns the position the bar was at before this call
                int SetProgressBarPos(int pos) const noexcept
                {
                    return static_cast<int>(SendQuery(TDM_SET_PROGRESS_BAR_POS, static_cast<WPARAM>(pos)));
                }
                //Escape hatch for the TDM_ messages with no wrapper above, the counterpart to Window::SendWinMsg.
                //post: if true, use PostMessage, if false use SendMessage
                long long SendTaskDlgMsg(UINT msg, WPARAM wParam, LPARAM lParam, bool post = false) const noexcept
                {
                    if (!post) { return SendQuery(msg, wParam, lParam); }
                    return PostMessageW(hwnd, msg, wParam, lParam);
                }
            private:
                static constexpr WPARAM IdOf(auto id) noexcept { return static_cast<WPARAM>(std::to_underlying(id)); }
                //TDM_ messages that answer with something worth reading go through SendQuery; the rest return
                //an undocumented value that would only be dropped anyway.
                void Send(UINT msg, WPARAM wparam = 0, LPARAM lparam = 0) const noexcept
                {
                    SendMessageW(hwnd, msg, wparam, lparam);
                }
                LRESULT SendQuery(UINT msg, WPARAM wparam = 0, LPARAM lparam = 0) const noexcept
                {
                    return SendMessageW(hwnd, msg, wparam, lparam);
                }
                HWND hwnd;
            };
            //Handlers for the TDN_* notifications, all optional. They run on the thread that called Create(),
            //nested inside it
            template<ControlIds TaskDialogButtons, ControlIds TaskDialogRadios = NoCustomButtons>
            struct TaskDialogCallbacks
            {
                using View = TaskDialogView<TaskDialogButtons, TaskDialogRadios>;
                //If for some application, a raw callback is needed. Return nullopt to fall through,
                //an HRESULT after handling. Unlike WNDPROC, this callback can only receive the TDN_ messages
                std::function<std::optional<HRESULT>(const View&, UINT, WPARAM, LPARAM)> on_notification;
                //the dialog exists but is not yet visible
                std::function<void(const View&)> on_created;
                //raised once per page
                std::function<void(const View&)> on_dialog_constructed;
                std::function<void(const View&)> on_navigated;
                //Returning KeepOpen swallows the click and leaves the dialog up.
                std::function<ButtonAction(const View&, std::variant<CommonButtons, TaskDialogButtons>)> on_button_clicked;
                std::function<void(const View&, TaskDialogRadios)> on_radio_clicked;
                //the new checked state, not the old one
                std::function<void(const View&, bool)> on_verification_clicked;
                //true when the expando was just opened
                std::function<void(const View&, bool)> on_expando_clicked;
                //The href of the clicked <A> tag; navigating is up to the handler. 
                // Setting this turns on hyperlink parsing for the whole dialog.
                std::function<void(const View&, std::wstring_view)> on_hyperlink_clicked;
                //Roughly every 200ms, with the time elapsed since the dialog was created or since the last
                //ResetTickCount. Setting this turns the timer on.
                std::function<TimerAction(const View&, std::chrono::milliseconds)> on_timer;
                //F1, or the help button of a dialog that has one
                std::function<void(const View&)> on_help;
                //the last notification, and the only one where the window is already beyond saving
                std::function<void(const View&)> on_destroyed;
                bool HasHandlers() const noexcept
                {
                    return on_notification || on_created || on_dialog_constructed || on_navigated || on_button_clicked
                        || on_radio_clicked || on_verification_clicked || on_expando_clicked
                        || on_hyperlink_clicked || on_timer || on_help || on_destroyed;
                }
            };
            template<ControlIds TaskDialogButtons, ControlIds TaskDialogRadios = NoCustomButtons>
            struct TaskDialogConfig
            {
                HINSTANCE inst = nullptr;   //only needed for MAKEINTRESOURCE strings/icons
                HWND parent = nullptr;
                Flags task_flags = Flags::Default;
                CommonButtons common_buttons = CommonButtons::None;
                std::wstring title;
                std::wstring main_instruction;
                std::wstring main_content;
                std::vector<TaskButtonData<TaskDialogButtons>> buttons;
                std::vector<TaskButtonData<TaskDialogRadios>> radios;
                std::optional<std::variant<StandardIcon, HICON>> main_icon;
                std::optional<ExpandedInfo> expanded_info;
                std::optional<std::wstring> footer_text;
                std::optional<std::wstring> verification_text;
                //a CommonButtons default only takes effect if that button is also in common_buttons
                std::optional<std::variant<CommonButtons, TaskDialogButtons>> default_button;
                std::optional<TaskDialogRadios> default_radio;
                //empty by default; setting on_timer or on_hyperlink_clicked also sets the flag each one needs
                TaskDialogCallbacks<TaskDialogButtons, TaskDialogRadios> callbacks;
            };
            //Returns E_INVALIDARG for a config whose response would be ambiguous, or whose default
            //names a control that was never added (which the dialog ignores silently).
            template<ControlIds TaskDialogButtons, ControlIds TaskDialogRadios>
            HRESULT ValidateTaskDialogConfig(const TaskDialogConfig<TaskDialogButtons, TaskDialogRadios>& cfg) noexcept
            {
                auto id_of = [](const auto& item) { return static_cast<int>(std::to_underlying(item.id)); };
                auto has_duplicate_ids = [&id_of](const auto& items) {
                    for (std::size_t i = 0; i < items.size(); ++i) {
                        for (std::size_t j = i + 1; j < items.size(); ++j) {
                            if (id_of(items[i]) == id_of(items[j])) return true;
                        }
                    }
                    return false;
                    };
                if (has_duplicate_ids(cfg.buttons) || has_duplicate_ids(cfg.radios)) return E_INVALIDARG;

                constexpr std::array common_set{ CommonButtons::Ok, CommonButtons::Yes, CommonButtons::No,
                    CommonButtons::Cancel, CommonButtons::Close, CommonButtons::Retry };
                const auto requested = std::to_underlying(cfg.common_buttons);
                for (const auto& b : cfg.buttons) {
                    const int id = id_of(b);
                    //IDCANCEL is reserved whatever common_buttons says: Esc and the X report it regardless.
                    if (id == IDCANCEL) return E_INVALIDARG;
                    for (auto cb : common_set) {
                        if ((requested & std::to_underlying(cb)) != 0 && id == CommonButtonToId(cb)) return E_INVALIDARG;
                    }
                }
                if (cfg.default_button) {
                    if (const auto* custom = std::get_if<TaskDialogButtons>(&*cfg.default_button)) {
                        if (std::ranges::none_of(cfg.buttons, [custom](const auto& b) { return b.id == *custom; })) {
                            return E_INVALIDARG;
                        }
                    }
                    else if ((requested & std::to_underlying(std::get<CommonButtons>(*cfg.default_button))) == 0) {
                        return E_INVALIDARG;
                    }
                }
                if (cfg.default_radio &&
                    std::ranges::none_of(cfg.radios, [&cfg](const auto& r) { return r.id == *cfg.default_radio; })) {
                    return E_INVALIDARG;
                }
                return S_OK;
            }
            template<ControlIds TaskDialogButtons, ControlIds TaskDialogRadios = NoCustomButtons>
            class TaskDialog
            {
            public:
                explicit TaskDialog(TaskDialogConfig<TaskDialogButtons, TaskDialogRadios> config) : cfg(std::move(config)) {}
                //blocking and modal. validate_config disables validation. Intended to be used if you validate the config before calling Create
                TaskDialogResponse<TaskDialogButtons, TaskDialogRadios> Create(bool validate_config = true) const
                {
                    TaskDialogResponse<TaskDialogButtons, TaskDialogRadios> res;
                    if (validate_config)
                    {
                        if (const HRESULT valid = ValidateTaskDialogConfig(cfg); FAILED(valid))
                        {
                            res.res = valid;
                            return res;
                        }
                    }
                    //TASKDIALOGCONFIG borrows every pointer and owns nothing, so these arrays have to outlive
                    //TaskDialogIndirect: function scope, not the scope of the block that fills them.
                    std::vector<TASKDIALOG_BUTTON> buttons;
                    buttons.reserve(cfg.buttons.size());
                    for (const auto& button : cfg.buttons)
                    {
                        buttons.push_back({ static_cast<int>(std::to_underlying(button.id)), button.text.c_str() });
                    }
                    std::vector<TASKDIALOG_BUTTON> radios;
                    radios.reserve(cfg.radios.size());
                    for (const auto& radio : cfg.radios)
                    {
                        radios.push_back({ static_cast<int>(std::to_underlying(radio.id)), radio.text.c_str() });
                    }
                    //An empty string is not the same as no string: the dialog only substitutes its own default
                    //(the exe name, for the title) when the pointer is null.
                    auto or_null = [](const std::wstring& s) -> PCWSTR { return s.empty() ? nullptr : s.c_str(); };

                    TASKDIALOGCONFIG task_config {
                        .cbSize = sizeof(TASKDIALOGCONFIG),
                        .hwndParent = cfg.parent,
                        .hInstance = cfg.inst,
                        //TASKDIALOG_FLAGS and TASKDIALOG_COMMON_BUTTON_FLAGS are both int, so the DWORD from
                        //to_underlying narrows; inside a braced initializer that is an error, not a warning.
                        .dwFlags = static_cast<TASKDIALOG_FLAGS>(std::to_underlying(cfg.task_flags)),
                        .dwCommonButtons = static_cast<TASKDIALOG_COMMON_BUTTON_FLAGS>(std::to_underlying(cfg.common_buttons)),
                        .pszWindowTitle = or_null(cfg.title),
                        .pszMainInstruction = or_null(cfg.main_instruction),
                        .pszContent = or_null(cfg.main_content),
                        .cButtons = static_cast<UINT>(buttons.size()),
                        .pButtons = buttons.empty() ? nullptr : buttons.data(),
                        .cRadioButtons = static_cast<UINT>(radios.size()),
                        .pRadioButtons = radios.empty() ? nullptr : radios.data(),
                    };
                    //get_if tests the active alternative - get would throw bad_variant_access on the other one.
                    if (cfg.main_icon)
                    {
                        if (const auto* hicon = std::get_if<HICON>(&cfg.main_icon.value()))
                        {
                            task_config.hMainIcon = *hicon;
                            task_config.dwFlags |= TDF_USE_HICON_MAIN;
                        }
                        else if (const auto icon = std::get<StandardIcon>(cfg.main_icon.value()); icon != StandardIcon::None)
                        {
                            task_config.pszMainIcon = MAKEINTRESOURCEW(std::to_underlying(icon));
                            task_config.dwFlags &= ~TDF_USE_HICON_MAIN;
                        }
                    }
                    //Leaving nDefaultButton at 0 selects the first button, so an absent default is not an error.
                    if (cfg.default_button)
                    {
                        task_config.nDefaultButton = std::visit([](auto id) -> int {
                            if constexpr (std::same_as<decltype(id), CommonButtons>) return CommonButtonToId(id);
                            else return static_cast<int>(std::to_underlying(id));
                            }, cfg.default_button.value());
                    }
                    if (cfg.default_radio)
                    {
                        task_config.nDefaultRadioButton = static_cast<int>(std::to_underlying(cfg.default_radio.value()));
                    }
                    if (cfg.expanded_info)
                    {
                        task_config.pszExpandedInformation = or_null(cfg.expanded_info->info);
                        if (cfg.expanded_info->expanded_label)
                        {
                            task_config.pszExpandedControlText = cfg.expanded_info->expanded_label->c_str();
                        }
                        if (cfg.expanded_info->collapsed_label)
                        {
                            task_config.pszCollapsedControlText = cfg.expanded_info->collapsed_label->c_str();
                        }
                    }
                    if (cfg.footer_text)
                    {
                        task_config.pszFooter = cfg.footer_text->c_str();
                    }
                    if (cfg.verification_text)
                    {
                        task_config.pszVerificationText = cfg.verification_text->c_str();
                    }
                    //Like the arrays above, this has to outlive TaskDialogIndirect: the dialog holds the pointer
                    //for as long as it is up and hands it back to every callback.
                    CallbackState state{ .callbacks = &cfg.callbacks };
                    if (cfg.callbacks.HasHandlers())
                    {
                        task_config.pfCallback = &CallbackProc;
                        task_config.lpCallbackData = reinterpret_cast<LONG_PTR>(&state);
                        //Two notifications only ever arrive if their flag is on, so asking for the handler is
                        //taken as asking for the flag.
                        if (cfg.callbacks.on_timer) task_config.dwFlags |= TDF_CALLBACK_TIMER;
                        if (cfg.callbacks.on_hyperlink_clicked) task_config.dwFlags |= TDF_ENABLE_HYPERLINKS;
                    }

                    int pressed = 0;
                    int radio_id = 0;
                    BOOL verified = FALSE;
                    //Null out-params for controls that were never configured, which keeps the optionals below honest.
                    res.res = TaskDialogIndirect(&task_config, &pressed,
                        radios.empty() ? nullptr : &radio_id,
                        cfg.verification_text ? &verified : nullptr);
                    //A handler that threw took the dialog down with it, so whatever the out-params say is the
                    //result of that teardown rather than a real answer.
                    if (state.failure)
                    {
                        std::rethrow_exception(state.failure);
                    }
                    if (FAILED(res.res))
                    {
                        return res;
                    }
                    //Esc and the X report IDCANCEL even when no Cancel button was configured, so a Cancel here
                    //does not imply the caller asked for one.
                    if (const auto common = IdToCommonButton(pressed))
                    {
                        res.button = common.value();
                    }
                    else
                    {
                        res.button = static_cast<TaskDialogButtons>(pressed);
                    }
                    if (!radios.empty())
                    {
                        res.radio = static_cast<TaskDialogRadios>(radio_id);
                    }
                    if (cfg.verification_text)
                    {
                        res.verification = (verified != FALSE);
                    }
                    return res;
                }
                TaskDialogConfig<TaskDialogButtons, TaskDialogRadios>& Configuration() noexcept { return cfg; }
                const TaskDialogConfig<TaskDialogButtons, TaskDialogRadios>& Configuration() const noexcept { return cfg; }
            private:
                //What the dialog carries in lpCallbackData for us: the handlers to dispatch to, and a place to
                //park an exception that cannot be thrown where it happened.
                struct CallbackState
                {
                    const TaskDialogCallbacks<TaskDialogButtons, TaskDialogRadios>* callbacks = nullptr;
                    std::exception_ptr failure;
                };
                //Throwing across TaskDialogIndirect would unwind straight through comctl32, so a handler that
                //throws instead ends the dialog here and its exception comes back out of Create().
                static HRESULT CALLBACK CallbackProc(HWND hwnd, UINT notification, WPARAM wparam, LPARAM lparam,
                    LONG_PTR ref_data) noexcept
                {
                    auto& state = *reinterpret_cast<CallbackState*>(ref_data);
                    //Notifications keep coming while the dialog tears itself down; none of them may run a
                    //handler again or overwrite the exception already on its way out.
                    if (state.failure) { return S_OK; }
                    const auto& callbacks = *state.callbacks;
                    const TaskDialogView<TaskDialogButtons, TaskDialogRadios> window{ hwnd };
                    try
                    {
                        if (callbacks.on_notification)
                        {
                            if (const auto handled = callbacks.on_notification(window, notification, wparam, lparam))
                            {
                                return handled.value();
                            }
                        }
                        switch (notification)
                        {
                        case TDN_CREATED:
                            if (callbacks.on_created) { callbacks.on_created(window); }
                            break;
                        case TDN_DIALOG_CONSTRUCTED:
                            if (callbacks.on_dialog_constructed) { callbacks.on_dialog_constructed(window); }
                            break;
                        case TDN_NAVIGATED:
                            if (callbacks.on_navigated) { callbacks.on_navigated(window); }
                            break;
                        case TDN_BUTTON_CLICKED:
                            if (callbacks.on_button_clicked)
                            {
                                const int id = static_cast<int>(wparam);
                                //Same split as the response: Esc and the X come through as IDCANCEL whether or
                                //not a Cancel button exists.
                                std::variant<CommonButtons, TaskDialogButtons> button{ static_cast<TaskDialogButtons>(id) };
                                if (const auto common = IdToCommonButton(id)) { button = common.value(); }
                                //S_FALSE is what tells the dialog to stay up.
                                if (callbacks.on_button_clicked(window, button) == ButtonAction::KeepOpen)
                                {
                                    return S_FALSE;
                                }
                            }
                            break;
                        case TDN_RADIO_BUTTON_CLICKED:
                            if (callbacks.on_radio_clicked)
                            {
                                callbacks.on_radio_clicked(window, static_cast<TaskDialogRadios>(static_cast<int>(wparam)));
                            }
                            break;
                        case TDN_VERIFICATION_CLICKED:
                            if (callbacks.on_verification_clicked) { callbacks.on_verification_clicked(window, wparam != 0); }
                            break;
                        case TDN_EXPANDO_BUTTON_CLICKED:
                            if (callbacks.on_expando_clicked) { callbacks.on_expando_clicked(window, wparam != 0); }
                            break;
                        case TDN_HYPERLINK_CLICKED:
                            if (callbacks.on_hyperlink_clicked)
                            {
                                const auto* href = reinterpret_cast<PCWSTR>(lparam);
                                callbacks.on_hyperlink_clicked(window, href ? std::wstring_view{ href } : std::wstring_view{});
                            }
                            break;
                        case TDN_TIMER:
                            //wparam counts milliseconds; S_FALSE puts it back to zero for the next tick.
                            if (callbacks.on_timer &&
                                callbacks.on_timer(window, std::chrono::milliseconds{ static_cast<DWORD>(wparam) })
                                    == TimerAction::ResetTickCount)
                            {
                                return S_FALSE;
                            }
                            break;
                        case TDN_HELP:
                            if (callbacks.on_help) { callbacks.on_help(window); }
                            break;
                        case TDN_DESTROYED:
                            if (callbacks.on_destroyed) { callbacks.on_destroyed(window); }
                            break;
                        default:
                            break;
                        }
                    }
                    catch (...)
                    {
                        state.failure = std::current_exception();
                        //IDCANCEL closes the dialog even without a Cancel button. Not from TDN_DESTROYED,
                        //where the window is already going away and the click would have nowhere to land.
                        if (notification != TDN_DESTROYED)
                        {
                            SendMessageW(hwnd, TDM_CLICK_BUTTON, IDCANCEL, 0);
                        }
                    }
                    return S_OK;
                }
                TaskDialogConfig<TaskDialogButtons, TaskDialogRadios> cfg;
            };
        }//end namespace TaskDialog export portion
        namespace literals {
            WPARAM operator"" _wp(WPARAM w) { return w; }
            LPARAM operator"" _lp(unsigned long long l) { return static_cast<LPARAM>(l); }
        }
        //on Win64, LRESULT and INT_PTR are both __int64 (long long), so this can be used for dialog procs as well
        using WndProcFunctor = std::function <LRESULT(HWND, UINT, WPARAM, LPARAM)>;
        struct WindowSize {
            int width;
            int height;
            WindowSize() :width(CW_USEDEFAULT), height(CW_USEDEFAULT) {}
            WindowSize(int w, int h) :width(w), height(h) {}
        };
        struct WindowStyles {
            DWORD standard_styles = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
            DWORD extended_styles = NO_STYLES;
        };

        //returns reference to the Window class associated with the HWND. Otherwise returns false.
        std::reference_wrapper<Window> GetWindowFromHWND(HWND window_handle) {
            std::lock_guard<std::mutex> wr_lock(winreg_mutex);
            auto& winref = *(windowreg.at(window_handle));
            return winref;
        }
        const std::string windows_reserved_names = "COM,CON,PRN,AUX,NUL,COM1,COM2,COM3,COM4,COM5,COM6,"
            "COM7,COM8,COM9,LPT,LPT1,LPT2,LPT3,LPT4,LPT5,LPT6,LPT7,LPT8,LPT9";
        class WindowLogger {
        public:
            WindowLogger() = default;
            WindowLogger(std::filesystem::path filepath) {
                logfile.open(filepath, std::ios::app);
            }
            WindowLogger(const WindowLogger&) = delete;
            WindowLogger& operator=(const WindowLogger&) = delete;
            WindowLogger(WindowLogger&& other) :logfile(std::move(other.logfile)) {}
            WindowLogger& operator=(WindowLogger&& other) {
                if (this == &other) return *this;
                logfile = std::move(other.logfile);
                return *this;
            }
            ~WindowLogger() {
                if (logfile) {
                    logfile.close();
                }
            }
            void open(const std::filesystem::path filepath) {
                if (logfile) {
                    logfile.close();
                }
                logfile.open(filepath, std::ios::app);
            }
            void close() {
                if (logfile) {
                    logfile.close();
                }
            }
            void write(std::wstring text) {
                logfile << text << '\n';
            }
            void write_win32_error(std::wstring prepend_text) {
                const DWORD err = GetLastError(); //capture before any other call can overwrite it
                std::array<wchar_t, BUFFSIZE> errstr{};
                const DWORD n = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    nullptr, err, 0, errstr.data(), BUFFSIZE, nullptr);
                std::wstring msg = n ? std::wstring(errstr.data(), n) : L"(no description)";
                while (!msg.empty() && (msg.back() == L'\r' || msg.back() == L'\n')) msg.pop_back();
                if (logfile) {
                    logfile << L"Windows API Error: " << prepend_text << L" [" << err << L"] " << msg << L'\n';
                }
            }
            std::wofstream& operator()() {
                return logfile;
            }
        private:
            std::wofstream logfile;
            static constexpr DWORD BUFFSIZE = 1024;
        };
        using OptionalWindowLogger = std::optional<WindowLogger>;
        //list of controls that can be initalized with CommonControl class (done by CreateWindow. Others are commented out for later implementation
        enum class ControlNames {
            Animation,
            Button,
            ComboBox,
            ComboBoxEx,
            DateTime,
            Edit,
            //            FlatScroll,
            Header,
            //            HotKey,
            //            ImageLists,
            IP,
            ListBox,
            ListView,
            MonthCalendar,
            Pager,
            ProgBar,
            //            PropSheet,
            Rebar,
            RichEdit,
            ScrollBar,
            Static,
            StatusBar,
            SysLink,//text is the text of the link
            Tab, //parent window must have WS_CLIPSIBILINGS style
            Toolbar,//creates empty toolbar, see documentation for messages to send to add buttons
            //            Tooltip,
            Trackbar,
            TreeView,
            UpDown,
        };
        struct CommonControlParams {
            ControlNames classname;
            std::wstring text;
            WORD id;
            int posX;
            int posY;
            WindowSize sz;
        };
        class CommonControl;

        typedef STARTUPINFO WinStartupinfo;
        typedef STARTUPINFOEX WinStartupEx;
        template<typename T>
        concept Startupinfo = std::is_same_v<T, STARTUPINFO> || std::is_same_v<T, STARTUPINFOEX>;
        template<Startupinfo T = STARTUPINFO>
        struct ProcCreationInfo {
            T st;
            PROCESS_INFORMATION pi;
            ProcCreationInfo() {
                ZeroMemory(&st, sizeof(T));
                ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
                if constexpr (std::is_same_v<T, STARTUPINFOEX>) {
                    st.StartupInfo.cb = sizeof(STARTUPINFOEX);
                }
                else {
                    st.cb = sizeof(STARTUPINFO);
                }
            }
            ~ProcCreationInfo() {
                //Only the process/thread handles returned by CreateProcess are owned here. STARTUPINFO's
                //std handles (if a caller ever sets them for redirection) belong to whoever created them,
                //so closing them here would be a double-close; leave them alone.
                if (pi.hThread) CloseHandle(pi.hThread);
                if (pi.hProcess) CloseHandle(pi.hProcess);
            }
        };
        enum class ProcCreationFlags : DWORD {
            NewConsoleNormal = CREATE_NEW_CONSOLE | NORMAL_PRIORITY_CLASS,
            NoConsoleNormal = CREATE_NO_WINDOW | NORMAL_PRIORITY_CLASS
        };
        template<Startupinfo T = STARTUPINFO>
        std::error_code CreateChildProcess(std::filesystem::path exepath, std::wstring command_line, ProcCreationInfo<T>& pi, bool return_immediately = false,
            DWORD flags = CREATE_NEW_CONSOLE | NORMAL_PRIORITY_CLASS, bool inherit_handles = false, LPSECURITY_ATTRIBUTES procattr = nullptr,
            LPSECURITY_ATTRIBUTES threadattr = nullptr, void* environment = nullptr, std::wstring startupdir = L"")
        {
            LPCWSTR stdir = nullptr;
            if (!startupdir.empty()) {
                stdir = startupdir.c_str();
            }
            if constexpr (std::is_same_v<T, STARTUPINFOEX>) {
                flags |= EXTENDED_STARTUPINFO_PRESENT;
            }
            auto check = CreateProcess(exepath.c_str(), command_line.data(), procattr, threadattr, inherit_handles, flags, environment, stdir, &pi.st, &pi.pi);
            if (!check) {
                return std::error_code{ static_cast<int>(GetLastError()),std::system_category() };
            }
            if (return_immediately) {
                return std::error_code{};
            }
            else {
                WaitForSingleObject(pi.pi.hProcess, INFINITE);
                DWORD exit_code;
                GetExitCodeProcess(pi.pi.hProcess, &exit_code);
                if (exit_code == 0) {
                    return std::error_code{};
                }
                else {
                    return std::error_code{ static_cast<int>(exit_code),std::generic_category() };
                }
            }
        }
        long long CreateModalDiagBox(HINSTANCE hinst, const wchar_t* template_id, DLGPROC callback = nullptr, HWND parent = nullptr, LPARAM linit = 0) {
            return DialogBoxParam(hinst, template_id, parent, callback, linit);
        }
        long long CreateModalDiagBox(HINSTANCE hinst, const int template_id, DLGPROC callback = nullptr, HWND parent = nullptr, LPARAM linit = 0) {
            return CreateModalDiagBox(hinst, MAKEINTRESOURCE(template_id), callback, parent, linit);
        }
        //for the templates named by a string rather than by an id
        long long CreateModalDiagBox(HINSTANCE hinst, const std::wstring& template_name, DLGPROC callback = nullptr, HWND parent = nullptr, LPARAM linit = 0) {
            return CreateModalDiagBox(hinst, template_name.c_str(), callback, parent, linit);
        }
        //returns default file path for sys config files for the app, optionally append a filename to the end
        //Use GetTempData to get temp folder, not SHGetKnownFolderPath
        std::filesystem::path GetSysConfDefaultFilepath(KnownFolderID folder, bool createdir, std::optional<std::filesystem::path> appdir,
            std::optional<std::wstring> filename_to_append = std::nullopt) {
            //default value if SHGetKnownFolderPath fails
            std::filesystem::path confpath(LR"(C:\ProgramData\)");
            if (folder == KnownFolderID::TemporaryData) {
                confpath = std::filesystem::temp_directory_path();
            }
            else {
                PWSTR buffer;
                auto check = SHGetKnownFolderPath(known_folder_guid.at(folder), KF_FLAG_CREATE, NULL, &buffer);
                std::unique_ptr < WCHAR, decltype(&CoTaskMemFree) > pbuf(buffer, &CoTaskMemFree);
                if (check == S_OK) {
                    std::wstring strbuf(buffer);
                    confpath = std::filesystem::path(strbuf);
                }
            }
            if (appdir) {
                confpath /= appdir.value();
            }
            if (filename_to_append) {
                confpath /= filename_to_append.value();
            }
            if (createdir && !std::filesystem::exists(confpath)) {
                try {
                    std::filesystem::create_directories(confpath);
                }
                catch (std::filesystem::filesystem_error& e) {
                    if (e.code() != std::errc::file_exists) {//give a message box (remove for release version) and propagate the exception if not because file already exists
                        std::ignore = CreateMessageBox(std::format(L"create_directories error. Code: {}", e.code().value()).c_str(),
                            L"GetSysConfDefaultFilepath");
                        throw;
                    }
                    else if (!std::filesystem::is_directory(confpath)) {
                        std::ignore = CreateMessageBox(std::format(L"create_directories error. Code: {}", e.code().value()).c_str(),
                            L"GetSysConfDefaultFilepath");
                        throw;
                    }
                }
            }
            return confpath;
        }
        //wrapper around GetModuleFilename, returing a path
        std::expected<std::filesystem::path, std::error_code> GetModuleFilepath(HMODULE hModule = nullptr) {
            size_t buffersize = MAX_PATH;
            DWORD last_error = ERROR_SUCCESS;
            DWORD res = {};
            bool path_ok = false;
            std::wstring buf;
            while (!path_ok) {
                buf.resize_and_overwrite(buffersize, [&hModule, &res, &last_error](wchar_t* buffer, size_t sz) {
                    res = GetModuleFileNameW(hModule, buffer, static_cast<DWORD>(sz));
                    last_error = GetLastError();
                    return res;
                    });
                if (res == buffersize) {
                    if (last_error == ERROR_INSUFFICIENT_BUFFER) {
                        buffersize *= 2;
                    }
                    else {
                        path_ok = true;
                    }
                }
                else if (res == 0) {
                    std::error_code err{ static_cast<int>(GetLastError()),std::system_category() };
                    return std::unexpected(err);
                }
                else {
                    path_ok = true;
                }
            }
            return std::filesystem::path(buf);
        }
        std::expected<HINSTANCE, std::error_code> GetModule(const wchar_t* modulename = nullptr) {
            auto res = GetModuleHandleW(modulename);
            if (res == NULL) {
                std::error_code err{ static_cast<int>(GetLastError()),std::system_category() };
                return std::unexpected(err);
            }
            else return res;
        }
    }
    //the main_window parameter should only be set for one window, the one that should trigger PostQuitMessage on destruction
    class Window {
    public:
        Window(WNDCLASSEX wc, WndProcFunctor callback, WindowSize sz, std::wstring_view title, bool main_window=false,
            OptionalWindowLogger logfile = std::nullopt, WindowStyles winstyle=WindowStyles())
            :styles(winstyle), create_size(sz), title(title), win(nullptr), additional_callback(callback),
            extra_data(this),log(std::move(logfile)),inst(wc.hInstance),main_window(main_window)
        {
            if (!IsRegistered(wc.lpszClassName)) {
                wc.lpfnWndProc = &Window::WindowProcDispatch;
                auto ret = RegisterClassExW(&wc);
                if (!ret) {
                    throw std::system_error(GetLastError(), std::system_category());
                }
                if (log) {
                    log.value().write(L"Window Class Registered");
                }
                OutputDebugString(std::format(L"RegisterClassEx ATOM: {}, for ClassName: {}\n", ret, wc.lpszClassName).c_str());
            }
            classname = wc.lpszClassName;
        }
        //for already existing window classes
        Window(HINSTANCE hInst,std::wstring className, WndProcFunctor callback, WindowSize sz, std::wstring_view title,
            bool main_window=false, OptionalWindowLogger logfile = std::nullopt, WindowStyles winstyle = WindowStyles())
            :styles(winstyle), create_size(sz), title(title), win(nullptr), additional_callback(callback),
            extra_data(this), log(std::move(logfile)), main_window(main_window) {
            inst = hInst;
            auto optWndclass = IsRegistered(className);
            if (!optWndclass) {
                throw std::runtime_error(std::format("Window Class must be previously registerd to use this Window constructor. Error Code: {}",GetLastError()));
            }
            else {
                classname = className;
            }
        }
        Window(Window&& other) noexcept :inst(other.inst), classname(std::move(other.classname)),create_size(other.create_size), class_style(other.class_style),
        styles(other.styles),log(std::move(other.log)),title(std::move(other.title)), extra_data(std::move(other.extra_data)), win(std::move(other.win)),
        additional_callback(std::move(other.additional_callback)), main_window(other.main_window) {
            //set the handles to zero
            other.inst = nullptr;
            other.extra_data = { nullptr,nullptr,ExtraDataType::NoData };
            //update the pointer in the registry to point to the new "this" pointer
            try {
                std::lock_guard<std::mutex> winreg_guard(winreg_mutex);
                windowreg.at(win.get()) = this;
            }
            //if win is not found (probably because it isn't set yet), don't worry about it
            catch (std::out_of_range&) {}
        }
        Window(const Window&) = delete;
         ~Window() noexcept {
             //remove the hwnd from the window procs to prevent recursion, then destroy these windows before main window
             for (auto& i : associated_windows) {
                 SendMessage(i->GetHWND(), WM_ASSOCIATEDWINDOW, 2, PtrToLP<LPARAM>(nullptr));
             }
             associated_windows.clear();
             win.reset();
        };

        Window& operator=(Window&& other) noexcept {
            if (this == &other) return *this;
            //first the assignments
            inst = other.inst;
            classname = std::move(other.classname);
            create_size = other.create_size;
            class_style = other.class_style;
            styles = other.styles;
            log = std::move(other.log);
            title = std::move(other.title);
            extra_data = std::move(other.extra_data);
            win = std::move(other.win);
            additional_callback = std::move(other.additional_callback);
            main_window = other.main_window;

            //and reset other
            other.inst = nullptr;
            other.extra_data = { nullptr,nullptr,ExtraDataType::NoData };
            //update the pointer in the registry to point to the new "this" pointer
            try {
                std::lock_guard<std::mutex> winreg_guard(winreg_mutex);
                windowreg.at(win.get()) = this;
            }
            //if win is not found (probably because it isn't set yet), don't worry about it
            catch (std::out_of_range&) {}

            return *this;
        }
        Window& operator=(const Window&) = delete;

        //Calls Create if Window not already created, then display. Extra data must be stored in ExtraWindowData::data or it be nullptr
        bool DisplayWindow(HWND parent = nullptr, int windowX = CW_USEDEFAULT, int windowY = CW_USEDEFAULT,
            void* data = nullptr,ExtraDataType dtype = ExtraDataType::NoData) {
            if (!win) {
                if (auto check = Create(parent, windowX, windowY, data,dtype);!check) {
                    if (log) {
                        log.value().write_win32_error(L"DisplayWindow() failure, from CreateWindowEx:::");
                    }
                    return false;
                }
            }
            ShowWindow(win.get(), SW_SHOWNORMAL);
            UpdateWindow(win.get());
            return true;
        }
        //Creates new Window, destroying the current one. If you wish to override positioning or make a child window, do so here.
        //storage of handles and registration of the Window with windowreg map are handled by the WM_NCCREATE message in the static WindowProcDispatch function
        //additional data is data to be stored in the Window in GWLP_USERDATA. Must be a pointer to struct of type ExtraWindowData.
        //winclass member can be any value
        bool Create(HWND parent = nullptr, int windowX = CW_USEDEFAULT, int windowY = CW_USEDEFAULT,
            void* data = nullptr,ExtraDataType dtype=ExtraDataType::NoData) {
            if (data) {
                if (dtype == ExtraDataType::NoData) {
                    throw std::logic_error("Must supply ExtraDataType (dtype param) with data pointer");
                }
                extra_data.data = data;
                extra_data.type = dtype;
            }
            data = &extra_data;
            SetLastError(0);
            auto win_handle = CreateWindowEx(styles.extended_styles, classname.c_str(), title.c_str(), styles.standard_styles, windowX, windowY,
                create_size.width, create_size.height, parent, NULL, inst, data);
            if (!win_handle) {
                auto cde = GetLastError();
                if (log) {
                    log.value().write_win32_error(L"CreateWindowEx Failure:::");
                }
                std::wstring fmsg;
                fmsg.resize_and_overwrite(500, [&cde](wchar_t* buffer, size_t nsize) {
                    return FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                        nullptr,
                        cde,
                        0,
                        buffer,
                        static_cast<DWORD>(nsize),
                        nullptr);
                    });
                //trim \r
                if (auto fmpos = fmsg.find_last_of(L"\r\n"); fmpos != std::wstring::npos)
                {
                    fmsg.erase(fmpos, 2);
                }
                if (fmsg.empty())
                {
                    fmsg = L"FormatMessage failed in debug string creation";
                }
                OutputDebugString(std::format(L"CreateWindowExFailure. GetLastError: {} (Code: {})\n",
                    fmsg, cde).c_str());
                return false;
            }
            return true;
        }
        //Will destroy the window and substitute a new class for a subsequent call to DisplayWindow. Returns old Window Class. Invalidates all pointers obtained through operator()
        //If desired can resize window, otherwise pass an empty optional, same for a title change
        WNDCLASSEX AlterWindow(WNDCLASSEX wc, bool unregister_old_class=false, std::optional<WindowSize> new_size=std::nullopt, 
            std::optional<std::wstring_view> new_title=std::nullopt,std::optional<DWORD> new_window_style=std::nullopt,
            std::optional<DWORD> new_ex_style=std::nullopt, WndProcFunctor extra_callback = WndProcFunctor()) {
            win.reset();
            WNDCLASSEX wcold = IsRegistered(classname).value();
            if (unregister_old_class) {
                UnregisterClass(classname.c_str(), inst);
            }
            wc.lpfnWndProc = &Window::WindowProcDispatch;
            auto ret = RegisterClassEx(&wc);
            if (!ret) {
                throw std::system_error(GetLastError(), std::system_category());
            }
            classname = wc.lpszClassName;
            if (new_size)
                create_size = new_size.value();
            if (new_title)
                title = new_title.value();
            if (new_window_style)
                styles.standard_styles = new_window_style.value();
            if (new_ex_style)
                styles.extended_styles = new_ex_style.value();
            if (extra_callback)
                additional_callback = extra_callback;
            if (log) {
                log.value().write(L"AlterWindow() used, old window destroyed, new class registered");
            }
            return wcold;
        }
        //associate windows to this window (guaranteed that the last reference will not go out of scope until this window class is destroyed
        void AssociateWindow(std::shared_ptr<Window> winptr) {
            using namespace literals;
            winptr->is_associated = true;
            associated_windows.push_back(winptr);
            SendMessage(winptr->GetHWND(), WM_ASSOCIATEDWINDOW, 1_wp, PtrToLP(win.get()));
        }
        //Get assocaited window weak_ptr
        std::optional<std::weak_ptr<Window>> GetAssociatedWindow(HWND win_handle) const noexcept {
            auto ret = std::ranges::find_if(associated_windows, [&win_handle](std::shared_ptr<Window> ptr) {
                if (ptr->GetHWND() == win_handle) return true;
                else return false;
                });
            if (ret == associated_windows.end()) {
                return std::nullopt;
            }
            else {
                return *ret;
            }
        }
        std::vector<std::shared_ptr<Window>> GetAllAssociatedWindows() const noexcept {
            return associated_windows;
        }
        //remove associated window
        void PopAssociatedWindow(HWND win_handle) {
            using namespace literals;
            std::erase_if(associated_windows, [&win_handle](std::shared_ptr<Window> ptr) {
                if (ptr->GetHWND() == win_handle) {
                    ptr->is_associated = false;
                    return true;
                }
                else return false;
                });
            SendMessage(win_handle, static_cast<UINT>(WM_ASSOCIATEDWINDOW), 2_wp, 0_lp);
        }
        //Simple Wrapper around Send/PostMessage. post: if true, use PostMessage, if false use SendMessage
        long long SendWinMsg(UINT msg, WPARAM wParam, LPARAM lParam, bool post = false) {
            LRESULT res = {};
            if (!post) {
                res=SendMessage(win.get(), msg, wParam, lParam);
            }
            else {
                res = PostMessage(win.get(), msg, wParam, lParam);
            }
            return res;
        }
        [[nodiscard]] bool IsAssociated() const noexcept {
            return is_associated;
        }
        //returns the managed handle for passing to Windows functions
        [[nodiscard]] HWND operator()() const noexcept{
            return win.get();
        }
        [[nodiscard]] HWND GetHWND() const noexcept {
            return win.get();
        }
        [[nodiscard]] std::wstring GetWindowClassName() const {
            return classname;
        }
        [[nodiscard]] WindowStyles GetWindowStyles() const noexcept{
            return styles;
        }
        [[nodiscard]] std::expected<RECT,std::error_code> CurrentWindowPosition() const noexcept {
            RECT current_bounds;
            auto check = GetClientRect(win.get(), &current_bounds);
            if (check == 0) {
                return std::unexpected(std::error_code{ static_cast<int>(GetLastError()),std::system_category() });
            }
            else {
                return current_bounds;
            }
        }
        [[nodiscard]] WindowLogger* GetWindowLogger() const noexcept {
            if (log) {
                auto* lf = std::addressof(log.value());
                return lf;
            }
            else return nullptr;
        }
        //returns the allocation type (ExtraDataType) of the extra data (if none, then ExtraDataType::NoType is returned)
        [[nodiscard]] ExtraDataType GetExtraDataAllocationType() const noexcept {
            return extra_data.type;
        }
        static LRESULT WindowProcDispatch(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
//            msgfile << L"Window Message: " << ConvertMessage(msg) << L'\n';
            if (msg == WM_NCCREATE) {
                //get pointer to window class stored in lParam
                auto crst = std::bit_cast<CREATESTRUCT*>(lParam);
                ExtraWindowData windata = *(static_cast<ExtraWindowData*>(crst->lpCreateParams));
                //attach HWND to Window class
                windata.winclass->win.reset(hwnd);
                //register Window with windowreg
                std::lock_guard<std::mutex> winreg_lock(winreg_mutex);
                windowreg.insert_or_assign(hwnd, windata.winclass);
                //add extra data passed to Create function (void* data) into GWLP_USERDATA
                if (windata.data) {
                    SetWindowLongPtr(hwnd, GWLP_USERDATA, std::bit_cast<LONG_PTR>(windata.data));
                }
                return windata.winclass->WindowProc(msg, wParam, lParam);
            }
            try {
                Window& window = GetWindowFromHWND(hwnd);
                return window.WindowProc(msg, wParam, lParam);
            }
            catch (...) {
                return DefWindowProc(hwnd, msg, wParam, lParam);
            }
        }
    private:
        HINSTANCE inst;
        bool main_window;
        std::wstring classname;
        WindowSize create_size;
        UINT class_style;
        WindowStyles styles;
        mutable OptionalWindowLogger log;
        std::wstring title;
        ExtraWindowData extra_data;
        RegisteredWindow win;
        WndProcFunctor additional_callback;
        bool is_associated = false;
        std::vector<std::shared_ptr<Window>> associated_windows;
        //handle specific messages. After processing, if there is an additonal_callback, pass all messages onto it unless a specific message disallows this
        // (runaddproc=false). If there is no callback, run DefWindowProc for unhandled messages and return 0 for handled ones
        //This function serves messages with same handling for all windows. To be specific, supply an additional callback to process them
        LRESULT WindowProc(UINT msg, WPARAM wParam, LPARAM lParam) {
            bool runaddproc = true;
            switch (msg) {
            case WM_DESTROY:
                if (main_window) {
                    PostQuitMessage(0);
                }
                break;
            default:
                if (additional_callback) {
                    return additional_callback(win.get(), msg, wParam, lParam);
                }
                else {
                    return DefWindowProc(win.get(), msg, wParam, lParam);
                }
            }
            if (additional_callback && runaddproc) {
                return additional_callback(win.get(), msg, wParam, lParam);
            }
            else
                return 0;
        }
        //check if Window class is already registered
        std::optional<WNDCLASSEX> IsRegistered(std::wstring classname) const noexcept {
            WNDCLASSEX wcls = {};
            wcls.cbSize = sizeof(WNDCLASSEX);
            auto check = GetClassInfoExW(inst, classname.c_str(), &wcls);
            if (check) {
                return wcls;
            }
            else {
                //getlasterrror
                return std::nullopt;
            }
        }
    };
    
    //Helper to convert control IDs to HMENUs
    inline auto ControlIdToHMENU(WORD id)
    {
        return std::bit_cast<HMENU>(static_cast<uintptr_t>(id));
    }
    export HBRUSH ColorToHBRUSH(int colorIndex) noexcept {
        return std::bit_cast<HBRUSH>(static_cast<uintptr_t>(colorIndex + 1));
    }

    class CommonControl {
    public:
        enum class CommonStyles :DWORD {
            Default = WS_VISIBLE | WS_CHILD,
            PushButton = WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            RadioBox = WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
            CheckBox = WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            ThreeStateCheckbox = WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTO3STATE,
            CommandLink = WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_COMMANDLINK,
            GroupBox = WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
            Static = WS_VISIBLE | WS_CHILD | SS_CENTER,
            StaticLeft = WS_VISIBLE | WS_CHILD | SS_LEFT,
            //SS_LEFT word-wraps: Use this for single-line labels whose text width isn't known up front.
            StaticLeftNoWrap = WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP,
            ListBox = WS_VISIBLE | WS_CHILD | LBS_STANDARD | LBS_HASSTRINGS,
            ComboBoxSimple = WS_VISIBLE | WS_CHILD | CBS_SIMPLE,
            ComboBoxDD = WS_VISIBLE | WS_CHILD | WS_VSCROLL | CBS_DROPDOWN,
            ComboBoxDDL = WS_VISIBLE | WS_CHILD | WS_VSCROLL | CBS_DROPDOWNLIST,
            HScrollBar = WS_VISIBLE | WS_CHILD | SBS_HORZ | SBS_BOTTOMALIGN,
            VScrollBar = WS_VISIBLE | WS_CHILD | SBS_VERT | SBS_RIGHTALIGN,
            Tab = WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS,
            Trackbar = WS_VISIBLE | WS_CHILD | TBS_AUTOTICKS | TBS_ENABLESELRANGE,
            TreeView = WS_VISIBLE | WS_CHILD | TVS_HASLINES | TVS_HASBUTTONS,
            UpDown = WS_VISIBLE | WS_CHILD | UDS_AUTOBUDDY | UDS_ARROWKEYS | UDS_WRAP,
            ProgBarMarquee = WS_VISIBLE | WS_CHILD | PBS_MARQUEE
        };
        //replace_styles: only applicable if custom_control_styles != 0, otherwise ignored.
        //Specifies if custom_control_styles should override the CommonStylesEnum (true,default) or be bitwise-or'd to it
        CommonControl(CommonControlParams cc_params, const Window& parent, CommonStyles control_styles = CommonStyles::Default,
            DWORD custom_control_styles = NO_STYLES, bool replace_styles = true)
            :cc_params(cc_params),parent_window(parent)
        {
            DWORD cstyle = {};
            if (custom_control_styles != 0) {
                if (replace_styles) {
                    cstyle = custom_control_styles;
                }
                else {
                    cstyle = static_cast<DWORD>(control_styles) | custom_control_styles;
                }
            }
            else {
                cstyle = static_cast<DWORD>(control_styles);
            }
            styles = cstyle;
            if (cc_params.classname == ControlNames::RichEdit) {
                LoadLibrary(L"Msftedit.dll");
            }
            else if (cc_params.classname == ControlNames::Tab) {
                //check if parent has WS_CLIPSIBLINGS style
                auto parentstyle = parent.GetWindowStyles();
                if ((parentstyle.standard_styles & WS_CLIPSIBLINGS) == WS_CLIPSIBLINGS) {
                    //if it does and the passed in styles don't have it, add it
                    if (!((cstyle & WS_CLIPSIBLINGS) == WS_CLIPSIBLINGS)) {
                        cstyle |= WS_CLIPSIBLINGS;
                    }
                }
                else {
                    //if it doesn't have it, we can't create this control
                    throw std::runtime_error("To use Tab Control, parent window must have WS_CLIPSIBLINGS style");
                }
            }
//            auto inst = reinterpret_cast<HINSTANCE>(GetWindowLongPtr(parent_window(), GWLP_HINSTANCE));
            auto tempwindow = CreateWindow(control_class_lookup.at(cc_params.classname), cc_params.text.c_str(), cstyle,
                cc_params.posX, cc_params.posY, cc_params.sz.width, cc_params.sz.height, parent_window(),
                ControlIdToHMENU(cc_params.id), nullptr, nullptr);
            parent_logger = parent_window.get().GetWindowLogger();
            if (tempwindow) {
                cc.reset(tempwindow);
                if (parent_logger) {
                    parent_logger->write(std::format(L"Created CommonControl: {}", control_class_lookup.at(cc_params.classname)));
                }
            }
            else {
                auto err = static_cast<int>(GetLastError());
                std::error_code ec(err, std::system_category());
                std::string errtxt = std::format("CommonControl construction failed for control with ID {:d}", cc_params.id);
                if (parent_logger) {
                    parent_logger->write_win32_error(L"CommonControl creation failure ");
                }
                throw std::system_error(ec, errtxt.c_str());
            }
        }
        CommonControl(ControlNames control_name, const WORD id, const Window& parent, std::wstring text, int posX, int posY, WindowSize csz,
            CommonStyles control_styles = CommonStyles::Default, DWORD custom_control_styles = NO_STYLES, bool replace_styles = true)
            :CommonControl({ control_name,text,id,posX,posY,csz }, parent, control_styles, custom_control_styles, replace_styles) {}
        //don't allow temporary Window classes
        CommonControl(std::wstring control_classname, const int control_id, Window&& parent, std::wstring text, int offsetX, int offsetY, WindowSize csz,
            CommonStyles control_styles = CommonStyles::Default, DWORD custom_control_styles = 0) = delete;
        CommonControl(CommonControlParams cc_params, Window&& parent, CommonStyles control_styles = CommonStyles::Default, DWORD custom_control_styles = NO_STYLES,
            bool replace_styles = true) = delete;
        CommonControl(const CommonControl&) = delete;
        CommonControl& operator=(const CommonControl&) = delete;
        CommonControl(CommonControl&&) noexcept = default;
        CommonControl& operator=(CommonControl&&) noexcept = default;
        ~CommonControl() noexcept {
            cc.reset();
        }
        [[nodiscard]] HWND operator()() const noexcept{
//            return GetDlgItem(parent_window(),control_id);
            return cc.get();
        }
        [[nodiscard]] CommonControlParams GetControlParams() const noexcept{
            return cc_params;
        }
        [[nodiscard]] HWND GetParentWindowHandle() const noexcept{
            return parent_window();
        }
        [[nodiscard]] DWORD GetStyles() const noexcept {
            return styles;
        }
        std::expected<RECT,std::error_code> GetCurrentPosition() const noexcept {
            RECT current_bounds;
            auto check = GetClientRect(cc.get(), &current_bounds);
            if (check == 0) {
                return std::unexpected(std::error_code{ static_cast<int>(GetLastError()),std::system_category() });
            }
            else {
                return current_bounds;
            }
        }
        //Calls InitCommonControlsEx for all the controls listed, list all controls used in the program
        static bool InitalizeCommonControls(std::vector<ControlNames> controls) {
            std::vector<DWORD> icc_ids;
            icc_ids.reserve(controls.size());
            for (auto& c : controls) {
                icc_ids.push_back(icc_lookup.at(c));
            }
            auto uicc = std::ranges::unique(icc_ids);
            DWORD icc_bit = {};
            for (auto u : uicc) {
                icc_bit |= u;
            }
            INITCOMMONCONTROLSEX iccex{ sizeof(INITCOMMONCONTROLSEX),icc_bit };
            return InitCommonControlsEx(&iccex);
        }
    private:
        CommonControlParams cc_params;
        WindowSize create_size;
        DWORD styles;
        GenericWindow cc;
        std::reference_wrapper<const Window> parent_window;
        WindowLogger* parent_logger;

        inline static const std::unordered_map<ControlNames, const wchar_t*> control_class_lookup{
            {ControlNames::Animation,ANIMATE_CLASSW},
            {ControlNames::Button,WC_BUTTONW},
            {ControlNames::ComboBox,WC_COMBOBOXW},
            {ControlNames::ComboBoxEx,WC_COMBOBOXEXW},
            {ControlNames::DateTime,DATETIMEPICK_CLASS},
            {ControlNames::Edit,WC_EDITW},
            {ControlNames::Header,WC_HEADERW},
            {ControlNames::IP,WC_IPADDRESSW},
            {ControlNames::ListBox,WC_LISTBOXW},
            {ControlNames::ListView,WC_LISTVIEWW},
            {ControlNames::MonthCalendar,MONTHCAL_CLASSW},
            {ControlNames::Pager,WC_PAGESCROLLERW},
            {ControlNames::ProgBar,PROGRESS_CLASS},
            {ControlNames::Rebar,REBARCLASSNAMEW},
            {ControlNames::RichEdit,MSFTEDIT_CLASS},
            {ControlNames::ScrollBar,WC_SCROLLBARW},
            {ControlNames::Static,WC_STATICW},
            {ControlNames::StatusBar,STATUSCLASSNAMEW},
            {ControlNames::SysLink,WC_LINK},
            {ControlNames::Tab,WC_TABCONTROL},
            {ControlNames::Toolbar,TOOLBARCLASSNAMEW},
            {ControlNames::Trackbar,TRACKBAR_CLASSW},
            {ControlNames::TreeView,WC_TREEVIEWW},
            {ControlNames::UpDown,UPDOWN_CLASSW}
        };

        inline static const std::unordered_map<ControlNames, DWORD> icc_lookup{
            {ControlNames::Animation,ICC_ANIMATE_CLASS},
            {ControlNames::Button,ICC_STANDARD_CLASSES},
            {ControlNames::ComboBox,ICC_STANDARD_CLASSES},
            {ControlNames::ComboBoxEx,ICC_USEREX_CLASSES},
            {ControlNames::DateTime,ICC_DATE_CLASSES},
            {ControlNames::Edit,ICC_STANDARD_CLASSES},
            {ControlNames::Header,ICC_LISTVIEW_CLASSES},
            {ControlNames::IP,ICC_INTERNET_CLASSES},
            {ControlNames::ListBox, ICC_STANDARD_CLASSES},
            {ControlNames::ListView,ICC_LISTVIEW_CLASSES},
            {ControlNames::MonthCalendar,ICC_DATE_CLASSES},
            {ControlNames::Pager,ICC_PAGESCROLLER_CLASS},
            {ControlNames::ProgBar,ICC_PROGRESS_CLASS},
            {ControlNames::Rebar,ICC_COOL_CLASSES},
            {ControlNames::RichEdit,ICC_STANDARD_CLASSES},
            {ControlNames::ScrollBar,ICC_STANDARD_CLASSES},
            {ControlNames::Static,ICC_STANDARD_CLASSES},
            {ControlNames::StatusBar,ICC_BAR_CLASSES},
            {ControlNames::SysLink,ICC_LINK_CLASS},
            {ControlNames::Tab,ICC_TAB_CLASSES},
            {ControlNames::Toolbar,ICC_BAR_CLASSES},
            {ControlNames::Trackbar,ICC_BAR_CLASSES},
            {ControlNames::TreeView,ICC_TREEVIEW_CLASSES},
            {ControlNames::UpDown,ICC_UPDOWN_CLASS}
        };
    };
    export void IncrementProgressBar(const CommonControl& cc,bool set_step=false, unsigned int step = 10) {
        using namespace literals;
        auto params = cc.GetControlParams();
        if (params.classname != ControlNames::ProgBar) {
            return;
        }
        auto styles = cc.GetStyles();
        if ((styles & PBS_MARQUEE) == PBS_MARQUEE) {
            return;
        }
        else {
            if (set_step) {
                SendMessage(cc(), static_cast<UINT>(PBM_SETSTEP), static_cast<WPARAM>(step), 0_lp);
            }
            SendMessage(cc(), static_cast<UINT>(PBM_STEPIT), 0_wp, 0_lp);
        }
    }
    export LRESULT CallDefaultWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    class ModelessDiagBox {
    private:
        HINSTANCE inst;
        const wchar_t* rid;
        std::function< INT_PTR(HWND, UINT, WPARAM, LPARAM) > cb;
        GenericWindow dlg;
        HWND parent;
        ExtraDialogData data;
        static INT_PTR DlgProcDispatch(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
            INT_PTR res = {};
            switch (msg) {
            case WM_INITDIALOG:
            {
                auto ewd = std::bit_cast<ExtraDialogData*>(lParam);
                SetWindowLongPtr(hWnd, DWLP_USER, std::bit_cast<LONG_PTR>(ewd->winclass));
                ewd->winclass->dlg.reset(hWnd);
                return ewd->winclass->cb(hWnd, msg, wParam, ewd->data);
            }
            default:
            {
                //Messages such as WM_SETFONT can arrive before WM_INITDIALOG, when DWLP_USER is still 0.
                //Dereferencing then is a null deref/crash; let the system handle those early messages.
                auto self = std::bit_cast<ModelessDiagBox*>(GetWindowLongPtr(hWnd, DWLP_USER));
                if (!self) {
                    return FALSE;
                }
                res = self->cb(hWnd, msg, wParam, lParam);
                break;
            }
            }
            return res;
        }
    public:
        ModelessDiagBox(HINSTANCE hInst, const wchar_t* template_id, std::function< INT_PTR(HWND, UINT, WPARAM, LPARAM) > callback, HWND parent = nullptr)
            :inst(hInst), rid(template_id), cb(callback), parent(parent), data(this) {}
        ModelessDiagBox(HINSTANCE hInst, const int template_id, std::function< INT_PTR(HWND, UINT, WPARAM, LPARAM) > callback, HWND parent = nullptr)
            :ModelessDiagBox(hInst, MAKEINTRESOURCE(template_id), callback, parent) {}
        bool Create(LPARAM lParamInit = 0,ExtraDataType data_type = ExtraDataType::NoData) {
            data.type = data_type;
            data.data = lParamInit;
            if (auto hret = CreateDialogParam(inst, rid, parent, &DlgProcDispatch, PtrToLP(&data)); hret) {
                //CreateDialogParam sends WM_INITDIALOG synchronously, and DlgProcDispatch already stored the
                //handle (dlg.reset(hWnd)) before we get here. Calling dlg.reset(hret) again would run
                //unique_ptr's deleter on that same HWND -> DestroyWindow -> the dialog vanishes the instant
                //it is created. Only adopt the handle if it wasn't already stored.
                if (dlg.get() != hret) {
                    dlg.reset(hret);
                }
                return true;
            }
            else return false;
        }
        bool Display(LPARAM lParamInit = 0, ExtraDataType data_type = ExtraDataType::NoData) {
            bool ret = true;
            if (!dlg) {
                ret = Create(lParamInit, data_type);
            }
            ShowWindow(dlg.get(), SW_SHOWNORMAL);
            return ret;
        }
        HWND operator()() const noexcept {
            return dlg.get();
        }
        HWND operator()(int dialog_control_number) const {
            return GetDlgItem(dlg.get(), dialog_control_number);
        }
        [[nodiscard]] ExtraDataType GetExtraDataAllocationType() const noexcept {
            return data.type;
        }
        long long SendDlgMsg(UINT msg, WPARAM wParam, LPARAM lParam, bool post = false) {
            if (!post) {
                return SendMessage(dlg.get(), msg, wParam, lParam);
            }
            else {
                return PostMessage(dlg.get(), msg, wParam, lParam);
            }
        }
        std::error_code UpdateDialogText(int static_control_id,std::wstring text) {
            auto sthwnd = GetDlgItem(dlg.get(), static_control_id);
            auto res = SetWindowText(sthwnd, text.c_str());
            if (res != 0) {
                return std::error_code();
            }
            else {
                auto errcode = GetLastError();
                return std::error_code(static_cast<int>(errcode), std::system_category());
            }
        }
    };
}