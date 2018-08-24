/* 
 * File:   FFmpegVideoDecodingThread.h
 * Author: TheSHEEEP
 *
 * Created on 10. Januar 2014, 11:57
 */
#include <iostream>
#ifndef FFMPEGVIDEODECODINGTHREAD_H
#define	FFMPEGVIDEODECODINGTHREAD_H

// Forward declarations
class FFmpegVideoPlayer;
namespace boost
{
    class thread;
    class mutex;
    class condition_variable;
}

/**
 * Helper struct with information for the decoding thread.
 */
struct ThreadInfo
{
    FFmpegVideoPlayer*          videoPlayer;
    boost::mutex*               decodingMutex;
    boost::condition_variable*  decodingCondVar;
    boost::mutex*               playerMutex;
    boost::condition_variable*  playerCondVar;
    bool                        isLoop;         
};

/**
 * This is the main video decoding thread.
 * It will decode the video until the buffer is full or the video is finished, then
 * goes into sleep.
 * The video player wakes it up regularly so it checks if it must continue decoding.
 * 
 * Such a thread is started each time a new video is being played/decoded.
 */
void videoDecodingThread(ThreadInfo* p_threadInfo);

#endif	/* FFMPEGVIDEODECODINGTHREAD_H */

