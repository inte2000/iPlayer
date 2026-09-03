/*
20260310 AI 生成（Web 问答，手工粘贴代码）
大模型：ChatGPT 4/DeepSeek V2
*/
#include "framework.h"
#include "UnicodeConvert.h"
#include "TUIPlayerUI.h"
#include "StringEx.h"
#include "PlayList.h"
#include "PlayListFile.h"
#include "ComEnv.h"
#include "AudioCD.h"
#include <format>
#include <cmath>
#include <thread>

using namespace ftxui;

namespace {

bool BuildCDTrackPlaylist(const std::wstring& sourceName, CPlayList& playlist)
{
    CAudioCD audioCD;
    if (!audioCD.Open(sourceName)) {
        return false;
    }

    std::wstring playlistName = audioCD.GetTitle();
    if (playlistName.empty()) {
        playlistName = GetFileNamePart(sourceName);
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

} // namespace

TUIPlayerUI::TUIPlayerUI()
    : m_screen(ScreenInteractive::Fullscreen())
    , m_running(false)
    , m_stopRefresh(false)
    , m_volume(50)
    , m_seekPosition(0.0f)
    , m_showVolume(false)
    , m_currentSeconds(0.0f)
    , m_totalSeconds(0.0f)
    , m_status(PlaybackStatus::Stoped)
    , m_isPlaylist(false)
    , m_playlistCursor(0)
    , m_lastClickIndex(-1)
    , m_lastClickTime(std::chrono::steady_clock::now())
{
}

TUIPlayerUI::~TUIPlayerUI()
{
    Exit();
}

bool TUIPlayerUI::Init(std::unique_ptr<CAudioDevice> audioDevice,
    const std::string& deviceId,
    const std::string& filename,
    bool bPlaylist,
    bool bCdSource,
    const std::string& speakerLayout)
{
    m_playback = CPlayback::Create(this, std::move(audioDevice));
    m_playback->SetOutputDeviceId(deviceId);
   
    std::unique_ptr<CSpeakerConfig> speakCfg = LoadSpeakerConfig(speakerLayout);
    m_playback->SetSpeakerConfig(std::move(speakCfg));

    m_isPlaylist = bPlaylist || bCdSource;
    if (bPlaylist)
    {
        if (!LoadPlaylist(filename))
            return false;
    }
    else if (bCdSource)
    {
        const std::wstring sourceName = LocalMBCSToUtf16Le(filename);
        if (!BuildCDTrackPlaylist(sourceName, m_playlist))
            return false;
    }
    else
    {
        MusicItem item;
        item.itemType = MUSIC_ITEM_TYPE_FILE;
        item.res_url = LocalMBCSToUtf16Le(filename);
        item.title = GetFileNamePart(item.res_url);
        m_playlist.SetName(L"single");
        m_playlist.Copy({ item });
        m_isPlaylist = false;
    }

    std::unique_ptr<CMusic> currentMusic = m_playlist.GetCurrentMusic();
    if (!currentMusic)
        return false;

    m_currentPlayingResUrl = currentMusic->GetResUrl();
    std::unique_ptr<CAudioSource> source = currentMusic->MakeAudioSource();
    if (!source)
        return false;

    if (!m_playback->SetAudioSource(std::move(source), true))
        return false;

    RefreshPlaylistTitles();

    return true;
}

std::wstring MakeNameInfoText(const std::wstring& name, float totalSeconds)
{    
    std::chrono::duration<double> totalDuration(totalSeconds);
    std::wstring infoStr;
    
    std::wstring stemName = GetFileNamePart(name);
    if(totalSeconds >= 3600.0)
        infoStr = std::format(L"{} ({:%T})", stemName, totalDuration);
    else
        infoStr = std::format(L"{} ({:%M:%S})", stemName, totalDuration);

    return infoStr;
}

void TUIPlayerUI::OnAudioBegin(uint32_t streamIdx, const CMediaTag& metaInfo, const std::wstring& name, float totalSeconds)
{
    std::lock_guard<std::mutex> lock(m_mutex);

     try
    {
        uint32_t streamCount = metaInfo.QueryTagInteger(MediaTag_Streams).value_or(1);
        if (streamCount > 1)
        {
            m_infoStr = MakeNameInfoText(name, totalSeconds);
            std::wstring postfix = std::format(L" - {}/{}", (streamIdx + 1), streamCount);
            m_infoStr += postfix;
            //SetWindowTitleWithSongInfo(infoStr.c_str());
        }
        else
        {
            m_infoStr = MakeNameInfoText(name, totalSeconds);
            //SetWindowTitleWithSongInfo(infoStr.c_str());
        }

        m_title = AdaptiveToUtf8(metaInfo.QueryTagString(MediaTag_Title).value_or(""));
        m_brief = AdaptiveToUtf8(metaInfo.QueryTagString(MediaTag_Brief).value_or(""));
        std::string artist = AdaptiveToUtf8(metaInfo.QueryTagString(MediaTag_Artists).value_or(""));
        m_album = AdaptiveToUtf8(metaInfo.QueryTagString(MediaTag_Album).value_or(""));
        if(m_title.empty())
            m_title = Utf16ToUtf8(GetFileNamePart(name));
        if(!artist.empty()) 
        {
            m_title += " - ";
            m_title += artist;
        }

        m_totalSeconds = totalSeconds;
        m_currentSeconds = 0.0f;
        m_status = PlaybackStatus::Playing;
    }  
    catch (...)
    {
    }
}

void TUIPlayerUI::OnAudioUpdate(float curSeconds, float* powerBands, int bands)
{
    (void)powerBands;
    (void)bands;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentSeconds = curSeconds;
}

bool TUIPlayerUI::OnAudioEnd(bool lastStream)
{
    (void)lastStream;

    if (m_isPlaylist)
    {
        std::unique_ptr<CMusic> nextMusic = m_playlist.GetNextMusic();
        if (nextMusic)
        {
            m_currentPlayingResUrl = nextMusic->GetResUrl();
            std::unique_ptr<CAudioSource> source = nextMusic->MakeAudioSource();
            if (source)
            {
                std::shared_ptr<CPlayback> playback = m_playback;
                RefreshPlaylistTitles();

                std::thread([playback, nextSource = std::move(source)]() mutable {
                    ComEnv env;  //Manual bug fixing
                    try
                    {
                        if (playback)
                            playback->SetAudioSource(std::move(nextSource), true);
                    }
                    catch (...)
                    {
                    }
                }).detach();

                return true;
            }
        }
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_status = PlaybackStatus::Stoped;
    m_currentSeconds = m_totalSeconds;

    // Return false to indicate no new audio source is being started
    // The playback system will close the current source
    return false;
}

void TUIPlayerUI::OnControlEvent(PlayControl ctrl)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    switch (ctrl)
    {
        case PlayControl::Play:
            m_status = PlaybackStatus::Playing;
            break;

        case PlayControl::Pause:
            m_status = PlaybackStatus::Paused;
            break;

        case PlayControl::Stop:
            m_status = PlaybackStatus::Stoped;
            m_currentSeconds = 0.0f;
            break;

        default:
            break;
    }
}

void TUIPlayerUI::OnVolumeChanged(BOOL bMute, int vol)
{
    (void)bMute;
    (void)vol;

    // Store volume/mute state if needed for UI display
    // For now, this is a no-op as the callback focuses on playback state
}

float TUIPlayerUI::GetCurrentPosition()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentSeconds;
}

float TUIPlayerUI::GetTotalSeconds()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalSeconds;
}

PlaybackStatus TUIPlayerUI::GetStatus()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_status;
}

bool TUIPlayerUI::LoadPlaylist(const std::string& playlistFile)
{
    if (!LoadPlaylistFile(playlistFile, m_playlist))
        return false;

    m_playlistTitles.clear();

    m_playlistCursor = 0;
    RefreshPlaylistTitles();
    return (m_playlist.GetCount() > 0);
}

void TUIPlayerUI::RefreshPlaylistTitles()
{
    m_playlistTitles.clear();
    m_playlistTitles.reserve(m_playlist.GetCount());

    const uint32_t count = m_playlist.GetCount();
    for (uint32_t i = 0; i < count; ++i)
    {
        MusicItem item;
        if (!m_playlist.GetItem(i, item))
            continue;

        std::wstring fileName = GetFileNamePart(item.res_url);
        std::string title = Utf16ToUtf8(fileName);
        if (!m_currentPlayingResUrl.empty() && (item.res_url == m_currentPlayingResUrl))
            title = "* " + title;
        m_playlistTitles.push_back(std::move(title));
    }
}

bool TUIPlayerUI::PlayPlaylistIndex(int index, bool autoPlay)
{
    const int count = static_cast<int>(m_playlist.GetCount());
    if ((index < 0) || (index >= count))
        return false;

    std::unique_ptr<CMusic> music = m_playlist.GetMusic(static_cast<uint32_t>(index));
    if (!music)
        return false;

    m_currentPlayingResUrl = music->GetResUrl();
    std::unique_ptr<CAudioSource> source = music->MakeAudioSource();
    if (!source)
        return false;

    m_playlistCursor = index;
    RefreshPlaylistTitles();

    return m_playback->SetAudioSource(std::move(source), autoPlay);
}

void TUIPlayerUI::OnPlaySelectedPlaylistItem()
{
    if (!m_isPlaylist)
        return;

    PlayPlaylistIndex(m_playlistCursor, true);
}

void TUIPlayerUI::BuildUI()
{
    m_btn_close = Button(" ✕ ", [&] { Exit(); }, ButtonOption::Animated(Color::RedLight)) | size(WIDTH, EQUAL, 6);
    m_btn_prev = Button(" << ", [&] { OnSeekBackward(); }, ButtonOption::Animated(Color::Green)) | size(WIDTH, EQUAL, 6) ;
    m_btn_play = Button("  > ", [&] { OnPlayPause(); }, ButtonOption::Animated(Color::Green)) | size(WIDTH, EQUAL, 6);
    m_btn_pause = Button(" || ", [&] { OnPlayPause(); }, ButtonOption::Animated(Color::Green)) | size(WIDTH, EQUAL, 6);
    m_btn_stop = Button(" ▀ ", [&] { OnStop(); }, ButtonOption::Animated(Color::Green)) | size(WIDTH, EQUAL, 6);
    m_btn_next = Button(" >> ", [&] { OnSeekForward(); }, ButtonOption::Animated(Color::Green)) | size(WIDTH, EQUAL, 6);

    auto controls = Container::Horizontal({
        m_btn_prev,
        m_btn_play,
        m_btn_pause,
        m_btn_stop,
        m_btn_next,
    });

    if (m_isPlaylist)
    {
        MenuOption option = MenuOption::Vertical();
        option.entries_option.transform = [this](const EntryState& state) {
            MusicItem item;
            bool isCurrent = false;
            if (m_playlist.GetItem(static_cast<uint32_t>(state.index), item))
                isCurrent = (!m_currentPlayingResUrl.empty() && (item.res_url == m_currentPlayingResUrl));

            if (isCurrent)
                return text(state.label) | color(Color::YellowLight) | bold;
            if (state.active)
                return text(state.label) | inverted;
            return text(state.label);
        };

        m_playlist_menu = Menu(&m_playlistTitles, &m_playlistCursor, option);
        m_playlist_menu = CatchEvent(m_playlist_menu, [&](Event event) {
            if (event == Event::Return)
            {
                OnPlaySelectedPlaylistItem();
                return true;
            }

            if (event.is_mouse())
            {
                const Mouse& mouse = event.mouse();
                if ((mouse.button == Mouse::Left) && (mouse.motion == Mouse::Pressed))
                {
                    const auto now = std::chrono::steady_clock::now();
                    const auto gapMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastClickTime).count();
                    if ((m_lastClickIndex == m_playlistCursor) && (gapMs <= 400))
                    {
                        OnPlaySelectedPlaylistItem();
                        m_lastClickIndex = -1;
                    }
                    else
                    {
                        m_lastClickIndex = m_playlistCursor;
                    }
                    m_lastClickTime = now;
                }
            }

            return false;
        });
    }

    if (m_isPlaylist)
    {
        m_main_container = Container::Vertical({
            m_btn_close,
            controls,
            m_playlist_menu,
        });
    }
    else
    {
        m_main_container = Container::Vertical({
            m_btn_close,
            controls,
        });
    }

    m_root = Renderer(m_main_container, [&] {
        auto progStatus = GetProgressStatus();
        auto metsInfo = GetMusicMetaInfo();
        auto mediaFmt = GetMusicFormatStatus();

        // ===== Title bar =====
        auto title_bar =
            hbox({
                text(" imPlayer v0.1") | bold,
                filler(),
                m_btn_close->Render(),
                }) | bgcolor(Color::Blue) | color(Color::White) | size(HEIGHT, EQUAL, 2);

        // ===== Info =====
        auto info =
            vbox({
                text("Title : " + metsInfo.first),
                text("Album : " + metsInfo.second),
                text("Format : " + mediaFmt),
                })
            | bgcolor(Color::RGB(40, 40, 40))
            | color(Color::White)
            | border;

        // ===== Progress =====
        auto progress =
            hbox({
                gauge(progStatus.first) | flex,
                text(" " + progStatus.second),
                })
                | bgcolor(Color::Black)
            | border;

        auto control_bar =
            hbox({
                filler(),
                m_btn_prev->Render(),
                text("  "),
                m_btn_play->Render(),
                text("  "),
                m_btn_pause->Render(),
                text("  "),
                m_btn_stop->Render(),
                text("  "),
                m_btn_next->Render(),
                filler(),
                })
                | bgcolor(Color::Gold3) | size(HEIGHT, EQUAL, 3) | border;

        Element playlist_box = text("");
        if (m_isPlaylist)
        {
            playlist_box = vbox({
                text("Playlist") | bold,
                m_playlist_menu->Render() | frame | vscroll_indicator | size(HEIGHT, LESS_THAN, 9),
            }) | border;
        }

        if (m_isPlaylist)
        {
            return vbox({
                title_bar,
                info,
                progress,
                control_bar,
                playlist_box,
            });
        }

        return vbox({ title_bar, info, progress, control_bar });
        });
}

std::pair<float, std::string> TUIPlayerUI::GetProgressStatus()
{
    std::lock_guard lock(m_mutex);

    std::chrono::duration<float> totalDuration(m_totalSeconds);
    std::chrono::duration<float> curSeconds(m_currentSeconds);
    float prog = m_currentSeconds / m_totalSeconds;
    std::string status = std::format("{:%M:%S}/{:%M:%S}", curSeconds, totalDuration);

    return {prog, status};
}

std::pair<std::string, std::string> TUIPlayerUI::GetMusicMetaInfo()
{
    std::lock_guard lock(m_mutex);

    return {m_title, m_album};
}

std::string TUIPlayerUI::GetMusicFormatStatus()
{
    std::lock_guard lock(m_mutex);

    return m_brief;
}  

void TUIPlayerUI::OnPlayPause()
{
    if (!m_playback)
        return;

    PlaybackStatus status = GetStatus();
    if (status == PlaybackStatus::Playing)
    {
        m_playback->Pause();
        //PlaybackStatus status =m_playback->GetPlaybackStatus();        
    }
    else
    {
        if (m_playback->HasAudioSource())
        {
            //m_playSeconds = 0.0f;
            m_playback->Play();
        }        
    }
}

void TUIPlayerUI::OnStop()
{
    if (m_playback)
    {
        m_playback->Stop();
    }
}

void TUIPlayerUI::OnSeekForward()
{
    if (!m_playback)
        return;

    float currentPos = GetCurrentPosition();
    float totalSeconds = GetTotalSeconds();
    float newPos = currentPos + 10.0f; // Seek forward 10 seconds

    if (newPos > totalSeconds)
        newPos = totalSeconds;

    m_playback->SeekPosition(newPos);
}

void TUIPlayerUI::OnSeekBackward()
{
    if (!m_playback)
        return;

    float currentPos = GetCurrentPosition();
    float newPos = currentPos - 10.0f; // Seek backward 10 seconds

    if (newPos < 0.0f)
        newPos = 0.0f;

    m_playback->SeekPosition(newPos);
}

void TUIPlayerUI::Run()
{
    if (!m_playback)
        return;

    m_running = true;
    m_stopRefresh = false;

    //m_screen = ScreenInteractive::TerminalOutput();
    BuildUI();

    m_refreshThread = std::thread([this]() {
        while (!m_stopRefresh.load(std::memory_order_acquire))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(120));
            if (m_running.load(std::memory_order_acquire))
                m_screen.PostEvent(Event::Custom);
        }
    });

    // Main event loop
    m_screen.Loop(m_root);

    m_running = false;
    m_stopRefresh.store(true, std::memory_order_release);
    if (m_refreshThread.joinable())
        m_refreshThread.join();
}

void TUIPlayerUI::Exit() 
{
    if(!m_running)
        return;
        
    if(m_playback)
    {
        m_playback->Shutdown();
    }
    m_stopRefresh.store(true, std::memory_order_release);
    m_running = false;
    m_screen.Exit();
}
