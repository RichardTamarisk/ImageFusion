extern "C" {
#include <SDL.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
}

#include <map>
#include <queue>
#include <memory>
#include <utility>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/flann.hpp>
#include <opencv2/opencv.hpp>
