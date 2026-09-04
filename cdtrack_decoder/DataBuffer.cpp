#include <algorithm>
#include <cstring>

#include "DataBuffer.h"
#include "CpuArchEndian.h"

DataBuffer::DataBuffer(SectorSource* source,
                       uint64_t startSector,
                       uint32_t sectorSize,
                       std::size_t totalSize,
                       std::size_t windowSize)
    : m_source(source)
    , m_startSector(startSector)
    , m_sectorSize(sectorSize)
    , m_totalSize(totalSize)
    , m_windowSize(std::clamp(windowSize, WindowSizeMin, WindowSizeMax))
    , m_position(0)
    , m_windowOffset(0)
    , m_windowValidBytes(0)
{
}

bool DataBuffer::Reset()
{
    m_position = 0;
    m_window.clear();
    m_windowOffset = 0;
    m_windowValidBytes = 0;
    return true;
}

bool DataBuffer::Seek(std::size_t pos)
{
    if (pos > m_totalSize) {
        return false;
    }

    m_position = pos;
    return true;
}

std::size_t DataBuffer::Tell() const
{
    return m_position;
}

std::size_t DataBuffer::TotalSize() const
{
    return m_totalSize;
}

uint32_t DataBuffer::Read(void* dst, uint32_t bytes)
{
    if ((dst == nullptr) || (bytes == 0) || (m_position >= m_totalSize)) {
        return 0;
    }

    uint8_t* out = static_cast<uint8_t*>(dst);
    std::size_t copied = 0;
    std::size_t remain = std::min<std::size_t>(bytes, m_totalSize - m_position);

    while (remain > 0)
    {
        if (!EnsureWindowFor(m_position)) {
            break;
        }

        const std::size_t inWindow = m_position - m_windowOffset;
        if (inWindow >= m_windowValidBytes) {
            break;
        }

        const std::size_t available = m_windowValidBytes - inWindow;
        const std::size_t chunk = std::min(available, remain);
        std::memcpy(out + copied, m_window.data() + inWindow, chunk);

        copied += chunk;
        remain -= chunk;
        m_position += chunk;
    }

    return static_cast<uint32_t>(copied);
}

bool DataBuffer::EnsureWindowFor(std::size_t pos)
{
    if ((pos >= m_windowOffset) && (pos < (m_windowOffset + m_windowValidBytes))) {
        return true;
    }

    const std::size_t mapOffset = (pos / m_sectorSize) * m_sectorSize;
    const std::size_t mapBytes = std::min(m_windowSize, m_totalSize - mapOffset);
    const uint32_t needSectors = static_cast<uint32_t>((mapBytes + m_sectorSize - 1) / m_sectorSize);
    if (needSectors == 0) {
        return false;
    }

    m_window.assign(static_cast<std::size_t>(needSectors) * m_sectorSize, 0);
    const uint64_t absSector = m_startSector + (mapOffset / m_sectorSize);
    const uint32_t gotSectors = m_source->ReadSectors(absSector, needSectors, m_window.data());
    if (gotSectors == 0) {
        m_window.clear();
        m_windowValidBytes = 0;
        return false;
    }

    const std::size_t gotBytes = static_cast<std::size_t>(gotSectors) * m_sectorSize;
#if defined(ARCH_CPU_BIG_ENDIAN)
    ConvertCDAudioToLittleEndian(m_window.data(), static_cast<uint32_t>(gotBytes));
#endif

    m_windowOffset = mapOffset;
    m_windowValidBytes = std::min(gotBytes, m_totalSize - m_windowOffset);
    return pos < (m_windowOffset + m_windowValidBytes);
}

void DataBuffer::ConvertCDAudioToLittleEndian(uint8_t* data, uint32_t bytes)
{
    if (data == nullptr) {
        return;
    }

    const uint32_t convertBytes = bytes - (bytes % 2);
    for (uint32_t i = 0; i < convertBytes; i += 2)
    {
        const uint8_t high = data[i];
        data[i] = data[i + 1];
        data[i + 1] = high;
    }
}
