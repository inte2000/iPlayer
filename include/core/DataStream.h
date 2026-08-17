/*
Human Action，指定接口定义
*/
#ifndef DATA_STREAM_H
#define DATA_STREAM_H

#include <memory>
#include <string>

typedef uint32_t DataStreamStyle;

constexpr uint32_t dsStyleFixedLength = 0x000001;
constexpr uint32_t dsStyleSeekable    = 0x000002;
constexpr uint32_t dsStyleWritable    = 0x000004;
constexpr uint32_t dsStyleTellPos     = 0x000008;

enum class SeekBase
{
    Begin = 0,
    Cur = 1,
    End = 2
};

class CDataStream
{
public:
    CDataStream() = default;
    virtual ~CDataStream() {}
    //
    CDataStream(const CDataStream&) = delete;
    CDataStream& operator =(const CDataStream&) = delete;
    CDataStream(CDataStream&&) = default;
    CDataStream& operator =(CDataStream&&) = default;

    DataStreamStyle GetStyle() const { return m_style; }
    const std::wstring& GetName() const { return m_name; }
    std::wstring& GetName() { return m_name; }

    template<typename T>
    T* QuerySource() {
        return dynamic_cast<T*>(this);
    }

    virtual uint32_t Read(void* pBuf, uint32_t size, uint32_t timeout = 0) = 0;
    virtual uint32_t Write(const void* pBuf, uint32_t size, uint32_t timeout = 0) = 0;
    virtual std::size_t GetLength() const = 0;
    virtual void Seek(SeekBase base, long long off) = 0;
    virtual std::size_t Tell() = 0;
protected:
    DataStreamStyle m_style;
    std::wstring m_name;
};

#endif //DATA_STREAM_H
