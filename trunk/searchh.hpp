#pragma once
#include <windows.h>

HANDLE SearchHAlloc(); //�������� ������ ��� ������, ������� ��������
void SearchHFree(HANDLE context); //���������� ������,  ���������� ��� ������
//����������� �������� ��� ������ �� �������
void SearchHPrepare(HANDLE context, const wchar_t *pattern, WORD nPattern,
	bool bCaseSensitive);
//������ ��������� � ������ �������, ��������� ����������
const wchar_t *SearchH(HANDLE context, const wchar_t *pattern, WORD nPattern,
	const wchar_t *str, size_t nStr, bool bCaseSensitive);
