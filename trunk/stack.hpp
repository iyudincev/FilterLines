#pragma once

void StackPush(void *p);
void *StackPop();
void StackClean();
void *StackPeek(size_t iNestLevel = 0);
