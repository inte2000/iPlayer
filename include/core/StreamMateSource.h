#ifndef STREAM_MATE_SOURCE_H
#define STREAM_MATE_SOURCE_H

class CDataStream;

struct MateSource
{
    virtual ~MateSource() = default;

    virtual CDataStream* CreateMateStream(const wchar_t* name) = 0;
    virtual void ReleaseMateStream(CDataStream*& stream) = 0;
};

#endif // STREAM_MATE_SOURCE_H
