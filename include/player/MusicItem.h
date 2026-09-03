#ifndef MUSIC_ITEM_H
#define MUSIC_ITEM_H

#include <cstdint>
#include <string>

constexpr int MUSIC_ITEM_TYPE_UNKNOWN = 0;
constexpr int MUSIC_ITEM_TYPE_FILE = 1;
constexpr int MUSIC_ITEM_TYPE_CD_TRACK = 2;
constexpr int MUSIC_ITEM_TYPE_CUE_TRACK = 3;
constexpr int MUSIC_ITEM_TYPE_NETWORK_STREAM = 4;

struct MusicItem
{
    int itemType = MUSIC_ITEM_TYPE_UNKNOWN;
    std::wstring res_url;

    int32_t track = 0;
    float duration = 0.0f;
    std::wstring title;
    std::wstring artists;
    std::wstring album;
    std::wstring lyricsFilePath;
    std::wstring albumArtFilePath;
};

#endif // MUSIC_ITEM_H
