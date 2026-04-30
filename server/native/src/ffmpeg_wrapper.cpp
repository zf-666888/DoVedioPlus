#include "ffmpeg_wrapper.h"
#include <iostream>
#include <cstring>
#include <algorithm>

namespace dovedioplus {

FFmpegWrapper::FFmpegWrapper() {
    avformat_network_init();
}

FFmpegWrapper::~FFmpegWrapper() {
    cleanup();
    avformat_network_deinit();
}

bool FFmpegWrapper::extractAudio(const std::string& inputPath, const std::string& outputPath, int quality) {
    cleanup();
    cancelled_ = false;

    if (avformat_open_input(&inputCtx_, inputPath.c_str(), nullptr, nullptr) < 0) {
        setError("无法打开输入文件: " + inputPath);
        return false;
    }

    if (avformat_find_stream_info(inputCtx_, nullptr) < 0) {
        setError("无法获取流信息");
        return false;
    }

    int audioIdx = -1;
    for (unsigned i = 0; i < inputCtx_->nb_streams; i++) {
        if (inputCtx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioIdx = i;
            break;
        }
    }

    if (audioIdx == -1) {
        setError("未找到音频流");
        return false;
    }

    AVCodecParameters* codecpar = inputCtx_->streams[audioIdx]->codecpar;
    const AVCodec* decoder = avcodec_find_decoder(codecpar->codec_id);
    if (!decoder) {
        setError("找不到解码器");
        return false;
    }

    decoderCtx_ = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(decoderCtx_, codecpar);
    avcodec_open2(decoderCtx_, decoder, nullptr);

    const AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_MP3);
    if (!encoder) {
        setError("找不到MP3编码器");
        return false;
    }

    encoderCtx_ = avcodec_alloc_context3(encoder);
    encoderCtx_->bit_rate = 320000 - (quality * 32000);
    encoderCtx_->sample_rate = decoderCtx_->sample_rate;
    encoderCtx_->channels = decoderCtx_->channels;
    encoderCtx_->channel_layout = av_get_default_channel_layout(decoderCtx_->channels);
    encoderCtx_->sample_fmt = encoder->sample_fmts[0];
    avcodec_open2(encoderCtx_, encoder, nullptr);

    avformat_alloc_output_context2(&outputCtx_, nullptr, nullptr, outputPath.c_str());
    AVStream* outStream = avformat_new_stream(outputCtx_, nullptr);
    avcodec_parameters_from_context(outStream->codecpar, encoderCtx_);

    if (!(outputCtx_->oformat->flags & AVFMT_NOFILE)) {
        avio_open(&outputCtx_->pb, outputPath.c_str(), AVIO_FLAG_WRITE);
    }

    avformat_write_header(outputCtx_, nullptr);

    swrCtx_ = swr_alloc();
    av_opt_set_channel_layout(swrCtx_, "in_channel_layout", decoderCtx_->channel_layout, 0);
    av_opt_set_channel_layout(swrCtx_, "out_channel_layout", encoderCtx_->channel_layout, 0);
    av_opt_set_int(swrCtx_, "in_sample_rate", decoderCtx_->sample_rate, 0);
    av_opt_set_int(swrCtx_, "out_sample_rate", encoderCtx_->sample_rate, 0);
    av_opt_set_sample_fmt(swrCtx_, "in_sample_fmt", decoderCtx_->sample_fmt, 0);
    av_opt_set_sample_fmt(swrCtx_, "out_sample_fmt", encoderCtx_->sample_fmt, 0);
    swr_init(swrCtx_);

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    int64_t totalDur = inputCtx_->duration;
    bool success = true;

    while (av_read_frame(inputCtx_, packet) >= 0) {
        if (cancelled_) { success = false; break; }
        if (packet->stream_index != audioIdx) { av_packet_unref(packet); continue; }

        avcodec_send_packet(decoderCtx_, packet);
        while (avcodec_receive_frame(decoderCtx_, frame) == 0) {
            AVFrame* resampled = av_frame_alloc();
            resampled->format = encoderCtx_->sample_fmt;
            resampled->channel_layout = encoderCtx_->channel_layout;
            resampled->sample_rate = encoderCtx_->sample_rate;
            resampled->nb_samples = frame->nb_samples;
            av_frame_get_buffer(resampled, 0);
            swr_convert(swrCtx_, resampled->data, resampled->nb_samples,
                       (const uint8_t**)frame->data, frame->nb_samples);

            avcodec_send_frame(encoderCtx_, resampled);
            AVPacket* outPkt = av_packet_alloc();
            while (avcodec_receive_packet(encoderCtx_, outPkt) == 0) {
                outPkt->stream_index = 0;
                av_packet_rescale_ts(outPkt, decoderCtx_->time_base, encoderCtx_->time_base);
                av_interleaved_write_frame(outputCtx_, outPkt);
                av_packet_unref(outPkt);
            }
            av_frame_free(&resampled);

            if (totalDur > 0 && frame->pts != AV_NOPTS_VALUE) {
                updateProgress(static_cast<double>(frame->pts) / totalDur);
            }
        }
        av_packet_unref(packet);
    }

    av_write_trailer(outputCtx_);
    av_packet_free(&packet);
    av_frame_free(&frame);
    return success;
}

FFmpegWrapper::VideoInfo FFmpegWrapper::getVideoInfo(const std::string& videoPath) {
    VideoInfo info;
    cleanup();

    if (avformat_open_input(&inputCtx_, videoPath.c_str(), nullptr, nullptr) < 0) {
        return info;
    }
    avformat_find_stream_info(inputCtx_, nullptr);

    for (unsigned i = 0; i < inputCtx_->nb_streams; i++) {
        AVStream* s = inputCtx_->streams[i];
        if (s->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            info.width = s->codecpar->width;
            info.height = s->codecpar->height;
            info.bitrate = s->codecpar->bit_rate;
            info.codec = avcodec_get_name(s->codecpar->codec_id);
            if (s->avg_frame_rate.den) info.fps = av_q2d(s->avg_frame_rate);
            break;
        }
    }

    if (inputCtx_->duration != AV_NOPTS_VALUE) {
        info.duration = inputCtx_->duration / AV_TIME_BASE;
    }
    return info;
}

bool FFmpegWrapper::convertFormat(const std::string& inputPath, const std::string& outputPath, const std::string& format) {
    cleanup();
    cancelled_ = false;

    if (avformat_open_input(&inputCtx_, inputPath.c_str(), nullptr, nullptr) < 0) return false;
    avformat_find_stream_info(inputCtx_, nullptr);

    avformat_alloc_output_context2(&outputCtx_, nullptr, format.c_str(), outputPath.c_str());

    for (unsigned i = 0; i < inputCtx_->nb_streams; i++) {
        AVStream* in = inputCtx_->streams[i];
        AVStream* out = avformat_new_stream(outputCtx_, nullptr);
        avcodec_parameters_copy(out->codecpar, in->codecpar);
        out->codecpar->codec_tag = 0;
    }

    if (!(outputCtx_->oformat->flags & AVFMT_NOFILE)) {
        avio_open(&outputCtx_->pb, outputPath.c_str(), AVIO_FLAG_WRITE);
    }
    avformat_write_header(outputCtx_, nullptr);

    AVPacket* pkt = av_packet_alloc();
    bool ok = true;
    while (av_read_frame(inputCtx_, pkt) >= 0) {
        if (cancelled_) { ok = false; break; }
        AVStream* in = inputCtx_->streams[pkt->stream_index];
        AVStream* out = outputCtx_->streams[pkt->stream_index];
        av_packet_rescale_ts(pkt, in->time_base, out->time_base);
        pkt->pos = -1;
        av_interleaved_write_frame(outputCtx_, pkt);
        av_packet_unref(pkt);
    }

    av_write_trailer(outputCtx_);
    av_packet_free(&pkt);
    return ok;
}

bool FFmpegWrapper::extractFrame(const std::string& videoPath, double timestamp, const std::string& outputPath) {
    cleanup();
    cancelled_ = false;

    if (avformat_open_input(&inputCtx_, videoPath.c_str(), nullptr, nullptr) < 0) return false;
    avformat_find_stream_info(inputCtx_, nullptr);

    int vidIdx = -1;
    for (unsigned i = 0; i < inputCtx_->nb_streams; i++) {
        if (inputCtx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) { vidIdx = i; break; }
    }
    if (vidIdx == -1) return false;

    AVCodecParameters* par = inputCtx_->streams[vidIdx]->codecpar;
    const AVCodec* dec = avcodec_find_decoder(par->codec_id);
    decoderCtx_ = avcodec_alloc_context3(dec);
    avcodec_parameters_to_context(decoderCtx_, par);
    avcodec_open2(decoderCtx_, dec, nullptr);

    int64_t seekTarget = static_cast<int64_t>(timestamp * AV_TIME_BASE);
    av_seek_frame(inputCtx_, -1, seekTarget, AVSEEK_FLAG_BACKWARD);

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    bool found = false;

    while (av_read_frame(inputCtx_, pkt) >= 0 && !found) {
        if (pkt->stream_index != vidIdx) { av_packet_unref(pkt); continue; }
        avcodec_send_packet(decoderCtx_, pkt);
        while (avcodec_receive_frame(decoderCtx_, frame) == 0) {
            double t = frame->pts * av_q2d(inputCtx_->streams[vidIdx]->time_base);
            if (std::abs(t - timestamp) < 0.5) {
                FILE* f = fopen(outputPath.c_str(), "wb");
                if (f) {
                    fprintf(f, "P6\n%d %d\n255\n", frame->width, frame->height);
                    for (int y = 0; y < frame->height; y++) {
                        fwrite(frame->data[0] + y * frame->linesize[0], 1, frame->width * 3, f);
                    }
                    fclose(f);
                    found = true;
                }
            }
        }
        av_packet_unref(pkt);
    }

    av_packet_free(&pkt);
    av_frame_free(&frame);
    return found;
}

void FFmpegWrapper::setProgressCallback(std::function<void(double)> cb) { progressCallback_ = cb; }
void FFmpegWrapper::cancel() { cancelled_ = true; }
std::string FFmpegWrapper::getLastError() const { return lastError_; }
void FFmpegWrapper::setError(const std::string& e) { lastError_ = e; }
void FFmpegWrapper::cleanup() {
    if (inputCtx_) { avformat_close_input(&inputCtx_); inputCtx_ = nullptr; }
    if (outputCtx_) {
        if (outputCtx_->pb) avio_closep(&outputCtx_->pb);
        avformat_free_context(outputCtx_);
        outputCtx_ = nullptr;
    }
    if (decoderCtx_) { avcodec_free_context(&decoderCtx_); decoderCtx_ = nullptr; }
    if (encoderCtx_) { avcodec_free_context(&encoderCtx_); encoderCtx_ = nullptr; }
    if (swrCtx_) { swr_free(&swrCtx_); swrCtx_ = nullptr; }
}
void FFmpegWrapper::updateProgress(double p) { if (progressCallback_) progressCallback_(std::clamp(p, 0.0, 1.0)); }

} // namespace dovedioplus
