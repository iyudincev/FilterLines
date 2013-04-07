/*
    FilterLines plugin for FAR Manager
    Copyright (C) 2009 Igor Yudincev

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
#include <windows.h>

//вернуть длину строки без завершающего нулевого символа
int Length(wchar_t *str)
{
	if (str == NULL) return 0;
	HANDLE hHeap = GetProcessHeap();
	return ((int)HeapSize(hHeap, 0, str) / sizeof(wchar_t)) - 1;
}

//выделить память под len+1 символов; если len=0, то освободить память
bool SetLength(wchar_t **pStr, int len)
{
	HANDLE hHeap = GetProcessHeap();
	if (*pStr==NULL) {
		if (len > 0) {
			*pStr = (wchar_t*)HeapAlloc(hHeap, 0, (len + 1) * sizeof(wchar_t));
			if (*pStr == NULL) return false;
		}
		return true;
	}
	//*pStr!=NULL
	if (len > 0) {
		*pStr = (wchar_t*)HeapReAlloc(hHeap, 0, *pStr, (len+1)*sizeof(wchar_t));
		if (*pStr == NULL) return false;
	}
	else {
		HeapFree(hHeap, 0, *pStr);
		*pStr = NULL;
	}
	return true;
}
