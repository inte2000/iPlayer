#include <algorithm>
#include <cstdint>

#include <cdio/cdio.h>

#include "AudioInfo.h"
#include "CDSectorsStream.h"
#include "MediaTagNames.h"
#include "UnicodeConvert.h"

namespace {

bool IsAsciiLetter(wchar_t c)
{
    return ((c >= L'a') && (c <= L'z')) || ((c >= L'A') && (c <= L'Z'));
}

std::wstring ToLowerAscii(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) {
        if ((c >= L'A') && (c <= L'Z')) {
            return static_cast<wchar_t>(c - L'A' + L'a');
        }
        return c;
    });
    return value;
}

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

void AddCdTextTag(CMediaTag& tag, const cdtext_t* cdText, cdtext_field_t field, const char* tagName)
{
    if ((cdText == nullptr) || (tagName == nullptr)) {
        return;
    }

    const char* discValue = cdtext_get_const(cdText, field, 0);
    if ((discValue != nullptr) && (*discValue != '\0')) {
        tag.AddTagString(tagName, discValue);
        return;
    }

    const track_t firstTrack = cdtext_get_first_track(cdText);
    if ((firstTrack == CDIO_INVALID_TRACK) || (firstTrack == 0)) {
        return;
    }

    const char* firstTrackValue = cdtext_get_const(cdText, field, firstTrack);
    if ((firstTrackValue != nullptr) && (*firstTrackValue != '\0')) {
        tag.AddTagString(tagName, firstTrackValue);
    }
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
    : m_cdio(nullptr, cdio_destroy)
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
    m_cdio.reset();
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
    if (!m_cdio) {
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
    if (!m_cdio || (buf == nullptr) || (count == 0) || (startNo < m_startSector)) {
        return 0;
    }

    const uint64_t endSector = m_startSector + m_totalSectors;
    if (startNo >= endSector) {
        return 0;
    }

    const uint64_t left = endSector - startNo;
    const uint32_t realCount = static_cast<uint32_t>(std::min<uint64_t>(left, count));
    const lsn_t lsn = static_cast<lsn_t>(startNo);
    const driver_return_code_t drc = cdio_read_audio_sectors(m_cdio.get(), buf, lsn, realCount);
    if (drc != DRIVER_OP_SUCCESS) {
        return 0;
    }

    return realCount;
}

uint32_t CCDSectorsStream::SectorSize() const
{
    return CDIO_CD_FRAMESIZE_RAW;
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
    const std::string source = Utf16LeToLocalMBCS(imgFile);
    if (source.empty()) {
        return false;
    }

    m_cdio.reset(cdio_open(source.c_str(), DRIVER_UNKNOWN));
    if (!m_cdio) {
        return false;
    }

    return InitFromOpenedCdio(imgFile, track);
}

bool CCDSectorsStream::OpenDevice(const std::wstring& deviceName, uint32_t track)
{
    wchar_t driveLetter = L'\0';
    if (!ParseDeviceDriveLetter(deviceName, driveLetter)) {
        return false;
    }

    std::wstring openName;
    openName.push_back(driveLetter);
    openName.push_back(L':');

    const std::string source = Utf16LeToLocalMBCS(openName);
    m_cdio.reset(cdio_open_cd(source.c_str()));
    if (!m_cdio) {
        return false;
    }

    return InitFromOpenedCdio(deviceName, track);
}

bool CCDSectorsStream::InitFromOpenedCdio(const std::wstring& sourceName, uint32_t track)
{
    if (!m_cdio) {
        return false;
    }

    const discmode_t mode = cdio_get_discmode(m_cdio.get());
    if ((mode != CDIO_DISC_MODE_CD_DA) && (mode != CDIO_DISC_MODE_CD_DAP)) {
        return false;
    }

    const track_t firstTrackNo = cdio_get_first_track_num(m_cdio.get());
    const track_t lastTrackNo = cdio_get_last_track_num(m_cdio.get());
    if ((firstTrackNo == CDIO_INVALID_TRACK) || (lastTrackNo == CDIO_INVALID_TRACK)) {
        return false;
    }

    const track_t targetTrack = static_cast<track_t>(track);
    if ((targetTrack < firstTrackNo) || (targetTrack > lastTrackNo)) {
        return false;
    }

    const lsn_t firstLsn = cdio_get_track_lsn(m_cdio.get(), targetTrack);
    const lsn_t lastLsn = cdio_get_track_last_lsn(m_cdio.get(), targetTrack);
    if ((firstLsn == CDIO_INVALID_LSN) || (lastLsn == CDIO_INVALID_LSN) || (lastLsn < firstLsn)) {
        return false;
    }

    m_name = sourceName;
    m_track = track;
    m_startSector = static_cast<uint64_t>(firstLsn);
    m_totalSectors = static_cast<uint64_t>(lastLsn - firstLsn + 1);
    m_curSector = m_startSector;

    FillMetaInfo();
    return true;
}

void CCDSectorsStream::FillMetaInfo()
{
    m_metaInfo.itemTag.Clear();
    m_metaInfo.itemSequence = 0;
    m_metaInfo.itemMediaType = "Audio CD";
    InitAudioFormat(&m_metaInfo.itemFormat, AudioDataFormat::PCM_S16, 2, 44100, 16);

    m_metaInfo.itemTag.AddTagString(MediaTag_Type, "Audio CD");
    m_metaInfo.itemTag.AddTagString(MediaTag_Brief, "PCM-S16, 44.1KHz, Stereo");
    m_metaInfo.itemTag.AddTagInteger(MediaTag_Channels, 2);
    m_metaInfo.itemTag.AddTagInteger(MediaTag_SamplesRate, 44100);
    m_metaInfo.itemTag.AddTagInteger(MediaTag_BitsPerSample, 16);

    cdtext_t* cdText = cdio_get_cdtext(m_cdio.get());
    if (cdText == nullptr) {
        return;
    }

    AddCdTextTag(m_metaInfo.itemTag, cdText, CDTEXT_FIELD_TITLE, MediaTag_Title);
    AddCdTextTag(m_metaInfo.itemTag, cdText, CDTEXT_FIELD_PERFORMER, MediaTag_Artists);
    AddCdTextTag(m_metaInfo.itemTag, cdText, CDTEXT_FIELD_COMPOSER, MediaTag_Comment);
    AddCdTextTag(m_metaInfo.itemTag, cdText, CDTEXT_FIELD_GENRE, MediaTag_Genre);
}
