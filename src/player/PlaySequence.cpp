#include <string_view>
#include <format>
#include "PlaySequence.h"


int32_t CPlaySequence::MoveTo(int32_t index)
{
    if ((index >= m_begin) && (index < m_end))
    {
        m_curIdx = index;
        m_PrevIdx = GetPreviousIndex(index);
        m_NextIdx = GetNextIndex(index);
        return m_curIdx;
    }
    else
        return -1;
}

int32_t CPlaySequence::MovePrevious()
{
    if ((m_end - m_begin) <= 0)
        return -1;
    if (m_PrevIdx < 0)
        return -1;

    m_NextIdx = m_curIdx;
    m_curIdx = m_PrevIdx;
    m_PrevIdx = GetPreviousIndex(m_curIdx);

    return m_curIdx;
}

int32_t CPlaySequence::MoveNext()
{
    if ((m_end - m_begin) <= 0)
        return -1;
    if (m_NextIdx < 0)
        return -1;

    m_PrevIdx = m_curIdx;
    m_curIdx = m_NextIdx;
    m_NextIdx = GetNextIndex(m_curIdx);

    return m_curIdx;
}


/******************************CForwardPlaySequence****************************/

CForwardPlaySequence::CForwardPlaySequence(int32_t begin, int32_t end, bool bCycling)
{
    m_bCycling = bCycling;
    m_seqMode = 0;
    SetRange(begin, end);
}

void CForwardPlaySequence::SetRange(int32_t begin, int32_t end)
{
    if ((m_begin != begin) || (m_end != end)) //有变化
    {
        m_begin = begin;
        m_end = end;
        m_NextIdx = m_PrevIdx = m_curIdx = -1;
    }
}

int32_t CForwardPlaySequence::GetNextIndex(int32_t index)
{
    if (index < (m_end - 1))
        return ++index;
    else
        return m_bCycling ? m_begin : -1;
}

int32_t CForwardPlaySequence::GetPreviousIndex(int32_t index)
{
    if (index > m_begin)
        return --index;
    else
        return m_bCycling ? (m_end - 1) : -1;
}



/******************************CBackwardPlaySequence****************************/

CBackwardPlaySequence::CBackwardPlaySequence(int32_t begin, int32_t end, bool bCycling)
{
    m_bCycling = bCycling;
    m_seqMode = 1;
    SetRange(begin, end);
}

void CBackwardPlaySequence::SetRange(int32_t begin, int32_t end)
{
    if ((m_begin != begin) || (m_end != end)) //有变化
    {
        m_begin = begin;
        m_end = end;
        m_NextIdx = m_PrevIdx = m_curIdx = -1;
    }
}

int32_t CBackwardPlaySequence::GetNextIndex(int32_t index)
{
    if (index > m_begin)
        return --index;
    else
        return m_bCycling ? (m_end - 1) :  -1;
}

int32_t CBackwardPlaySequence::GetPreviousIndex(int32_t index)
{
    if (index < (m_end - 1))
        return ++index;
    else
        return m_bCycling ? m_begin : -1;
}

std::unique_ptr<CPlaySequence> MakePlaySequence(int mode, int32_t begin, int32_t end, bool bCycling)
{
    if(mode == 1)
        return std::make_unique<CBackwardPlaySequence>(begin, end, bCycling);
    else
        return std::make_unique<CForwardPlaySequence>(begin, end, bCycling);
}
