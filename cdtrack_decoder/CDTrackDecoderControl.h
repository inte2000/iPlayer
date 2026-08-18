#ifndef CDTRACK_DECODER_CONTROL_H
#define CDTRACK_DECODER_CONTROL_H

#include <cstddef>
#include <cstdint>
#include <memory>

#include "AudioInfo.h"
#include "DataStream.h"
#include "StreamMetaSource.h"
#include "StreamSectorSource.h"

class DataBuffer;

class CDTrackDecoderControl
{
public:
    static constexpr std::size_t DataBufferWindowDefault = 4 * 1024 * 1024;

    CDTrackDecoderControl();
    ~CDTrackDecoderControl();

    bool Init(CDataStream* stream, std::size_t windowSize = DataBufferWindowDefault);
    bool Start(uint32_t streamIndex);
    void Stop();
    bool IsStarted() const;
    bool IsCanSeeking() const;

    uint32_t DecodeFrames(void* pBuf, uint32_t frames, const AudioFormat* audioFmt);
    void SeekToFrame(std::size_t frames);

    const DsMetaInfo* GetMetaInfo() const;
    const AudioFormat& SourceFormat() const;
    std::size_t CurrentFrames() const;
    std::size_t TotalFrames() const;

    static bool IsSupportOutputFormat(const AudioFormat* audioFmt);

private:
    CDataStream* m_stream;
    MetaSource* m_metaSource;
    SectorSource* m_sectorSource;
    AudioFormat m_sourceFmt;
    std::size_t m_currentFrames;
    std::size_t m_totalFrames;
    bool m_started;
    std::unique_ptr<DataBuffer> m_dataBuffer;
};

#endif // CDTRACK_DECODER_CONTROL_H
