#pragma once

#include <string>
#include <functional>
#include <memory>
#include <atomic>

namespace dovedioplus {

class Downloader {
public:
    Downloader();
    ~Downloader();

    Downloader(const Downloader&) = delete;
    Downloader& operator=(const Downloader&) = delete;

    bool download(const std::string& url, const std::string& outputPath, int threadCount = 4);
    bool downloadVideo(const std::string& url, const std::string& outputPath, const std::string& format = "mp4");
    int64_t getFileSize(const std::string& url);

    void setProgressCallback(std::function<void(double)> callback);
    void cancel();
    std::string getLastError() const;
    int64_t getDownloadedBytes() const;
    int64_t getDownloadSpeed() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace dovedioplus
