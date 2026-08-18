#ifndef CDTRACK_DECODER_DATA_BUFFER_H
#define CDTRACK_DECODER_DATA_BUFFER_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "StreamSectorSource.h"

class DataBuffer
{
public:
    static constexpr std::size_t WindowSizeMin = 4 * 1024 * 1024;
    static constexpr std::size_t WindowSizeMax = 8 * 1024 * 1024;

    DataBuffer(SectorSource* source,
               uint64_t startSector,
               uint32_t sectorSize,
               std::size_t totalSize,
               std::size_t windowSize);

    bool Reset();
    bool Seek(std::size_t pos);
    std::size_t Tell() const;
    std::size_t TotalSize() const;
    uint32_t Read(void* dst, uint32_t bytes);

private:
    bool EnsureWindowFor(std::size_t pos);
    static void ConvertCDAudioToLittleEndian(uint8_t* data, uint32_t bytes);

private:
    SectorSource* m_source;
    uint64_t m_startSector;
    uint32_t m_sectorSize;
    std::size_t m_totalSize;
    std::size_t m_windowSize;
    std::size_t m_position;

    std::vector<uint8_t> m_window;
    std::size_t m_windowOffset;
    std::size_t m_windowValidBytes;
};

#endif // CDTRACK_DECODER_DATA_BUFFER_H
