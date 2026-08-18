#include <algorithm>

#include "CDTrackDecoderControl.h"
#include "DataBuffer.h"

namespace {

void InitSourceAudioFormat(AudioFormat& fmt)
{
    InitAudioFormat(&fmt, AudioDataFormat::PCM_S16, 2, 44100, 16);
}

} // namespace

CDTrackDecoderControl::CDTrackDecoderControl()
    : m_stream(nullptr)
    , m_metaSource(nullptr)
    , m_sectorSource(nullptr)
    , m_currentFrames(0)
    , m_totalFrames(0)
    , m_started(false)
{
    InitSourceAudioFormat(m_sourceFmt);
}

CDTrackDecoderControl::~CDTrackDecoderControl() = default;

bool CDTrackDecoderControl::Init(CDataStream* stream, std::size_t windowSize)
{
    m_stream = stream;
    if (m_stream == nullptr) {
        return false;
    }

    m_metaSource = m_stream->QuerySource<MetaSource>();
    m_sectorSource = m_stream->QuerySource<SectorSource>();
    if (m_sectorSource == nullptr) {
        return false;
    }
    if (m_sectorSource->Type() != SectorSourceType::AudioCD) {
        return false;
    }

    const uint32_t sectorSize = m_sectorSource->SectorSize();
    if ((sectorSize == 0) || ((sectorSize % m_sourceFmt.blockAlign) != 0)) {
        return false;
    }

    const uint64_t startSector = m_sectorSource->GetStartSectors();
    const uint32_t sectorsCount = m_sectorSource->GetSectorsCount();
    const std::size_t totalSize = static_cast<std::size_t>(sectorsCount) * sectorSize;
    m_totalFrames = totalSize / m_sourceFmt.blockAlign;

    m_dataBuffer = std::make_unique<DataBuffer>(m_sectorSource, startSector, sectorSize, totalSize, windowSize);
    m_currentFrames = 0;
    m_started = false;
    return true;
}

bool CDTrackDecoderControl::Start(uint32_t streamIndex)
{
    if ((streamIndex != 0) && (streamIndex != static_cast<uint32_t>(-1))) {
        return false;
    }
    if (!m_dataBuffer) {
        return false;
    }

    m_dataBuffer->Reset();
    m_currentFrames = 0;
    m_started = true;
    return true;
}

void CDTrackDecoderControl::Stop()
{
    m_started = false;
}

bool CDTrackDecoderControl::IsStarted() const
{
    return m_started;
}

bool CDTrackDecoderControl::IsCanSeeking() const
{
    return m_dataBuffer != nullptr;
}

uint32_t CDTrackDecoderControl::DecodeFrames(void* pBuf, uint32_t frames, const AudioFormat* audioFmt)
{
    if (!m_started || !m_dataBuffer || (pBuf == nullptr) || !IsSupportOutputFormat(audioFmt) || (frames == 0)) {
        return 0;
    }

    const std::size_t targetBytes = static_cast<std::size_t>(frames) * m_sourceFmt.blockAlign;
    const uint32_t readBytes = m_dataBuffer->Read(pBuf, static_cast<uint32_t>(targetBytes));
    const uint32_t readFrames = readBytes / m_sourceFmt.blockAlign;
    m_currentFrames = m_dataBuffer->Tell() / m_sourceFmt.blockAlign;
    return readFrames;
}

void CDTrackDecoderControl::SeekToFrame(std::size_t frames)
{
    if (!m_dataBuffer) {
        return;
    }

    const std::size_t target = std::min(frames, m_totalFrames);
    const std::size_t targetBytes = target * m_sourceFmt.blockAlign;
    if (m_dataBuffer->Seek(targetBytes)) {
        m_currentFrames = target;
    }
}

const DsMetaInfo* CDTrackDecoderControl::GetMetaInfo() const
{
    return (m_metaSource != nullptr) ? m_metaSource->GetMetaInformation() : nullptr;
}

const AudioFormat& CDTrackDecoderControl::SourceFormat() const
{
    return m_sourceFmt;
}

std::size_t CDTrackDecoderControl::CurrentFrames() const
{
    return m_currentFrames;
}

std::size_t CDTrackDecoderControl::TotalFrames() const
{
    return m_totalFrames;
}

bool CDTrackDecoderControl::IsSupportOutputFormat(const AudioFormat* audioFmt)
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
