/*
20260310 AI 生成（Web 问答，手工粘贴代码）
大模型：ChatGPT 4/DeepSeek V2
*/
#ifndef TUI_PLAYER_UI_H
#define TUI_PLAYER_UI_H

#include <memory>
#include <atomic>
#include <functional>
#include <chrono>
#include <thread>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"

#include "Playback.h"
#include "MusicItem.h"
#include "PlayList.h"

class TUIPlayerUI : public PlaybackCallback
{
public:
    explicit TUIPlayerUI();
    ~TUIPlayerUI();

    bool Init(std::unique_ptr<CAudioDevice> audioDevice,
        const std::string& deviceId,
        const std::string& filename,
        bool bPlaylist,
        bool bCdSource,
        const std::string& speakerLayout);

    void Run();
    void Exit();

    void OnAudioBegin(uint32_t streamIdx, const CMediaTag& metaInfo, const std::wstring& name, float totalSeconds) override;
    void OnAudioUpdate(float curSeconds, float* powerBands, int bands) override;
    bool OnAudioEnd(bool lastStream) override;
    void OnControlEvent(PlayControl ctrl) override;
    void OnVolumeChanged(BOOL bMute, int vol) override;

    float GetCurrentPosition();
    float GetTotalSeconds();
    PlaybackStatus GetStatus();

private:
    void BuildUI();
    std::pair<float, std::string> GetProgressStatus();
    std::pair<std::string, std::string> GetMusicMetaInfo();
    std::string GetMusicFormatStatus();

    void OnPlayPause();
    void OnStop();
    void OnSeekForward();
    void OnSeekBackward();
    bool LoadPlaylist(const std::string& playlistFile);
    bool PlayPlaylistIndex(int index, bool autoPlay);
    void RefreshPlaylistTitles();
    void OnPlaySelectedPlaylistItem();

    ftxui::ScreenInteractive m_screen;
    ftxui::Component m_root;
    ftxui::Component m_main_container;

    ftxui::Component m_btn_close;
    ftxui::Component m_btn_prev;
    ftxui::Component m_btn_play;
    ftxui::Component m_btn_pause;
    ftxui::Component m_btn_stop;
    ftxui::Component m_btn_next;
    ftxui::Component m_playlist_menu;

    std::shared_ptr<CPlayback> m_playback;
    std::thread m_refreshThread;

    std::atomic<bool> m_running;
    std::atomic<bool> m_stopRefresh;
    int m_volume;
    float m_seekPosition;
    std::atomic<bool> m_showVolume;

    mutable std::mutex m_mutex;

    float m_currentSeconds;
    float m_totalSeconds;
    std::wstring m_infoStr;
    PlaybackStatus m_status;
    std::string m_title;
    std::string m_album;
    std::string m_brief;

    bool m_isPlaylist;
    int m_playlistCursor;
    int m_lastClickIndex;
    std::chrono::steady_clock::time_point m_lastClickTime;
    std::vector<std::string> m_playlistTitles;
    CPlayList m_playlist;
    std::wstring m_currentPlayingResUrl;
};

#endif // TUI_PLAYER_UI_H
