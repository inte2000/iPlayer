#include "Music.h"

#include <utility>

#include "AudioSource.h"

CFileMusic::CFileMusic(const MusicItem& item)
    : m_item(item)
{
}

CFileMusic::CFileMusic(MusicItem&& item)
    : m_item(std::move(item))
{
}

uint32_t CFileMusic::GetType() const
{
    return static_cast<uint32_t>(m_item.itemType);
}

std::wstring CFileMusic::GetResUrl() const
{
    return m_item.res_url;
}

std::unique_ptr<CAudioSource> CFileMusic::MakeAudioSource() const
{
    return MakeFileAudioSource(m_item.res_url);
}

int32_t CFileMusic::GetTrack() const
{
    return m_item.track;
}

float CFileMusic::GetDuration() const
{
    return m_item.duration;
}

std::wstring CFileMusic::GetTitle() const
{
    return m_item.title;
}

std::wstring CFileMusic::GetArtists() const
{
    return m_item.artists;
}

std::wstring CFileMusic::GetAlbum() const
{
    return m_item.album;
}

std::wstring CFileMusic::GetLyricsFilePath() const
{
    return m_item.lyricsFilePath;
}

std::wstring CFileMusic::GetAlbumArtFilePath() const
{
    return m_item.albumArtFilePath;
}

CCDTrackMusic::CCDTrackMusic(const MusicItem& item)
    : m_item(item)
{
}

CCDTrackMusic::CCDTrackMusic(MusicItem&& item)
    : m_item(std::move(item))
{
}

uint32_t CCDTrackMusic::GetType() const
{
    return static_cast<uint32_t>(m_item.itemType);
}

std::wstring CCDTrackMusic::GetResUrl() const
{
    return m_item.res_url;
}

std::unique_ptr<CAudioSource> CCDTrackMusic::MakeAudioSource() const
{
    return MakeCDTrackAudioSource(m_item.res_url, static_cast<uint32_t>(m_item.track));
}

int32_t CCDTrackMusic::GetTrack() const
{
    return m_item.track;
}

float CCDTrackMusic::GetDuration() const
{
    return m_item.duration;
}

std::wstring CCDTrackMusic::GetTitle() const
{
    return m_item.title;
}

std::wstring CCDTrackMusic::GetArtists() const
{
    return m_item.artists;
}

std::wstring CCDTrackMusic::GetAlbum() const
{
    return m_item.album;
}

std::wstring CCDTrackMusic::GetLyricsFilePath() const
{
    return m_item.lyricsFilePath;
}

std::wstring CCDTrackMusic::GetAlbumArtFilePath() const
{
    return m_item.albumArtFilePath;
}
