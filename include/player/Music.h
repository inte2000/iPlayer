#ifndef MUSIC_H
#define MUSIC_H

#include <cstdint>
#include <memory>
#include <string>

#include "MusicItem.h"

class CAudioSource;

class CMusic
{
public:
    virtual ~CMusic() = default;

    virtual uint32_t GetType() const = 0;
    virtual std::wstring GetResUrl() const = 0;
    virtual std::unique_ptr<CAudioSource> MakeAudioSource() const = 0;

    virtual int32_t GetTrack() const = 0;
    virtual float GetDuration() const = 0;
    virtual std::wstring GetTitle() const = 0;
    virtual std::wstring GetArtists() const = 0;
    virtual std::wstring GetAlbum() const = 0;
    virtual std::wstring GetLyricsFilePath() const = 0;
    virtual std::wstring GetAlbumArtFilePath() const = 0;
};

class CFileMusic : public CMusic
{
public:
    explicit CFileMusic(const MusicItem& item);
    explicit CFileMusic(MusicItem&& item);

    uint32_t GetType() const override;
    std::wstring GetResUrl() const override;
    std::unique_ptr<CAudioSource> MakeAudioSource() const override;

    int32_t GetTrack() const override;
    float GetDuration() const override;
    std::wstring GetTitle() const override;
    std::wstring GetArtists() const override;
    std::wstring GetAlbum() const override;
    std::wstring GetLyricsFilePath() const override;
    std::wstring GetAlbumArtFilePath() const override;

private:
    MusicItem m_item;
};

class CCDTrackMusic : public CMusic
{
public:
    explicit CCDTrackMusic(const MusicItem& item);
    explicit CCDTrackMusic(MusicItem&& item);

    uint32_t GetType() const override;
    std::wstring GetResUrl() const override;
    std::unique_ptr<CAudioSource> MakeAudioSource() const override;

    int32_t GetTrack() const override;
    float GetDuration() const override;
    std::wstring GetTitle() const override;
    std::wstring GetArtists() const override;
    std::wstring GetAlbum() const override;
    std::wstring GetLyricsFilePath() const override;
    std::wstring GetAlbumArtFilePath() const override;

private:
    MusicItem m_item;
};

#endif // MUSIC_H
