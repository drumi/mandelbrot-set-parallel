#pragma once

#include <cstdint>

namespace BMPImage 
{
    void save(char const* path, int const height, int const width, uint8_t const* imageArrPtr);
    void saveWithPalette(char const* path, int const height, int const width, uint8_t const* paletteArrPtr, uint8_t const palette[3 * 256]);
}