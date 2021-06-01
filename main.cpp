#include "bmp.h"

#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

struct Complex
{
    double real;
    double imag;
};

namespace 
{
    int constexpr N = 64;

    namespace DEFAULT
    {
        namespace FLAG
        {
            char const* const FILE_NAME     = "-o";
            char const* const IMAGE_WIDTH   = "-w";
            char const* const IMAGE_HEIGHT  = "-h";
            char const* const GRANULARITY   = "-g";
            char const* const THREADS_COUNT = "-t";
            char const* const ZOOM_LEVEL    = "-z";
            char const* const POINT_ORIGIN  = "-p";
            char const* const ITERATIONS    = "-c";
        }   

        namespace IMAGE
        {
            std::string const NAME = "mandelbrot.bmp";
            int const WIDTH = 3840;
            int const HEIGHT = 2160;
            double const ZOOM_LEVEL = 1.0;
            Complex const POINT_ORIGIN = {0, 0};
        }

        namespace THREADS
        {
            int const GRANULARITY = 16;
            int const ITERATIONS = 256;
            int const COUNT = std::thread::hardware_concurrency();
        }

        double const INFINITY_THRESHOLD = 4.0;
    }

    uint8_t palette[3 * 256] = {
        #include "palette.rpal"
    };
}

namespace
{
    uint8_t* paletteArr = nullptr;
#ifdef _DYNAMIC_
    std::atomic<int> chunksReserved = {0};
#endif
}

#ifdef _MEASURE_
#include <chrono>
namespace
{
    class Clock
    {
        std::chrono::time_point<std::chrono::high_resolution_clock> start = std::chrono::high_resolution_clock::now();

        public:
        int getElapsedMilliseconds() const
        {
            std::chrono::time_point<std::chrono::high_resolution_clock> now = std::chrono::high_resolution_clock::now();
            return std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        }
    };
}
#endif

#if defined _GRANULARITY_VISUAL_ || defined _GRANULARITY_VISUAL_EXTENDED_
    auto ___ = [](){
        palette[3 * 255    ] = 0;
        palette[3 * 255 + 1] = 255;
        palette[3 * 255 + 2] = 0;
        return 0;
    }();
#endif

struct ProgramParameters
{
    int    imageWidth      = DEFAULT::IMAGE::WIDTH;
    int    imageHeight     = DEFAULT::IMAGE::HEIGHT;
    int    threadsCount    = DEFAULT::THREADS::COUNT;
    int    granularity     = DEFAULT::THREADS::GRANULARITY;
    int    iterationsCount = DEFAULT::THREADS::ITERATIONS;
    double zoomLevel       = DEFAULT::IMAGE::ZOOM_LEVEL;

    std::string imageOutputName = DEFAULT::IMAGE::NAME;
    Complex pointOrigin = DEFAULT::IMAGE::POINT_ORIGIN;
};

struct ThreadParameters
{
    int chunkSize;
    int chunksCount;
    int remainderChunkSize;
    int paletteArrSize;

    double dx;
    double dy;

    Complex bottomLeftCoordinates;
    Complex upperRightCoordinates;
};

void printExecutingParameters(ProgramParameters const p)
{
    std::cout << '\n'
              << DEFAULT::FLAG::FILE_NAME     << " for file name in bmp format.  " << "Executing: " << DEFAULT::FLAG::FILE_NAME     <<" " << p.imageOutputName    << "\n"
              << DEFAULT::FLAG::IMAGE_WIDTH   << " for image width.              " << "Executing: " << DEFAULT::FLAG::IMAGE_WIDTH   <<" " << p.imageWidth         << "\n"
              << DEFAULT::FLAG::IMAGE_HEIGHT  << " for image height.             " << "Executing: " << DEFAULT::FLAG::IMAGE_HEIGHT  <<" " << p.imageHeight        << "\n"
              << DEFAULT::FLAG::GRANULARITY   << " for granularity.              " << "Executing: " << DEFAULT::FLAG::GRANULARITY   <<" " << p.granularity        << "\n"
              << DEFAULT::FLAG::THREADS_COUNT << " for thread count.             " << "Executing: " << DEFAULT::FLAG::THREADS_COUNT <<" " << p.threadsCount       << "\n"
              << DEFAULT::FLAG::ZOOM_LEVEL    << " for image zoom.               " << "Executing: " << DEFAULT::FLAG::ZOOM_LEVEL    <<" " << p.zoomLevel          << "\n"
              << DEFAULT::FLAG::ITERATIONS    << " for complex iterations count. " << "Executing: " << DEFAULT::FLAG::ITERATIONS    <<" " << p.iterationsCount    << "\n"
              << DEFAULT::FLAG::POINT_ORIGIN  << " for image center point.       " << "Executing: " << DEFAULT::FLAG::POINT_ORIGIN  <<" " << p.pointOrigin.real << " " << p.pointOrigin.imag << "\n"
              << '\n';
}

ProgramParameters handleInput(int argc, const char** argv)
{
    ProgramParameters result;

    int i = 1;
    while(i < argc)
    {
        std::string inputFlag(argv[i]);

        if(inputFlag == DEFAULT::FLAG::FILE_NAME)
        {
            result.imageOutputName = argv[i+1];
            i += 2;
        }
        else if(inputFlag == DEFAULT::FLAG::IMAGE_WIDTH)
        {
            result.imageWidth = atoi(argv[i+1]);
            i += 2;
        }
        else if(inputFlag == DEFAULT::FLAG::IMAGE_HEIGHT)
        {
            result.imageHeight = atoi(argv[i+1]);
            i += 2;
        }
        else if(inputFlag == DEFAULT::FLAG::GRANULARITY)
        {
            result.granularity = atoi(argv[i+1]);
            i += 2;
        }
        else if(inputFlag == DEFAULT::FLAG::THREADS_COUNT) 
        {
            result.threadsCount = atoi(argv[i+1]);
            i += 2;;
        }
        else if(inputFlag == DEFAULT::FLAG::ZOOM_LEVEL) 
        {
            result.zoomLevel = atof(argv[i+1]);
            i += 2;
        }
        else if(inputFlag == DEFAULT::FLAG::POINT_ORIGIN)
        {
            result.pointOrigin = { atof(argv[i+1]), atof(argv[i+2]) };
            i += 3;
        }
        else if(inputFlag == DEFAULT::FLAG::ITERATIONS)
        {
            result.iterationsCount = atoi(argv[i+1]);
            i += 2;
        }
        else
        {
            std::cerr << "Invalid parameter supplied: " << argv[i] << '\n';
            exit(-1);
        }
    }

    return result;
}

void allocateEmptyPaletteArr(ProgramParameters const p)
{
    paletteArr = new uint8_t[p.imageWidth * p.imageHeight]();
}

void clean()
{
    delete[] paletteArr;
}

ThreadParameters generateThreadParameters(ProgramParameters const p)
{
    ThreadParameters result;

    result.paletteArrSize     = p.imageHeight * p.imageWidth;
    result.chunkSize          = (result.paletteArrSize  / (p.granularity * p.threadsCount));
    result.chunksCount        = result.paletteArrSize / result.chunkSize;
    result.remainderChunkSize = result.paletteArrSize % result.chunkSize;
    
    double const zoom = 2.0 / p.zoomLevel;
    double const aspectRatio = p.imageHeight / (double) p.imageWidth;

    result.bottomLeftCoordinates = {-zoom + p.pointOrigin.real, -zoom * aspectRatio + p.pointOrigin.imag};
    result.upperRightCoordinates = { zoom + p.pointOrigin.real,  zoom * aspectRatio + p.pointOrigin.imag};

    result.dx = (result.upperRightCoordinates.real - result.bottomLeftCoordinates.real);
    result.dy = (result.upperRightCoordinates.imag - result.bottomLeftCoordinates.imag);

    return result;
}

void computeStepsVec(int const iterations, double const cR[N], double const cI[N], int res[N])
{
    double currR[N];
    double currI[N];
    double iSq[N];
    double rSq[N];

    uint8_t count = 0;

    for (size_t i = 0; i < N; ++i)
        currI[i] = cI[i];

    for (size_t i = 0; i < N; ++i)
        currR[i] = cR[i];
    
    for (size_t i = 0; i < N; ++i)
        iSq[i] = cI[i] * cI[i];

    for (size_t i = 0; i < N; ++i)
        rSq[i] = cR[i] * cR[i];
    

    for (int i = 1; i <= iterations && count != N; ++i)
    {
        for (size_t j = 0; j < N; j++)
        {
            if(rSq[j] + iSq[j] > DEFAULT::INFINITY_THRESHOLD && res[j] == 0)
            {
                res[j] = i;
                ++count;
            }
        } 

        for (size_t j = 0; j < N; ++j)
            currI[j] = 2.0 * currI[j] * currR[j] + cI[j];

        for (size_t j = 0; j < N; ++j)
            currR[j] = rSq[j] - iSq[j] + cR[j];

        for (size_t j = 0; j < N; ++j)
            iSq[j] = currI[j] * currI[j];

        for (size_t j = 0; j < N; ++j)
            rSq[j] = currR[j] * currR[j];
    }
}

void computePortionOfPaletteArr(int const paletteArrStartIndex, int const paletteArrEndIndex, int const imageWidth, int const imageHeight, int const iterationsCount, ThreadParameters const t)
{
    double bufI[N];
    double bufR[N];

    int k = 0;

    int y = paletteArrStartIndex / imageWidth;
    int x = paletteArrStartIndex % imageWidth;

    double const stepX = (1.0 / imageWidth) * t.dx;
    double const stepY = (1.0 / imageHeight) * t.dy;
    
    for(int i = paletteArrStartIndex; i < paletteArrEndIndex; ++i)
    { 
        double const real = x * stepX + t.bottomLeftCoordinates.real;
        double const imag = y * stepY + t.bottomLeftCoordinates.imag;

        ++x;

        if(x == imageWidth)
        {
            x = 0;
            ++y;
        }

        bufR[k] = real;
        bufI[k] = imag;
        ++k;

        if(k == N)
        {
            k = 0;
            int res[N] = {0};

            computeStepsVec(iterationsCount, bufR, bufI, res);

            for (size_t j = 0; j < N; ++j)
            {
                uint8_t const paletteIdx = (UINT8_MAX * res[j]) / iterationsCount;
                paletteArr[i - N + 1 + j] = paletteIdx;
                if(res[j] == 0)
                    paletteArr[i - N + 1 + j] = 255; // Wrap around for points that do not diverge
            }
            
#if defined _GRANULARITY_VISUAL_ || defined _GRANULARITY_VISUAL_EXTENDED_
            for (size_t j = 0; j < N; j++)
            {

                if(paletteArr[i - N + 1 + j] == 255)
                    paletteArr[i - N + 1 + j] = 254;
            }
#endif
        }
        else if(i == (paletteArrEndIndex - 1))
        {
            int res[N] = {0};
            computeStepsVec(iterationsCount, bufR, bufI, res);

            for (size_t j = 0; j < k; ++j)
            {
                uint8_t const paletteIdx = (UINT8_MAX * res[j]) / iterationsCount;
                paletteArr[i - k + 1 + j] = paletteIdx;

                if(res[j] == 0)
                    paletteArr[i - k + 1 + j] = 255;
#if defined _GRANULARITY_VISUAL_ || defined _GRANULARITY_VISUAL_EXTENDED_
                if(paletteIdx == 255)
                    paletteArr[i - k + 1 + j] = 254;
#endif
            }
        }
    }

#if defined _GRANULARITY_VISUAL_ || defined _GRANULARITY_VISUAL_EXTENDED_
    paletteArr[paletteArrEndIndex - 1] = 255;
    #ifdef _GRANULARITY_VISUAL_EXTENDED_
    for (size_t i = 0; i < 255; i++)
        paletteArr[paletteArrEndIndex - i] = 255;
    
    #endif
#endif
}

void computePaletteArr(ProgramParameters const p, ThreadParameters const t, int const threadId)
{
#ifdef _MEASURE_
    Clock const threadClock;
#endif

    int currentChunkNumber = threadId - p.threadsCount;
    int totalChunksCompleted = 0;

    // Handle normal chunks
#ifdef _DYNAMIC_
    while ((currentChunkNumber = chunksReserved++) < t.chunksCount)
#else
    while ((currentChunkNumber += p.threadsCount) < t.chunksCount)
#endif
    {
        ++totalChunksCompleted;

        int const paletteArrStartIndex = currentChunkNumber * t.chunkSize;
        int const paletteArrEndIndex = (currentChunkNumber + 1) * t.chunkSize; // Non inclusive

        computePortionOfPaletteArr(paletteArrStartIndex, paletteArrEndIndex, p.imageWidth, p.imageHeight, p.iterationsCount, t);
    }

    // Handle remainder
    if(t.remainderChunkSize != 0 && (currentChunkNumber == t.chunksCount))
    {
        ++totalChunksCompleted;

        int const paletteArrStartIndex = currentChunkNumber * t.chunkSize;
        int const paletteArrEndIndex = t.paletteArrSize;

        computePortionOfPaletteArr(paletteArrStartIndex, paletteArrEndIndex, p.imageWidth, p.imageHeight, p.iterationsCount, t);
    }

#ifdef _MEASURE_
    std::string const out = "Thread with id: " + std::to_string(threadId) + " finished " + std::to_string(totalChunksCompleted) +
                        + " chunks with elapsed time: " + std::to_string(threadClock.getElapsedMilliseconds()) + "ms\n";
    std::cout << out;
#endif
}

int main(int const argc, const char** argv) 
{
#ifdef _MEASURE_
    Clock const programClock;
#endif

    ProgramParameters const programParameters = handleInput(argc, argv);
    printExecutingParameters(programParameters);
    allocateEmptyPaletteArr(programParameters);
    
    ThreadParameters const threadParameters = generateThreadParameters(programParameters);
    std::vector<std::thread> workers(programParameters.threadsCount - 1);

#ifdef _MEASURE_
    Clock const forkClock;
#endif

    for(int i = 0; i < programParameters.threadsCount - 1; ++i)
        workers[i] = std::move(std::thread(computePaletteArr, programParameters, threadParameters, i));

    int const mainId = programParameters.threadsCount - 1;
    computePaletteArr(programParameters, threadParameters, mainId);

    for(int i = 0; i < programParameters.threadsCount - 1; ++i)
        workers[i].join();

#ifdef _MEASURE_
    std::cout << "\nTotal time from fork start to join end: " << forkClock.getElapsedMilliseconds() << "ms\n";
#endif

#ifdef _MEASURE_
    Clock const imageSaveClock;
#endif
    
     BMPImage::saveWithPalette(programParameters.imageOutputName.c_str(), programParameters.imageHeight, programParameters.imageWidth, paletteArr, palette);

#ifdef _MEASURE_
    std::cout << "Total time for saving image as bmp: " << imageSaveClock.getElapsedMilliseconds() << "ms\n";
#endif

    clean();

#ifdef _MEASURE_
    std::cout << "Total time for program execution: " << programClock.getElapsedMilliseconds() << "ms\n";
#endif

    return 0;
}
