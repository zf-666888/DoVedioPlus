#include "downloader.h"
#include <curl/curl.h>
#include <fstream>
#include <thread>
#include <chrono>
#include <iostream>

namespace dovedioplus {

class Downloader::Impl {
public:
    std::function<void(double)> progressCallback;
    std::atomic<bool> cancelled{false};
    std::atomic<int64_t> downloadedBytes{0};
    std::atomic<int64_t> downloadSpeed{0};
    std::string lastError;

    std::chrono::steady_clock::time_point lastSpeedUpdate = std::chrono::steady_clock::now();
    std::atomic<int64_t> lastBytes{0};

    void updateSpeed() {
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSpeedUpdate).count();
        if (ms >= 1000) {
            int64_t cur = downloadedBytes.load();
            downloadSpeed = (cur - lastBytes.load()) * 1000 / ms;
            lastBytes = cur;
            lastSpeedUpdate = now;
        }
    }
};

static size_t writeCallback(void* data, size_t size, size_t nmemb, void* userp) {
    auto* ofs = static_cast<std::ofstream*>(userp);
    size_t total = size * nmemb;
    ofs->write(static_cast<char*>(data), total);
    return total;
}

static int progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                            curl_off_t ultotal, curl_off_t ulnow) {
    auto* impl = static_cast<Downloader::Impl*>(clientp);
    if (impl->cancelled) return 1; // 返回非0取消

    impl->downloadedBytes = dlnow;
    if (dltotal > 0 && impl->progressCallback) {
        impl->progressCallback(static_cast<double>(dlnow) / dltotal);
    }
    impl->updateSpeed();
    return 0;
}

Downloader::Downloader() : pImpl_(std::make_unique<Impl>()) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

Downloader::~Downloader() {
    curl_global_cleanup();
}

bool Downloader::download(const std::string& url, const std::string& outputPath, int threadCount) {
    pImpl_->cancelled = false;
    pImpl_->downloadedBytes = 0;

    if (url.empty() || outputPath.empty()) {
        pImpl_->lastError = "URL或输出路径为空";
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        pImpl_->lastError = "libcurl 初始化失败";
        return false;
    }

    std::ofstream out(outputPath, std::ios::binary);
    if (!out.is_open()) {
        pImpl_->lastError = "无法创建输出文件: " + outputPath;
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, pImpl_.get());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 600L);

    CURLcode res = curl_easy_perform(curl);

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);
    out.close();

    if (res != CURLE_OK) {
        pImpl_->lastError = std::string("下载失败: ") + curl_easy_strerror(res);
        return false;
    }
    if (httpCode >= 400) {
        pImpl_->lastError = "HTTP " + std::to_string(httpCode);
        return false;
    }

    return true;
}

bool Downloader::downloadVideo(const std::string& url, const std::string& outputPath, const std::string& format) {
    return download(url, outputPath, 4);
}

int64_t Downloader::getFileSize(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) return -1;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    double contentLength = 0;
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &contentLength);
    }
    curl_easy_cleanup(curl);
    return static_cast<int64_t>(contentLength);
}

void Downloader::setProgressCallback(std::function<void(double)> cb) { pImpl_->progressCallback = cb; }
void Downloader::cancel() { pImpl_->cancelled = true; }
std::string Downloader::getLastError() const { return pImpl_->lastError; }
int64_t Downloader::getDownloadedBytes() const { return pImpl_->downloadedBytes; }
int64_t Downloader::getDownloadSpeed() const { return pImpl_->downloadSpeed; }

} // namespace dovedioplus
