#pragma once

#include <string>
#include <functional>
#include <memory>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

namespace dovedioplus {

class FFmpegWrapper {
public:
    FFmpegWrapper();
    ~FFmpegWrapper();

    FFmpegWrapper(const FFmpegWrapper&) = delete;
    FFmpegWrapper& operator=(const FFmpegWrapper&) = delete;

    struct VideoInfo {
        int width = 0;
        int height = 0;
        double duration = 0.0;
        int bitrate = 0;
        std::string codec;
        double fps = 0.0;
    };

    bool extractAudio(const std::string& inputPath, const std::string& outputPath, int quality = 2);
    VideoInfo getVideoInfo(const std::string& videoPath);
    bool convertFormat(const std::string& inputPath, const std::string& outputPath, const std::string& format);
    bool extractFrame(const std::string& videoPath, double timestamp, const std::string& outputPath);

    void setProgressCallback(std::function<void(double)> callback);
    void cancel();
    std::string getLastError() const;
    void setError(const std::string& error);

private:
    bool cancelled_ = false;
    std::string lastError_;
    std::function<void(double)> progressCallback_;

    AVFormatContext* inputCtx_ = nullptr;
    AVFormatContext* outputCtx_ = nullptr;
    AVCodecContext* decoderCtx_ = nullptr;
    AVCodecContext* encoderCtx_ = nullptr;
    SwrContext* swrCtx_ = nullptr;

    void cleanup();
    void updateProgress(double progress);
};

} // namespace dovedioplus
