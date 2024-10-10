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
