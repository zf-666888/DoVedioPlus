#include "video_processor.h"
#include "ffmpeg_wrapper.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <future>
#include <thread>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace dovedioplus {

class VideoProcessor::Impl {
public:
    FFmpegWrapper ffmpeg;
    std::function<void(double)> progressCallback;
    std::atomic<bool> cancelled{false};
};

VideoProcessor::VideoProcessor() : pImpl_(std::make_unique<Impl>()) {
    pImpl_->ffmpeg.setProgressCallback([this](double p) {
        if (pImpl_->progressCallback) pImpl_->progressCallback(p);
    });
}

VideoProcessor::~VideoProcessor() = default;

bool VideoProcessor::extractAudio(const std::string& videoPath, const std::string& outputPath, int quality) {
    pImpl_->cancelled = false;
    if (videoPath.empty()) return false;
    fs::path out(outputPath);
    if (out.has_parent_path()) fs::create_directories(out.parent_path());
    return pImpl_->ffmpeg.extractAudio(videoPath, outputPath, quality);
}

std::string VideoProcessor::getVideoInfo(const std::string& videoPath) {
    pImpl_->cancelled = false;
    if (videoPath.empty()) return R"({"error":"path empty"})";
    auto info = pImpl_->ffmpeg.getVideoInfo(videoPath);
    json j = {{"width",info.width},{"height",info.height},{"duration",info.duration},
              {"bitrate",info.bitrate},{"codec",info.codec},{"fps",info.fps}};
    return j.dump();
}

bool VideoProcessor::convertFormat(const std::string& input, const std::string& output, const std::string& format) {
    pImpl_->cancelled = false;
    if (input.empty() || output.empty()) return false;
    fs::path out(output);
    if (out.has_parent_path()) fs::create_directories(out.parent_path());
    return pImpl_->ffmpeg.convertFormat(input, output, format);
}

bool VideoProcessor::extractFrame(const std::string& videoPath, double timestamp, const std::string& outputPath) {
    pImpl_->cancelled = false;
    if (videoPath.empty()) return false;
    fs::path out(outputPath);
    if (out.has_parent_path()) fs::create_directories(out.parent_path());
    return pImpl_->ffmpeg.extractFrame(videoPath, timestamp, outputPath);
}

int VideoProcessor::batchProcess(const std::vector<std::string>& inputs, const std::string& outputDir, const std::string& format) {
    pImpl_->cancelled = false;
    fs::create_directories(outputDir);
    int count = 0;
    std::vector<std::future<bool>> futures;

    for (const auto& input : inputs) {
        if (pImpl_->cancelled) break;
        fs::path p(input);
        std::string out = (fs::path(outputDir) / (p.stem().string() + "." + format)).string();
        futures.push_back(std::async(std::launch::async, [this, input, out, format]() {
            return convertFormat(input, out, format);
        }));
    }

    for (auto& f : futures) if (f.get()) count++;
    return count;
}

void VideoProcessor::setProgressCallback(std::function<void(double)> cb) { pImpl_->progressCallback = cb; }
void VideoProcessor::cancel() { pImpl_->cancelled = true; pImpl_->ffmpeg.cancel(); }
std::string VideoProcessor::getLastError() const { return pImpl_->ffmpeg.getLastError(); }

} // namespace dovedioplus
