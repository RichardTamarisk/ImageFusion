#ifndef STITCHER_H
#define STITCHER_H

#include <opencv2/core.hpp>
#include <string>

#ifdef _WIN32
    #define EXPORT __declspec(dllexport)
#else
    #define EXPORT
#endif

extern "C" {
    EXPORT bool stitchImages(const char* img1_path, const char* img2_path, const char* output_path);
}

#endif // STITCHER_H