#include "PlayList.h"

#include <utility>

CPlayList::CPlayList()
    : m_sequence(MakePlaySequence(0, 0, 0, false))
    , m_curSeqIndex(-1)
{
}

const std::wstring& CPlayList::GetName() const
{
    return m_name;
}

void CPlayList::SetName(const std::wstring& name)
{
    m_name = name;
}

uint32_t CPlayList::GetCount() const
{
    return static_cast<uint32_t>(m_items.size());
}

std::unique_ptr<CMusic> CPlayList::GetMusic(uint32_t index)
{
    if (index >= m_items.size())
        return nullptr;

    if (m_sequence)
    {
        m_curSeqIndex = m_sequence->MoveTo(static_cast<int32_t>(index));
        if (m_curSeqIndex < 0)
            return nullptr;
    }
    else
    {
        m_curSeqIndex = static_cast<int32_t>(index);
    }

    return MakeMusicByIndex(m_curSeqIndex);
}

std::unique_ptr<CMusic> CPlayList::GetCurrentMusic()
{
    if (m_items.empty() || !m_sequence)
        return nullptr;

    if ((m_curSeqIndex < 0) || (m_curSeqIndex >= static_cast<int32_t>(m_items.size())))
    {
        m_curSeqIndex = (m_sequence->GetSequenceMode() == 1) ? (static_cast<int32_t>(m_items.size()) - 1) : 0;
        m_curSeqIndex = m_sequence->MoveTo(m_curSeqIndex);
    }

    if (m_curSeqIndex < 0)
        return nullptr;

    return MakeMusicByIndex(m_curSeqIndex);
}

std::unique_ptr<CMusic> CPlayList::GetNextMusic()
{
    if (GetCurrentMusic() == nullptr)
        return nullptr;

    m_curSeqIndex = m_sequence->MoveNext();
    if (m_curSeqIndex < 0)
        return nullptr;

    return MakeMusicByIndex(m_curSeqIndex);
}

std::unique_ptr<CMusic> CPlayList::GetPrevMusic()
{
    if (GetCurrentMusic() == nullptr)
        return nullptr;

    m_curSeqIndex = m_sequence->MovePrevious();
    if (m_curSeqIndex < 0)
        return nullptr;

    return MakeMusicByIndex(m_curSeqIndex);
}

bool CPlayList::SetSequence(std::unique_ptr<CPlaySequence> sequence)
{
    if (!sequence)
        return false;

    m_sequence = std::move(sequence);
    m_curSeqIndex = -1;
    SyncSequenceRange();
    return true;
}

std::unique_ptr<CMusic> CPlayList::MakeMusicByIndex(int32_t index) const
{
    if ((index < 0) || (index >= static_cast<int32_t>(m_items.size())))
        return nullptr;

    const MusicItem& item = m_items[static_cast<size_t>(index)];
    if (item.itemType == MUSIC_ITEM_TYPE_FILE)
        return std::make_unique<CFileMusic>(item);
    if (item.itemType == MUSIC_ITEM_TYPE_CD_TRACK)
        return std::make_unique<CCDTrackMusic>(item);

    return nullptr;
}

void CPlayList::SyncSequenceRange()
{
    if (!m_sequence)
        return;

    m_sequence->SetRange(0, static_cast<int32_t>(m_items.size()));
    if (m_items.empty())
    {
        m_curSeqIndex = -1;
        return;
    }

    if ((m_curSeqIndex < 0) || (m_curSeqIndex >= static_cast<int32_t>(m_items.size())))
        m_curSeqIndex = (m_sequence->GetSequenceMode() == 1) ? (static_cast<int32_t>(m_items.size()) - 1) : 0;

    m_curSeqIndex = m_sequence->MoveTo(m_curSeqIndex);
}

bool CPlayList::GetItem(uint32_t index, MusicItem& item) const
{
    if (index >= m_items.size())
        return false;

    item = m_items[index];
    return true;
}

bool CPlayList::AddItem(const MusicItem& item)
{
    m_items.push_back(item);
    SyncSequenceRange();
    return true;
}

bool CPlayList::AddItem(MusicItem&& item)
{
    m_items.push_back(std::move(item));
    SyncSequenceRange();
    return true;
}

void CPlayList::RemoveItem(uint32_t index)
{
    if (index >= m_items.size())
        return;

    m_items.erase(m_items.begin() + index);
    if ((m_curSeqIndex >= 0) && (static_cast<uint32_t>(m_curSeqIndex) > index))
        --m_curSeqIndex;

    SyncSequenceRange();
}

bool CPlayList::Copy(const std::vector<MusicItem>& items)
{
    m_items = items;
    SyncSequenceRange();
    return true;
}
