/*
 * COMMDLG - File Dialogs
 *
 * Copyright 1994 Martin Ayotte
 * Copyright 1996 Albrecht Kleine
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */
#include <stdarg.h>
#include "windef.h"
#include "winbase.h"
#include "wine/winbase16.h"
#include "wingdi.h"
#include "winuser.h"
#include "commdlg.h"
#include "cdlg16.h"
#include "wine/debug.h"
#include <windows.h>
#include "resource.h"
#include <DbgHelp.h>

WINE_DEFAULT_DEBUG_CHANNEL(commdlg);
#include <pshpack1.h>
typedef struct
{
    BYTE pop_eax;   //58
    BYTE push;      //68
    DWORD this_;
    BYTE push_eax;  //50
    BYTE mov_eax;   //B8
    DWORD address;
    BYTE jmp;       //FF E0
    BYTE eax;       //E0
    BOOL used;
    SEGPTR segofn16;
    OPENFILENAME16 ofn16;
} COMMDLGTHUNK;
#include <poppack.h>
#define MAX_THUNK 5
COMMDLGTHUNK *thunk_array;

LRESULT WINAPI DIALOG_CallDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, WNDPROC proc);
static UINT_PTR CALLBACK thunk_hook(COMMDLGTHUNK *thunk, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_INITDIALOG)
    {
        wp = HWND_16(wp);
        lp = thunk->segofn16;// thunk->ofn16.lCustData;
    }
    UINT_PTR result = DIALOG_CallDialogProc(hwnd, msg, wp, lp, thunk->ofn16.lpfnHook);
    return result;
}

static void init_thunk()
{
    if (thunk_array)
        return;
    thunk_array = VirtualAlloc(NULL, MAX_THUNK * sizeof(COMMDLGTHUNK), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
}

COMMDLGTHUNK *allocate_thunk(OPENFILENAME16 ofn16, SEGPTR ofnseg)
{
    init_thunk();
    for (int i = 0; i < MAX_THUNK; i++)
    {
        if (!thunk_array[i].used)
        {
            thunk_array[i].pop_eax  = 0x58;
            thunk_array[i].push     = 0x68;
            thunk_array[i].this_    = thunk_array + i;
            thunk_array[i].push_eax = 0x50;
            thunk_array[i].mov_eax  = 0xB8;
            thunk_array[i].address  = thunk_hook;
            thunk_array[i].jmp      = 0xFF;
            thunk_array[i].eax      = 0xE0;
            thunk_array[i].used     = TRUE;
            thunk_array[i].segofn16 = ofnseg;
            thunk_array[i].ofn16    = ofn16;
            return thunk_array + i;
        }
    }
    return NULL;
}

static UINT_PTR CALLBACK dummy_hook( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    return FALSE;
}

/***********************************************************************
 *           FileOpenDlgProc   (COMMDLG.6)
 */
BOOL16 CALLBACK FileOpenDlgProc16(HWND16 hWnd16, UINT16 wMsg, WPARAM16 wParam, LPARAM lParam)
{
    FIXME( "%04x %04x %04x %08lx: stub\n", hWnd16, wMsg, wParam, lParam );
    return FALSE;
}

/***********************************************************************
 *           FileSaveDlgProc   (COMMDLG.7)
 */
BOOL16 CALLBACK FileSaveDlgProc16(HWND16 hWnd16, UINT16 wMsg, WPARAM16 wParam, LPARAM lParam)
{
    FIXME( "%04x %04x %04x %08lx: stub\n", hWnd16, wMsg, wParam, lParam );
    return FALSE;
}

DWORD get_ofn_flag(DWORD flag)
{
	//LFN
	if (TRUE)
	{
		return flag | OFN_NOLONGNAMES;
	}
	return flag;
}
#pragma comment(lib, "dbghelp.lib")
DLGTEMPLATE *WINAPI dialog_template16_to_template32(HINSTANCE16 hInst, LPCVOID dlgTemplate, DWORD *size);
BOOL dynamic_resource(LPOPENFILENAME16 lpofn)
{
    HGLOBAL16 hmem;
    HRSRC16 hRsrc;
    LPCVOID data;
    if (!(hRsrc = FindResource16(lpofn->hInstance, MapSL(lpofn->lpTemplateName), (LPSTR)RT_DIALOG))) return 0;
    if (!(hmem = LoadResource16(lpofn->hInstance, hRsrc))) return 0;
    if (!(data = LockResource16(hmem))) return 0;
    PVOID BaseAddress = GetModuleHandleW(L"commdlg.dll16");
    IMAGE_RESOURCE_DIRECTORY *dir;
    IMAGE_RESOURCE_DIR_STRING_U *str;
    ULONG size;
    PVOID root = ImageDirectoryEntryToData(BaseAddress, TRUE, IMAGE_DIRECTORY_ENTRY_RESOURCE, &size);
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(root, &mbi, sizeof(mbi));
    DWORD oldprotect;
    VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_READWRITE, &oldprotect);
    dir = root;
    IMAGE_RESOURCE_DIRECTORY_ENTRY *entry;
    int min, max, pos;

    entry = (IMAGE_RESOURCE_DIRECTORY_ENTRY *)(dir + 1);
    min = dir->NumberOfNamedEntries;
    max = min + dir->NumberOfIdEntries - 1;
    pos = min;
    while (pos <= max)
    {
        if (entry[pos].Id == RT_BITMAP)
        {
            entry[pos].Id = RT_DIALOG;
            break;
        }
        pos++;
    }
    HRSRC hRsrc32;
    HGLOBAL hmem32;
    LPVOID data32;
    if (!(hRsrc32 = FindResourceA(BaseAddress, IDB_BITMAP1, (LPSTR)RT_DIALOG))) return 0;
    if (!(hmem32 = LoadResource(BaseAddress, hRsrc32))) return 0;
    if (!(data32 = LockResource(hmem32))) return 0;
    DWORD sized;
    DLGTEMPLATE *tmp32 = dialog_template16_to_template32(lpofn->hInstance, data, &sized);
    memcpy(data32, tmp32, sized);
    DWORD _;
    VirtualProtect(mbi.BaseAddress, mbi.RegionSize, oldprotect, &_);
    return TRUE;
}
/***********************************************************************
 *           GetOpenFileName   (COMMDLG.1)
 *
 * Creates a dialog box for the user to select a file to open.
 *
 * RETURNS
 *    TRUE on success: user selected a valid file
 *    FALSE on cancel, error, close or filename-does-not-fit-in-buffer.
 *
 * BUGS
 *    unknown, there are some FIXMEs left.
 */
BOOL16 WINAPI GetOpenFileName16( SEGPTR ofn ) /* [in/out] address of structure with data*/
{
    LPOPENFILENAME16 lpofn = MapSL(ofn);
    OPENFILENAME16 ofn16 = *lpofn;
    dynamic_resource(lpofn);
    OPENFILENAMEA ofn32;
    BOOL ret;

    if (!lpofn) return FALSE;

    ofn32.lStructSize       = OPENFILENAME_SIZE_VERSION_400A;
    ofn32.hwndOwner         = HWND_32( lpofn->hwndOwner );
    ofn32.lpstrFilter       = MapSL( lpofn->lpstrFilter );
    ofn32.lpstrCustomFilter = MapSL( lpofn->lpstrCustomFilter );
    ofn32.nMaxCustFilter    = lpofn->nMaxCustFilter;
    ofn32.nFilterIndex      = lpofn->nFilterIndex;
    ofn32.lpstrFile         = MapSL( lpofn->lpstrFile );
    ofn32.nMaxFile          = lpofn->nMaxFile;
    ofn32.lpstrFileTitle    = MapSL( lpofn->lpstrFileTitle );
    ofn32.nMaxFileTitle     = lpofn->nMaxFileTitle;
    ofn32.lpstrInitialDir   = MapSL( lpofn->lpstrInitialDir );
    ofn32.lpstrTitle        = MapSL( lpofn->lpstrTitle );
	ofn32.Flags             = get_ofn_flag(lpofn->Flags | OFN_ENABLEHOOK);
    ofn32.nFileOffset       = lpofn->nFileOffset;
    ofn32.nFileExtension    = lpofn->nFileExtension;
    ofn32.lpstrDefExt       = MapSL( lpofn->lpstrDefExt );
    ofn32.lCustData         = lpofn->lCustData;
    ofn32.lpfnHook          = dummy_hook;  /* this is to force old 3.1 dialog style */

    if (lpofn->Flags & OFN_ENABLETEMPLATE)
    {
        ofn32.lpTemplateName = MAKEINTRESOURCEA(IDB_BITMAP1);
        ofn32.hInstance = GetModuleHandleW(L"commdlg.dll16");
    }
    if (lpofn->Flags & OFN_ENABLEHOOK)
    {
        ofn32.lpfnHook = allocate_thunk(ofn16, ofn);
        if (!ofn32.lpfnHook)
        {
            ERR("could not allocate GetOpenFileName16 thunk\n");
            ofn32.lpfnHook = dummy_hook;
        }
    }

    if ((ret = GetOpenFileNameA( &ofn32 )))
    {
	lpofn->nFilterIndex   = ofn32.nFilterIndex;
	lpofn->nFileOffset    = ofn32.nFileOffset;
	lpofn->nFileExtension = ofn32.nFileExtension;
    }
    return ret;
}

/***********************************************************************
 *           GetSaveFileName   (COMMDLG.2)
 *
 * Creates a dialog box for the user to select a file to save.
 *
 * RETURNS
 *    TRUE on success: user enters a valid file
 *    FALSE on cancel, error, close or filename-does-not-fit-in-buffer.
 *
 * BUGS
 *    unknown. There are some FIXMEs left.
 */
BOOL16 WINAPI GetSaveFileName16( SEGPTR ofn ) /* [in/out] address of structure with data*/
{
    LPOPENFILENAME16 lpofn = MapSL(ofn);
    OPENFILENAMEA ofn32;
    BOOL ret;

    if (!lpofn) return FALSE;

    ofn32.lStructSize       = OPENFILENAME_SIZE_VERSION_400A;
    ofn32.hwndOwner         = HWND_32( lpofn->hwndOwner );
    ofn32.lpstrFilter       = MapSL( lpofn->lpstrFilter );
    ofn32.lpstrCustomFilter = MapSL( lpofn->lpstrCustomFilter );
    ofn32.nMaxCustFilter    = lpofn->nMaxCustFilter;
    ofn32.nFilterIndex      = lpofn->nFilterIndex;
    ofn32.lpstrFile         = MapSL( lpofn->lpstrFile );
    ofn32.nMaxFile          = lpofn->nMaxFile;
    ofn32.lpstrFileTitle    = MapSL( lpofn->lpstrFileTitle );
    ofn32.nMaxFileTitle     = lpofn->nMaxFileTitle;
    ofn32.lpstrInitialDir   = MapSL( lpofn->lpstrInitialDir );
    ofn32.lpstrTitle        = MapSL( lpofn->lpstrTitle );
    ofn32.Flags             = get_ofn_flag((lpofn->Flags & ~OFN_ENABLETEMPLATE) | OFN_ENABLEHOOK);
    ofn32.nFileOffset       = lpofn->nFileOffset;
    ofn32.nFileExtension    = lpofn->nFileExtension;
    ofn32.lpstrDefExt       = MapSL( lpofn->lpstrDefExt );
    ofn32.lCustData         = lpofn->lCustData;
    ofn32.lpfnHook          = dummy_hook;  /* this is to force old 3.1 dialog style */

    if (lpofn->Flags & OFN_ENABLETEMPLATE)
        FIXME( "custom templates no longer supported, using default\n" );
    if (lpofn->Flags & OFN_ENABLEHOOK)
        FIXME( "custom hook %p no longer supported\n", lpofn->lpfnHook );

    if ((ret = GetSaveFileNameA( &ofn32 )))
    {
	lpofn->nFilterIndex   = ofn32.nFilterIndex;
	lpofn->nFileOffset    = ofn32.nFileOffset;
	lpofn->nFileExtension = ofn32.nFileExtension;
    }
    return ret;
}


/***********************************************************************
 *	GetFileTitle		(COMMDLG.27)
 */
short WINAPI GetFileTitle16(LPCSTR lpFile, LPSTR lpTitle, UINT16 cbBuf)
{
	return GetFileTitleA(lpFile, lpTitle, cbBuf);
}

//////////////////////////////////////
void __wine_spec_init_ctor()
{
	ERR("NOTIMPL:__wine_spec_init_ctor()\n");
}
void __wine_spec_dll_entry()
{
	ERR("NOTIMPL:__wine_spec_dll_entry(?)\n");
}