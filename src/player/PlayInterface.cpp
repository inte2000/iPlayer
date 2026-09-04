#include <iostream>
#include <thread>
#include <mutex>
#include <conio.h>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <format>
#include <optional>
#include <unordered_set>
#include <vector>
#include "AudioSource.h"
#include "AudioTarget.h"
#include "WasapiAudioDevice.h"
#include "DsAudioDevice.h"
#include "UnicodeConvert.h"
#include "AudioDeviceMgmt.h"
#include "Playback.h"
#include "EncodingParams.h"
#include "encoder/EncoderFactory.h"
#include "encoder/EncoderParamName.h"
#include "encoder/EncoderParamterDefineUtils.h"
#include "encoder/WavEncoder.h"
#include "ScopeGuard.h"
#include "TUIPlayerUI.h"
#include "PlayInterface.h"
#include "PlayList.h"
#include "PlayListFile.h"
#include "ComEnv.h"
#include "DecoderFactory.h"
#include "AudioCD.h"
#include "StdFileSystem.h"

static std::string s_deviceId, s_devideName, s_deviceType;

struct ConvertFormatItem
{
    std::string name;
    std::string encoderName;
    uint32_t streamFmt;
};

static const ConvertFormatItem s_convertFormats[] =
{
    { "wav", CWavEncoder::GetName(), StreamFormatWav }
};

static std::optional<ConvertFormatItem> FindConvertFormat(const std::string& ffmt)
{
    std::string lowerName = ffmt;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    for (const auto& item : s_convertFormats)
    {
        if (item.name == lowerName) {
            return item;
        }
    }

    return std::nullopt;
}

static std::optional<std::pair<AudioDataFormat, uint32_t>> ParseConvertCfmt(const std::string& cfmt)
{
    std::string upperFmt = cfmt;
    std::transform(upperFmt.begin(), upperFmt.end(), upperFmt.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });

    if (upperFmt == "S16") {
        return std::make_pair(AudioDataFormat::PCM_S16, 16U);
    }
    if (upperFmt == "S24") {
        return std::make_pair(AudioDataFormat::PCM_S24, 24U);
    }
    if (upperFmt == "S32") {
        return std::make_pair(AudioDataFormat::PCM_S32, 32U);
    }
    if (upperFmt == "U8") {
        return std::make_pair(AudioDataFormat::PCM_U8, 8U);
    }
    if (upperFmt == "F32") {
        return std::make_pair(AudioDataFormat::Float32, 32U);
    }
    if (upperFmt == "F64") {
        return std::make_pair(AudioDataFormat::Float64, 64U);
    }

    return std::nullopt;
}

static const EncoderParamterDefine* FindDefineByName(const std::vector<EncoderParamterDefine>& defs, const std::string& name)
{
    for (const auto& def : defs)
    {
        if (def.GetName() == name) {
            return &def;
        }
    }

    return nullptr;
}

static bool ValidateDefineValue(const EncoderParamterDefine& def, const EncoderParamter::ValueType& value)
{
    const auto& options = def.GetOptionValues();
    if (options.empty()) {
        return true;
    }

    return std::find(options.begin(), options.end(), value) != options.end();
}

static void SetEncoderParamByDefine(std::vector<EncoderParamter>& params, const EncoderParamterDefine& def, EncoderParamter::ValueType value)
{
    if (!ValidateDefineValue(def, value)) {
        throw std::runtime_error(std::format("invalid value for encoder parameter: {}", def.GetName()));
    }

    EncoderParamter* param = FindEncoderParamter(params, def.GetName());
    if (param != nullptr) {
        param->SetType(def.GetType());
        param->SetValue(std::move(value));
        return;
    }

    AddEncoderParamter(params, def.GetName(), def.GetType(), std::move(value));
}

class PlaybackInterface : public PlaybackCallback
{
public:
    void BindPlayback(const std::shared_ptr<CPlayback>& playback)
    {
        m_playback = playback;
    }

    void BindPlaylist(CPlayList* playlist)
    {
        m_playlist = playlist;
    }

    void OnAudioBegin(uint32_t streamIdx, const CMediaTag& metaInfo, const std::wstring& name, float totalSeconds) override 
    {
        m_curSeconds = 0.0f;
        std::wcout << L"Audio playing start: " << name << std::endl;
    }
    void OnAudioUpdate(float curSeconds, float *powerBands, int bands) override
    {
        if(std::fabs(m_curSeconds - curSeconds) > 1.0f)
        {
            m_curSeconds = curSeconds;
            std::cout << "\rAudio playing seconds: " << m_curSeconds << std::flush;
        }
    }
    
    bool OnAudioEnd(bool lastStream) override
    {
        (void)lastStream;
        std::cout << "Audio playing end: " << std::endl;

        if ((m_playlist != nullptr) && (m_playback != nullptr))
        {
            std::unique_ptr<CMusic> nextMusic = m_playlist->GetNextMusic();
            if (nextMusic)
            {
                std::unique_ptr<CAudioSource> nextSource = nextMusic->MakeAudioSource();
                if (nextSource)
                {
                    std::shared_ptr<CPlayback> playback = m_playback;
                    std::thread([playback, source = std::move(nextSource)]() mutable {
                        ComEnv env; //Manual bug fixing
                        try
                        {
                            if (playback)
                                playback->SetAudioSource(std::move(source), true);
                        }
                        catch (...)
                        {
                        }
                    }).detach();

                    return true;
                }
            }
        }

        return true;
    }
    void OnControlEvent(PlayControl ctrl) override
    {

    }
    void OnVolumeChanged(BOOL bMute, int vol) override
    {

    }
protected:
    float m_curSeconds;
    std::shared_ptr<CPlayback> m_playback;
    CPlayList* m_playlist = nullptr;
};

static std::unique_ptr<CAudioDevice> MakeAudioDevice(const std::string& type, const std::string& name, const std::string& deviceId)
{
    if (type.compare("Native") == 0)
    {
        if (name.compare("Wasapi (Share mode)") == 0)
        {
            return std::make_unique<CWasapiAudioDevice>(deviceId, WasapiMode::Share);
        }
        else if (name.compare("Wasapi (Exclusive mode)") == 0)
        {
            return std::make_unique<CWasapiAudioDevice>(deviceId, WasapiMode::Exclusive);
        }
        else if (name.compare("DirectSound") == 0)
        {
            return std::make_unique<CDsAudioDevice>(deviceId);
        }
        else
            throw std::runtime_error("Invalid audio device name, check the config file and correct it!");
    }
    else if (type.compare("Plugin") == 0)
    {
        throw std::runtime_error("Invalid audio device type, current not support!");
    }
    else
        throw std::runtime_error("Invalid audio device type, check the config file and correct it!");
}

void SetupDevice(const std::string& type, const std::string& deviceName, const std::string& deviceId)
{
    s_deviceType = type;
    s_devideName = deviceName;
    s_deviceId = deviceId;
}

static void ApplyPlaylistSequence(CPlayList& playlist, int sequenceMode)
{
    const int32_t end = static_cast<int32_t>(playlist.GetCount());
    if (sequenceMode == 1) {
        playlist.SetSequence(std::make_unique<CBackwardPlaySequence>(0, end, false));
        return;
    }

    playlist.SetSequence(std::make_unique<CForwardPlaySequence>(0, end, false));
}

static bool BuildCDTrackPlaylist(const std::wstring& sourceName, CPlayList& playlist)
{
    CAudioCD audioCD;
    if (!audioCD.Open(sourceName)) {
        return false;
    }

    std::wstring playlistName = audioCD.GetTitle();
    if (playlistName.empty()) {
        playlistName = std::filesystem::path(sourceName).filename().wstring();
    }
    if (playlistName.empty()) {
        playlistName = L"Audio CD";
    }
    playlist.SetName(playlistName);

    const uint32_t trackCount = audioCD.GetTrackCount();
    for (uint32_t i = 0; i < trackCount; ++i)
    {
        const CD_TRACK_INFO& trackInfo = audioCD.GetTrackInfo(i);
        if (!trackInfo.isAudio || (trackInfo.length <= 0)) {
            continue;
        }

        MusicItem item;
        item.itemType = MUSIC_ITEM_TYPE_CD_TRACK;
        item.res_url = sourceName;
        item.track = static_cast<int32_t>(i + 1);
        item.duration = audioCD.GetTrackTime(i);
        item.title = audioCD.GetTrackTitle(i);
        item.artists = audioCD.GetTrackArtist(i);
        item.album = audioCD.GetTrackAlbum(i);
        playlist.AddItem(std::move(item));
    }

    return (playlist.GetCount() > 0);
}

static std::vector<std::string> SplitExtList(const std::string& extList)
{
    std::vector<std::string> result;

    std::string token;
    for (char ch : extList)
    {
        if (ch == ';')
        {
            if (!token.empty())
            {
                std::transform(token.begin(), token.end(), token.begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
                result.push_back(token);
                token.clear();
            }
            continue;
        }
        token.push_back(ch);
    }

    if (!token.empty())
    {
        std::transform(token.begin(), token.end(), token.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        result.push_back(token);
    }

    return result;
}

static std::unordered_set<std::string> BuildDecoderExtSet()
{
    std::unordered_set<std::string> extSet;
    const auto extFilters = CDecoderFactory::GetInstance().GetPluginDecoderExtFilters();
    for (const auto& item : extFilters)
    {
        const auto extItems = SplitExtList(item.extList);
        for (const auto& ext : extItems)
        {
            if (!ext.empty())
                extSet.insert(ext);
        }
    }

    return extSet;
}

static std::filesystem::path ResolvePlaylistSavePath(const std::filesystem::path& folderPath, const std::string& playlistFile)
{
    if (!playlistFile.empty())
    {
        std::filesystem::path customPath = LocalMBCSToUtf16Le(playlistFile);
        if (customPath.has_extension())
            return customPath;
        return customPath.replace_extension(L".playlist");
    }

    std::filesystem::path savePath = GetApplicationBasePath();
    savePath /= "playlists";
    std::wstring baseName = folderPath.filename().wstring();
    if (baseName.empty())
        baseName = L"default";
    savePath /= baseName;
    savePath.replace_extension(L".playlist");
    return savePath;
}

static bool IsSupportedFileByExtOrParser(const std::filesystem::path& path, const std::unordered_set<std::string>& extSet)
{
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    if (!extSet.empty() && extSet.find(ext) != extSet.end())
        return true;

    const uint32_t fmt = CDecoderFactory::GetInstance().ParseFileFormat(path.wstring());
    return fmt != StreamFormatUnknown;
}

int MakePlayListFileInterface(const std::string& folder, bool recursion, const std::string& playlistFile)
{
    const std::filesystem::path folderPath = LocalMBCSToUtf16Le(folder);
    if (!std::filesystem::exists(folderPath) || !std::filesystem::is_directory(folderPath))
    {
        std::cerr << "invalid folder path: " << folder << std::endl;
        return -1;
    }

    const auto extSet = BuildDecoderExtSet();

    CPlayList playlist;
    playlist.SetName(folderPath.filename().wstring());

    auto addEntry = [&playlist, &extSet](const std::filesystem::directory_entry& entry) {
        if (!entry.is_regular_file())
            return;

        const std::filesystem::path filePath = entry.path();
        if (!IsSupportedFileByExtOrParser(filePath, extSet))
            return;

        MusicItem item;
        item.itemType = MUSIC_ITEM_TYPE_FILE;
        item.res_url = std::filesystem::absolute(filePath).wstring();
        item.title = filePath.filename().wstring();
        playlist.AddItem(std::move(item));
    };

    if (recursion)
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(folderPath))
        {
            addEntry(entry);
        }
    }
    else
    {
        for (const auto& entry : std::filesystem::directory_iterator(folderPath))
        {
            addEntry(entry);
        }
    }

    if (playlist.GetCount() == 0)
    {
        std::cerr << "no playable files found in folder" << std::endl;
        return -1;
    }

    const std::filesystem::path savePath = ResolvePlaylistSavePath(folderPath, playlistFile);
    std::error_code ec;
    std::filesystem::create_directories(savePath.parent_path(), ec);

    const std::string utf8SavePath = Utf16ToUtf8(savePath.wstring());
    if (!SavePlaylistFile(utf8SavePath, playlist))
    {
        std::cerr << "fail to save playlist file: " << utf8SavePath << std::endl;
        return -1;
    }

    std::cout << "playlist saved: " << utf8SavePath << ", items=" << playlist.GetCount() << std::endl;
    return 0;
}

void StartPlayingInterface(const std::string& filename,
    bool bPlaylist,
    bool bCdSource,
    int sequenceMode,
    const std::string& speakerLayout)
{
    CAudioDeviceMgmt devMgmt; 

    PlaybackInterface pif;
    std::unique_ptr<CAudioDevice> audioDevice = MakeAudioDevice(s_deviceType, s_devideName, s_deviceId);
    std::shared_ptr<CPlayback> playback = CPlayback::Create(&pif, std::move(audioDevice));
    playback->SetOutputDeviceId(s_deviceId);
    pif.BindPlayback(playback);

    //std::string utf8Speakerfile = GetSpeakerConfigFilePathname();
    std::unique_ptr<CSpeakerConfig> speakCfg = LoadSpeakerConfig(speakerLayout);
    playback->SetSpeakerConfig(std::move(speakCfg));

    CPlayList playlist;
    auto loadMusic = [&](std::unique_ptr<CMusic> music, bool autoStart) {
        if (!music)
            return false;

        std::unique_ptr<CAudioSource> source = music->MakeAudioSource();
        if (!source)
            return false;

        return playback->SetAudioSource(std::move(source), autoStart);
    };

    if (bPlaylist || bCdSource)
    {
        if (bCdSource)
        {
            const std::wstring sourceName = LocalMBCSToUtf16Le(filename);
            if (!BuildCDTrackPlaylist(sourceName, playlist))
                throw std::runtime_error("fail to build playlist from CD source");
        }
        else
        {
            if (!LoadPlaylistFile(filename, playlist))
                throw std::runtime_error("fail to load playlist file");
        }

        if (playlist.GetCount() == 0)
            throw std::runtime_error("playlist is empty");

        ApplyPlaylistSequence(playlist, sequenceMode);

        pif.BindPlaylist(&playlist);

        if (!loadMusic(playlist.GetCurrentMusic(), true))
            throw std::runtime_error("playlist has no valid items");
    }
    else
    {
        pif.BindPlaylist(nullptr);
        std::wstring wfilename = LocalMBCSToUtf16Le(filename);
        std::unique_ptr<CAudioSource> audioSource = MakeFileAudioSource(wfilename);
        playback->SetAudioSource(std::move(audioSource), true);
    }

    std::cout << "Press s=play, p=pause, e=stop, n=next, b=prev, c=exit" << std::endl;
    char userKey = ' ';
    while (userKey != 'c')
    {
        if (_kbhit())
        {
            userKey = static_cast<char>(_getch());
            if (userKey == 's')
            {
                if (playback->HasAudioSource())
                    playback->Play();
            }
            else if (userKey == 'p')
            {
                if (playback->HasAudioSource())
                    playback->Pause();
            }
            else if (userKey == 'e')
            {
                if (playback->HasAudioSource())
                    playback->Stop();
            }
            else if ((userKey == 'n') && (bPlaylist || bCdSource))
            {
                if (!loadMusic(playlist.GetNextMusic(), true))
                    std::cout << "no next playlist item" << std::endl;
            }
            else if ((userKey == 'b') && (bPlaylist || bCdSource))
            {
                if (!loadMusic(playlist.GetPrevMusic(), true))
                    std::cout << "no previous playlist item" << std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    playback->Shutdown();
}

void StartPlayingTuiInterface(const std::string& filename,
    bool bPlaylist,
    bool bCdSource,
    int sequenceMode,
    const std::string& speakerLayout)
{
    std::unique_ptr<CAudioDevice> audioDevice = MakeAudioDevice(s_deviceType, s_devideName, s_deviceId);
    TUIPlayerUI tuiUI;
    if(!tuiUI.Init(std::move(audioDevice), s_deviceId, filename, bPlaylist, bCdSource, sequenceMode, speakerLayout))
        throw std::runtime_error("Fail to init tui object!");
                
    tuiUI.Run();        
}

int ConvertMediaFileInterface(const std::string& srcFilename,
    const std::string& outFilename,
    const std::string& formatName,
    uint32_t sampleRate,
    const std::string& cfmt,
    uint32_t channels)
{
    if (srcFilename.empty()) {
        throw std::runtime_error("missing source filename (-f)");
    }
    if (outFilename.empty()) {
        throw std::runtime_error("missing output filename (-out)");
    }
    if (sampleRate == 0) {
        throw std::runtime_error("invalid sample rate");
    }
    if (channels == 0) {
        throw std::runtime_error("invalid channels");
    }

    const auto fmtItem = FindConvertFormat(formatName);
    if (!fmtItem.has_value()) {
        throw std::runtime_error(std::format("unsupported convert format: {}", formatName));
    }

    const auto cfmtPair = ParseConvertCfmt(cfmt);
    if (!cfmtPair.has_value()) {
        throw std::runtime_error(std::format("unsupported cfmt: {}", cfmt));
    }

    const auto encoderItemOpt = CEncoderFactory::GetInstance().GetEncoderItem(fmtItem->encoderName);
    if (!encoderItemOpt.has_value()) {
        throw std::runtime_error(std::format("encoder not found for format {}: {}", formatName, fmtItem->encoderName));
    }

    const auto defs = encoderItemOpt->QueryParamters();
    std::vector<EncoderParamter> params = BuildDefaultEncoderParamters(defs);

    const auto* codeFormatDef = FindDefineByName(defs, EncoderParamName::CodeFormat);
    const auto* sampleRateDef = FindDefineByName(defs, EncoderParamName::SampleRates);
    const auto* bitsDef = FindDefineByName(defs, EncoderParamName::BitsPerSample);
    const auto* channelDef = FindDefineByName(defs, EncoderParamName::Channels);
    if ((codeFormatDef == nullptr) || (sampleRateDef == nullptr) || (bitsDef == nullptr) || (channelDef == nullptr)) {
        throw std::runtime_error("encoder parameter definition incomplete");
    }

    SetEncoderParamByDefine(params, *codeFormatDef, static_cast<uint32_t>(cfmtPair->first));
    SetEncoderParamByDefine(params, *sampleRateDef, sampleRate);
    SetEncoderParamByDefine(params, *bitsDef, cfmtPair->second);
    SetEncoderParamByDefine(params, *channelDef, channels);

    const std::wstring srcFilenameW = LocalMBCSToUtf16Le(srcFilename);
    const std::wstring outFilenameW = LocalMBCSToUtf16Le(outFilename);

    auto source = MakeFileAudioSource(srcFilenameW);
    auto target = MakeFileAudioTarget(outFilenameW, fmtItem->streamFmt, encoderItemOpt->name);
    if (!target) {
        throw std::runtime_error("failed to create audio target");
    }

    target->SetMetaInformation(source->GetMetaInformation());

    if (!target->InitEncoder(params)) {
        throw std::runtime_error("failed to initialize encoder");
    }

    const AudioFormat outFmt = target->GetAudioFormat();
    const uint32_t totalStreams = source->GetTotalAudioStreams();
    for (uint32_t streamIdx = 0; streamIdx < totalStreams; ++streamIdx)
    {
        if (!source->StartAudioStream(streamIdx)) {
            throw std::runtime_error(std::format("failed to start stream {}", streamIdx));
        }
        auto stopGuard = MakeGuard([&]() {
            source->StopAudioStream(streamIdx);
        });

        if (!source->SetOutputFormat(outFmt, false)) {
            throw std::runtime_error(std::format("failed to negotiate output format for stream {}", streamIdx));
        }

        const AudioFormat audioFmt = source->GetOutputFormat();
        const uint32_t bufSize = audioFmt.blockAlign * DECODE_BUF_FRAMES;
        if (bufSize == 0) {
            throw std::runtime_error("invalid negotiated output buffer size");
        }

        std::vector<uint8_t> buffer(bufSize);
        for (;;)
        {
            const uint32_t readFrames = source->ReadBuffer(buffer.data(), bufSize, DECODE_BUF_FRAMES);
            if (readFrames == 0) {
                break;
            }

            const uint32_t writeFrames = target->WriteBuffer(buffer.data(), readFrames, audioFmt);
            if (writeFrames != readFrames) {
                throw std::runtime_error("target write frames mismatch");
            }
        }

        if (target->FlushBuffer() == 0) {
            throw std::runtime_error("failed to flush encoder output");
        }
    }

    return 0;
}
