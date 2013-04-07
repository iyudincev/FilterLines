#pragma once
#include <windows.h>

#define MAX_PATTERN_LEN 16

HANDLE SearchAlloc(); //�������� ������ ��� ������, ������� ��������
void SearchFree(HANDLE context); //���������� ������,  ���������� ��� ������
//����������� �������� ��� ������ �� �������
void SearchPrepare(HANDLE context, const wchar_t *pattern, bool bCaseSensitive);
//������� �������� � �������� ��������� ��� ����������� �������
void SearchUnprepare(HANDLE context, const wchar_t *pattern, bool bCaseSensitive);
//������ ��������� � ������ �������, ��������� ����������
const wchar_t *Search(HANDLE context, const wchar_t *str);
