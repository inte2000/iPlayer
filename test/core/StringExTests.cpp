#include <catch2/catch_test_macros.hpp>

#include "StringEx.h"

TEST_CASE("IsAsciiLetter handles ASCII letters", "[core][stringex]")
{
    CHECK(IsAsciiLetter(L'A'));
    CHECK(IsAsciiLetter(L'z'));
    CHECK_FALSE(IsAsciiLetter(L'0'));
    CHECK_FALSE(IsAsciiLetter(L'_'));
    CHECK_FALSE(IsAsciiLetter(L'中'));
}

TEST_CASE("ToLowerAscii lowers uppercase ASCII only", "[core][stringex]")
{
    const std::wstring source = L"AbC-09_中Z";
    const std::wstring lowered = ToLowerAscii(source);

    CHECK(lowered == L"abc-09_中z");
}
