/*
20260527 ��������
��ģ�ͣ�ChatGPT 5.3 Codex
����������todo_task_84.txt
*/
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <wavpack/wavpack.h>

#include "DataStream.h"
#include "StreamMateSource.h"
#include "AudioInfo.h"
#include "MediaTag.h"

class WavpackPlayCtrl
{
public:
    WavpackPlayCtrl();
    ~WavpackPlayCtrl();

    bool Init(CDataStream* stream, uint32_t streamFmt);
    void Release();

    bool OpenStream(uint32_t streamIndex);
    void StopStream(uint32_t streamIndex);

    bool IsSupportOutput(const AudioFormat* audioFmt) const;
    bool IsCanSeeking() const;

    uint32_t DecodeFrames(void* pBuf, uint32_t frames, const AudioFormat* audioFmt);
    void Seek(std::size_t frames);

    std::size_t GetCurrentFrame() const { return m_curFrames; }
    std::size_t GetTotalFrame() const { return m_totalFrames; }
    float GetDurationSeconds() const;
    uint32_t GetStreamCount() const { return 1; }
    uint32_t GetActiveStreamIndex() const { return m_activeStreamIdx; }
    const AudioFormat& GetSourceAudioFormat() const { return m_srcAudioFmt; }
    void FillMetaTags(CMediaTag& tags) const;

private:
    typedef struct tagStreamSource
    {
        CDataStream* stream = nullptr;
        bool seekable = false;
        uint8_t pushbackByte = 0;
        bool hasPushback = false;
    } StreamSource;

    static int32_t ReadBytes(void* id, void* data, int32_t bcount);
    static int32_t WriteBytes(void* id, void* data, int32_t bcount);
    static int64_t GetPos(void* id);
    static int SetPosAbs(void* id, int64_t pos);
    static int SetPosRel(void* id, int64_t delta, int mode);
    static int PushBackByte(void* id, int c);
    static int64_t GetLength(void* id);
    static int CanSeek(void* id);
    static int TruncateHere(void* id);
    static int CloseReader(void* id);

    void InitStreamSource(StreamSource& source, CDataStream* pStream);
    std::unique_ptr<CDataStream> OpenWvcStream(CDataStream* pStream);
    void InitWavpackReader(WavpackStreamReader64& reader);
    std::string ReadTagValue(const char* key) const;

private:
    CDataStream* m_stream;
    std::unique_ptr<CDataStream> m_wvcStream;
    StreamSource m_wvSource;
    StreamSource m_wvcSource;
    WavpackContext* m_ctx;
    WavpackStreamReader64 m_reader;
    AudioFormat m_srcAudioFmt;
    std::size_t m_curFrames;
    std::size_t m_totalFrames;
    uint32_t m_streamFmt;
    uint32_t m_activeStreamIdx;
    uint32_t m_sourceBitsPerSample;
    bool m_sourceIsFloat;
    std::vector<int32_t> m_tempS32;
};
