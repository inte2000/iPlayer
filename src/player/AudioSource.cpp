#include <filesystem>
#include "AudioSource.h"
#include "FileStream.h"
#include "CDSectorsStream.h"
//#include "Utf8String.h"
#include "UnicodeConvert.h"

#include "DecoderFactory.h"
#include "DecoderParametersMgmt.h" 

static std::runtime_error MakeRuntimeError(const char* msg, const std::wstring& filename)
{
    std::string strMsg = msg;
    strMsg += Utf16LeToLocalMBCS(filename);

    return std::runtime_error(strMsg);
}

std::unique_ptr<CAudioSource> MakeFileAudioSource(const std::wstring& filename)
{
    std::unique_ptr<CDataStream> streamPtr = MakeFileStream(filename, true);
    if (!streamPtr)
        throw MakeRuntimeError("Fail to open file: ", filename);

    CDecoderFactory& factory = CDecoderFactory::GetInstance();
    uint32_t fileFmt = factory.ParseFileFormat(filename);
    std::unique_ptr<CAudioDecoder> decoderPtr = factory.MakeAudioDecoder(fileFmt);
    if(!decoderPtr)
        throw MakeRuntimeError("Fail to generate decoder for: ", filename);

    return std::make_unique<CAudioSource>(std::move(streamPtr), std::move(decoderPtr), fileFmt);
    //return std::unique_ptr<CAudioSource>(new CAudioSource{});
}

std::unique_ptr<CAudioSource> MakeCDTrackAudioSource(const std::wstring& sourceName, uint32_t track)
{
    if (track == 0) {
        throw std::runtime_error("CD track index must start from 1");
    }

    std::unique_ptr<CDataStream> streamPtr = MakeCDSectorsStream(sourceName, track);
    if (!streamPtr) {
        throw MakeRuntimeError("Fail to open CD track stream: ", sourceName);
    }

    CDecoderFactory& factory = CDecoderFactory::GetInstance();
    std::unique_ptr<CAudioDecoder> decoderPtr = factory.MakeAudioDecoder(StreamFormatCDT);
    if (!decoderPtr) {
        throw std::runtime_error("Fail to generate CD track decoder");
    }

    return std::make_unique<CAudioSource>(std::move(streamPtr), std::move(decoderPtr), StreamFormatCDT);
}

CAudioSource::CAudioSource()
{
    InitEmptyAudioFormat(&m_outFmt);
    m_bDeviceOutput = true;
    m_needConvert = false;
    m_bSeekOnGoing = false;
    m_bDecodingFinished = false;
}

CAudioSource::CAudioSource(std::unique_ptr<CDataStream> stream, std::unique_ptr<CAudioDecoder> decoder, uint32_t streamFmt)
{
    m_stream = std::move(stream);
    m_decoder = std::move(decoder);
    m_decoder->Attach(m_stream.get()); 
    InitEmptyAudioFormat(&m_outFmt);
    m_bDeviceOutput = true;
    CDecoderParametersMgmt& mgmt = CDecoderParametersMgmt::GetInstance();
    const CDecodeInitCtx* decodeInit = mgmt.GetDecodeParamter(streamFmt);
    m_decoder->InitDecode(decodeInit);      
    m_needConvert = false;
    m_bSeekOnGoing = false;
    m_bDecodingFinished = false;
}

CAudioSource::~CAudioSource()
{
    if (m_decoder)
        m_decoder->StopStream(-1); //关闭当前流
}

std::wstring CAudioSource::GetName() const
{
    if (!m_stream || !m_decoder)
        return L"";

    std::wstring name = m_stream->GetName();

    return name;
}

static AudioFormat GetMostMatchDecoderFormat(CAudioDecoder *pDecoder, const AudioFormat& audioFmt, const AudioFormat& deviceFmt)
{
    AudioFormat decodeFmt = audioFmt;
    if (deviceFmt.format == AudioDataFormat::PCM_S16)
    {
        decodeFmt.format = AudioDataFormat::PCM_S16;
        decodeFmt.bitsPerSample = 16;
        decodeFmt.blockAlign = sizeof(int16_t) * decodeFmt.numChannels;
        if (pDecoder->IsSupportOutput(&decodeFmt))
            return decodeFmt;
    }
    else if (deviceFmt.format == AudioDataFormat::PCM_S32)
    {
        decodeFmt.format = AudioDataFormat::PCM_S32;
        decodeFmt.bitsPerSample = 32;
        decodeFmt.blockAlign = sizeof(int32_t) * decodeFmt.numChannels;
        if (pDecoder->IsSupportOutput(&decodeFmt))
            return decodeFmt;
    }
    else if (deviceFmt.format == AudioDataFormat::PCM_S24)
    {
        decodeFmt.format = AudioDataFormat::PCM_S24;
        decodeFmt.bitsPerSample = 24;
        decodeFmt.blockAlign = 3 * decodeFmt.numChannels;
        if (pDecoder->IsSupportOutput(&decodeFmt))
            return decodeFmt;

        decodeFmt.format = AudioDataFormat::PCM_S24_32;
        decodeFmt.bitsPerSample = 24;
        decodeFmt.blockAlign = sizeof(int32_t) * decodeFmt.numChannels;
        if (pDecoder->IsSupportOutput(&decodeFmt))
            return decodeFmt;
    }
    else if (deviceFmt.format == AudioDataFormat::Float32)
    {
        decodeFmt.format = AudioDataFormat::Float32;
        decodeFmt.bitsPerSample = 32;
        decodeFmt.blockAlign = sizeof(int32_t) * decodeFmt.numChannels;
        if (pDecoder->IsSupportOutput(&decodeFmt))
            return decodeFmt;
    }

    decodeFmt = audioFmt;

    return decodeFmt;
}

bool CAudioSource::SetOutputFormat(const AudioFormat& outFmt, bool bDevide)
{
    m_needConvert = false;
    m_outFmt = outFmt;
    m_bDeviceOutput = bDevide;
    if (!m_decoder->IsSupportOutput(&outFmt))
    {
        m_needConvert = true;
        AudioFormat audioFmt = m_decoder->GetAudioFormat(-1);
        //这里的处理原则是尽量不做音频数据格式的转换，只要格式一样，m_Effects 就只做采样和通道的转换和映射
        m_decodeFmt = GetMostMatchDecoderFormat(m_decoder.get(), audioFmt, outFmt);


        uint32_t totalBytes = m_decodeFmt.blockAlign * DECODE_BUF_FRAMES;
        m_tmpBuf.reset();
        m_tmpBuf = std::make_unique<uint8_t[]>(totalBytes);
        m_tmpSize = totalBytes; 

        m_Effects.Release();
        if (!m_Effects.Init(m_decodeFmt, m_outFmt)) 
            return false;
    }
    else
    {
        m_decodeFmt = m_outFmt;

        uint32_t bytesPerSample = GetBitsPerSampleByFormat(m_outFmt.format) / 8;
        uint32_t totalBytes = bytesPerSample * m_outFmt.numChannels * DECODE_BUF_FRAMES;
        m_tmpBuf.reset();
        m_tmpBuf = std::make_unique<uint8_t[]>(totalBytes);
        m_tmpSize = totalBytes; 

        m_Effects.Release();
        if (!m_Effects.Init(m_outFmt, m_outFmt)) 
            return false;
    }

    return true;
}

void CAudioSource::Reset()
{
    InitEmptyAudioFormat(&m_outFmt);
    m_needConvert = false;
    m_bSeekOnGoing = false;

    m_Effects.Release();
    m_tmpBuf.reset();
    m_tmpSize = 0; 

    m_decoder->Reset();
}

bool CAudioSource::StartAudioStream(uint32_t streamIdx, std::size_t begin, std::size_t end, uint32_t loop)
{
    if (!m_stream || !m_decoder)
        return false;

    return m_decoder->StartStream(streamIdx, begin, end, loop);
}

void CAudioSource::StopAudioStream(uint32_t streamIdx)
{
    if (!m_stream || !m_decoder)
        return;

    m_decoder->StopStream(streamIdx);
}

//如果没有指定解码格式，就使用 audio source 自己的格式
uint32_t CAudioSource::ReadBuffer(uint8_t* buf, uint32_t bufSize, uint32_t frames)
{
    if (!m_stream || !m_decoder)
        return 0;

    if (m_decoder->GetCurrentFrame() >= m_decoder->GetTotalFrame())
        return 0;
    
    if (m_outFmt.format == AudioDataFormat::UNKNOWN)
    {
        InitEmptyAudioFormat(&m_outFmt);
        AudioFormat audioFmt = m_decoder->GetAudioFormat(-1);
        return m_decoder->Decode(buf, bufSize, frames, &audioFmt);
    }
    if (!m_needConvert)
    {
        return m_decoder->Decode(buf, bufSize, frames, &m_decodeFmt); 
    }
    else
    {
        uint32_t readFrames = m_decoder->Decode(m_tmpBuf.get(), m_tmpSize, frames, &m_decodeFmt);
        if (readFrames > 0)
        {
            uint32_t inSamples = readFrames * m_decodeFmt.numChannels;
            double ch_rate = double(m_outFmt.numChannels) / m_decodeFmt.numChannels;
            uint32_t outSamples = uint32_t((double(inSamples) * m_outFmt.sampleRate / m_decodeFmt.sampleRate + 0.5) * ch_rate);
                if (bufSize < (outSamples * (m_outFmt.bitsPerSample / 8)))
                return 0;

            bool bFlushLast = m_decoder->GetCurrentFrame() >= m_decoder->GetTotalFrame();
            uint32_t realSamples = m_Effects.Process(m_tmpBuf.get(), inSamples, buf, outSamples, bFlushLast);
            return (realSamples / m_outFmt.numChannels);
        }
    }

    return 0;
}

float CAudioSource::GetTotalSeconds() const 
{
    std::size_t totalFrames = m_decoder->GetTotalFrame();
    AudioFormat audioFmt = m_decoder->GetAudioFormat(-1);
    return float(totalFrames) / float(audioFmt.sampleRate);
}

std::size_t CAudioSource::SourceFrameToDeviceFrame(std::size_t srcFrame) const 
{
    if (m_outFmt.format == AudioDataFormat::UNKNOWN) //
        return srcFrame;

    AudioFormat audioFmt = m_decoder->GetAudioFormat(-1);
    //采样率没变化，也不需要转换
    if (m_outFmt.sampleRate == audioFmt.sampleRate)
        return srcFrame;

    double rate = double(m_outFmt.sampleRate) / audioFmt.sampleRate;

    return static_cast<std::size_t>(rate * srcFrame);
}

std::size_t CAudioSource::DeviceFrameToSourceFrame(std::size_t devFrame) const 
{
    if (m_outFmt.format == AudioDataFormat::UNKNOWN) //
        return devFrame;

    AudioFormat audioFmt = m_decoder->GetAudioFormat(-1);
    //采样率没变化，也不需要转换
    if (m_outFmt.sampleRate == audioFmt.sampleRate)
        return devFrame;

    double rate = double(audioFmt.sampleRate) / m_outFmt.sampleRate;

    return static_cast<std::size_t>(rate * devFrame);
}

void CAudioSource::SeekToFrame(std::size_t frames)
{
    if (!m_stream || !m_decoder)
        return;

    m_bSeekOnGoing = true;
    std::size_t sourceFrames = DeviceFrameToSourceFrame(frames);
    m_decoder->SeekTo(sourceFrames);
}

std::size_t CAudioSource::GetCurrentFrame() const
{
    if (!m_decoder)
        return 0;

    std::size_t srcFrames = m_decoder->GetCurrentFrame();
    return SourceFrameToDeviceFrame(srcFrames);
}

std::size_t CAudioSource::GetTotalFrames() const 
{ 
    //return SourceFrameToDeviceFrame(m_audioInfo.m_totalFrames);
    if (!m_decoder)
        return 0;

    std::size_t srcFrames = m_decoder->GetTotalFrame();
    return SourceFrameToDeviceFrame(srcFrames);
}

bool CAudioSource::IsLastAudioStream() const
{
    if (m_decoder)
    {
        uint32_t curStreamIdx = m_decoder->GetCurrentStreamIndex();
        return (curStreamIdx >= (m_decoder->GetAudioStreamCount() - 1));
    }

    return true;
}

float CAudioSource::FramesToSeconds(std::size_t frames) const 
{
    AudioFormat audioFmt = m_decoder->GetAudioFormat(-1);
    if (m_outFmt.format == AudioDataFormat::UNKNOWN) //
        return float(frames) / float(audioFmt.sampleRate);

    //if (m_devFmt.sampleRate == m_audioInfo.m_audioFmt.sampleRate)
    //    return float(frames) / float(m_devFmt.sampleRate);

    return float(frames) / float(m_outFmt.sampleRate);
}

std::size_t CAudioSource::SecondsToFrames(float seconds) const
{
    AudioFormat audioFmt = m_decoder->GetAudioFormat(-1);
    if (m_outFmt.format == AudioDataFormat::UNKNOWN) //
        return static_cast<std::size_t>(audioFmt.sampleRate * seconds);

    //if (m_devFmt.sampleRate == m_audioInfo.m_audioFmt.sampleRate)
    //    return static_cast<std::size_t>(m_devFmt.sampleRate * seconds);

    return static_cast<std::size_t>(m_outFmt.sampleRate * seconds);
}
