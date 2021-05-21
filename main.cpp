#include "bmp.h"

#include <iostream>
#include <string>
#include <complex>
#include <thread>
#include <vector>
#include <atomic>

namespace 
{
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
            int const BYTES_PER_PIXEL = 3;
            std::complex<double> const POINT_ORIGIN(0, 0);
        }

        namespace THREADS
        {
            int const GRANULARITY = 1;
            int const ITERATIONS = 100;
            int const COUNT = 1;
        }

        double const INFINITY_THRESHOLD = 4.0;
    }
}

namespace
{
    uint8_t* rawImage = nullptr;
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

struct ProgramParameters
{
    int    imageWidth      = DEFAULT::IMAGE::WIDTH;
    int    imageHeight     = DEFAULT::IMAGE::HEIGHT;
    int    threadsCount    = DEFAULT::THREADS::COUNT;
    int    granularity     = DEFAULT::THREADS::GRANULARITY;
    int    iterationsCount = DEFAULT::THREADS::ITERATIONS;
    double zoomLevel       = DEFAULT::IMAGE::ZOOM_LEVEL;

    std::string imageOutputName = DEFAULT::IMAGE::NAME;
    std::complex<double> pointOrigin = DEFAULT::IMAGE::POINT_ORIGIN;
};

struct ThreadParameters
{
    int chunkSize;
    int chunksCount;
    int remainderChunkSize;
    int imageTotalSize;
    int bytesPerWidth;
    int bytesPerHeight;

    double dx;
    double dy;

    std::complex<double> bottomLeftCoordinates;
    std::complex<double> upperRightCoordinates;
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
              << DEFAULT::FLAG::POINT_ORIGIN  << " for image center point.       " << "Executing: " << DEFAULT::FLAG::POINT_ORIGIN  <<" " << p.pointOrigin.real() << " " << p.pointOrigin.imag() << "\n"
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
            result.pointOrigin = std::complex<double>(atof(argv[i+1]), atof(argv[i+2]));
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

void generateEmptyImage(ProgramParameters const p)
{
    rawImage = new uint8_t[p.imageWidth * p.imageHeight * DEFAULT::IMAGE::BYTES_PER_PIXEL]();
}

void clean()
{
    delete[] rawImage;
}

ThreadParameters generateThreadParameters(ProgramParameters const p)
{
    ThreadParameters result;

    int const totalPixels = p.imageHeight * p.imageWidth;

    result.imageTotalSize     = totalPixels * DEFAULT::IMAGE::BYTES_PER_PIXEL; 
    result.chunkSize          = (totalPixels / (p.granularity * p.threadsCount)) * DEFAULT::IMAGE::BYTES_PER_PIXEL;
    result.chunksCount        = result.imageTotalSize / result.chunkSize;
    result.remainderChunkSize = result.imageTotalSize % result.chunkSize;
    
    double const zoom = 2.0 / p.zoomLevel;
    double const aspectRatio = p.imageHeight / (double) p.imageWidth;

    result.bottomLeftCoordinates = std::complex<double>(-zoom, -zoom * aspectRatio) + p.pointOrigin;
    result.upperRightCoordinates = std::complex<double>( zoom,  zoom * aspectRatio) + p.pointOrigin;
    
    result.bytesPerWidth = p.imageWidth * DEFAULT::IMAGE::BYTES_PER_PIXEL;
    result.bytesPerHeight = p.imageHeight;

    result.dx = (result.upperRightCoordinates.real() - result.bottomLeftCoordinates.real());
    result.dy = (result.upperRightCoordinates.imag() - result.bottomLeftCoordinates.imag());

    return result;
}

int computeSteps(int const iterations, std::complex<double> const c)
{
    std::complex<double> curr = c;

    for (int i = 0; i < iterations; ++i)
    {
        curr *= curr;
        curr += c;

        double const growthIndex = curr.real() * curr.real() + curr.imag() * curr.imag();

        if(growthIndex > DEFAULT::INFINITY_THRESHOLD)
            return i;
    }

    return 0;
}

void computePortionOfImage(int const imageStartIndex, int const imageEndIndex, ProgramParameters const p, ThreadParameters const t)
{
    for(int i = imageStartIndex; i < imageEndIndex; i += 3)
    {
        int const y = i / t.bytesPerWidth;
        int const x = i % t.bytesPerWidth;

        double const realFraction = (x / (double)t.bytesPerWidth);
        double const imagFraction = (y / (double)t.bytesPerHeight);

        double const real = realFraction * t.dx + t.bottomLeftCoordinates.real();
        double const imag = imagFraction * t.dy + t.bottomLeftCoordinates.imag();

        std::complex<double> const c(real, imag);
        int const steps = computeSteps(p.iterationsCount, c);
        uint8_t const color = UINT8_MAX * steps / (double)p.iterationsCount;

        rawImage[i  ] = color; // b
        rawImage[i+1] = color; // g
     // rawImage[i+2] = 0;     // r; 0 by default
    }
}

void computeImage(ProgramParameters const p, ThreadParameters const t, int const threadId)
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

        int const imageStartIndex = currentChunkNumber * t.chunkSize;
        int const imageEndIndex = (currentChunkNumber + 1) * t.chunkSize - 1;

        computePortionOfImage(imageStartIndex, imageEndIndex, p, t);
    }

    // Handle remainder
    if(t.remainderChunkSize != 0 && (currentChunkNumber == t.chunksCount))
    {
        ++totalChunksCompleted;

        int const imageStartIndex = currentChunkNumber * t.chunkSize;
        int const imageEndIndex = t.imageTotalSize - 1;

        computePortionOfImage(imageStartIndex, imageEndIndex, p, t);
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
    generateEmptyImage(programParameters);
    
    ThreadParameters const threadParameters = generateThreadParameters(programParameters);
    std::vector<std::thread> workers(programParameters.threadsCount);

#ifdef _MEASURE_
    Clock const forkClock;
#endif

    for(int i = 0; i < programParameters.threadsCount; ++i)
        workers[i] = std::move(std::thread(computeImage, programParameters, threadParameters, i));

    for(int i = 0; i < programParameters.threadsCount; ++i)
        workers[i].join();

#ifdef _MEASURE_
    std::cout << "\nTotal time from fork start to join end: " << forkClock.getElapsedMilliseconds() << "ms\n";
#endif

#ifdef _MEASURE_
    Clock const imageSaveClock;
#endif
    
    BMPImage::save(programParameters.imageOutputName.c_str(), programParameters.imageHeight, programParameters.imageWidth, rawImage);

#ifdef _MEASURE_
    std::cout << "Total time for saving image as bmp: " << imageSaveClock.getElapsedMilliseconds() << "ms\n";
#endif

    clean();

#ifdef _MEASURE_
    std::cout << "Total time for program execution: " << programClock.getElapsedMilliseconds() << "ms\n";
#endif

    return 0;
}
