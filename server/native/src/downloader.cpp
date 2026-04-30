#include "downloader.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>

namespace dovedioplus {

class Downloader::Impl {
public:
    std::function<void(double)> progressCallback;
    std::atomic<bool> cancelled{false};
    std::atomic<int64_t> downloadedBytes{0};
    std::atomic<int64_t> downloadSpeed{0};
    std::string lastError;
    std::chrono::steady_clock::time_point lastSpeedUpdate = std::chrono::steady_clock::now();
    int64_t lastBytes = 0;
};

Downloader::Downloader() : pImpl_(std::make_unique<Impl>()) {}
Downloader::~Downloader() = default;

bool Downloader::download(const std::string& url, const std::string& outputPath, int threadCount) {
    pImpl_->cancelled = false;
    pImpl_->downloadedBytes = 0;

    if (url.empty() || outputPath.empty()) {
        pImpl_->lastError = "URL或路径为空";
        return false;
    }

    // 模拟多线程下载（实际需集成libcurl）
    std::ofstream out(outputPath, std::ios::binary);
    if (!out.is_open()) {
        pImpl_->lastError = "无法创建输出文件";
        return false;
    }

    for (int i = 0; i <= 100; i += 10) {
        if (pImpl_->cancelled) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        pImpl_->downloadedBytes = i * 1024;
        if (pImpl_->progressCallback) pImpl_->progressCallback(i / 100.0);
    }

    out.close();
    return true;
}

bool Downloader::downloadVideo(const std::string& url, const std::string& outputPath, const std::string& format) {
    return download(url, outputPath, 4);
}

int64_t Downloader::getFileSize(const std::string& url) {
    return 1024 * 1024; // 模拟1MB
}

void Downloader::setProgressCallback(std::function<void(double)> cb) { pImpl_->progressCallback = cb; }
void Downloader::cancel() { pImpl_->cancelled = true; }
std::string Downloader::getLastError() const { return pImpl_->lastError; }
int64_t Downloader::getDownloadedBytes() const { return pImpl_->downloadedBytes; }
int64_t Downloader::getDownloadSpeed() const { return pImpl_->downloadSpeed; }

} // namespace dovedioplus
