#ifndef STREAM_MATE_SOURCE_H
#define STREAM_MATE_SOURCE_H

#include <memory>

class CDataStream;

struct MateSource
{
    virtual std::unique_ptr<CDataStream> CreateMateStream(const wchar_t* name) = 0;
};

#endif // STREAM_MATE_SOURCE_H
