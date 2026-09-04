#ifndef PLAY_SEQUENCE_H
#define PLAY_SEQUENCE_H

#include <string>
#include <memory>

class CPlaySequence
{
public:
    virtual ~CPlaySequence() = default;
    virtual void SetRange(int32_t begin, int32_t end) = 0;
    void EnableCycling(bool bCycling) { m_bCycling = bCycling; }
    int32_t MoveTo(int32_t index);
    int32_t MovePrevious();
    int32_t MoveNext();
    int32_t GetCurrentIndex() const { return m_curIdx; }
    int32_t GetNextIndex() const { return m_NextIdx; }
    int32_t GetPreviousIndex() const { return m_PrevIdx; }
    int32_t GetSequenceMode() const { return m_seqMode; }
protected:
    virtual int32_t GetNextIndex(int32_t index) = 0;
    virtual int32_t GetPreviousIndex(int32_t index) = 0;

    int32_t m_curIdx = -1;
    int32_t m_PrevIdx = -1;
    int32_t m_NextIdx = -1;
    int32_t m_begin = 0;
    int32_t m_end = 0;
    int32_t m_seqMode = 0;
    bool m_bCycling = false;
};

class CForwardPlaySequence : public CPlaySequence
{
public:
    CForwardPlaySequence(int32_t begin, int32_t end, bool bCycling = false);
    void SetRange(int32_t begin, int32_t end) override;
    int32_t GetNextIndex(int32_t index) override;
    int32_t GetPreviousIndex(int32_t index) override;
private:
};

class CBackwardPlaySequence : public CPlaySequence
{
public:
    CBackwardPlaySequence(int32_t begin, int32_t end, bool bCycling = false);
    void SetRange(int32_t begin, int32_t end) override;
    int32_t GetNextIndex(int32_t index) override;
    int32_t GetPreviousIndex(int32_t index) override;
private:
};


std::unique_ptr<CPlaySequence> MakePlaySequence(int mode, int32_t begin, int32_t end, bool bCycling = false);

#endif //PLAY_SEQUENCE_H
