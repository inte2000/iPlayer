#ifndef AUDIO_SOURCE_H
#define AUDIO_SOURCE_H

#include <memory>
#include <string>
#include "AudioInfo.h"
#include "DataStream.h"
#include "AudioDecoder.h"
#include "SoxEffects.h"

//const uint32_t DECODE_BUF_FRAMES = 16384;
constexpr uint32_t DECODE_BUF_FRAMES = 32 * 1024; //64k

class CAudioSource
{
public:
    CAudioSource();
    CAudioSource(std::unique_ptr<CDataStream> stream, std::unique_ptr<CAudioDecoder> decoder, uint32_t streamFmt);
    virtual ~CAudioSource();

    std::wstring GetName() const;
    const CAudioDecoder* GetDecoder() const { return m_decoder.get(); }
    AudioFormat GetAudioFormat() const { return m_decoder->GetAudioFormat(-1); }
    const AudioFormat& GetOutputFormat() const { return m_outFmt; }
    uint32_t GetTotalAudioStreams() const { return (m_decoder != nullptr) ? m_decoder->GetAudioStreamCount() : 0; }
    CMediaTag GetMetaInformation() const { return m_decoder->GetTags(-1); }
    bool SetOutputFormat(const AudioFormat& outFmt, bool bDevide);
    void Reset(); //将 CDataStream 和 CAudioDecoder 重置到初始状态
    bool StartAudioStream(uint32_t streamIdx, std::size_t begin = 0, std::size_t end = -1, uint32_t loop = 1);
    void StopAudioStream(uint32_t streamIdx);
    uint32_t ReadBuffer(uint8_t* buf, uint32_t bufSize, uint32_t frames);
    float GetTotalSeconds() const;
    std::size_t SourceFrameToDeviceFrame(std::size_t srcFrame) const;
    std::size_t DeviceFrameToSourceFrame(std::size_t devFrame) const;

    void SeekToFrame(std::size_t frames);
    bool IsSeekOnGoing() const { return m_bSeekOnGoing; }
    void ClearSeekOnGoing() { m_bSeekOnGoing = false; }
    std::size_t GetCurrentFrame() const;
    std::size_t GetTotalFrames() const;
    bool IsLastAudioStream() const;
    bool IsDecodingFinished() const { return m_bDecodingFinished; }
    void SetDecodingFinished(bool bFinished) { m_bDecodingFinished = bFinished; }

    float FramesToSeconds(std::size_t frames) const;
    std::size_t SecondsToFrames(float seconds) const;

private:
    bool m_bDeviceOutput;
    AudioFormat m_outFmt;
    AudioFormat m_decodeFmt; 
    std::string m_name; // Audio source name, file name or net server name
    std::unique_ptr<CDataStream> m_stream;
    std::unique_ptr<CAudioDecoder> m_decoder;
    bool m_bSeekOnGoing;
    bool m_bDecodingFinished;

    CSoxEffects m_Effects;
    bool m_needConvert;
    std::unique_ptr<uint8_t[]> m_tmpBuf; 
    uint32_t m_tmpSize;
};

std::unique_ptr<CAudioSource> MakeFileAudioSource(const std::wstring& filename);
std::unique_ptr<CAudioSource> MakeCDTrackAudioSource(const std::wstring& sourceName, uint32_t track);


#endif //AUDIO_SOURCE_H
