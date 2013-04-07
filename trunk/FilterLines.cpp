/*
    FilterLines plugin for FAR Manager
    Copyright (C) 2013 Igor Yudincev

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <initguid.h>
#include <tchar.h>
#include "arraysize.h"
#include "plugin.hpp"
#include "stack.hpp"
#include "mystring.hpp"
#include "search.hpp"
#include "searchh.hpp"
#include "far_settings.h"
#include "ver.h"


#define PluginName L"FilterLines"


// {0A2DEABF-6103-43DB-AA7A-BD1F7A23E0ED}
DEFINE_GUID(PluginId,
0xa2deabf, 0x6103, 0x43db, 0xaa, 0x7a, 0xbd, 0x1f, 0x7a, 0x23, 0xe0, 0xed);

// {1F61B3B0-1E10-4B6D-9940-900B8CA61B11}
DEFINE_GUID(DialogId,
0x1f61b3b0, 0x1e10, 0x4b6d, 0x99, 0x40, 0x90, 0xb, 0x8c, 0xa6, 0x1b, 0x11);


PluginStartupInfo Info;
static int nChanges;         //число Undo дл€ восстановлени€ исходного текста
static wchar_t *PrevPattern; //строка, по которой был отфильтрован текст
static bool bPrevCaseSensitive;
static bool bRefresh;        //установлена галка "Ќемедленное обновление"
static bool bSequentialInput;//все символы допечатывались в конец строки
static intptr_t InitialLine,InitialCol,InitialTop; //позици€ курсора в исходном тексте
static bool bLineChanged;    //исходна€ позици€ курсора была изменена
static intptr_t OriginalLine;//строка исходного текста, соответствующа€ тек.позиции курсора
static intptr_t TotalLines;  //число строк в исходном тексте
static HANDLE ctx;           //контекст поиска

enum {
	MTitle,
	MCase,
	MOk,
	MCancel,
};

enum {
	ixSearchString=1,
	ixCaseSensitive=2,
	ixOk=3,
	ixCancel=4
};

typedef enum tagScrollType {
	stUp=-1,
	stDn=1,
	stPgUp=-2,
	stPgDn=2,
	stHome=-3,
	stEnd=3
} ScrollType;


#if defined(__GNUC__)
#ifdef __cplusplus
extern "C"{
#endif
	BOOL WINAPI DllMainCRTStartup(HANDLE hDll,DWORD dwReason,LPVOID lpReserved);
#ifdef __cplusplus
};
#endif

BOOL WINAPI DllMainCRTStartup(HANDLE hDll,DWORD dwReason,LPVOID lpReserved)
{
	(void) hDll;
	(void) dwReason;
	(void) lpReserved;
	return TRUE;
}
#endif

static const wchar_t* GetMsg(intptr_t MsgId)
{
	return(Info.GetMsg(&PluginId, MsgId));
}

static intptr_t GetCheck(HANDLE hDlg, intptr_t i)
{
	return (intptr_t)Info.SendDlgMessage(hDlg, DM_GETCHECK, i, 0);
}

//создать маску удалЄнных строк дл€ текущей правки
inline static char *MakeLineMask()
{
	char *Lines;
	Lines = (char*)malloc(TotalLines/8+1);
	memset(Lines, 0, TotalLines/8+1);
	return Lines;
}

//пометить строку текста предыдущей правки, как удалЄнную
inline static void MarkDeletedLine(char *Lines, intptr_t nOrig)
{
	Lines[(size_t)(nOrig/8)] |= 1<<((size_t)(nOrig%8));
}

//преобразовать є строки текущей правки в є строки предыдущей
//Lines-битова€ маска удалЄнных строк
inline static intptr_t CalcLineInPrev(char *Lines, intptr_t nLine)
{
	intptr_t iLines=0;
	char mLines=0x01;
	int iRemaining=0;
	for (intptr_t i=0;i<TotalLines;i++) {
		if (!(Lines[iLines]&mLines)) { //строка не была удалена
			if (iRemaining==nLine)
				return i;
			iRemaining++;
		}
		mLines<<=1;
		if (mLines==0) {mLines=0x01; iLines++;}
	}
	return -1;
}

//преобразовать є строки текущей правки в є строки исходного текста
inline static intptr_t CalcOriginalLine(intptr_t nLine)
{
	intptr_t nOrig=nLine;
	for (size_t iNestLevel=0;;iNestLevel++) {
		char *Lines=(char*)StackPeek(iNestLevel);
		if (Lines==NULL)
			return nOrig;
		nOrig = CalcLineInPrev(Lines, nOrig);
		if (nOrig==-1)
			return -1;
	}
}

static void Undo()
{

	char *Lines = (char*)StackPop();
	free(Lines);

	EditorUndoRedo eur = {sizeof(EditorUndoRedo), EUR_UNDO};
	Info.EditorControl(-1, ECTL_UNDOREDO, 0, &eur);

	struct EditorSetPosition es = {sizeof(EditorSetPosition)};
	if (--nChanges == 0) {
		if (bLineChanged) {
			es.CurLine = OriginalLine;
			es.TopScreenLine = OriginalLine;
		}
		else {
			es.CurLine = InitialLine;
			es.TopScreenLine = InitialTop;
		}
		es.CurPos = InitialCol;
		es.CurTabPos = es.LeftPos = es.Overtype = -1;
	}
	else {
		es.CurLine = 0;
		es.CurPos = 0;
		es.TopScreenLine = 0;
		es.CurTabPos = es.LeftPos = es.Overtype = -1;
	}
	Info.EditorControl(-1, ECTL_SETPOSITION, 0, &es);
}

static void UndoAll()
{
	while (nChanges>0)
		Undo();
}

static void HighlightLine(intptr_t nLine)
{
	EditorInfo einfo = {sizeof(EditorInfo)};
	Info.EditorControl(-1, ECTL_GETINFO, 0, &einfo);

	EditorSelect sel = {sizeof(EditorSelect)};
	sel.BlockHeight=1;
	sel.BlockStartLine=nLine;
	sel.BlockStartPos=0;
	sel.BlockType=BTYPE_STREAM;
	sel.BlockWidth=einfo.WindowSizeX;
	Info.EditorControl(-1, ECTL_SELECT, 0, &sel);
}

static void RemoveHighlight()
{
	EditorSelect sel = {sizeof(EditorSelect)};
	sel.BlockType=BTYPE_NONE;
	Info.EditorControl(-1, ECTL_SELECT, 0, &sel);
}

static void ScrollText(ScrollType st)
{
	EditorInfo einfo = {sizeof(EditorInfo)};
	EditorSetPosition es = {sizeof(EditorSetPosition)};

	Info.EditorControl(-1, ECTL_GETINFO, 0, &einfo);
	switch (st) {
	case stDn:
		es.CurLine = einfo.CurLine+1;
		break;
	case stUp:
		es.CurLine = einfo.CurLine-1;
		break;
	case stPgDn:
		es.CurLine = einfo.CurLine+einfo.WindowSizeY-1;
		es.TopScreenLine = einfo.TopScreenLine+einfo.WindowSizeY-1;
		break;
	case stPgUp:
		es.CurLine = einfo.CurLine-einfo.WindowSizeY+1;
		es.TopScreenLine = einfo.TopScreenLine-einfo.WindowSizeY+1;
		break;
	case stEnd:
		es.CurLine = /*es.TopScreenLine = */einfo.TotalLines-1;
		break;
	default:
		es.CurLine = /*es.TopScreenLine = */0;
		break;
	}
	bLineChanged = true;

	es.CurPos = -1;
	es.CurTabPos = es.LeftPos = es.Overtype = -1;
	Info.EditorControl(-1, ECTL_SETPOSITION, 0, &es);

	//обновим є исходной строки
	Info.EditorControl(-1, ECTL_GETINFO, 0, &einfo);
	char *Lines = (char*)StackPeek();
	OriginalLine = CalcOriginalLine(einfo.CurLine);
	HighlightLine(einfo.CurLine);

	Info.AdvControl(&PluginId, ACTL_REDRAWALL, 0, NULL);
}

static void Filter(const wchar_t *sFilter, bool bCaseSensitive)
{
	if (sFilter!=NULL) {
		size_t nFilter = wcslen(sFilter);
		if (nFilter>0xFFFF) nFilter = 0xFFFF;
		wchar_t *sFilterLower=NULL;
		EditorUndoRedo eur = {sizeof(EditorUndoRedo)};
		EditorSetPosition es = {sizeof(EditorSetPosition)};
		char *Lines; //маска удалЄнных строк

		Lines = MakeLineMask();
		StackPush(Lines);

		if (nFilter<=MAX_PATTERN_LEN) {
			SearchPrepare(ctx, sFilter, bCaseSensitive);
		}
		else {
			SearchHPrepare(ctx, sFilter, (WORD)nFilter, bCaseSensitive);
			if (!bCaseSensitive) {
				SetLength(&sFilterLower, (int)nFilter);
				wcscpy(sFilterLower, sFilter);
				CharLower(sFilterLower);
			}
		}
		//непустой фильтр - создаЄм блок Undo и мен€ем текст внутри него
		eur.Command=EUR_BEGIN;
		Info.EditorControl(-1, ECTL_UNDOREDO, 0, &eur);

		//ищем текст из диалога в каждой строке
		EditorInfo einfo = {sizeof(EditorInfo)};
		Info.EditorControl(-1, ECTL_GETINFO, 0, &einfo);
		intptr_t nLines = einfo.TotalLines;
		intptr_t line = nLines;
		es.CurPos = 0;
		es.CurTabPos = es.LeftPos = es.Overtype = es.TopScreenLine = -1;
		while (--line>=0) {
			es.CurLine = line;
			Info.EditorControl(-1, ECTL_SETPOSITION, 0, &es);

			EditorGetString egs = {sizeof(EditorGetString), -1};
			Info.EditorControl(-1, ECTL_GETSTRING, 0, &egs);
			const wchar_t *found;
			if (nFilter <= MAX_PATTERN_LEN)
				found = Search(ctx, egs.StringText);
			else
				found = SearchH(ctx,
					bCaseSensitive ? sFilter : sFilterLower,
					(WORD)nFilter,
					egs.StringText,
					egs.StringLength,
					bCaseSensitive);
			if (found==NULL) {
				Info.EditorControl(-1, ECTL_DELETESTRING, 0, NULL);
				MarkDeletedLine(Lines, line);
			}
		}

		es.CurLine = 0;
		es.CurPos = 0;
		es.TopScreenLine = 0;
		es.CurTabPos = es.LeftPos = es.Overtype = -1;
		Info.EditorControl(-1, ECTL_SETPOSITION, 0, &es);

		RemoveHighlight();

		eur.Command=EUR_END;
		Info.EditorControl(-1, ECTL_UNDOREDO, 0, &eur);

		nChanges++;
		if (nFilter<=MAX_PATTERN_LEN) {
			SearchUnprepare(ctx,sFilter,bCaseSensitive);
		}
		else {
			if (!bCaseSensitive)
				SetLength(&sFilterLower, 0);
		}
	}
}

static intptr_t WINAPI MyDlgProc(HANDLE hDlg, intptr_t Msg, intptr_t Param1, void *Param2)
{
	FarGetDialogItem DialogItem={sizeof(FarGetDialogItem)};
	const INPUT_RECORD* record;

	switch (Msg)
	{
	case DN_CONTROLINPUT:
		record=(const INPUT_RECORD *)Param2;
		if (record->EventType==KEY_EVENT &&
			record->Event.KeyEvent.bKeyDown &&
			record->Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED &&
			(record->Event.KeyEvent.dwControlKeyState & LEFT_CTRL_PRESSED ||
			record->Event.KeyEvent.dwControlKeyState & RIGHT_CTRL_PRESSED))
		{
			switch (record->Event.KeyEvent.wVirtualKeyCode) {
			case VK_DOWN:
				ScrollText(stDn);
				return TRUE;
			case VK_UP:
				ScrollText(stUp);
				return TRUE;
			case VK_NEXT:
				ScrollText(stPgDn);
				return TRUE;
			case VK_PRIOR:
				ScrollText(stPgUp);
				return TRUE;
			case VK_END:
				ScrollText(stEnd);
				return TRUE;
			case VK_HOME:
				ScrollText(stHome);
				return TRUE;
			}
		}
		break;

	case DN_BTNCLICK:
		if (Param1==ixCaseSensitive) {
			bool bCaseSensitive = Param2!=0;
			if (bCaseSensitive!=bPrevCaseSensitive)
				UndoAll();
			bPrevCaseSensitive = bCaseSensitive;
		}
		Filter(PrevPattern, bPrevCaseSensitive);
		Info.AdvControl(&PluginId, ACTL_REDRAWALL, 0, NULL);
		break;

	case DN_EDITCHANGE:
		DialogItem.Size = (size_t)Info.SendDlgMessage(hDlg, DM_GETDLGITEM, ixSearchString, NULL);
		DialogItem.Item = (FarDialogItem*)malloc(DialogItem.Size);
		if (DialogItem.Item) {
			Info.SendDlgMessage(hDlg, DM_GETDLGITEM, ixSearchString, &DialogItem);
			size_t nChars = wcslen(DialogItem.Item->Data);
			size_t nPrevChars = Length(PrevPattern);
			bool bFilter = true;
			if (PrevPattern!=NULL) {
				//получаетс€ ли нова€ строка укорачиванием старой на символ
				if (bSequentialInput && nChars+1 == nPrevChars &&
					wcsncmp(DialogItem.Item->Data, PrevPattern, nChars) == 0)
				{
					//да - откатываем одну правку
					Undo();
					bFilter = false;
				}
				else
				{
					//получаетс€ ли нова€ строка добавлением символа к старой
					if (!(nChars == nPrevChars+1 &&
						wcsncmp(DialogItem.Item->Data, PrevPattern, nPrevChars) == 0))
					{
						//если нет - считаем, что откатывать по одной правке нельз€
						bSequentialInput = false;
					}
					//можно ли искать новую строку среди уже отфильтрованных
					if (wcsstr(DialogItem.Item->Data, PrevPattern) == NULL)
					{
						//нет - откатываем все правки
						UndoAll();
					}
				}
			}
			//выделить контекст заново при необходимости
			if (Length(PrevPattern)<MAX_PATTERN_LEN && nChars>=MAX_PATTERN_LEN) {
				SearchFree(ctx);
				ctx = SearchHAlloc();
			}
			else if (Length(PrevPattern)>=MAX_PATTERN_LEN && nChars<MAX_PATTERN_LEN) {
				SearchHFree(ctx);
				ctx = SearchAlloc();
			}

			//запомнить значение фильтра
			if (nChars!=0) {
				SetLength(&PrevPattern, (int)nChars);
				wcscpy(PrevPattern, DialogItem.Item->Data);
			}
			else {
				SetLength(&PrevPattern, 0);
				bSequentialInput = true;
			}
			free(DialogItem.Item);
			if (bFilter)
				Filter(PrevPattern, bPrevCaseSensitive);
			Info.AdvControl(&PluginId, ACTL_REDRAWALL, 0, NULL);
		}
		break;
	}
	return Info.DefDlgProc(hDlg, Msg, Param1, Param2);
}

void WINAPI SetStartupInfoW(const PluginStartupInfo *psi)
{
	Info = *psi;
}

void WINAPI GetGlobalInfoW(GlobalInfo *Info)
{
	Info->StructSize = sizeof(GlobalInfo);
	Info->MinFarVersion = MAKEFARVERSION(3, 0, 0, 2927, VS_RELEASE);
	Info->Version = MAKEFARVERSION(VER_MAJOR, VER_MINOR, 0, 0, VS_RELEASE);
	Info->Guid = PluginId;
	Info->Title = PluginName;
	Info->Description = L"FilterLines";
	Info->Author = _T(VER_AUTHOR);
}

void WINAPI GetPluginInfoW(PluginInfo *Info)
{
	static const wchar_t *PluginMenuStrings[1];

	PluginMenuStrings[0] = GetMsg(MTitle);

	Info->StructSize = sizeof(PluginInfo);
	Info->Flags = PF_EDITOR | PF_DISABLEPANELS;
	Info->DiskMenu.Strings = 0;
	Info->DiskMenu.Guids = NULL;
	Info->DiskMenu.Count = 0;
	Info->PluginMenu.Strings = PluginMenuStrings;
	Info->PluginMenu.Count = ArraySize(PluginMenuStrings);
	Info->PluginMenu.Guids = &PluginId;
	Info->PluginConfig.Strings = 0;
	Info->PluginConfig.Count = 0;
	Info->CommandPrefix = 0;
}

HANDLE WINAPI OpenW(const OpenInfo *OInfo)
{
	static FarDialogItem DlgItems[]={
		/* 0*/  {DI_DOUBLEBOX,2, 1,37, 5, {0},NULL      ,NULL      ,0                                ,GetMsg(MTitle)},
		/* 1*/  {DI_EDIT     ,4, 2,34, 2, {1},PluginName,NULL      ,DIF_HISTORY                      ,L""},
		/* 2*/  {DI_CHECKBOX ,4, 3, 0, 0, {0},0         ,NULL      ,0                                ,GetMsg(MCase)},
		/* 3*/  {DI_BUTTON   ,0, 4, 0, 0, {0},0         ,NULL      ,DIF_CENTERGROUP|DIF_DEFAULTBUTTON,GetMsg(MOk)},
		/* 4*/  {DI_BUTTON   ,0, 4, 0, 0, {0},0         ,NULL      ,DIF_CENTERGROUP                  ,GetMsg(MCancel)}
	};

	CFarSettings settings(PluginId);
	bPrevCaseSensitive = settings.Get(L"CaseSensitive", false);
	DlgItems[ixCaseSensitive].Selected = bPrevCaseSensitive ? 1 : 0;
	HANDLE hEdit = Info.DialogInit(&PluginId, &DialogId,
		-1, -1, 40, 7,
		PluginName,
		DlgItems, ArraySize(DlgItems),
		0, 0,
		MyDlgProc, 0);
	if (hEdit == INVALID_HANDLE_VALUE)
		return INVALID_HANDLE_VALUE;

	EditorInfo einfo = {sizeof(EditorInfo)};
	Info.EditorControl(-1, ECTL_GETINFO, 0, &einfo);
	InitialLine = einfo.CurLine;
	InitialCol = einfo.CurPos;
	InitialTop = einfo.TopScreenLine;
	TotalLines = einfo.TotalLines;

	nChanges = 0;
	bSequentialInput = true;
	bLineChanged = false;
	ctx = SearchAlloc();

	if (Info.DialogRun(hEdit)!=ixOk)
		UndoAll();

	RemoveHighlight();

	settings.Set(L"CaseSensitive", bPrevCaseSensitive);

	Info.DialogFree(hEdit);
	if (Length(PrevPattern)<MAX_PATTERN_LEN)
		SearchFree(ctx);
	else
		SearchHFree(ctx);
	SetLength(&PrevPattern,0);

	return INVALID_HANDLE_VALUE;
}
