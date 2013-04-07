#pragma once
#include <windows.h>

HANDLE SearchHAlloc(); //выделить пам€ть дл€ поиска, вернуть контекст
void SearchHFree(HANDLE context); //освободить пам€ть,  выделенную дл€ поиска
//подготовить контекст дл€ поиска по шаблону
void SearchHPrepare(HANDLE context, const wchar_t *pattern, WORD nPattern,
	bool bCaseSensitive);
//искать вхождение в строке шаблона, заданного контекстом
const wchar_t *SearchH(HANDLE context, const wchar_t *pattern, WORD nPattern,
	const wchar_t *str, size_t nStr, bool bCaseSensitive);
