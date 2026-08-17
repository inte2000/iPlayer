#ifndef STREAM_SECTOR_SOURCE_H
#define STREAM_SECTOR_SOURCE_H

#include <cstdint>

enum class SectorSourceType
{
    AudioCD,
    DTS_CD,
    SACD,
    AudioDVD
};

struct SectorSource
{
    virtual ~SectorSource() = default;

    virtual SectorSourceType Type() const = 0;
    virtual uint64_t GetStartSectors() const = 0;
    virtual uint32_t GetSectorsCount() const = 0;
    virtual uint32_t ReadSectors(uint64_t startNo, uint32_t count, void* buf) const = 0;
    virtual uint32_t SectorSize() const = 0;
    virtual uint32_t SeekToSector(uint64_t sectorNo) = 0;
};

#endif // STREAM_SECTOR_SOURCE_H
