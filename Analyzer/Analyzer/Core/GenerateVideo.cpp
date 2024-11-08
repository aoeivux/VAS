#include "GenerateVideo.h"
#include "Utils/Log.h"
#include "Utils/Common.h"
#include "Config.h"
#include "GenerateAlarm.h"

#ifndef WIN32
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs/legacy/constants_c.h>
#else
#ifndef _DEBUG
#include <turbojpeg.h>
#else
#include <opencv2/opencv.hpp>
#endif
#endif


extern "C" {
#include "libswscale/swscale.h"
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
}
#pragma warning(disable: 4996)

namespace AVSAnalyzer {

    bool genUnCompressImage(AVSAlarmImage* image, unsigned char*& out_bgr, int out_bgrSize) {

#ifdef WIN32
#ifndef _DEBUG
        unsigned char* jpeg_data = image->getData();
        unsigned long jpeg_size = image->getSize();
        int height = image->getHeight();
        int width = image->getWidth();
        int channels = image->getChannels();

        // 用于初始化 JPEG 解压缩处理的环境。它的主要作用是创建一个解压缩句柄 tjhandle，这个句柄将用于后续的解压缩操作
        tjhandle handle = tjInitDecompress();
        if (nullptr == handle) {
            return false;
        }

        int subsamp, cs;
        // 解压缩 JPEG 图像的头部信息，以获取图像的基本属性
        /*  args:
            handle：指向之前用 tjInitDecompress 初始化的解压缩句柄。
			jpeg_data：指向 JPEG 图像数据的指针。
			jpeg_size：JPEG 数据的大小（字节数）。
			width：指向变量的指针，用于存储解压缩后的图像宽度。
			height：指向变量的指针，用于存储解压缩后的图像高度。
			subsamp：指向变量的指针，用于返回图像的子采样格式（如 4:4:4、4:2:2 等）。
			cs：指向变量的指针，用于返回图像的颜色空间（如 RGB、YUV 等）。
        */
        int ret = tjDecompressHeader3(handle, jpeg_data, jpeg_size, &width, &height, &subsamp, &cs);
        
        // TJ_CS_GRAY：表示图像为灰度图，仅包含亮度信息
        if (cs == TJCS_GRAY) channels = 1;
        else channels = 3;

        int pf = TJCS_RGB;
        int ps = tjPixelSize[pf];
        
        /*
            函数参数说明：

			handle：之前初始化的解压缩句柄。
			jpeg_data：指向 JPEG 图像数据的指针。
			jpeg_size：JPEG 数据的大小（字节数）。
			out_bgr：指向输出缓冲区的指针，解压缩后的图像数据将存储在这里。
			width：解压缩后的图像宽度。
			width * channels：输出缓冲区每行的字节数（宽度乘以通道数）。
			height：解压缩后的图像高度。
			TJPF_BGR：指定输出数据格式为 BGR（蓝、绿、红）颜色顺序。
			TJFLAG_NOREALLOC：指示不重新分配输出缓冲区。
        */

        ret = tjDecompress2(handle, jpeg_data, jpeg_size, out_bgr, width, width * channels, height, TJPF_BGR, TJFLAG_NOREALLOC);

        tjDestroy(handle);

        if (ret != 0) {
            return false;
        }
        return true;


#else
        std::vector<uchar> jpegBuffer(image->getSize());
        memcpy(jpegBuffer.data(), image->getData(), image->getSize());
        cv::Mat bgr_image = cv::imdecode(jpegBuffer, CV_LOAD_IMAGE_UNCHANGED);
        memcpy(out_bgr, bgr_image.data, out_bgrSize);

        return true;

#endif // !_DEBUG


#endif //WIN32
    }


    // bgr24转yuv420p

    static unsigned char clipValue(unsigned char x, unsigned char min_val, unsigned char  max_val) {

        if (x > max_val) {
            return max_val;
        }
        else if (x < min_val) {
            return min_val;
        }
        else {
            return x;
        }
    }

    static bool bgr24ToYuv420p(unsigned char* bgrBuf, int w, int h, unsigned char* yuvBuf) {

        unsigned char* ptrY, * ptrU, * ptrV, * ptrRGB;
        memset(yuvBuf, 0, w * h * 3 / 2);
        ptrY = yuvBuf;
        ptrU = yuvBuf + w * h;
        ptrV = ptrU + (w * h * 1 / 4);
        unsigned char y, u, v, r, g, b;

        // 每一行
        for (int j = 0; j < h; ++j) {

            ptrRGB = bgrBuf + w * j * 3;

            // 每一列
            for (int i = 0; i < w; i++) {

                b = *(ptrRGB++);
                g = *(ptrRGB++);
                r = *(ptrRGB++);


                y = (unsigned char)((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
                u = (unsigned char)((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
                v = (unsigned char)((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
                *(ptrY++) = clipValue(y, 0, 255);
                if (j % 2 == 0 && i % 2 == 0) {
                    *(ptrU++) = clipValue(u, 0, 255);
                }
                else {
                    if (i % 2 == 0) {
                        *(ptrV++) = clipValue(v, 0, 255);
                    }
                }
            }
        }
        return true;
    }



    GenerateVideo::GenerateVideo(Config* config, AVSAlarm* alarm) :
        mConfig(config),mAlarm(alarm)
    {
        LOGI("");
    }

    GenerateVideo::~GenerateVideo()
    {
        LOGI("");
        destoryCodecCtx();

    }

    bool GenerateVideo::initCodecCtx(const char* url) {

        if (avformat_alloc_output_context2(&mFmtCtx, NULL, "flv", url) < 0) {
            LOGE("avformat_alloc_output_context2 error");
            return false;
        }

        // 初始化视频编码器 start
        AVCodec* videoCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!videoCodec) {
            LOGE("avcodec_find_decoder error");
            return false;
        }
        mVideoCodecCtx = avcodec_alloc_context3(videoCodec);
        if (!mVideoCodecCtx) {
            LOGE("avcodec_alloc_context3 error");
            return false;
        }
        int bit_rate = 4000000; 

        // CBR：Constant BitRate - 固定比特率
    //    mVideoCodecCtx->flags |= AV_CODEC_FLAG_QSCALE;
    //    mVideoCodecCtx->bit_rate = bit_rate;
    //    mVideoCodecCtx->rc_min_rate = bit_rate;
    //    mVideoCodecCtx->rc_max_rate = bit_rate;
    //    mVideoCodecCtx->bit_rate_tolerance = bit_rate;

        //VBR
        mVideoCodecCtx->flags |= AV_CODEC_FLAG_QSCALE;
        mVideoCodecCtx->rc_min_rate = bit_rate / 2;
        mVideoCodecCtx->rc_max_rate = bit_rate / 2 + bit_rate;
        mVideoCodecCtx->bit_rate = bit_rate;

        //ABR：Average Bitrate - 平均码率
    //    mVideoCodecCtx->bit_rate = bit_rate;

        mVideoCodecCtx->codec_id = videoCodec->id;
        mVideoCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;// 不支持AV_PIX_FMT_BGR24直接进行编码
        mVideoCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
        mVideoCodecCtx->width = mAlarm->width;
        mVideoCodecCtx->height = mAlarm->height;
        mVideoCodecCtx->time_base = { 1,mAlarm->fps };
        mVideoCodecCtx->framerate = { mAlarm->fps, 1 };
        mVideoCodecCtx->gop_size = mAlarm->fps;
        mVideoCodecCtx->max_b_frames = 5;
        mVideoCodecCtx->thread_count = 1;

        //mVideoCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;  //全局参数
            
        //这些数据用于描述视频编码的基本参数，例如分辨率、帧率等
        unsigned char sps_pps[] = { 0x00 ,0x00 ,0x01,0x67,0x42,0x00 ,0x2a ,0x96 ,0x35 ,0x40 ,0xf0 ,0x04 ,
                            0x4f ,0xcb ,0x37 ,0x01 ,0x01 ,0x01 ,0x40 ,0x00 ,0x01 ,0xc2 ,0x00 ,0x00 ,0x57 ,
                            0xe4 ,0x01 ,0x00 ,0x00 ,0x00 ,0x01 ,0x68 ,0xce ,0x3c ,0x80, 0x00 };

        mVideoCodecCtx->extradata_size = sizeof(sps_pps);
        mVideoCodecCtx->extradata = (uint8_t*)av_mallocz(mVideoCodecCtx->extradata_size);
        memcpy(mVideoCodecCtx->extradata, sps_pps, mVideoCodecCtx->extradata_size);


        AVDictionary* video_codec_options = NULL;
        //add: k=>v
        av_dict_set(&video_codec_options, "profile", "main", 0);
        av_dict_set(&video_codec_options, "preset", "superfast", 0);
        //av_dict_set(&video_codec_options, "tune", "fastdecode", 0);

        //  尝试能否打开编解码器, 如果初始化失败，该函数将返回负值，表示错误。例如，可能是因为提供了不兼容的参数或编码器无法处理给定格式
        if (avcodec_open2(mVideoCodecCtx, videoCodec, &video_codec_options) < 0) {
            LOGE("avcodec_open2 error");
            return false;
        }

        // 在mFmCtx上下文中创建一个新的媒体流,并指定编解码器
        mVideoStream = avformat_new_stream(mFmtCtx, videoCodec);
        if (!mVideoStream) {
            LOGE("avformat_new_stream error");
            return false;
        }

        /*
         *在调用 avformat_new_stream 时，FFmpeg 会为 AVStream 结构体分配内存并初始化其字段，包括流的 ID
         *在 avformat_new_stream 成功返回后，mVideoStream->id 已经会被设置为 mFmtCtx->nb_streams - 1，
         *即新流在 streams 数组中的位置。在这种情况下是多余的。
         *
          */
        mVideoStream->id = mFmtCtx->nb_streams - 1;

        // stream的time_base参数非常重要，它表示将现实中的一秒钟分为多少个时间基, 在下面调用avformat_write_header时自动完成
        avcodec_parameters_from_context(mVideoStream->codecpar, mVideoCodecCtx);
        mVideoIndex = mVideoStream->id;
        // 初始化视频编码器 end



        av_dump_format(mFmtCtx, 0, url, 1);

        // open output url
        if (!(mFmtCtx->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&mFmtCtx->pb, url, AVIO_FLAG_WRITE) < 0) {
                LOGE("avio_open error url=%s", url);
                return false;
            }
        }


        AVDictionary* fmt_options = NULL;
        //av_dict_set(&fmt_options, "bufsize", "1024", 0);
        //av_dict_set(&fmt_options, "muxdelay", "0.1", 0);
        //av_dict_set(&fmt_options, "tune", "zerolatency", 0);

        mFmtCtx->video_codec_id = mFmtCtx->oformat->video_codec;

        if (avformat_write_header(mFmtCtx, &fmt_options) < 0) { // 调用该函数会将所有stream的time_base，自动设置一个值，通常是1/90000或1/10Auuu00，这表示一秒钟表示的时间基长度
            LOGE("avformat_write_header error");
            return false;
        }

        return true;
    }
    void GenerateVideo::destoryCodecCtx() {

        //std::this_thread::sleep_for(std::chrono::milliseconds(1));

        if (mFmtCtx) {
            // 推流需要释放start
            if (mFmtCtx && !(mFmtCtx->oformat->flags & AVFMT_NOFILE)) {
                avio_close(mFmtCtx->pb);
            }
            // 推流需要释放end



            avformat_free_context(mFmtCtx);
            mFmtCtx = NULL;
        }

        if (mVideoCodecCtx) {
            if (mVideoCodecCtx->extradata) {
                av_free(mVideoCodecCtx->extradata);
                mVideoCodecCtx->extradata = NULL;
            }

            avcodec_close(mVideoCodecCtx);
            avcodec_free_context(&mVideoCodecCtx);
            mVideoCodecCtx = NULL;
            mVideoIndex = -1;
        }


    }

    bool GenerateVideo::run() {
 
        std::string url = mConfig->rootVideoDir +
            "/" + mAlarm->controlCode + "-" + std::to_string(getCurTimestamp()) + ".flv";
        
        /* 编码合成
         * avformat_alloc_output_context2()
         *
         * avcodec_find_encoder()
         *
         * avcodec_alloc_context3()
         *
         * avcodec_open2()
         *
         * av_dict_set()
         *
         * avformat_new_stream()
         * 
         * avcodec_parameters_from_context()
         *
         * avio_open()
         *
         * avformat_write_header()
         *
         * */
        if (!initCodecCtx(url.data())) {
            return false;
        }

        int width = mAlarm->width;
        int height = mAlarm->height;
  
        AVFrame* frame_yuv420p = av_frame_alloc();
        frame_yuv420p->format = mVideoCodecCtx->pix_fmt;
        frame_yuv420p->width = width;
        frame_yuv420p->height = height;

        int frame_yuv420p_buff_size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, width, height, 1);
        uint8_t* frame_yuv420p_buff = (uint8_t*)av_malloc(frame_yuv420p_buff_size);
        
        // 函数将 frame_yuv420p->data 数组中的每个指针填充为指向图像数据缓冲区 frame_yuv420p_buff 中 Y、U 和 V 分量的起始位置
        // frame_yuv420p->linesize是通过图像的宽度(参数width)和像素格式来计算的
        av_image_fill_arrays(frame_yuv420p->data, frame_yuv420p->linesize,
            frame_yuv420p_buff,
            AV_PIX_FMT_YUV420P,
            width, height, 1);


        AVPacket* pkt = av_packet_alloc();// 编码后的视频帧
        int64_t  frameCount = 1;

        int ret = -1;
        int receive_packet_count = -1;

        AVSAlarmImage* image;
        int channels = 3;
        int bgrSize = width * height * channels;
        unsigned char* bgr = (unsigned char*)malloc(bgrSize);//创建堆内存

        for (size_t i = 0; i < mAlarm->images.size(); i++)
        {
            // iamge: AVSAlarmImage
            image = mAlarm->images[i];

            // 获取解压图片
            if (genUnCompressImage(image, bgr, bgrSize)) {
                //解压缩成功

                 // frame_bgr 转  frame_yuv420p, 并转结果存储到frame_yuv420p_buff
                bgr24ToYuv420p(bgr, width, height, frame_yuv420p_buff);


                // av_rescale_q_rnd 用于在不同时间基之间进行时间或大小的转
                /*
                    参数说明
                    src (int64_t src)：
                    要转换的值，例如时间戳、持续时间或其他需要在不同时间基间转换的数值。

                    src_base (AVRational src_base)：
                    源时间基（或大小基），用 AVRational 结构表示，通常是一个分数，表示源值的单位时间（如每秒的帧数）。

                    dst_base (AVRational dst_base)：
                    目标时间基（或大小基），同样是一个 AVRational 结构，表示转换后值的单位时间。

                    round (AVRounding round)：
                    舍入选项，控制转换过程中数值的舍入方式。
                  */

                frame_yuv420p->pts = frame_yuv420p->pkt_dts = av_rescale_q_rnd(frameCount,
                    mVideoCodecCtx->time_base,
                    mVideoStream->time_base,
                    (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

                frame_yuv420p->pkt_duration = av_rescale_q_rnd(1,
                    mVideoCodecCtx->time_base,
                    mVideoStream->time_base,
                    (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

                frame_yuv420p->pkt_pos = frameCount;


                ret = avcodec_send_frame(mVideoCodecCtx, frame_yuv420p);
                if (ret >= 0) {
                    receive_packet_count = 0;
                    while (true) {
                        ret = avcodec_receive_packet(mVideoCodecCtx, pkt);
                        if (ret >= 0) {

                            //LOGI("encode 1 frame spend：%lld(ms),frameCount=%lld, encodeSuccessCount = %lld, frameQSize=%d,ret=%d", 
                            //    (t2 - t1), frameCount, encodeSuccessCount, frameQSize, ret);

                            pkt->stream_index = mVideoIndex;

                            pkt->pos = frameCount;
                            pkt->duration = frame_yuv420p->pkt_duration;


                            int wframe = av_write_frame(mFmtCtx, pkt);
                            if (wframe < 0) {
                                LOGE("writePkt : wframe=%d", wframe);
                            }
                            ++receive_packet_count;


                            if (receive_packet_count > 1) {
                                LOGI("avcodec_receive_packet success: receive_packet_count=%d", receive_packet_count);
                            }
                        }
                        else {
                            if (0 == receive_packet_count) {
                                LOGE("avcodec_receive_packet error : ret=%d", ret);
                            }

                            break;
                        }
                    }

                }
                else {
                    LOGE("avcodec_send_frame error : ret=%d", ret);
                }
                frameCount++;
                //std::string imageName = mAlarm->videoDir + "\\" + std::to_string(getCurTimestamp()) + "_" + std::to_string(Common_GetRandom())+"_" + std::to_string(i) + ".jpg";
                //Common_SaveCompressImage(image, imageName);
                //bool s = false;
                //bool s = Common_SaveBgr(image->getHeight(), image->getWidth(), image->getChannels(),
                //    bgr, imageName);
                //printf("%s,s=%d\n", imageName.data(),s);
            }
            else {
                LOGE("Common_UnCompressImage error");
            }

           
        }

        free(bgr);
        bgr = nullptr;

        av_write_trailer(mFmtCtx);//写文件尾

        av_packet_unref(pkt);
        pkt = nullptr;


        av_free(frame_yuv420p_buff);
        frame_yuv420p_buff = nullptr;

        av_frame_free(&frame_yuv420p);
        //av_frame_unref(frame_yuv420p);
        frame_yuv420p = nullptr;


        return true;
        
    }
}
