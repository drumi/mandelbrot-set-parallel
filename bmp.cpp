// This code was based on https://stackoverflow.com/questions/2654480/writing-bmp-image-in-pure-c-c-without-other-libraries

#include "bmp.h"

#include <fstream>
#include <cstdint>
#include <iostream>

namespace BMPImage 
{
    namespace
    {
        int const BYTES_PER_PIXEL  =  3; 
        int const FILE_HEADER_SIZE = 14;
        int const INFO_HEADER_SIZE = 40;
    
        void createBitmapFileHeader (uint8_t* fileHeader, int const height, int const stride)
        {
            int const fileSize = FILE_HEADER_SIZE + INFO_HEADER_SIZE + (stride * height);

            fileHeader[ 0] = (uint8_t)('B');
            fileHeader[ 1] = (uint8_t)('M');
            fileHeader[ 2] = (uint8_t)(fileSize      );
            fileHeader[ 3] = (uint8_t)(fileSize >>  8);
            fileHeader[ 4] = (uint8_t)(fileSize >> 16);
            fileHeader[ 5] = (uint8_t)(fileSize >> 24);
            fileHeader[10] = (uint8_t)(FILE_HEADER_SIZE + INFO_HEADER_SIZE);
        }

        void createBitmapInfoHeader (uint8_t* infoHeader, int const height, int const width)
        {
            infoHeader[ 0] = (uint8_t)(INFO_HEADER_SIZE);
            infoHeader[ 4] = (uint8_t)(width      );
            infoHeader[ 5] = (uint8_t)(width >>  8);
            infoHeader[ 6] = (uint8_t)(width >> 16);
            infoHeader[ 7] = (uint8_t)(width >> 24);
            infoHeader[ 8] = (uint8_t)(height      );
            infoHeader[ 9] = (uint8_t)(height >>  8);
            infoHeader[10] = (uint8_t)(height >> 16);
            infoHeader[11] = (uint8_t)(height >> 24);
            infoHeader[12] = (uint8_t)(1);
            infoHeader[14] = (uint8_t)(BYTES_PER_PIXEL*8);
        }
    
        void writeFileAndInfoHeader(std::ofstream& file, int const height, int const width)
        {
            uint8_t fileHeader[FILE_HEADER_SIZE] = {0};
            uint8_t infoHeader[INFO_HEADER_SIZE] = {0}; 

            int const widthInBytes = width * BYTES_PER_PIXEL;
            int const paddingSize = (4 - widthInBytes % 4) % 4;
            int const stride = widthInBytes + paddingSize;

            createBitmapFileHeader(fileHeader, height, stride);
            createBitmapInfoHeader(infoHeader, height, width);

            file.write((const char*)fileHeader, sizeof(fileHeader));
            file.write((const char*)infoHeader, sizeof(infoHeader));
        }
    }

    void save(char const* path, int const height, int const width, uint8_t const* imageArrPtr)
    {
        std::ofstream file(path, std::ios::binary);
        
        writeFileAndInfoHeader(file, height, width);

        int const widthInBytes = width * BYTES_PER_PIXEL;
        int const paddingSize = (4 - widthInBytes % 4) % 4;

        for (int i = 0; i < height; ++i) 
        {
            file.write((const char*)(imageArrPtr + (i * widthInBytes)), widthInBytes);
            file.write((const char*)NULL, paddingSize);
        }

        file.close();
    }

    void saveWithPalette(char const* path, int const height, int const width, uint8_t const* paletteArrPtr, uint8_t const palette[BYTES_PER_PIXEL * 256])
    {
        int const imageWidthInBytes = width * BYTES_PER_PIXEL;
        int const paddingSize = (4 - imageWidthInBytes % 4) % 4;

        std::ofstream file(path, std::ios::binary);
        uint8_t* buf = new uint8_t[imageWidthInBytes];
        
        writeFileAndInfoHeader(file, height, width);

        for (size_t i = 0; i < height; ++i)
        {
            for (int j = 0; j < width; ++j) 
            {
                buf[j * BYTES_PER_PIXEL    ] = palette[paletteArrPtr[j + i * width] * BYTES_PER_PIXEL    ];
                buf[j * BYTES_PER_PIXEL + 1] = palette[paletteArrPtr[j + i * width] * BYTES_PER_PIXEL + 1];
                buf[j * BYTES_PER_PIXEL + 2] = palette[paletteArrPtr[j + i * width] * BYTES_PER_PIXEL + 2];

            }

            file.write((const char*)buf, imageWidthInBytes);
            file.write((const char*)NULL, paddingSize);
        }

        delete[] buf;
        file.close();
    }
}