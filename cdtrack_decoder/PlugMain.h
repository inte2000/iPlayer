#pragma once

#include <cstddef>
#include <windows.h>

#include "iplugsdk.h"

extern "C" {
int WINAPI Plug_OnRegister(const ApplicationConfig* app, PluginRegister* regInfo);
void WINAPI Plug_GetErrMessage(char* msgBuf, uint32_t bufSize);
uint32_t WINAPI Plug_ParseFileTypeID(const char* filename);
int WINAPI Plug_GetPluginInformation(PluginInfo* info);
void* WINAPI Plug_OnInitialize(const PluginInitialize* init);
int WINAPI Plug_StartStream(void* ctxhdr, const PluginStart* param);
int WINAPI Plug_StopStream(void* ctxhdr, uint32_t mediaStreamIdx);
int WINAPI Plug_IsSupportOutput(void* ctxhdr, uint32_t mediaStreamIdx, const AudioFormat* audioFmt);
int WINAPI Plug_IsCanSeeking(void* ctxhdr, uint32_t mediaStreamIdx);
void WINAPI Plug_OnUninitialize(void* ctxhdr);
uint32_t WINAPI Plug_DecodeFrames(void* ctx, void* pBuf, uint32_t frames, const AudioFormat* audioFmt);
void WINAPI Plug_SeekToFrame(void* ctx, std::size_t frames);
int WINAPI Plug_QueryMetaInfo(void* ctxhdr, uint32_t streamIdx, AudioMetaTags* metaTags);
int WINAPI Plug_GetAudioStatusInfo(void* ctxhdr, PluginAudioInfo* info);
void WINAPI Plug_ResetDecoder(void* ctxhdr);
void WINAPI Plug_ConfigPlugin(HWND hWnd);
}
