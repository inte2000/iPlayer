#include <cstring>
#include <memory>

#include <windows.h>

#include "CDTrackDecoderControl.h"
#include "PlugMain.h"

const char8_t* plugname = u8"CD Track decoder";
const char8_t* plugpublisher = u8"imPlayer Group";

namespace {

char errorMsg[256] = {};

typedef struct tagFileExtRegItem
{
    uint32_t st;
    const char* desc;
    const char* extList;
} FileExtRegItem;

typedef struct tagDecoderContext
{
    AudioContextHeader hdr;
    std::wstring mediaName;
    std::unique_ptr<CDTrackDecoderControl> control;
} DecoderContext;

void SyncHeaderByControl(DecoderContext* ctx)
{
    if ((ctx == nullptr) || !ctx->control) {
        return;
    }

    ctx->hdr.streamCount = 1;
    ctx->hdr.m_totalFrames = static_cast<long long>(ctx->control->TotalFrames());
    ctx->hdr.durations = static_cast<float>(static_cast<double>(ctx->control->TotalFrames()) / ctx->control->SourceFormat().sampleRate);
}

} // namespace

int WINAPI Plug_OnRegister(const ApplicationConfig* app, PluginRegister* regInfo)
{
    if (app->major_ver > 5)
    {
        strcpy_s(errorMsg, "This plugin module only tested for version below 5.0");
        return -1;
    }
    if (regInfo->size != sizeof(PluginRegister))
    {
        strcpy_s(errorMsg, "This plugin module version not match with host app");
        return -2;
    }

    const FileExtRegItem fmtMap[] = {
        {StreamFormatCDT, "Audio CD Track", ".cdtrack"},
    };

    uint32_t formatCount = static_cast<uint32_t>(sizeof(fmtMap) / sizeof(fmtMap[0]));
    if (formatCount > PLUG_FORMAT_MAX_LIMIT) {
        formatCount = PLUG_FORMAT_MAX_LIMIT;
    }

    for (uint32_t i = 0; i < formatCount; ++i)
    {
        regInfo->fmt_reg[i].id = fmtMap[i].st;
        strcpy_s(regInfo->fmt_reg[i].desc, fmtMap[i].desc);
        strcpy_s(regInfo->fmt_reg[i].ext_list, fmtMap[i].extList);
    }
    regInfo->format_count = formatCount;

    return 0;
}

void WINAPI Plug_GetErrMessage(char* msgBuf, uint32_t bufSize)
{
    if ((msgBuf == nullptr) || (bufSize == 0)) {
        return;
    }

    strcpy_s(msgBuf, bufSize, errorMsg);
}

uint32_t WINAPI Plug_ParseFileTypeID(const char* filename)
{
    (void)filename;
    return StreamFormatUnknown;
}

int WINAPI Plug_GetPluginInformation(PluginInfo* info)
{
    if (info->size != sizeof(PluginInfo))
    {
        strcpy_s(errorMsg, "This plugin module version not match with host app");
        return -2;
    }

    info->plug_type = PluginType::Decoder;
    info->ver_major = 1;
    info->ver_minor = 1;
    strcpy_s(info->name, 64, reinterpret_cast<const char*>(plugname));
    strcpy_s(info->publisher, 128, reinterpret_cast<const char*>(plugpublisher));

    return 0;
}

void* WINAPI Plug_OnInitialize(const PluginInitialize* init)
{
    if ((init == nullptr) || (init->pStream == nullptr))
    {
        strcpy_s(errorMsg, "Invalid initialize parameter.");
        return nullptr;
    }

    auto* ctx = new DecoderContext{};
    if (ctx == nullptr) {
        return nullptr;
    }

    ctx->hdr.size = sizeof(AudioContextHeader);
    ctx->hdr.filesize = init->pStream->GetLength();
    ctx->hdr.streamCount = 1;
    ctx->hdr.streamIndex = static_cast<uint32_t>(-1);
    ctx->mediaName = init->pStream->GetName();

    ctx->control = std::make_unique<CDTrackDecoderControl>();
    if (!ctx->control->Init(init->pStream))
    {
        strcpy_s(errorMsg, "Failed to initialize CD track decoder control.");
        delete ctx;
        return nullptr;
    }

    SyncHeaderByControl(ctx);
    return ctx;
}

int WINAPI Plug_StartStream(void* ctxhdr, const PluginStart* param)
{
    DecoderContext* ctx = static_cast<DecoderContext*>(ctxhdr);
    const uint32_t reqStreamIdx = (param == nullptr) ? static_cast<uint32_t>(-1) : param->mediaStreamIdx;
    if (!ctx)
    {
        sprintf_s(errorMsg, "Failed to start media stream [%u]: plus not initialized!", reqStreamIdx);
        return -1;
    }
    if ((param == nullptr) || !ctx->control)
    {
        strcpy_s(errorMsg, "Failed to start media stream: invalid start parameter.");
        return -1;
    }
    if (!ctx->control->Start(param->mediaStreamIdx))
    {
        sprintf_s(errorMsg, "Failed to start media stream [%u]: unsupported stream index.", param->mediaStreamIdx);
        return -1;
    }

    ctx->hdr.streamIndex = 0;
    SyncHeaderByControl(ctx);
    return 0;
}

int WINAPI Plug_StopStream(void* ctxhdr, uint32_t mediaStreamIdx)
{
    DecoderContext* ctx = static_cast<DecoderContext*>(ctxhdr);
    if (!ctx || !ctx->control)
    {
        sprintf_s(errorMsg, "Failed to stop media stream [%u]: plus not initialized!", mediaStreamIdx);
        return -1;
    }

    ctx->control->Stop();
    ctx->hdr.streamIndex = static_cast<uint32_t>(-1);
    return 0;
}

int WINAPI Plug_IsSupportOutput(void* ctxhdr, uint32_t mediaStreamIdx, const AudioFormat* audioFmt)
{
    DecoderContext* ctx = static_cast<DecoderContext*>(ctxhdr);
    if (!ctx || !ctx->control)
    {
        strcpy_s(errorMsg, "Current audio stream is closed!!");
        return -1;
    }

    if ((mediaStreamIdx != 0) && (mediaStreamIdx != static_cast<uint32_t>(-1)))
    {
        sprintf_s(errorMsg, "Unsupported media stream index: %u", mediaStreamIdx);
        return 0;
    }

    return CDTrackDecoderControl::IsSupportOutputFormat(audioFmt) ? 1 : 0;
}

int WINAPI Plug_IsCanSeeking(void* ctxhdr, uint32_t mediaStreamIdx)
{
    DecoderContext* ctx = static_cast<DecoderContext*>(ctxhdr);
    if (!ctx || !ctx->control)
    {
        strcpy_s(errorMsg, "Current audio stream is closed!!");
        return 0;
    }

    if ((mediaStreamIdx != 0) && (mediaStreamIdx != static_cast<uint32_t>(-1)))
    {
        sprintf_s(errorMsg, "Unsupported media stream index: %u", mediaStreamIdx);
        return 0;
    }

    return ctx->control->IsCanSeeking() ? 1 : 0;
}

void WINAPI Plug_OnUninitialize(void* ctxhdr)
{
    DecoderContext* ctx = static_cast<DecoderContext*>(ctxhdr);
    if (!ctx) {
        return;
    }

    delete ctx;
}

uint32_t WINAPI Plug_DecodeFrames(void* ctx, void* pBuf, uint32_t frames, const AudioFormat* audioFmt)
{
    DecoderContext* decCtx = static_cast<DecoderContext*>(ctx);
    if (!decCtx || !decCtx->control || !decCtx->control->IsStarted())
    {
        strcpy_s(errorMsg, "Current audio stream is closed!!");
        return 0;
    }

    const uint32_t readFrames = decCtx->control->DecodeFrames(pBuf, frames, audioFmt);
    decCtx->hdr.streamIndex = 0;
    return readFrames;
}

void WINAPI Plug_SeekToFrame(void* ctx, std::size_t frames)
{
    DecoderContext* decCtx = static_cast<DecoderContext*>(ctx);
    if (!decCtx || !decCtx->control) {
        return;
    }

    decCtx->control->SeekToFrame(frames);
    decCtx->hdr.streamIndex = 0;
}

int WINAPI Plug_QueryMetaInfo(void* ctxhdr, uint32_t streamIdx, AudioMetaTags* metaTags)
{
    DecoderContext* ctx = static_cast<DecoderContext*>(ctxhdr);
    if (!ctx || !ctx->control)
    {
        strcpy_s(errorMsg, "Current audio stream is closed!!");
        return -1;
    }
    if ((metaTags == nullptr) || (metaTags->size < sizeof(AudioMetaTags)))
    {
        strcpy_s(errorMsg, "Failed to query meta info: metaTags size not match!");
        return -1;
    }

    if (streamIdx == static_cast<uint32_t>(-1)) {
        streamIdx = 0;
    }
    if (streamIdx != 0)
    {
        sprintf_s(errorMsg, "Unsupported media stream index: %u", streamIdx);
        return -1;
    }

    metaTags->size = sizeof(AudioMetaTags);
    metaTags->streamIdx = streamIdx;
    metaTags->tags.Clear();

    const DsMetaInfo* info = ctx->control->GetMetaInfo();
    if (info != nullptr) {
        metaTags->tags = info->itemTag;
    }

    return 0;
}

int WINAPI Plug_GetAudioStatusInfo(void* ctxhdr, PluginAudioInfo* info)
{
    DecoderContext* ctx = static_cast<DecoderContext*>(ctxhdr);
    if (!ctx || !ctx->control)
    {
        strcpy_s(errorMsg, "Current audio stream is closed!!");
        return -1;
    }
    if ((info == nullptr) || (info->size < sizeof(PluginAudioInfo)))
    {
        strcpy_s(errorMsg, "Failed to get audio status: info size not match!");
        return -1;
    }

    info->size = sizeof(PluginAudioInfo);
    if ((info->flags & AudioInfoFlagFormat) != 0) {
        info->audioFmt = ctx->control->SourceFormat();
    }
    if ((info->flags & AudioInfoFlagActiveStream) != 0) {
        info->mediaStreamIdx = ctx->control->IsStarted() ? 0 : static_cast<uint32_t>(-1);
    }
    if ((info->flags & AudioInfoFlagStreamCount) != 0) {
        info->mediaStreamCount = 1;
    }
    if ((info->flags & AudioInfoFlagCurrentFrames) != 0) {
        info->currentFrames = ctx->control->CurrentFrames();
    }
    if ((info->flags & AudioInfoFlagTotalFrames) != 0) {
        info->totalFrames = ctx->control->TotalFrames();
    }

    return 0;
}

void WINAPI Plug_ResetDecoder(void* ctxhdr)
{
    DecoderContext* ctx = static_cast<DecoderContext*>(ctxhdr);
    if (!ctx || !ctx->control)
    {
        strcpy_s(errorMsg, "Current audio stream is closed!!");
        return;
    }

    ctx->control->SeekToFrame(0);
}

void WINAPI Plug_ConfigPlugin(HWND hWnd)
{
    (void)hWnd;
}
