#pragma once
#include <windows.h>

#define MAX_PATTERN_LEN 16

HANDLE SearchAlloc(); //выделить память для поиска, вернуть контекст
void SearchFree(HANDLE context); //освободить память,  выделенную для поиска
//подготовить контекст для поиска по шаблону
void SearchPrepare(HANDLE context, const wchar_t *pattern, bool bCaseSensitive);
//вернуть контекст в исходное состояние для последующих поисков
void SearchUnprepare(HANDLE context, const wchar_t *pattern, bool bCaseSensitive);
//искать вхождение в строке шаблона, заданного контекстом
const wchar_t *Search(HANDLE context, const wchar_t *str);
