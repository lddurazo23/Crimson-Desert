#pragma once
enum MH_STATUS {MH_OK,MH_ERROR_ALREADY_INITIALIZED};
extern MH_STATUS MH_Initialize();
extern MH_STATUS MH_CreateHook(void*,void*,void**);
extern MH_STATUS MH_EnableHook(void*);
extern MH_STATUS MH_RemoveHook(void*);
