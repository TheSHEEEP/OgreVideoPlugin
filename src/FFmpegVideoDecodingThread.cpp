
#include "FFmpegVideoDecodingThread.h"

extern "C"
{
    #ifndef INT64_C
    #define INT64_C(c) (c ## LL)
    #define UINT64_C(c) (c ## ULL)
    #endif
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libswresample/swresample.h>
    #include <libavutil/opt.h>
}
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <boost/lexical_cast.hpp>
#include <OgreLog.h>
#include <string>

#include "FFmpegVideoPlayer.h"

// This is not really thread safe, so make sure not to change 
// the used log too often while decoding ;)
static Ogre::Log* staticOgreLog = NULL;

//------------------------------------------------------------------------------
// Used internally to decoding thread, to determine desired audio sample format
inline AVSampleFormat getAVSampleFormat(AudioSampleFormat fmt)
{
	AVSampleFormat ret;
	switch (fmt)
	{
	case ASF_S16:
		ret = AV_SAMPLE_FMT_S16;
		break;
	case ASF_FLOAT:
	default:
		ret = AV_SAMPLE_FMT_FLT;
		break;
	}

	return ret;
}

//------------------------------------------------------------------------------
// This is a slightly modified version of the default ffmpeg log callback
void log_callback(void* ptr, int level, const char* fmt, va_list vl)
{
    static int print_prefix = 1;
    static int count;
    static char prev[1024];
    char line[1024];
    std::string message = "";

    // Do not get logs we do not want
    if (level > av_log_get_level())
    {
        return;
    }

    // Let FFmpeg do the line formatting
    av_log_format_line(ptr, level, fmt, vl, line, 1024, &print_prefix);

    // We do not want repeated messages, just count them
    if (print_prefix && !strcmp(line, prev) && *line && line[strlen(line) - 1] != '\r')
    {
        count++;
        return;
    }
    if (count > 0) 
    {
        message.append("    Last message repeated " + boost::lexical_cast<std::string>(count) + " times\n");
        count = 0;
    }
    strcpy(prev, line);
    message.append(line);
    
    // Print/log the message
    if (!staticOgreLog)
    {
        std::cout << message << std::endl;
    }
    else
    {
        staticOgreLog->logMessage(message, Ogre::LML_NORMAL);
    }
}

//------------------------------------------------------------------------------
bool openCodecContext(  AVFormatContext* p_formatContext, AVMediaType p_type, VideoInfo& p_videoInfo, 
                        int& p_outStreamIndex)
{
    AVStream* stream;
    AVCodecContext* decodeCodecContext = NULL;
    AVCodec* decodeCodec = NULL;
    
    // Find the stream
    p_outStreamIndex = av_find_best_stream(p_formatContext, p_type, -1, -1, NULL, 0);
    if (p_outStreamIndex < 0) 
    {
        p_videoInfo.error = "Could not find stream of type: ";
        p_videoInfo.error.append(av_get_media_type_string(p_type));
        return false;
    } 
    else 
    {
        stream = p_formatContext->streams[p_outStreamIndex];
        
        // Find decoder codec
        decodeCodecContext = stream->codec;
        decodeCodec = avcodec_find_decoder(decodeCodecContext->codec_id);
        if (!decodeCodec) 
        {
            p_videoInfo.error = "Failed to find codec: ";
            p_videoInfo.error.append(av_get_media_type_string(p_type));
            return false;
        }
        
//        if (p_type == AVMEDIA_TYPE_AUDIO)
//        {
//            decodeCodecContext->request_sample_fmt = AV_SAMPLE_FMT_S16;
//        }
        
        // Open decodec codec & context
        if (avcodec_open2(decodeCodecContext, decodeCodec, NULL) < 0) 
        {
            p_videoInfo.error = "Failed to open codec: ";
            p_videoInfo.error.append(av_get_media_type_string(p_type));
            return false;
        }
    }
    
    return true;
}

//------------------------------------------------------------------------------
int decodeAudioPacket(  AVPacket& p_packet, AVCodecContext* p_audioCodecContext, AVStream* p_stream, 
                        AVFrame* p_frame, SwrContext* p_swrContext, uint8_t** p_destBuffer, int p_destLinesize,
                        FFmpegVideoPlayer* p_player, VideoInfo& p_videoInfo, bool p_isLoop)
{
    // Decode audio frame
    int got_frame = 0;
    int decoded = avcodec_decode_audio4(p_audioCodecContext, p_frame, &got_frame, &p_packet);
    if (decoded < 0) 
    {
        p_videoInfo.error = "Error decoding audio frame.";
        return decoded;
    }
    
    if(decoded <= p_packet.size)
    {
        /* Move the unread data to the front and clear the end bits */
        int remaining = p_packet.size - decoded;
        memmove(p_packet.data, &p_packet.data[decoded], remaining);
        av_shrink_packet(&p_packet, remaining);
    }
    
    // Frame is complete, store it in audio frame queue
    if (got_frame)
    {
        int outputSamples = swr_convert(p_swrContext, 
                                        p_destBuffer, p_destLinesize, 
                                        (const uint8_t**)p_frame->extended_data, p_frame->nb_samples);
        
		int bufferSize = av_get_bytes_per_sample(getAVSampleFormat(p_player->getAudioSampleFormat())) * p_videoInfo.audioNumChannels
                            * outputSamples;
        
        int64_t duration = p_frame->pkt_duration;
        int64_t pts = p_frame->pts;
        int64_t dts = p_frame->pkt_dts;
        
        if (staticOgreLog && p_player->getLogLevel() == LOGLEVEL_EXCESSIVE)
        {
            staticOgreLog->logMessage("Audio frame bufferSize / duration / pts / dts: " 
                    + boost::lexical_cast<std::string>(bufferSize) + " / "
                    + boost::lexical_cast<std::string>(duration) + " / "
                    + boost::lexical_cast<std::string>(pts) + " / "
                    + boost::lexical_cast<std::string>(dts), Ogre::LML_NORMAL);
        }
        
        // Calculate frame life time
        double frameLifeTime = ((double)p_stream->time_base.num) / (double)p_stream->time_base.den;
        frameLifeTime *= duration;
        p_videoInfo.audioDecodedDuration += frameLifeTime;
        
        // If we are a loop, only start adding frames after 0.5 seconds have been decoded
        if (p_isLoop && p_videoInfo.audioDecodedDuration < 0.5)
        {
            staticOgreLog->logMessage("Skipping audio frame");
            return decoded;
        }
        
        // Create the audio frame
        AudioFrame* frame = new AudioFrame();
        frame->dataSize = bufferSize;
        frame->data = new uint8_t[bufferSize];
        memcpy(frame->data, p_destBuffer[0], bufferSize);
        frame->lifeTime = frameLifeTime;
        
        p_player->addAudioFrame(frame);
    }
    
    return decoded;
}

//------------------------------------------------------------------------------
int decodeVideoPacket(  AVPacket& p_packet, AVCodecContext* p_videoCodecContext, AVStream* p_stream, 
                        AVFrame* p_frame, SwsContext* p_swsContext, AVPicture* p_destPic, 
                        FFmpegVideoPlayer* p_player, VideoInfo& p_videoInfo, bool p_isLoop)
{
    // Decode audio frame
    int got_frame = 0;
    int decoded = avcodec_decode_video2(p_videoCodecContext, p_frame, &got_frame, &p_packet);
    if (decoded < 0) 
    {
        p_videoInfo.error = "Error decoding video frame.";
        return decoded;
    }
    
    // Frame is complete, sws_scale it and store it in video frame queue
    if (got_frame)
    {
        // Convert the image into the video frame
        sws_scale(p_swsContext, p_frame->data, p_frame->linesize,
                    0, p_videoCodecContext->height, p_destPic->data, p_destPic->linesize);
        
        // Use packet duration and packet dts to get the lifetime and position of a frame
        // PTS is highly erroneous and sometimes not even used at all (theora & vorbis)
        int64_t duration = p_frame->pkt_duration;
        int64_t pts = p_frame->pts;
        int64_t dts = p_frame->pkt_dts;
        
//        if (staticOgreLog && p_player->getLogLevel() == LOGLEVEL_EXCESSIVE)
//        {
//            staticOgreLog->logMessage("Video frame duration / pts / dts: " 
//                    + boost::lexical_cast<std::string>(duration) + " / "
//                    + boost::lexical_cast<std::string>(pts) + " / "
//                    + boost::lexical_cast<std::string>(dts), Ogre::LML_NORMAL);
//        }
        
        // Calculate frame life time
        double frameLifeTime = ((double)p_stream->time_base.num) / (double)p_stream->time_base.den;
        frameLifeTime *= duration;
        p_videoInfo.videoDecodedDuration += frameLifeTime;
        
        // If we are a loop, only start adding frames after 0.5 seconds have been decoded
        if (p_isLoop && p_videoInfo.videoDecodedDuration < 0.5)
        {
            return decoded;
        }
        
        // Create the video frame
        VideoFrame* videoFrame = new VideoFrame();
        int size = p_destPic->linesize[0] * p_videoCodecContext->height;
        videoFrame->dataSize = size;
        videoFrame->data = new uint8_t[size];
        memcpy(videoFrame->data, p_destPic->data[0], size);
        videoFrame->lifeTime = frameLifeTime;
        
        // If the lifeTime is below 0.01 seconds, which would mean 1/100 fps, something
        // is very fishy with the time_base, duration or similar. Use r_frame_rate of the stream instead.
        // This is guessing, more or less! Only works with constant fps streams.
        if (videoFrame->lifeTime < 0.01)
        {
            videoFrame->lifeTime = 1.0 / ((double)(p_stream->r_frame_rate.num) / (double)(p_stream->r_frame_rate.den));
        }
        
        // Insert the frame into the video queue
        p_player->addVideoFrame(videoFrame);
    }
    
    return decoded;
}

//------------------------------------------------------------------------------
void videoDecodingThread(ThreadInfo* p_threadInfo)
{
    // Read ThreadInfo struct, then delete it
    FFmpegVideoPlayer* videoPlayer = p_threadInfo->videoPlayer;
    VideoInfo& videoInfo = videoPlayer->getVideoInfo();
    boost::mutex* playerMutex = p_threadInfo->playerMutex;
    boost::condition_variable* playerCondVar = p_threadInfo->playerCondVar;
    boost::mutex* decodeMutex = p_threadInfo->decodingMutex;
    boost::condition_variable* decodeCondVar = p_threadInfo->decodingCondVar;
    bool isLoop = p_threadInfo->isLoop;
    staticOgreLog = videoPlayer->getLog();
    delete p_threadInfo;
    
    // Initialize FFmpeg  
    av_register_all();
    av_log_set_callback(log_callback);
    av_log_set_level(AV_LOG_WARNING);
    
    // Initialize video decoding, filling the VideoInfo
    // Open the input file
    AVFormatContext* formatContext = NULL;
    const char* name = videoPlayer->getVideoFilename().c_str();
    if (avformat_open_input(&formatContext, name, NULL, NULL) < 0) 
    {
        videoInfo.error = "Could not open input: ";
        videoInfo.error.append(videoPlayer->getVideoFilename());
        playerCondVar->notify_all();
        return;
    }
    
    // Read stream information
    if (avformat_find_stream_info(formatContext, NULL) < 0) 
    {
        videoInfo.error = "Could not find stream information.";
        playerCondVar->notify_all();
        return;
    }
    
    // Get streams
    // Audio stream
    AVStream* audioStream = NULL;
    AVCodecContext* audioCodecContext = NULL;
    int audioStreamIndex = -1;
    if (!openCodecContext(formatContext, AVMEDIA_TYPE_AUDIO, videoInfo, audioStreamIndex)) 
    {
        // The error itself is set by openCodecContext
        playerCondVar->notify_all();
        return;
    }
    audioStream = formatContext->streams[audioStreamIndex];
    audioCodecContext = audioStream->codec;
    
    // Video stream
    AVStream* videoStream = NULL;
    AVCodecContext* videoCodecContext = NULL;
    int videoStreamIndex = -1;
    if (!openCodecContext(formatContext, AVMEDIA_TYPE_VIDEO, videoInfo, videoStreamIndex)) 
    {
        // The error itself is set by openCodecContext
        playerCondVar->notify_all();
        return;
    }
    videoStream = formatContext->streams[videoStreamIndex];
    videoCodecContext = videoStream->codec;
    
    // Dump information
    av_dump_format(formatContext, 0, videoPlayer->getVideoFilename().c_str(), 0);
    
    // Store useful information in VideoInfo struct
    double timeBase = ((double)audioStream->time_base.num) / (double)audioStream->time_base.den;
    videoInfo.audioDuration = audioStream->duration * timeBase;
    videoInfo.audioSampleRate = audioCodecContext->sample_rate;
    videoInfo.audioBitRate = audioCodecContext->bit_rate;
    videoInfo.audioNumChannels = 
            videoInfo.audioNumChannels > 0 ? videoInfo.audioNumChannels : audioCodecContext->channels;
    
    timeBase = ((double)videoStream->time_base.num) / (double)videoStream->time_base.den;
    videoInfo.videoDuration = videoStream->duration * timeBase;
    videoInfo.videoWidth = videoCodecContext->width;
    videoInfo.videoHeight = videoCodecContext->height;
    
    // If the a duration is below 0 seconds, something is very fishy. 
    // Use format duration instead, it's the best guess we have
    if (videoInfo.audioDuration < 0.0)
    {
        videoInfo.audioDuration = ((double)formatContext->duration) / AV_TIME_BASE;
    }
    if (videoInfo.videoDuration < 0.0)
    {
        videoInfo.videoDuration = ((double)formatContext->duration) / AV_TIME_BASE;
    }
 
    // Store the longer of both durations. This is what determines when looped videos
    // will begin anew
    videoInfo.longerDuration = videoInfo.videoDuration > videoInfo.audioDuration ? 
                                videoInfo.videoDuration : videoInfo.audioDuration;
            
    // Wake up video player
    videoInfo.infoFilled = true;
    playerCondVar->notify_all();
    
    // Initialize packet, set data to NULL, let the demuxer fill it
    AVPacket packet;
    av_init_packet(&packet);
    packet.data = NULL;
    packet.size = 0;
    
    // Initialize SWS context
    SwsContext* swsContext = NULL;
    swsContext = sws_getCachedContext(swsContext,
                                videoInfo.videoWidth, videoInfo.videoHeight, videoCodecContext->pix_fmt, 
                                videoInfo.videoWidth, videoInfo.videoHeight, AV_PIX_FMT_RGBA, 
                                SWS_BICUBIC, NULL, NULL, NULL);
    
    // Create destination picture
    AVFrame* destPic = av_frame_alloc();
    
    avpicture_alloc((AVPicture*)destPic, AV_PIX_FMT_RGBA, videoInfo.videoWidth, videoInfo.videoHeight);
    
    // Get the correct target channel layout
    uint64_t targetChannelLayout;
    // Keep the source layout
    if (audioCodecContext->channels == videoInfo.audioNumChannels)
    {
        targetChannelLayout = audioCodecContext->channel_layout;
    }
    // Or determine a new one
    else
    {
        switch (videoInfo.audioNumChannels)
        {
            case 1:
                targetChannelLayout = AV_CH_LAYOUT_MONO;
                break;
                
            case 2:
                targetChannelLayout = AV_CH_LAYOUT_STEREO;
                break;
                
            default:
                targetChannelLayout = audioCodecContext->channel_layout;
                break;
        }
    }
    
    // Initialize SWR context
    SwrContext* swrContext = swr_alloc_set_opts(NULL, 
                targetChannelLayout, getAVSampleFormat(videoPlayer->getAudioSampleFormat()), audioCodecContext->sample_rate,
                audioCodecContext->channel_layout, audioCodecContext->sample_fmt, audioCodecContext->sample_rate, 
                0, NULL);
    int result = swr_init(swrContext);
    if (result != 0) 
    {
        videoInfo.error = "Could not initialize swr context: " + boost::lexical_cast<std::string>(result);
        playerCondVar->notify_all();
        return;
    }
    
    // Create destination sample buffer
    uint8_t** destBuffer = NULL;
    int destBufferLinesize;
    av_samples_alloc_array_and_samples( &destBuffer,
                                        &destBufferLinesize,
                                        videoInfo.audioNumChannels,
                                        2048,
                                        getAVSampleFormat(videoPlayer->getAudioSampleFormat()),
                                        0);
    
    // Main decoding loop
    // Read the input file frame by frame
    AVFrame* frame = NULL;
    while (av_read_frame(formatContext, &packet) >= 0) 
    {
        if(videoPlayer->getVideoBufferIsFull())
        std::cout<<"Decoding loop:VideoBuffer Full"<<std::endl;

        if(videoPlayer->getAudioBufferIsFull())
         std::cout<<"Decoding loop:AudioBuffer Full"<<std::endl;

        // Only start decoding when at least one of the buffers is not full
        while (videoPlayer->getVideoBufferIsFull() && videoPlayer->getAudioBufferIsFull())
        {
            boost::unique_lock<boost::mutex> lock(*decodeMutex);
            boost::chrono::steady_clock::time_point const timeOut = 
                boost::chrono::steady_clock::now() + boost::chrono::milliseconds((int)videoPlayer->getBufferTarget() * 1000);
            decodeCondVar->wait_until(lock, timeOut);
            
            if (videoInfo.decodingAborted)
            {
                   std::cout<<"Decoding loop:Aborted"<<std::endl;
                break;
            }
        }
            
        // Break if the decoding was aborted
        if (videoInfo.decodingAborted)
        {
            std::cout<<"Decoding loop:Aborted2"<<std::endl;
            break;
        }
        
        // Initialize frame
        if (!frame) 
        {
            if (!(frame = av_frame_alloc())) 
            {
                std::cout<<"Decoding loop:Out of memory."<<std::endl;
                videoInfo.error = "Out of memory.";
                return;
            }
        } 
        
        else
        {
              av_frame_unref(frame);
        }
        
        // Decode the packet
        AVPacket orig_pkt = packet;
        do 
        {
            int decoded = 0;
            if (packet.stream_index == audioStreamIndex)
            {
                 std::cout<<"Decoding:AudioPacket."<<std::endl;
                decoded = decodeAudioPacket(packet, audioCodecContext, audioStream, frame, swrContext,
                                            destBuffer, destBufferLinesize, videoPlayer, videoInfo, isLoop);
            }
            else if (packet.stream_index == videoStreamIndex)
            {
                 std::cout<<"Decoding:VideoPacket."<<std::endl;
                decoded = decodeVideoPacket(packet, videoCodecContext, videoStream, frame, swsContext, 
                                            (AVPicture*)destPic, videoPlayer, videoInfo, isLoop);
            }
            else
            {
                // This means that we have a stream that is neither our video nor audio stream
                // Just skip the package
                break;
            }
            
            // decoded will be negative on an error
            if (decoded < 0)
            {
                // The error itself is set by the decode functions
                 std::cout<<"Decoding:decode<0."<<std::endl;
                playerCondVar->notify_all();
                return;
            }
            
            // Increment data pointer, subtract from size
            packet.data += decoded;
            packet.size -= decoded;
        } while (packet.size > 0);
        
        av_free_packet(&orig_pkt);
    }
     std::cout<<"Decoding:Done!."<<std::endl;
    // We're done. Close everything
    av_frame_free(&frame);
    avpicture_free((AVPicture*)destPic);
    av_frame_free(&destPic);
    avcodec_close(videoCodecContext);
    avcodec_close(audioCodecContext);
    sws_freeContext(swsContext);
    av_freep(&destBuffer[0]);
    swr_free(&swrContext);
    avformat_close_input(&formatContext);
    
    videoInfo.audioDuration = videoInfo.audioDecodedDuration;
    videoInfo.decodingDone = videoInfo.decodingAborted ? false : true;
}
