/*
    Case-insensitive substring search using BoyerЦMooreЦHorspool algorithm
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
#include "arraysize.h"

typedef struct tagSearchHContext {
	WORD positions[65536];
} SearchHContext;

HANDLE SearchHAlloc()
{
	SearchHContext *ctx = (SearchHContext *)malloc(sizeof(SearchHContext)); //позиции букв
	return (HANDLE)ctx;
}

void SearchHFree(HANDLE context)
{
	free((SearchHContext *)context);
}

void SearchHPrepare(HANDLE context, const wchar_t *pattern, WORD nPattern, 
	bool bCaseSensitive)
{
	wchar_t c;
	SearchHContext *ctx = (SearchHContext *)context;
	for (size_t i=0; i < ArraySize(ctx->positions); i++)
		ctx->positions[i] = nPattern;
	for (int i=0; i < nPattern - 1; i++) {
		c = pattern[i];
		if (bCaseSensitive) {
			ctx->positions[c] = nPattern - i - 1;
		}
		else {
			ctx->positions[(size_t)CharLower((LPWSTR)c)] = nPattern - i - 1;
			ctx->positions[(size_t)CharUpper((LPWSTR)c)] = nPattern - i - 1;
		}
	}
}

//!!! передаЄм шаблон pattern приведЄнным к нижнему регистру
const wchar_t *SearchH(HANDLE context, const wchar_t *pattern, WORD nPattern,
	const wchar_t *str, size_t nStr, bool bCaseSensitive)
{
	if (nPattern == 0) return str;
	register const wchar_t *pStr = str;
	register size_t iStr = nStr;
	size_t last = nPattern - 1;
	SearchHContext *ctx = (SearchHContext *)context;
	if (bCaseSensitive)
		while (iStr >= nPattern) {
 			for (size_t scan = last; pStr[scan] == pattern[scan]; scan--) {
				if (scan == 0) return pStr;
			}
			WORD delta = ctx->positions[pStr[last]];
			iStr -= delta;
			pStr += delta;
		}
	else
		while (iStr >= nPattern) {
			for (size_t scan = last; (wchar_t)CharLower((LPWSTR)pStr[scan]) == pattern[scan]; scan--) {
				if (scan == 0) return pStr;
			}
			WORD delta = ctx->positions[pStr[last]];
			iStr -= delta;
			pStr += delta;
		}
	return NULL;
}
