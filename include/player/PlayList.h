#ifndef PLAY_LIST_H
#define PLAY_LIST_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Music.h"
#include "MusicItem.h"
#include "PlaySequence.h"

class CPlayList
{
public:
    CPlayList();

    const std::wstring& GetName() const;
    void SetName(const std::wstring& name);
    uint32_t GetCount() const;
    int32_t GetCurrentIndex() const { return m_curSeqIndex; }
    std::unique_ptr<CMusic> GetMusic(uint32_t index);
    std::unique_ptr<CMusic> GetCurrentMusic();
    std::unique_ptr<CMusic> GetNextMusic();
    std::unique_ptr<CMusic> GetPrevMusic();
    bool SetSequence(std::unique_ptr<CPlaySequence> sequence);
    bool GetItem(uint32_t index, MusicItem& item) const;
    bool AddItem(const MusicItem& item);
    bool AddItem(MusicItem&& item);
    void RemoveItem(uint32_t index);
    bool Copy(const std::vector<MusicItem>& items);

protected:
    std::unique_ptr<CMusic> MakeMusicByIndex(int32_t index) const;
    void SyncSequenceRange();

    std::wstring m_name;
    std::vector<MusicItem> m_items;
    std::unique_ptr<CPlaySequence> m_sequence;
    int32_t m_curSeqIndex;
};

#endif // PLAY_LIST_H
