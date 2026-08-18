#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

#include <windows.h>

#include "PlugMain.h"
#include "StreamMetaSource.h"
#include "StreamSectorSource.h"

const char8_t* plugname = u8"CD Track decoder";
const char8_t* plugpublisher = u8"imPlayer Group";

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
    CDataStream* stream;
    MetaSource* metaSource;
    SectorSource* sectorSource;
    AudioFormat sourceFmt;
    std::size_t currentFrames;
    std::size_t totalFrames;
    uint32_t framesPerSector;
    bool started;
    std::vector<uint8_t> cache;
    std::size_t cacheOffset;
} DecoderContext;

char errorMsg[256] = {};

static void InitSourceAudioFormat(AudioFormat& fmt)
{
    InitAudioFormat(&fmt, AudioDataFormat::PCM_S16, 2, 44100, 16);
}

static bool IsSupportS16Stereo(const AudioFormat* audioFmt)
{
    if (audioFmt == nullptr) {
        return false;
    }

    if (audioFmt->format != AudioDataFormat::PCM_S16) {
        return false;
    }
    if (audioFmt->numChannels != 2) {
        return false;
    }
    if ((audioFmt->bitsPerSample != 0) && (audioFmt->bitsPerSample != 16)) {
        return false;
    }

    return true;
}

static void ConvertCDAudioToLittleEndian(uint8_t* data, uint32_t bytes)
{
    if (data == nullptr) {
        return;
    }

    const uint32_t convertBytes = bytes - (bytes % 2);
    for (uint32_t i = 0; i < convertBytes; i += 2)
    {
        const uint8_t high = data[i];
        data[i] = data[i + 1];
        data[i + 1] = high;
    }
}

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
        {StreamFormatCDA, "Audio CD Track", ".cda"},
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
    info->ver_minor = 0;
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

    DecoderContext* pCtx = new DecoderContext{};
    if (pCtx == nullptr) {
        return nullptr;
    }

    pCtx->hdr.size = sizeof(AudioContextHeader);
    pCtx->hdr.filesize = init->pStream->GetLength();
    pCtx->hdr.streamCount = 1;
    pCtx->hdr.streamIndex = static_cast<uint32_t>(-1);
    pCtx->mediaName = init->pStream->GetName();
    pCtx->stream = init->pStream;
    pCtx->metaSource = init->pStream->QuerySource<MetaSource>();
    pCtx->sectorSource = init->pStream->QuerySource<SectorSource>();
    pCtx->currentFrames = 0;
    pCtx->started = false;
    pCtx->cacheOffset = 0;
    InitSourceAudioFormat(pCtx->sourceFmt);

    if (pCtx->sectorSource == nullptr)
    {
        strcpy_s(errorMsg, "CD track stream missing SectorSource interface.");
        delete pCtx;
        return nullptr;
    }
    if (pCtx->sectorSource->Type() != SectorSourceType::AudioCD)
    {
        strcpy_s(errorMsg, "Current SectorSource type is unsupported.");
        delete pCtx;
        return nullptr;
    }

    const uint32_t sectorSize = pCtx->sectorSource->SectorSize();
    if ((sectorSize == 0) || ((sectorSize % pCtx->sourceFmt.blockAlign) != 0))
    {
        strcpy_s(errorMsg, "Invalid AudioCD sector size.");
        delete pCtx;
        return nullptr;
    }

    pCtx->framesPerSector = sectorSize / pCtx->sourceFmt.blockAlign;
    pCtx->totalFrames = static_cast<std::size_t>(pCtx->sectorSource->GetSectorsCount()) * pCtx->framesPerSector;
    pCtx->hdr.m_totalFrames = static_cast<long long>(pCtx->totalFrames);
    pCtx->hdr.durations = static_cast<float>(static_cast<double>(pCtx->totalFrames) / pCtx->sourceFmt.sampleRate);

    return pCtx;
}

int WINAPI Plug_StartStream(void* ctxhdr, const PluginStart* param)
{
    DecoderContext* pCtx = static_cast<DecoderContext*>(ctxhdr);
    const uint32_t reqStreamIdx = (param == nullptr) ? static_cast<uint32_t>(-1) : param->mediaStreamIdx;
    if (!pCtx)
    {
        sprintf_s(errorMsg, "Failed to start media stream [%u]: plus not initialized!", reqStreamIdx);
        return -1;
    }
    if ((param == nullptr) || (pCtx->stream == nullptr) || (pCtx->sectorSource == nullptr))
    {
        strcpy_s(errorMsg, "Failed to start media stream: invalid start parameter.");
        return -1;
    }
    if ((param->mediaStreamIdx != 0) && (param->mediaStreamIdx != static_cast<uint32_t>(-1)))
    {
        sprintf_s(errorMsg, "Unsupported media stream index: %u", param->mediaStreamIdx);
        return -1;
    }

    pCtx->sectorSource->SeekToSector(pCtx->sectorSource->GetStartSectors());
    pCtx->stream->Seek(SeekBase::Begin, 0);
    pCtx->currentFrames = 0;
    pCtx->cache.clear();
    pCtx->cacheOffset = 0;
    pCtx->started = true;

    pCtx->hdr.streamIndex = 0;
    pCtx->hdr.streamCount = 1;
    return 0;
}

int WINAPI Plug_StopStream(void* ctxhdr, uint32_t mediaStreamIdx)
{
    DecoderContext* pCtx = static_cast<DecoderContext*>(ctxhdr);
    if (!pCtx)
    {
        sprintf_s(errorMsg, "Failed to stop media stream [%u]: plus not initialized!", mediaStreamIdx);
        return -1;
    }

    pCtx->started = false;
    pCtx->hdr.streamIndex = static_cast<uint32_t>(-1);
    return 0;
}

int WINAPI Plug_IsSupportOutput(void* ctxhdr, uint32_t mediaStreamIdx, const AudioFormat* audioFmt)
{
    DecoderContext* pCtx = static_cast<DecoderContext*>(ctxhdr);
    if (!pCtx)
    {
        strcpy_s(errorMsg, "Current audio stream is closed!!");
        return -1;
    }

    if ((mediaStreamIdx != 0) && (mediaStreamIdx != static_cast<uint32_t>(-1)))
    {
        sprintf_s(errorMsg, "Unsupported media stream index: %u", mediaStreamIdx);
        return 0;
    }

    return IsSupportS16Stereo(audioFmt) ? 1 : 0;
}

int WINAPI Plug_IsCanSeeking(void* ctxhdr, uint32_t mediaStreamIdx)
{
    DecoderContext* pCtx = static_cast<DecoderContext*>(ctxhdr);
    if (!pCtx)
    {
        strcpy_s(errorMsg, "Current audio stream is closed!!");
        return 0;
    }

    if ((mediaStreamIdx != 0) && (mediaStreamIdx != static_cast<uint32_t>(-1)))
    {
        sprintf_s(errorMsg, "Unsupported media stream index: %u", mediaStreamIdx);
        return 0;
    }

    return (pCtx->sectorSource != nullptr) ? 1 : 0;
}

void WINAPI Plug_OnUninitialize(void* ctxhdr)
{
    DecoderContext* pCtx = static_cast<DecoderContext*>(ctxhdr);
    if (!pCtx) {
        return;
    }

    delete pCtx;
}

uint32_t WINAPI Plug_DecodeFrames(void* ctx, void* pBuf, uint32_t frames, const AudioFormat* audioFmt)
{
    DecoderContext* pCtx = static_cast<DecoderContext*>(ctx);
    if (!pCtx || !pCtx->started || (pCtx->stream == nullptr) || (pBuf == nullptr))
    {
        strcpy_s(errorMsg, "Current audio stream is closed!!");
        return 0;
    }
    if (!IsSupportS16Stereo(audioFmt)) {
        return 0;
    }
    if (frames == 0) {
        return 0;
    }

    const uint32_t blockAlign = pCtx->sourceFmt.blockAlign;
    const std::size_t targetBytes = static_cast<std::size_t>(frames) * blockAlign;
    uint8_t* outBuf = static_cast<uint8_t*>(pBuf);
    std::size_t copied = 0;
    const uint32_t sectorSize = pCtx->sectorSource->SectorSize();

    while (copied < targetBytes)
    {
        if (pCtx->cacheOffset < pCtx->cache.size())
        {
            const std::size_t cacheLeft = pCtx->cache.size() - pCtx->cacheOffset;
            const std::size_t needBytes = targetBytes - copied;
            const std::size_t copyBytes = std::min(cacheLeft, needBytes);
            std::memcpy(outBuf + copied, pCtx->cache.data() + pCtx->cacheOffset, copyBytes);
            copied += copyBytes;
            pCtx->cacheOffset += copyBytes;
            continue;
        }

        std::vector<uint8_t> readBuf(sectorSize + targetBytes - copied);
        const uint32_t readBytes = pCtx->stream->Read(readBuf.data(), static_cast<uint32_t>(targetBytes - copied));
        if (readBytes == 0) {
            break;
        }

        readBuf.resize(readBytes);
        ConvertCDAudioToLittleEndian(readBuf.data(), readBytes);
        pCtx->cache.swap(readBuf);
        pCtx->cacheOffset = 0;
    }

    const uint32_t readFrames = static_cast<uint32_t>(copied / blockAlign);
    pCtx->currentFrames += readFrames;
    pCtx->hdr.streamIndex = 0;

    return readFrames;
}

void WINAPI Plug_SeekToFrame(void* ctx, std::size_t frames)
{
    DecoderContext* pCtx = static_cast<DecoderContext*>(ctx);
    if (!pCtx || (pCtx->sectorSource == nullptr) || (pCtx->stream == nullptr)) {
        return;
    }

    const std::size_t totalFrames = pCtx->totalFrames;
    if (frames > totalFrames) {
        frames = totalFrames;
    }

    const uint64_t startSector = pCtx->sectorSource->GetStartSectors();
    const uint32_t sectorSize = pCtx->sectorSource->SectorSize();
    const std::size_t sectorIndex = frames / pCtx->framesPerSector;
    const std::size_t frameInsideSector = frames % pCtx->framesPerSector;
    const uint64_t targetSector = startSector + static_cast<uint64_t>(sectorIndex);

    pCtx->sectorSource->SeekToSector(targetSector);
    pCtx->stream->Seek(SeekBase::Begin, static_cast<long long>(sectorIndex * sectorSize));
    pCtx->cache.clear();
    pCtx->cacheOffset = 0;

    if (frameInsideSector > 0)
    {
        std::vector<uint8_t> secBuf(sectorSize);
        const uint32_t got = pCtx->stream->Read(secBuf.data(), sectorSize);
        if (got > 0)
        {
            secBuf.resize(got);
            ConvertCDAudioToLittleEndian(secBuf.data(), got);
            pCtx->cache.swap(secBuf);
            pCtx->cacheOffset = std::min(pCtx->cache.size(), frameInsideSector * pCtx->sourceFmt.blockAlign);
        }
    }

    pCtx->currentFrames = frames;
    pCtx->hdr.streamIndex = 0;
}

int WINAPI Plug_QueryMetaInfo(void* ctxhdr, uint32_t streamIdx, AudioMetaTags* metaTags)
{
    DecoderContext* pCtx = static_cast<DecoderContext*>(ctxhdr);
    if (!pCtx)
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

    if (pCtx->metaSource != nullptr)
    {
        const DsMetaInfo* info = pCtx->metaSource->GetMetaInformation();
        if (info != nullptr) {
            metaTags->tags = info->itemTag;
        }
    }

    return 0;
}

int WINAPI Plug_GetAudioStatusInfo(void* ctxhdr, PluginAudioInfo* info)
{
    DecoderContext* pCtx = static_cast<DecoderContext*>(ctxhdr);
    if (!pCtx)
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
        info->audioFmt = pCtx->sourceFmt;
    }
    if ((info->flags & AudioInfoFlagActiveStream) != 0) {
        info->mediaStreamIdx = pCtx->started ? 0 : static_cast<uint32_t>(-1);
    }
    if ((info->flags & AudioInfoFlagStreamCount) != 0) {
        info->mediaStreamCount = 1;
    }
    if ((info->flags & AudioInfoFlagCurrentFrames) != 0) {
        info->currentFrames = pCtx->currentFrames;
    }
    if ((info->flags & AudioInfoFlagTotalFrames) != 0) {
        info->totalFrames = pCtx->totalFrames;
    }

    return 0;
}

void WINAPI Plug_ResetDecoder(void* ctxhdr)
{
    DecoderContext* pCtx = static_cast<DecoderContext*>(ctxhdr);
    if (!pCtx)
    {
        strcpy_s(errorMsg, "Current audio stream is closed!!");
        return;
    }

    Plug_SeekToFrame(pCtx, 0);
}

void WINAPI Plug_ConfigPlugin(HWND hWnd)
{
    (void)hWnd;
}
