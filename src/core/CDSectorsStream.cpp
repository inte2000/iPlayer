#include <algorithm>
#include <cstdint>

#include "AudioCD.h"
#include "AudioInfo.h"
#include "CDSectorsStream.h"
#include "MediaTagNames.h"
#include "StringEx.h"
#include "UnicodeConvert.h"

namespace {

bool ParseDeviceDriveLetter(const std::wstring& name, wchar_t& driveLetter)
{
    const std::wstring lower = ToLowerAscii(name);
    const std::wstring prefix1 = L"\\device\\";
    const std::wstring prefix2 = L"\\\\device\\";

    auto checkWithPrefix = [&](const std::wstring& prefix) {
        if (lower.size() != (prefix.size() + 2)) {
            return false;
        }
        if (lower.compare(0, prefix.size(), prefix) != 0) {
            return false;
        }

        const wchar_t letter = name[prefix.size()];
        const wchar_t colon = name[prefix.size() + 1];
        if (!IsAsciiLetter(letter) || (colon != L':')) {
            return false;
        }

        driveLetter = letter;
        return true;
    };

    return checkWithPrefix(prefix1) || checkWithPrefix(prefix2);
}

} // namespace

std::unique_ptr<CDataStream> MakeCDSectorsStream(const std::wstring& name, uint32_t track)
{
    auto stream = std::make_unique<CCDSectorsStream>();
    if (!stream->Open(name, track)) {
        return nullptr;
    }
    return stream;
}

CCDSectorsStream::CCDSectorsStream()
    : m_audioCD(nullptr)
    , m_metaInfo{}
    , m_track(0)
    , m_curSector(0)
    , m_totalSectors(0)
    , m_startSector(0)
{
    m_style = dsStyleFixedLength | dsStyleSeekable | dsStyleTellPos;
    m_metaInfo.itemSequence = 0;
    m_metaInfo.itemMediaType = "Audio CD";
    InitAudioFormat(&m_metaInfo.itemFormat, AudioDataFormat::PCM_S16, 2, 44100, 16);
}

CCDSectorsStream::~CCDSectorsStream()
{
    Close();
}

bool CCDSectorsStream::IsDeviceName(const std::wstring& name)
{
    wchar_t driveLetter = L'\0';
    return ParseDeviceDriveLetter(name, driveLetter);
}

bool CCDSectorsStream::Open(const std::wstring& name, uint32_t track)
{
    Close();

    if (name.empty() || (track == 0)) {
        return false;
    }

    if (IsDeviceName(name)) {
        return OpenDevice(name, track);
    }

    return OpenImage(name, track);
}

bool CCDSectorsStream::Close()
{
    if (m_audioCD) {
        m_audioCD->Close();
        m_audioCD.reset();
    }

    m_name.clear();
    m_track = 0;
    m_curSector = 0;
    m_totalSectors = 0;
    m_startSector = 0;
    m_metaInfo.itemTag.Clear();
    m_metaInfo.itemSequence = 0;
    m_metaInfo.itemMediaType = "Audio CD";
    InitAudioFormat(&m_metaInfo.itemFormat, AudioDataFormat::PCM_S16, 2, 44100, 16);
    return true;
}

uint32_t CCDSectorsStream::Read(void* pBuf, uint32_t size, uint32_t timeout)
{
    (void)timeout;

    if ((pBuf == nullptr) || (size == 0)) {
        return 0;
    }

    const uint32_t secSize = SectorSize();
    if (secSize == 0) {
        return 0;
    }

    const uint32_t reqSectors = (size + secSize - 1) / secSize;
    const uint32_t readSectors = ReadSectors(m_curSector, reqSectors, pBuf);
    m_curSector += readSectors;
    return readSectors * secSize;
}

uint32_t CCDSectorsStream::Write(const void* pBuf, uint32_t size, uint32_t timeout)
{
    (void)pBuf;
    (void)size;
    (void)timeout;
    return 0;
}

std::size_t CCDSectorsStream::GetLength() const
{
    return static_cast<std::size_t>(m_totalSectors * SectorSize());
}

void CCDSectorsStream::Seek(SeekBase base, long long off)
{
    const uint64_t secSize = SectorSize();
    if (secSize == 0) {
        return;
    }
    if ((off % static_cast<long long>(secSize)) != 0) {
        return;
    }

    const int64_t totalBytes = static_cast<int64_t>(GetLength());
    const int64_t curBytes = static_cast<int64_t>(Tell());
    int64_t targetBytes = curBytes;

    if (base == SeekBase::Begin) {
        targetBytes = off;
    }
    else if (base == SeekBase::Cur) {
        targetBytes = curBytes + off;
    }
    else if (base == SeekBase::End) {
        targetBytes = totalBytes + off;
    }

    if ((targetBytes < 0) || (targetBytes > totalBytes)) {
        return;
    }
    if ((targetBytes % static_cast<int64_t>(secSize)) != 0) {
        return;
    }

    const uint64_t offsetSectors = static_cast<uint64_t>(targetBytes / static_cast<int64_t>(secSize));
    m_curSector = m_startSector + offsetSectors;
}

std::size_t CCDSectorsStream::Tell()
{
    if (m_curSector < m_startSector) {
        return 0;
    }
    return static_cast<std::size_t>((m_curSector - m_startSector) * SectorSize());
}

const DsMetaInfo* CCDSectorsStream::GetMetaInformation() const
{
    if (!m_audioCD) {
        return nullptr;
    }

    return &m_metaInfo;
}

SectorSourceType CCDSectorsStream::Type() const
{
    return SectorSourceType::AudioCD;
}

uint64_t CCDSectorsStream::GetStartSectors() const
{
    return m_startSector;
}

uint32_t CCDSectorsStream::GetSectorsCount() const
{
    return static_cast<uint32_t>(m_totalSectors);
}

uint32_t CCDSectorsStream::ReadSectors(uint64_t startNo, uint32_t count, void* buf) const
{
    if (!m_audioCD || (buf == nullptr) || (count == 0) || (startNo < m_startSector)) {
        return 0;
    }

    const uint64_t endSector = m_startSector + m_totalSectors;
    if (startNo >= endSector) {
        return 0;
    }

    const uint64_t left = endSector - startNo;
    const uint32_t realCount = static_cast<uint32_t>(std::min<uint64_t>(left, count));
    const uint32_t readBytes = m_audioCD->ReadTrack(m_track - 1,
                                                    static_cast<int32_t>(startNo - m_startSector),
                                                    realCount,
                                                    static_cast<uint8_t*>(buf),
                                                    realCount * SectorSize());
    return readBytes / SectorSize();
}

uint32_t CCDSectorsStream::SectorSize() const
{
    return RAW_SECTOR_SIZE;
}

uint32_t CCDSectorsStream::SeekToSector(uint64_t sectorNo)
{
    const uint64_t endSector = m_startSector + m_totalSectors;
    if ((sectorNo < m_startSector) || (sectorNo > endSector)) {
        return 0;
    }

    m_curSector = sectorNo;
    return static_cast<uint32_t>(m_curSector);
}

bool CCDSectorsStream::OpenImage(const std::wstring& imgFile, uint32_t track)
{
    m_audioCD = std::make_unique<CAudioCD>();
    if (!m_audioCD->Open(imgFile)) {
        m_audioCD.reset();
        return false;
    }

    return InitFromOpenedAudioCD(imgFile, track);
}

bool CCDSectorsStream::OpenDevice(const std::wstring& deviceName, uint32_t track)
{
    wchar_t driveLetter = L'\0';
    if (!ParseDeviceDriveLetter(deviceName, driveLetter)) {
        return false;
    }

    std::wstring openName = L"CDDevice--";
    openName.push_back(driveLetter);
    openName.push_back(L':');

    m_audioCD = std::make_unique<CAudioCD>();
    if (!m_audioCD->Open(openName)) {
        m_audioCD.reset();
        return false;
    }

    return InitFromOpenedAudioCD(deviceName, track);
}

bool CCDSectorsStream::InitFromOpenedAudioCD(const std::wstring& sourceName, uint32_t track)
{
    if (!m_audioCD || !m_audioCD->IsOpened()) {
        return false;
    }

    if ((track == 0) || (track > m_audioCD->GetTrackCount())) {
        return false;
    }

    const CD_TRACK_INFO& trackInfo = m_audioCD->GetTrackInfo(track - 1);
    if (!trackInfo.isAudio || (trackInfo.length <= 0)) {
        return false;
    }

    m_name = sourceName;
    m_track = track;
    m_startSector = static_cast<uint64_t>(trackInfo.lsn);
    m_totalSectors = static_cast<uint64_t>(trackInfo.length);
    m_curSector = m_startSector;

    FillMetaInfo();
    return true;
}

void CCDSectorsStream::FillMetaInfo()
{
    m_metaInfo.itemTag.Clear();
    m_metaInfo.itemSequence = m_track;
    m_metaInfo.itemMediaType = "Audio CD";
    InitAudioFormat(&m_metaInfo.itemFormat, AudioDataFormat::PCM_S16, 2, 44100, 16);

    m_metaInfo.itemTag.AddTagString(MediaTag_Type, "Audio CD");
    m_metaInfo.itemTag.AddTagString(MediaTag_Brief, "PCM-S16, 44.1KHz, Stereo");
    m_metaInfo.itemTag.AddTagInteger(MediaTag_Channels, 2);
    m_metaInfo.itemTag.AddTagInteger(MediaTag_SamplesRate, 44100);
    m_metaInfo.itemTag.AddTagInteger(MediaTag_BitsPerSample, 16);

    if (!m_audioCD || (m_track == 0)) {
        return;
    }

    const uint32_t index = m_track - 1;
    const std::wstring title = m_audioCD->GetTrackTitle(index);
    if (!title.empty()) {
        m_metaInfo.itemTag.AddTagString(MediaTag_Title, Utf16ToUtf8(title));
    }

    const std::wstring artist = m_audioCD->GetTrackArtist(index);
    if (!artist.empty()) {
        m_metaInfo.itemTag.AddTagString(MediaTag_Artists, Utf16ToUtf8(artist));
    }

    const std::wstring album = m_audioCD->GetTrackAlbum(index);
    if (!album.empty()) {
        m_metaInfo.itemTag.AddTagString(MediaTag_Album, Utf16ToUtf8(album));
    }
}
