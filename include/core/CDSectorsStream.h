#ifndef CD_SECTORS_STREAM_H
#define CD_SECTORS_STREAM_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "DataStream.h"
#include "StreamMetaSource.h"
#include "StreamSectorSource.h"

typedef struct _CdIo CdIo_t;

class CCDSectorsStream : public CDataStream,
                         public MetaSource,
                         public SectorSource
{
public:
    CCDSectorsStream();
    ~CCDSectorsStream() override;

    static bool IsDeviceName(const std::wstring& name);

    bool Open(const std::wstring& name, uint32_t track);
    bool Close();

    uint32_t Read(void* pBuf, uint32_t size, uint32_t timeout = 0) override;
    uint32_t Write(const void* pBuf, uint32_t size, uint32_t timeout = 0) override;
    std::size_t GetLength() const override;
    void Seek(SeekBase base, long long off) override;
    std::size_t Tell() override;

    const DsMetaInfo* GetMetaInformation() const override;

    SectorSourceType Type() const override;
    uint64_t GetStartSectors() const override;
    uint32_t GetSectorsCount() const override;
    uint32_t ReadSectors(uint64_t startNo, uint32_t count, void* buf) const override;
    uint32_t SectorSize() const override;
    uint32_t SeekToSector(uint64_t sectorNo) override;

protected:
    bool OpenImage(const std::wstring& imgFile, uint32_t track);
    bool OpenDevice(const std::wstring& deviceName, uint32_t track);

private:
    using CdioDeleter = void(*)(CdIo_t*);

    bool InitFromOpenedCdio(const std::wstring& sourceName, uint32_t track);
    void FillMetaInfo();

private:
    std::unique_ptr<CdIo_t, CdioDeleter> m_cdio;
    DsMetaInfo m_metaInfo;
    uint32_t m_track;
    uint64_t m_curSector;
    uint64_t m_totalSectors;
    uint64_t m_startSector;
};

std::unique_ptr<CDataStream> MakeCDSectorsStream(const std::wstring& name, uint32_t track);

#endif // CD_SECTORS_STREAM_H
