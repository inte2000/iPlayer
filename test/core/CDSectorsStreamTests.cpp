#include <catch2/catch_test_macros.hpp>

#include "CDSectorsStream.h"

TEST_CASE("CCDSectorsStream device name format", "[core][cdio][stream]")
{
    CHECK(CCDSectorsStream::IsDeviceName(L"\\device\\D:"));
    CHECK(CCDSectorsStream::IsDeviceName(L"\\\\device\\Z:"));

    CHECK_FALSE(CCDSectorsStream::IsDeviceName(L"\\device\\12:"));
    CHECK_FALSE(CCDSectorsStream::IsDeviceName(L"\\device\\D"));
    CHECK_FALSE(CCDSectorsStream::IsDeviceName(L"D:"));
    CHECK_FALSE(CCDSectorsStream::IsDeviceName(L"test.cue"));
}

TEST_CASE("CCDSectorsStream seek alignment on empty stream", "[core][cdio][stream]")
{
    CCDSectorsStream stream;

    CHECK(stream.GetLength() == 0);
    CHECK(stream.Tell() == 0);
    CHECK(stream.GetStartSectors() == 0);
    CHECK(stream.GetSectorsCount() == 0);

    stream.Seek(SeekBase::Begin, 1);
    CHECK(stream.Tell() == 0);

    stream.Seek(SeekBase::Begin, 2352);
    CHECK(stream.Tell() == 0);
}

TEST_CASE("CCDSectorsStream open requires valid track", "[core][cdio][stream]")
{
    CCDSectorsStream stream;

    CHECK_FALSE(stream.Open(L"", 1));
    CHECK_FALSE(stream.Open(L"dummy.cue", 0));
    CHECK_FALSE(stream.Open(L"dummy.cue", 1));

    auto ptr = MakeCDSectorsStream(L"dummy.cue", 0);
    CHECK(ptr == nullptr);
}
