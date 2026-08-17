#ifndef MEMORY_BUFFER_DATA_STREAM_H
#define MEMORY_BUFFER_DATA_STREAM_H

#include <string>
#include <fstream>
#include "DataStream.h"


class CMemoryBufStream : public CDataStream
{
public:
    CMemoryBufStream(bool bReadOnly = true) {
        m_bReadOnly = bReadOnly;
        m_style = dsStyleFixedLength | dsStyleSeekable | dsStyleWritable | dsStyleTellPos;
        m_name = L"Memory Buffer Stream";
        m_length = 0;
        m_curPos = 0;
        m_totalSize = 0;
        m_pBuf = nullptr;
    }
    ~CMemoryBufStream() { Close(); }

    bool Open(std::size_t totalSize);
    void Close() { m_pBuf.reset(); }
    const uint8_t* GetBuffer() const { return m_pBuf.get(); }
    uint32_t Read(void* pBuf, uint32_t size, uint32_t timeout = 0) override;
    uint32_t Write(const void* pBuf, uint32_t size, uint32_t timeout = 0) override;
    std::size_t GetLength() const override { return m_length; }
    void Seek(SeekBase base, long long off) override;
    std::size_t Tell() override { return m_curPos; }
protected:
    bool ResetSize(std::size_t newSize);
private:
    std::size_t m_length;
    std::size_t m_curPos;
    std::size_t m_totalSize;
    std::unique_ptr<uint8_t[]> m_pBuf;
    bool m_bReadOnly;
};

std::unique_ptr<CDataStream> MakeMemoryBufStream(std::size_t totalSize, bool bReadOnly);

#endif //MEMORY_BUFFER_DATA_STREAM_H
