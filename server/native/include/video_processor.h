#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace dovedioplus {

class VideoProcessor {
public:
    VideoProcessor();
    ~VideoProcessor();

    VideoProcessor(const VideoProcessor&) = delete;
    VideoProcessor& operator=(const VideoProcessor&) = delete;

    bool extractAudio(const std::string& videoPath, const std::string& outputPath, int quality = 2);
    std::string getVideoInfo(const std::string& videoPath);
    bool convertFormat(const std::string& inputPath, const std::string& outputPath, const std::string& format);
    bool extractFrame(const std::string& videoPath, double timestamp, const std::string& outputPath);
    int batchProcess(const std::vector<std::string>& inputPaths, const std::string& outputDir, const std::string& format);

    void setProgressCallback(std::function<void(double)> callback);
    void cancel();
    std::string getLastError() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace dovedioplus
