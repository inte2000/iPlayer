/*
20260527 ��������
��ģ�ͣ�ChatGPT 5.3 Codex
����������todo_task_84.txt
*/
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>

#include "MediaTagNames.h"
#include "WavpackPlayCtrl.h"

namespace {

int16_t S32ToS16(int32_t sample, uint32_t bitsPerSample)
{
    if (bitsPerSample == 0) {
        bitsPerSample = 16;
    }

    int32_t value = sample;
    if (bitsPerSample > 16) {
        value >>= static_cast<int>(bitsPerSample - 16);
    }
    else if (bitsPerSample < 16) {
        value <<= static_cast<int>(16 - bitsPerSample);
    }

    value = std::clamp(value, -0x8000, 0x7FFF);
    return static_cast<int16_t>(value);
}

int32_t S32ToS32(int32_t sample, uint32_t bitsPerSample)
{
    if (bitsPerSample == 0) {
        bitsPerSample = 16;
    }

    int64_t value = sample;
    if (bitsPerSample < 32) {
        value <<= static_cast<int>(32 - bitsPerSample);
    }
    else if (bitsPerSample > 32) {
        value >>= static_cast<int>(bitsPerSample - 32);
    }

    value = std::clamp<int64_t>(value, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max());
    return static_cast<int32_t>(value);
}

float S32ToF32(int32_t sample, uint32_t bitsPerSample)
{
    if (bitsPerSample == 0) {
        bitsPerSample = 16;
    }
    const double scale = std::ldexp(1.0, static_cast<int>(bitsPerSample - 1));
    if (scale <= 0.0) {
        return 0.0f;
    }

    const double v = static_cast<double>(sample) / scale;
    return static_cast<float>(std::clamp(v, -1.0, 0.9999999403953552));
}

int16_t F32ToS16(float sample)
{
    const float v = std::clamp(sample, -1.0f, 1.0f);
    if (v <= -1.0f) {
        return std::numeric_limits<int16_t>::min();
    }
    return static_cast<int16_t>(std::lrint(v * 32767.0f));
}

int32_t F32ToS32(float sample)
{
    const float v = std::clamp(sample, -1.0f, 1.0f);
    if (v <= -1.0f) {
        return std::numeric_limits<int32_t>::min();
    }

    const double scaled = static_cast<double>(v) * static_cast<double>(std::numeric_limits<int32_t>::max());
    return static_cast<int32_t>(std::llround(scaled));
}

} // namespace

WavpackPlayCtrl::WavpackPlayCtrl()
    : m_stream(nullptr)
    , m_wvcStream(nullptr)
    , m_ctx(nullptr)
    , m_curFrames(0)
    , m_totalFrames(0)
    , m_streamFmt(StreamFormatUnknown)
    , m_activeStreamIdx(static_cast<uint32_t>(-1))
    , m_sourceBitsPerSample(0)
    , m_sourceIsFloat(false)
    , m_tempS32()
{
    InitEmptyAudioFormat(&m_srcAudioFmt);
}

WavpackPlayCtrl::~WavpackPlayCtrl()
{
    Release();
}

bool WavpackPlayCtrl::Init(CDataStream* stream, uint32_t streamFmt)
{
    Release();
    if (stream == nullptr) {
        return false;
    }

    m_stream = stream;
    m_ctx = nullptr;

    InitWavpackReader(m_reader);
    InitStreamSource(m_wvSource, m_stream);

    m_wvcStream = OpenWvcStream(m_stream);
    InitStreamSource(m_wvcSource, m_wvcStream.get());
    return true;
}

void WavpackPlayCtrl::Release()
{
    StopStream(-1);
    m_stream = nullptr;
    m_wvcStream.reset();
    //m_totalFrames = 0;
    //m_sourceIsFloat = false;
    m_streamFmt = StreamFormatUnknown;
    m_activeStreamIdx = static_cast<uint32_t>(-1);
    m_sourceBitsPerSample = 0;
    m_sourceIsFloat = false;
    m_tempS32.clear();
}

bool WavpackPlayCtrl::OpenStream(uint32_t streamIndex)
{
    StopStream(-1);

    if (streamIndex == -1)
        streamIndex = 0;

    int openFlags = OPEN_TAGS | OPEN_NORMALIZE;
    void* wvcId = nullptr;
    if (m_wvcSource.stream != nullptr) 
    {
        openFlags |= OPEN_WVC;
        wvcId = &m_wvcSource;
    }

    char error[80] = {};
    m_ctx = WavpackOpenFileInputEx64(&m_reader, &m_wvSource, wvcId, error, openFlags, 0);
    if (m_ctx == nullptr)
    {
        return false;
    }

    const uint32_t channels = static_cast<uint32_t>(std::max(1, WavpackGetNumChannels(m_ctx)));
    const uint32_t sampleRate = WavpackGetNativeSampleRate(m_ctx);
    const uint32_t bitsPerSample = WavpackGetBitsPerSample(m_ctx);
    int mode = WavpackGetMode(m_ctx);
    m_sourceIsFloat = (mode & MODE_FLOAT) != 0;

    if ((sampleRate == 0) || (channels == 0)) 
    {
        StopStream(-1);
        return false;
    }

    uint32_t outBits = 32;
    AudioDataFormat outFmt = AudioDataFormat::Float32;
    if (!m_sourceIsFloat) 
    {
        outBits = bitsPerSample;
        if ((outBits != 16) && (outBits != 24) && (outBits != 32)) 
        {
            outBits = 32;
        }
        outFmt = AudioFormatByBitsPerSample(outBits);
    }
    int chLayout = WavpackGetChannelMask(m_ctx);
    InitAudioFormat(&m_srcAudioFmt, outFmt, channels, sampleRate, 0, 0, chLayout);

    const int64_t totalSamples = WavpackGetNumSamples64(m_ctx);
    m_totalFrames = (totalSamples > 0) ? static_cast<std::size_t>(totalSamples) : 0;
    m_activeStreamIdx = streamIndex;
    m_curFrames = 0;

    Seek(m_curFrames);

    return true;
}

void WavpackPlayCtrl::StopStream(uint32_t streamIndex)
{
    if ((streamIndex == m_activeStreamIdx) || (streamIndex == -1))
    {
        if (m_ctx != nullptr) 
        {
            WavpackCloseFile(m_ctx);
            m_ctx = nullptr;
        }
        InitStreamSource(m_wvSource, m_stream);
        InitStreamSource(m_wvcSource, m_wvcStream.get());

        m_activeStreamIdx = -1;
        m_curFrames = 0;
    }
}

bool WavpackPlayCtrl::IsSupportOutput(const AudioFormat* audioFmt) const
{
    if (audioFmt == nullptr) {
        return false;
    }
    if (audioFmt->sampleRate != m_srcAudioFmt.sampleRate) {
        return false;
    }
    if (audioFmt->numChannels != m_srcAudioFmt.numChannels) {
        return false;
    }

    return (audioFmt->format == AudioDataFormat::PCM_S16)
        || (audioFmt->format == AudioDataFormat::PCM_S32)
        || (audioFmt->format == AudioDataFormat::Float32);
}

bool WavpackPlayCtrl::IsCanSeeking() const
{
    if (m_stream == nullptr) {
        return false;
    }

    return (m_stream->GetStyle() & dsStyleSeekable) != 0;
}

uint32_t WavpackPlayCtrl::DecodeFrames(void* pBuf, uint32_t frames, const AudioFormat* audioFmt)
{
    if ((m_ctx == nullptr) || (pBuf == nullptr) || (frames == 0) || !IsSupportOutput(audioFmt)) {
        return 0;
    }

    uint32_t requestFrames = frames;
    if (m_totalFrames > 0) {
        requestFrames = static_cast<uint32_t>(std::min<std::size_t>(requestFrames, m_totalFrames - std::min(m_curFrames, m_totalFrames)));
    }
    if (requestFrames == 0) {
        return 0;
    }

    const uint32_t channels = m_srcAudioFmt.numChannels;
    m_tempS32.resize(static_cast<std::size_t>(requestFrames) * channels);
    const uint32_t gotFrames = WavpackUnpackSamples(m_ctx, m_tempS32.data(), requestFrames);
    if (gotFrames == 0) {
        return 0;
    }

    const uint32_t gotSamples = gotFrames * channels;
    if (m_sourceIsFloat)
    {
        if (audioFmt->format == AudioDataFormat::PCM_S16)
        {
            int16_t* out = static_cast<int16_t*>(pBuf);
            for (uint32_t i = 0; i < gotSamples; ++i)
            {
                out[i] = F32ToS16(std::bit_cast<float>(m_tempS32[i]));
            }
        }
        else if (audioFmt->format == AudioDataFormat::PCM_S32)
        {
            int32_t* out = static_cast<int32_t*>(pBuf);
            for (uint32_t i = 0; i < gotSamples; ++i)
            {
                out[i] = F32ToS32(std::bit_cast<float>(m_tempS32[i]));
            }
        }
        else
        {
            float* out = static_cast<float*>(pBuf);
            for (uint32_t i = 0; i < gotSamples; ++i)
            {
                out[i] = std::bit_cast<float>(m_tempS32[i]);
            }
        }
    }
    else if (audioFmt->format == AudioDataFormat::PCM_S16)
    {
        int16_t* out = static_cast<int16_t*>(pBuf);
        for (uint32_t i = 0; i < gotSamples; ++i)
        {
            out[i] = S32ToS16(m_tempS32[i], m_sourceBitsPerSample);
        }
    }
    else if (audioFmt->format == AudioDataFormat::PCM_S32)
    {
        int32_t* out = static_cast<int32_t*>(pBuf);
        for (uint32_t i = 0; i < gotSamples; ++i)
        {
            out[i] = S32ToS32(m_tempS32[i], m_sourceBitsPerSample);
        }
    }
    else
    {
        float* out = static_cast<float*>(pBuf);
        for (uint32_t i = 0; i < gotSamples; ++i)
        {
            out[i] = S32ToF32(m_tempS32[i], m_sourceBitsPerSample);
        }
    }

    m_curFrames += gotFrames;
    const int64_t cur = WavpackGetSampleIndex64(m_ctx);
    if (cur >= 0) {
        m_curFrames = static_cast<std::size_t>(cur);
    }

    return gotFrames;
}

void WavpackPlayCtrl::Seek(std::size_t frames)
{
    if (m_ctx == nullptr) {
        return;
    }

    std::size_t target = frames;
    if (m_totalFrames > 0) {
        target = std::min(target, m_totalFrames);
    }

    if (WavpackSeekSample64(m_ctx, static_cast<int64_t>(target)) != 0) {
        m_curFrames = target;
    }
}

float WavpackPlayCtrl::GetDurationSeconds() const
{
    if ((m_srcAudioFmt.sampleRate == 0) || (m_totalFrames == 0)) {
        return 0.0f;
    }

    return static_cast<float>(static_cast<double>(m_totalFrames) / static_cast<double>(m_srcAudioFmt.sampleRate));
}

void WavpackPlayCtrl::FillMetaTags(CMediaTag& tags) const
{
    tags.Clear();

    const std::string type = "WavPack";
    const std::string brief = std::format("{}: {}", type, AudioFormatBrifStr(&m_srcAudioFmt));

    tags.AddTagInteger(MediaTag_Streams, 1);
    tags.AddTagString(MediaTag_Type, type);
    tags.AddTagString(MediaTag_Brief, brief);
    tags.AddTagDecimal(MediaTag_Duration, GetDurationSeconds());
    tags.AddTagInteger(MediaTag_BitsPerSample, static_cast<int32_t>(m_srcAudioFmt.bitsPerSample));
    tags.AddTagInteger(MediaTag_SamplesRate, static_cast<int32_t>(m_srcAudioFmt.sampleRate));
    tags.AddTagInteger(MediaTag_Channels, static_cast<int32_t>(m_srcAudioFmt.numChannels));
    tags.AddTagInteger(MediaTag_ChannelsLayout, static_cast<int32_t>(m_srcAudioFmt.chLayout));
    tags.AddTagInteger(MediaTag_BitsRate, static_cast<int32_t>(m_srcAudioFmt.bitsPerSample * m_srcAudioFmt.sampleRate * m_srcAudioFmt.numChannels));

    const std::string title = ReadTagValue("Title");
    const std::string artist = ReadTagValue("Artist");
    const std::string album = ReadTagValue("Album");
    const std::string genre = ReadTagValue("Genre");
    const std::string comment = ReadTagValue("Comment");
    std::string date = ReadTagValue("Year");
    if (date.empty()) {
        date = ReadTagValue("Date");
    }

    if (!title.empty()) {
        tags.AddTagString(MediaTag_Title, title);
    }
    if (!artist.empty()) {
        tags.AddTagString(MediaTag_Artists, artist);
    }
    if (!album.empty()) {
        tags.AddTagString(MediaTag_Album, album);
    }
    if (!genre.empty()) {
        tags.AddTagString(MediaTag_Genre, genre);
    }
    if (!comment.empty()) {
        tags.AddTagString(MediaTag_Comment, comment);
    }
    if (!date.empty()) {
        tags.AddTagString(MediaTag_Date, date);
    }
}

int32_t WavpackPlayCtrl::ReadBytes(void* id, void* data, int32_t bcount)
{
    StreamSource* source = static_cast<StreamSource*>(id);
    if ((source == nullptr) || (source->stream == nullptr) || (data == nullptr) || (bcount <= 0)) {
        return 0;
    }

    uint8_t* out = static_cast<uint8_t*>(data);
    int32_t done = 0;
    if (bcount <= 0)
        return 0;
    
    // consume pushback first
    if (source->hasPushback)
    {
        *out++ = source->pushbackByte;
        source->hasPushback = false;
        done++;
        bcount--;
        if (!bcount)
            return done;
    }

    // direct read
    int32_t got = (int32_t)source->stream->Read(out, bcount);
    if (got > 0)
        done += got;

    return done;
}

int32_t WavpackPlayCtrl::WriteBytes(void* id, void* data, int32_t bcount)
{
    (void)id;
    (void)data;
    (void)bcount;
    return 0;
}

int64_t WavpackPlayCtrl::GetPos(void* id)
{
    StreamSource* source = static_cast<StreamSource*>(id);
    if ((source == nullptr) || (source->stream == nullptr)) {
        return -1;
    }

    int64_t pos = source->stream->Tell();
    if (source->hasPushback)
        pos--;

    return pos;
}

int WavpackPlayCtrl::SetPosAbs(void* id, int64_t pos)
{
    StreamSource* source = static_cast<StreamSource*>(id);
    if ((source == nullptr) || (source->stream == nullptr)) {
        return 0;
    }

    if (!source->seekable)
        return -1;

    source->hasPushback = false;
    source->stream->Seek(SeekBase::Begin, (uint64_t)pos);

    return 0;
}

int WavpackPlayCtrl::SetPosRel(void* id, int64_t delta, int mode)
{
    StreamSource* source = static_cast<StreamSource*>(id);
    if ((source == nullptr) || (source->stream == nullptr)) {
        return 0;
    }

    if (!source->seekable)
        return -1;

    source->hasPushback = false;

    uint64_t base = 0;
    switch (mode)
    {
    case SEEK_SET:
        base = 0;
        break;
    case SEEK_CUR:
        base = (uint64_t)GetPos(id);
        break;
    case SEEK_END:
        base = source->stream->GetLength();
        break;
    default:
        return -1;
    }

    int64_t target = (int64_t)base + delta;
    if (target < 0)
        return -1;

    source->stream->Seek(SeekBase::Begin, (uint64_t)target);

    return 0;
}

int WavpackPlayCtrl::PushBackByte(void* id, int c)
{
    StreamSource* source = static_cast<StreamSource*>(id);
    if ((source == nullptr) || (source->stream == nullptr) || (c == EOF)) {
        return EOF;
    }

    // only one-byte pushback supported
    if (source->hasPushback)
        return EOF;

    source->pushbackByte = static_cast<uint8_t>(c);
    source->hasPushback = true;

    return c;
}

int64_t WavpackPlayCtrl::GetLength(void* id)
{
    StreamSource* source = static_cast<StreamSource*>(id);
    if ((source == nullptr) || (source->stream == nullptr)) {
        return 0;
    }

    if (!source->seekable)
        return -1;

    return static_cast<int64_t>(source->stream->GetLength());
}

int WavpackPlayCtrl::CanSeek(void* id)
{
    StreamSource* source = static_cast<StreamSource*>(id);
    if ((source == nullptr) || (source->stream == nullptr)) {
        return 0;
    }

    return source->seekable ? 1 : 0;
}

int WavpackPlayCtrl::TruncateHere(void* id)
{
    (void)id;
    return 0;
}

int WavpackPlayCtrl::CloseReader(void* id)
{
    (void)id;
    return 0;
}

void WavpackPlayCtrl::InitStreamSource(StreamSource& source, CDataStream* pStream)
{
    source.stream = pStream;
    source.hasPushback = false;
    source.pushbackByte = 0;
    if (pStream != nullptr)
        source.seekable = ((pStream->GetStyle() & dsStyleSeekable) != 0);
    else
        source.seekable = false;
}

std::unique_ptr<CDataStream> WavpackPlayCtrl::OpenWvcStream(CDataStream* pStream)
{
    if (pStream == nullptr) {
        return nullptr;
    }

    MateSource* source = pStream->QuerySource<MateSource>();
    if (source == nullptr) {
        return nullptr;
    }

    std::filesystem::path sourcePath{ pStream->GetName() };
    if (sourcePath.has_filename()) {
        auto accompanyName = sourcePath.filename();
        accompanyName.replace_extension(L".wvc");
        return source->CreateMateStream(accompanyName.c_str());
    }

    return nullptr;
}

void WavpackPlayCtrl::InitWavpackReader(WavpackStreamReader64& reader)
{
    reader.read_bytes = ReadBytes;
    reader.write_bytes = WriteBytes;
    reader.get_pos = GetPos;
    reader.set_pos_abs = SetPosAbs;
    reader.set_pos_rel = SetPosRel;
    reader.push_back_byte = PushBackByte;
    reader.get_length = GetLength;
    reader.can_seek = CanSeek;
    reader.truncate_here = TruncateHere;
    reader.close = CloseReader;
}

std::string WavpackPlayCtrl::ReadTagValue(const char* key) const
{
    if ((m_ctx == nullptr) || (key == nullptr)) {
        return {};
    }

    const int len = WavpackGetTagItem(const_cast<WavpackContext*>(m_ctx), key, nullptr, 0);
    if (len <= 0) {
        return {};
    }

    std::string value;
    value.resize(static_cast<std::size_t>(len) + 1);
    if (WavpackGetTagItem(const_cast<WavpackContext*>(m_ctx), key, value.data(), len + 1) <= 0) {
        return {};
    }
    value.resize(static_cast<std::size_t>(len));
    return value;
}
