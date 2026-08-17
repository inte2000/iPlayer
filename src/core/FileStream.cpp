#include <cassert>
#include <filesystem>
#include "UnicodeConvert.h"
#include "FileStream.h"


std::unique_ptr<CDataStream> MakeFileStream(const std::wstring& filename, bool bReadOnly)
{
    auto ptr = std::make_unique<CFileStream>(bReadOnly);
    if (ptr)
    {
        if (!ptr->Open(filename))
        {
            std::wstring errmsg = std::format(L"Fail to open file: {}", filename);
            throw std::runtime_error(Utf16LeToLocalMBCS(errmsg));
        }
    }
    return ptr;
    //return std::unique_ptr<CFileStream>(new CFileStream{});
}

bool CFileStream::Open(const std::wstring& filename)
{
    std::ios_base::openmode mode = std::ios::binary;
    bool bReadOnly = !(m_style & dsStyleWritable);
    mode |= (bReadOnly ? std::ios::in : std::ios::out);
    m_file.open(filename, mode);
    if (m_file.is_open())
    {
        m_file.seekg(0, std::ios::end);
        m_length = m_file.tellg();  //
        //
        m_file.seekg(0, std::ios::beg);
        m_curPos = 0;

        //std::filesystem::path pathname{ filename };
        //m_name = pathname.filename().wstring();
        m_name = filename;
    }
    return m_file.is_open();
}

uint32_t CFileStream::Read(void* pBuf, uint32_t size, uint32_t timeout)
{
    if (!m_file.is_open())
        return 0;
    if (m_file.eof())
        return 0;

    //uint32_t readcount = 0;
    m_file.read((char*)pBuf, size);
    uint32_t readcount = static_cast<uint32_t>(m_file.gcount());
    m_curPos += readcount;
    return readcount;
}

uint32_t CFileStream::Write(const void* pBuf, uint32_t size, uint32_t timeout)
{
    if (!(m_style & dsStyleWritable) || !m_file.is_open())
        return 0;

    m_file.write((const char *)pBuf, size);
    if (m_file.fail())
        return 0;

    m_curPos += size;
    return size;
}

std::size_t CFileStream::GetLength() const
{
    if (!m_file.is_open())
        return 0;
    if(!(m_style & dsStyleWritable))
        return m_length;
    else
    {
        auto curpos = m_file.tellg();  //
        m_file.seekg(0, std::ios::end);
        auto curLength = m_file.tellg();  //
        m_file.seekg(curpos, std::ios::beg); //

        return curLength;
    }
}
void CFileStream::Seek(SeekBase base, long long off)
{
    bool bNegLeft = (off < 0);
    std::size_t poff = std::abs(off);
    if (m_file.is_open())
    {
        std::size_t newpos = 0;
        if (base == SeekBase::Begin)
            newpos = bNegLeft ? 0 : poff;
        else if (base == SeekBase::End)
            newpos = bNegLeft ? (m_length - poff) : m_length;
        else
            newpos = bNegLeft ? (m_curPos - poff) : (m_curPos + poff);

        if (m_curPos != newpos)
        {
            if (m_file.eof())
                m_file.clear();

            m_file.seekg(newpos, std::ios::beg);
            m_curPos = newpos;
        }
    }
}

std::size_t CFileStream::Tell()
{
    if (!m_file.is_open())
        return 0;

    if (!(m_style & dsStyleWritable))
        return m_curPos;

    auto curpos = m_file.tellg();  //
    assert(m_curPos == curpos);

    m_curPos = curpos;
    return m_curPos; 
}

CDataStream* CFileStream::CreateMateStream(const wchar_t* name)
{
    if (name == nullptr)
        return nullptr;

    std::filesystem::path pathname{ m_name };
    pathname.replace_filename(name);

    if (std::filesystem::exists(pathname))
    {
        auto mateStream = MakeFileStream(pathname.wstring(), true);
        return mateStream.release();
    }

    return nullptr;
}

void CFileStream::ReleaseMateStream(CDataStream*& stream)
{
    delete stream;
    stream = nullptr;
}

#if 0
void CFileStream::Seek(SeekBase base, long long off)
{
    if (m_file)
    {
        if(base == SeekBase::Begin)
            m_file.seekg(off, std::ios::beg);
        else if (base == SeekBase::End)
            m_file.seekg(off, std::ios::end);
        else
            m_file.seekg(off, std::ios::cur);
    }
}
#endif
