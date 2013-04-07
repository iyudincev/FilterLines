/*
    Case-insensitive substring search
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

#if MAX_PATTERN_LEN <= 16
typedef WORD PatternMask;
#else
#if MAX_PATTERN_LEN <= 32
typedef DWORD PatternMask;
#else
#error MAX_PATTERN_LEN must be <= 32
#endif
#endif

typedef struct tagSearchContext {
	size_t length;           //длина строки шаблона
	PatternMask match;    //маска полного совпадения с шаблоном
	PatternMask positions[65536]; //маски позиций в шаблоне для каждого символа UCS-2
} SearchContext;

HANDLE SearchAlloc()
{
	SearchContext *ctx = (SearchContext *)malloc(sizeof(SearchContext)); //позиции букв
	memset(ctx->positions, 0xFF, sizeof(ctx->positions));
	return (HANDLE)ctx;
}

void SearchFree(HANDLE context)
{
	free((SearchContext *)context);
}

void SearchPrepare(HANDLE context, const wchar_t *pattern, bool bCaseSensitive)
{
	wchar_t c;
	SearchContext *ctx = (SearchContext *)context;
	ctx->length = wcslen(pattern);
	ctx->match = 0;
	for (int m=1; (c = *pattern)!=0; pattern++, m <<= 1) {
		if (bCaseSensitive) {
			ctx->positions[c] &= ~m;
		}
		else {
			ctx->positions[(size_t)CharLower((LPWSTR)c)] &= ~m;
			ctx->positions[(size_t)CharUpper((LPWSTR)c)] &= ~m;
		}
		ctx->match |= m;
	}
	ctx->match = ~(ctx->match >> 1);
}

void SearchUnprepare(HANDLE context, const wchar_t *pattern, bool bCaseSensitive)
{
	wchar_t c;
	SearchContext *ctx = (SearchContext *)context;
	for (; (c = *pattern) != 0; pattern++) {
		if (bCaseSensitive) {
			ctx->positions[c] = ~0;
		}
		else {
			ctx->positions[(size_t)CharLower((LPWSTR)c)] = ~0;
			ctx->positions[(size_t)CharUpper((LPWSTR)c)] = ~0;
		}
	}
}

const wchar_t *Search(HANDLE context, const wchar_t *str)
{
	size_t c; //wchar_t
	SearchContext *ctx = (SearchContext *)context;
	PatternMask shift = ~0;
	for (; (c = *str) != 0; str++) {
		shift = shift << 1 | ctx->positions[c];
		if (shift < ctx->match)
			return str - ctx->length + 1;
	}
	return NULL;
}
