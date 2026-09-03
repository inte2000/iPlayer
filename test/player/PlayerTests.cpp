#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <vector>

#include "Music.h"
#include "MusicItem.h"
#include "PlayInterface.h"
#include "PlayList.h"
#include "PlayListFile.h"
#include "PlaySequence.h"

TEST_CASE("CFileMusic exposes MusicItem properties", "[player]")
{
    MusicItem item;
    item.itemType = MUSIC_ITEM_TYPE_FILE;
    item.res_url = L"E:\\Music\\song.flac";
    item.track = 2;
    item.duration = 215.5f;
    item.title = L"Song";
    item.artists = L"A & B";
    item.album = L"Album";
    item.lyricsFilePath = L"E:\\Music\\song.lrc";
    item.albumArtFilePath = L"E:\\Music\\cover.jpg";

    CFileMusic music(item);

    REQUIRE(music.GetType() == MUSIC_ITEM_TYPE_FILE);
    REQUIRE(music.GetResUrl() == L"E:\\Music\\song.flac");
    REQUIRE(music.GetTrack() == 2);
    REQUIRE(music.GetDuration() == 215.5f);
    REQUIRE(music.GetTitle() == L"Song");
    REQUIRE(music.GetArtists() == L"A & B");
    REQUIRE(music.GetAlbum() == L"Album");
    REQUIRE(music.GetLyricsFilePath() == L"E:\\Music\\song.lrc");
    REQUIRE(music.GetAlbumArtFilePath() == L"E:\\Music\\cover.jpg");
}

TEST_CASE("CPlayList supports add remove and query", "[player]")
{
    CPlayList playList;
    playList.SetName(L"My List");

    MusicItem first;
    first.itemType = MUSIC_ITEM_TYPE_FILE;
    first.res_url = L"E:\\Music\\first.mp3";
    first.title = L"First";

    MusicItem second;
    second.itemType = MUSIC_ITEM_TYPE_FILE;
    second.res_url = L"E:\\Music\\second.flac";
    second.title = L"Second";

    REQUIRE(playList.GetName() == L"My List");
    REQUIRE(playList.GetCount() == 0);
    REQUIRE(playList.AddItem(first));
    REQUIRE(playList.AddItem(std::move(second)));
    REQUIRE(playList.GetCount() == 2);

    std::unique_ptr<CMusic> music0 = playList.GetMusic(0);
    REQUIRE(music0 != nullptr);
    REQUIRE(music0->GetType() == MUSIC_ITEM_TYPE_FILE);
    REQUIRE(music0->GetTitle() == L"First");

    playList.RemoveItem(0);
    REQUIRE(playList.GetCount() == 1);

    std::unique_ptr<CMusic> music1 = playList.GetMusic(0);
    REQUIRE(music1 != nullptr);
    REQUIRE(music1->GetTitle() == L"Second");
}

TEST_CASE("CPlayList copy replaces item list", "[player]")
{
    CPlayList playList;

    std::vector<MusicItem> items;
    MusicItem itemA;
    itemA.itemType = MUSIC_ITEM_TYPE_FILE;
    itemA.res_url = L"E:\\Music\\a.wav";
    itemA.title = L"A";
    items.push_back(itemA);

    MusicItem itemB;
    itemB.itemType = MUSIC_ITEM_TYPE_FILE;
    itemB.res_url = L"E:\\Music\\b.wav";
    itemB.title = L"B";
    items.push_back(itemB);

    REQUIRE(playList.Copy(items));
    REQUIRE(playList.GetCount() == 2);

    std::unique_ptr<CMusic> copied = playList.GetMusic(1);
    REQUIRE(copied != nullptr);
    REQUIRE(copied->GetTitle() == L"B");
}

TEST_CASE("CPlayList returns null for unsupported type", "[player]")
{
    CPlayList playList;

    MusicItem streamItem;
    streamItem.itemType = MUSIC_ITEM_TYPE_NETWORK_STREAM;
    streamItem.res_url = L"https://radio.example.com/live";
    REQUIRE(playList.AddItem(streamItem));

    std::unique_ptr<CMusic> music = playList.GetMusic(0);
    REQUIRE(music == nullptr);
}

TEST_CASE("CPlayList supports CD track music items", "[player]")
{
    CPlayList playList;

    MusicItem cdItem;
    cdItem.itemType = MUSIC_ITEM_TYPE_CD_TRACK;
    cdItem.res_url = L"CDDevice--F:";
    cdItem.track = 3;
    cdItem.title = L"Track 03";
    cdItem.artists = L"Artist";
    cdItem.album = L"Album";

    REQUIRE(playList.AddItem(cdItem));

    std::unique_ptr<CMusic> music = playList.GetMusic(0);
    REQUIRE(music != nullptr);
    CHECK(music->GetType() == MUSIC_ITEM_TYPE_CD_TRACK);
    CHECK(music->GetResUrl() == L"CDDevice--F:");
    CHECK(music->GetTrack() == 3);
    CHECK(music->GetTitle() == L"Track 03");
}

TEST_CASE("Playlist file save and load round trip", "[player]")
{
    const std::filesystem::path tmp = std::filesystem::temp_directory_path() / "implayer_playlist_test.json";
    std::error_code ec;
    std::filesystem::remove(tmp, ec);

    CPlayList source;
    source.SetName(L"Daily Mix");

    MusicItem fileItem;
    fileItem.itemType = MUSIC_ITEM_TYPE_FILE;
    fileItem.res_url = L"E:\\Music\\mix\\01.mp3";
    fileItem.track = 1;
    fileItem.duration = 188.25f;
    fileItem.title = L"Song 1";
    fileItem.artists = L"Artist A & Artist B";
    fileItem.album = L"Mix Album";
    fileItem.lyricsFilePath = L"E:\\Music\\mix\\01.lrc";
    fileItem.albumArtFilePath = L"E:\\Music\\mix\\cover.jpg";
    REQUIRE(source.AddItem(fileItem));

    MusicItem streamItem;
    streamItem.itemType = MUSIC_ITEM_TYPE_NETWORK_STREAM;
    streamItem.res_url = L"https://radio.example.com/live";
    streamItem.title = L"Live";
    REQUIRE(source.AddItem(streamItem));

    REQUIRE(SavePlaylistFile(tmp.string(), source));

    CPlayList loaded;
    REQUIRE(LoadPlaylistFile(tmp.string(), loaded));
    REQUIRE(loaded.GetName() == L"Daily Mix");
    REQUIRE(loaded.GetCount() == 2);

    MusicItem loaded0;
    REQUIRE(loaded.GetItem(0, loaded0));
    CHECK(loaded0.itemType == MUSIC_ITEM_TYPE_FILE);
    CHECK(loaded0.res_url == L"E:\\Music\\mix\\01.mp3");
    CHECK(loaded0.track == 1);
    CHECK(loaded0.duration == 188.25f);
    CHECK(loaded0.artists == L"Artist A & Artist B");

    MusicItem loaded1;
    REQUIRE(loaded.GetItem(1, loaded1));
    CHECK(loaded1.itemType == MUSIC_ITEM_TYPE_NETWORK_STREAM);
    CHECK(loaded1.res_url == L"https://radio.example.com/live");

    std::filesystem::remove(tmp, ec);
}

TEST_CASE("LoadPlaylistFile returns false on invalid json", "[player]")
{
    const std::filesystem::path tmp = std::filesystem::temp_directory_path() / "implayer_playlist_invalid_test.json";
    std::error_code ec;
    std::filesystem::remove(tmp, ec);

    {
        std::ofstream ofs(tmp);
        REQUIRE(ofs.is_open());
        ofs << "{\"name\":\"bad\",\"items\":123}";
    }

    CPlayList loaded;
    REQUIRE_FALSE(LoadPlaylistFile(tmp.string(), loaded));

    std::filesystem::remove(tmp, ec);
}

TEST_CASE("MakePlayListFileInterface scans folder and saves playlist", "[player]")
{
    const std::filesystem::path baseDir = std::filesystem::temp_directory_path() / "implayer_playlist_make_test";
    const std::filesystem::path inputDir = baseDir / "music";
    const std::filesystem::path outFile = baseDir / "generated.playlist";

    std::error_code ec;
    std::filesystem::remove_all(baseDir, ec);
    std::filesystem::create_directories(inputDir / "sub", ec);

    const std::filesystem::path wavFile = inputDir / "a.wav";
    {
        std::ofstream ofs(wavFile, std::ios::binary);
        REQUIRE(ofs.is_open());

        const unsigned char wavHeader[44] = {
            'R','I','F','F', 36,0,0,0, 'W','A','V','E',
            'f','m','t',' ', 16,0,0,0, 1,0, 1,0,
            68,172,0,0, 136,88,1,0, 2,0, 16,0,
            'd','a','t','a', 0,0,0,0
        };
        ofs.write(reinterpret_cast<const char*>(wavHeader), sizeof(wavHeader));
    }

    {
        std::ofstream txt(inputDir / "ignore.txt");
        REQUIRE(txt.is_open());
        txt << "ignore";
    }

    REQUIRE(MakePlayListFileInterface(inputDir.string(), false, outFile.string()) == 0);

    CPlayList playlist;
    REQUIRE(LoadPlaylistFile(outFile.string(), playlist));
    REQUIRE(playlist.GetCount() == 1);

    MusicItem item;
    REQUIRE(playlist.GetItem(0, item));
    CHECK(item.itemType == MUSIC_ITEM_TYPE_FILE);
    CHECK(item.title == L"a.wav");

    std::filesystem::remove_all(baseDir, ec);
}

TEST_CASE("Forward and backward play sequence index movement", "[player]")
{
    CForwardPlaySequence forward(0, 4, false);
    CPlaySequence& fbase = forward;
    REQUIRE(forward.MoveTo(1) == 1);
    REQUIRE(fbase.GetNextIndex() == 2);
    REQUIRE(fbase.GetPreviousIndex() == 0);
    REQUIRE(forward.MoveNext() == 2);
    REQUIRE(forward.MovePrevious() == 1);

    CBackwardPlaySequence backward(0, 4, false);
    CPlaySequence& bbase = backward;
    REQUIRE(backward.MoveTo(2) == 2);
    REQUIRE(bbase.GetNextIndex() == 1);
    REQUIRE(bbase.GetPreviousIndex() == 3);
    REQUIRE(backward.MoveNext() == 1);
    REQUIRE(backward.MovePrevious() == 2);
}

TEST_CASE("CPlayList GetCurrent Next Prev with sequence", "[player]")
{
    CPlayList playList;

    MusicItem first;
    first.itemType = MUSIC_ITEM_TYPE_FILE;
    first.res_url = L"E:\\Music\\first.mp3";
    first.title = L"First";
    REQUIRE(playList.AddItem(first));

    MusicItem second;
    second.itemType = MUSIC_ITEM_TYPE_FILE;
    second.res_url = L"E:\\Music\\second.mp3";
    second.title = L"Second";
    REQUIRE(playList.AddItem(second));

    MusicItem third;
    third.itemType = MUSIC_ITEM_TYPE_FILE;
    third.res_url = L"E:\\Music\\third.mp3";
    third.title = L"Third";
    REQUIRE(playList.AddItem(third));

    std::unique_ptr<CMusic> cur = playList.GetCurrentMusic();
    REQUIRE(cur != nullptr);
    CHECK(cur->GetTitle() == L"First");

    std::unique_ptr<CMusic> next = playList.GetNextMusic();
    REQUIRE(next != nullptr);
    CHECK(next->GetTitle() == L"Second");

    std::unique_ptr<CMusic> prev = playList.GetPrevMusic();
    REQUIRE(prev != nullptr);
    CHECK(prev->GetTitle() == L"First");

    REQUIRE(playList.SetSequence(std::make_unique<CBackwardPlaySequence>(0, static_cast<int32_t>(playList.GetCount()), false)));

    cur = playList.GetCurrentMusic();
    REQUIRE(cur != nullptr);
    CHECK(cur->GetTitle() == L"Third");

    next = playList.GetNextMusic();
    REQUIRE(next != nullptr);
    CHECK(next->GetTitle() == L"Second");

    prev = playList.GetPrevMusic();
    REQUIRE(prev != nullptr);
    CHECK(prev->GetTitle() == L"Third");
}
