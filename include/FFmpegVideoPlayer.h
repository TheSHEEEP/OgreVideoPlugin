/* 
 * File:   FFmpegVideoPlayer.h
 * Author: TheSHEEEP
 *
 * Created on 10. Januar 2014, 10:57
 */

#ifndef FFMPEGVIDEOPLAYER_H
#define	FFMPEGVIDEOPLAYER_H

#include "FFmpegPluginPrerequisites.h"
#include "FFmpegVideoDecodingThread.h"

#include <OgreFrameListener.h>
#include <OgreTextureManager.h>
#include <deque>

// Forward declarations
namespace Ogre
{
    class Log;
}

/**
 * Helper struct that holds various video information.
 *  All common information is stored here to have video & audio packages as small as possible.
 */
struct VideoInfo
{
    VideoInfo();
    
    bool            infoFilled;         // This is set to true by the decoding thread as soon as it has 
                                        // set up the streams for decoding
    bool            decodingDone;       // This is set to true by the decoding thread as soon as everything
                                        // has been decoded
    bool            decodingAborted;    // This can be set to true to force the decoding thread to stop
    
    double          audioDuration;          // Audio duration in seconds (estimated by FFmpeg before decoding, set to
                                            // audioDurationUpdated after decoding is finished)
    double          audioDecodedDuration;   // Decoded audio duration in seconds (duration updated after each decoded frame)
    unsigned int    audioSampleRate;        // Sample rate
    unsigned int    audioBitRate;           // Bit rate
    unsigned int    audioNumChannels;       // Number of audio channels.
    
    double          videoDecodedDuration;   // Decoded video duration in seconds (duration updated after each decoded frame)
    double          videoDuration;          // Video duration in seconds
    unsigned int    videoWidth;             // The width of the video in pixels
    unsigned int    videoHeight;            // The height of the video in pixels
    
    double          longerDuration;         // The duration of video or audio, whatever is longer
    Ogre::String    error;                  // This is set to the error that happened
};

enum LogLevel
{
    LOGLEVEL_INVALID = -1,
    LOGLEVEL_MINIMAL,           // Only errors will be logged
    LOGLEVEL_NORMAL,            // Above, plus warnings and important info from the player
    LOGLEVEL_EXCESSIVE,         // Above, plus info about each decoded frame. This one will make your log file pretty big.
    NUM_LOGLEVELS
};

/**
 * Struct that holds one audio frame.
 */
struct AudioFrame
{
    AudioFrame()
        : lifeTime (0.0)
        , data(NULL)
        , dataSize(0)
    {}
    
    AudioFrame(const AudioFrame& other)
    {
        lifeTime = other.lifeTime;
        dataSize = other.dataSize;
        data = new uint8_t[dataSize];
        memcpy(data, other.data, dataSize);
    }
    
    ~AudioFrame()
    {
        if (data != NULL)
        {
            delete [] data;
        }
    }
    
    double          lifeTime;   // How long this frame should last. In seconds.
    uint8_t*        data;
    unsigned int    dataSize;
};

/**
 * Struct that holds one video frame.
 */
struct VideoFrame
{
    VideoFrame()
        : lifeTime (0.0)
        , data(NULL)
        , dataSize(0)
    {}
    
    VideoFrame(const VideoFrame& other)
    {
        lifeTime = other.lifeTime;
        dataSize = other.dataSize;
        data = new uint8_t[dataSize];
        memcpy(data, other.data, dataSize);
    }
    
    ~VideoFrame()
    {
        if (data != NULL)
        {
            delete [] data;
        }
    }
    
    
    double          lifeTime;   // How long this frame should last. In seconds.
    uint8_t*        data;       // The image data
    unsigned int    dataSize;
};

// Helpful defines
#define FFMPEG_PLAYER FFmpegVideoPlayer::getSingletonPtr()

/**
 * This is the main video player class.
 * Interact with this if you want to play videos!
 * 
 * Playback assumes video master for synchronization.
 */
class FFmpegVideoPlayer : public Ogre::FrameListener
{
private:
    /**
     * Constructor.
     */
    FFmpegVideoPlayer();
    
    static FFmpegVideoPlayer* _instance;
    
public:
    /**
     * @return A pointer to the instance of this singleton.
     */
    static FFmpegVideoPlayer* getSingletonPtr()
    {
        if (!_instance)
        {
            _instance = new FFmpegVideoPlayer();
        }
        return _instance;
    }
    
    /**
     * Destructor.
     */
    ~FFmpegVideoPlayer();
    
    /**
     * @param p_log The log the video player shall use. Pass 0 if no logging should be done.
     */
    void setLog(Ogre::Log* p_log);
    
    /**
     * @return  The Ogre log the video player uses for logging.
     */
    Ogre::Log* getLog();
    
    /**
     * @param p_level   The new log level for the player's log.
     */
    void setLogLevel(LogLevel p_level);
    
    /**
     * @return  The log level for the player's log.
     */
    LogLevel getLogLevel() const;
    
    /**
     * @param p_name    The material name to play the video on.
     */
    void setMaterialName(const Ogre::String& p_name);
    
    /**
     * @return The material that is currently being used.
     */
    const Ogre::String& getMaterialName() const;
    
    /**
     * @param p_name    The texture name to play the video on.
     *                  Make sure the material has a texture with that name.
     */
    void setTextureName(const Ogre::String& p_name);
    
    /**
     * @return The texture that is currently being used.
     */
    const Ogre::String& getTextureName() const;
    
    /**
     * @param p_name    The video filename.
     */
    void setVideoFilename(const Ogre::String& p_name);
    
    /**
     * @return The material that is currently being used.
     */
    const Ogre::String& getVideoFilename() const;
    
    /**
     * @return  The VideoInfo object. Use this to read/write information about the video.
     *          This is being updated with video information as soon as the video starts decoding.
     */
    VideoInfo& getVideoInfo();
    
    /**
     * @param p_targetSeconds   How many seconds the player should buffer.
     */
    void setBufferTarget(double p_targetSeconds);
    
    /**
     * @return How many seconds of video the buffer buffers.
     */
    float getBufferTarget() const;
    
    /**
     * @param p_numChannels The number of audio channels FFmpeg will decode to.
     *                      Pass 0 to keep the number of channels of the video source.
     *                      1 for mono, 2 for stereo.
     *                      All other values are not supported and will most likely break something.
     */
    void setForcedAudioChannels(int p_numChannels);
    
    /**
     * @param p_frame   The video frame to add to the end of the buffer.
     */
    void addAudioFrame(AudioFrame* p_frame);
    
    /**
     * @param p_frame   The video frame to add to the end of the buffer.
     */
    void addVideoFrame(VideoFrame* p_frame);
    
    /**
     * @return  Returns true if the audio buffer is full (contains buffer target in seconds).
     */
    bool getAudioBufferIsFull();
    
    /**
     * @return  Returns true if the video buffer is full (contains buffer target in seconds).
     */
    bool getVideoBufferIsFull();
    
    /**
     * @return  True if there currently is a video playing.
     */
    bool getIsPlaying() const;
    
    /**
     * @return True if the video is currently paused. 
     *          Will only return true if a playing video is paused, 
     *          not if there is no video playing at all.
     */
    bool getIsPaused() const;
    
    /**
     * @return  True if the player is currently waiting for the buffers to fill.
     */
    bool getIsWaitingForBuffers() const;
    
    /**
     * Notifies the player that the audio playback is done.
     */
    void setAudioPlaybackDone();
    
    /**
     * @param p_looping If this is true, the video will loop.
     * @note:   Looping requires slightly more memory as the first 0.5 second of the video is 
     *          always stored to be able to start from beginning faster.
     *          IMPORTANT: You MUST call setAudioPlaybackDone when you are done with the audio playback if 
     *                      you want the video to loop.
     */
    void setIsLooping(bool p_looping);
    
    /**
     * @return True if the video palyer is in looping playback.
     */
    bool getIsLooping() const;
    
    /**
     * Starts playing the video.
     * This will decode the video and while doing so, play the video on a material.
     * Audio handling is still up to you.
     * 
     * If you want to take care of the playback all yourself, use startDecoding instead.
     * 
     * @return  True if everything worked correctly.
     */
    bool startPlaying();
    
    /**
     * Starts decoding the video.
     * 
     * Does not do any kind of playback, only fills the audio and video buffers.
     * You have to take care of the playback.
     * 
     * If you want the video to be automatically played on a material (no sound!),
     * you need to use startPlaying().
     * 
     * @param p_leaveFramesIntact   If this is true, the left over video and audio 
     *                              frames will not be deleted. Useful for looping.
     * @return  True if everything worked correctly.
     */
    bool startDecoding(bool p_leaveFramesIntact = false);
    
    /**
     * Pauses the video playback.
     */
    void pauseVideo();
    
    /**
     * Resumes the video playback.
     */
    void resumeVideo();
    
    /**
     * Stops video playback. 
     * This clears all buffers and stops the decoding.
     */
    void stopVideo();
    
    /**
     * Distributes all decoded audio frame data into the passed vectors.
     * As evenly as possible. This means that if you want 4 buffers to be filled, 
     * but only 2 audio frames are decoded, it will still only fill two buffers.
     * @param p_numBuffers             The number of buffers to fill.
     * @param p_outAudioBuffers        The vector to put the buffers into.
     * @param p_outAudioBufferSizes    The size of each buffer in bytes.
     * @param p_outTotalBuffersTime    The total play time of all out buffers.
     * @return  How many buffers could be filled.
     */
    int distributeDecodedAudioFrames(   unsigned int p_numBuffers, 
                                        std::vector<uint8_t*>& p_outAudioBuffers, 
                                        std::vector<unsigned int>& p_outAudioBufferSizes,
                                        double& p_outTotalBuffersTime);
    
    /**
     * This is how you get the video frames to play.
     * READ CAREFULLY:
     *      You have to call this function in your update loop and pass it the time since the last frame.
     *      The function will then make sure you get the correct frame for the current time.
     *      If the time since the last update is very long, some frames will get dropped.
     *      If the time since the last update is not long enough, the last frame will be repeated.
     * @note    Make sure to delete the frame when you are done with it!
     * @param p_time    The time since the last frame. In seconds.
     * @return  The current video frame to play. Or NULL if you are supposed to keep the last returned frame.
     */
    VideoFrame* passVideoTimeAndGetFrame(double p_time);
    
    /**
     * Will check the decoding for errors and update the Ogre material if in playback mode.
     * @param p_evt The frame event. Contains the time since the last frame.
     * @return  True to go ahead, false to abort rendering and drop out of the rendering loop.
     */
    virtual bool frameStarted(const Ogre::FrameEvent& p_evt);
    
private:
    Ogre::String    _materialName;
    Ogre::String    _textureName;
    Ogre::String    _videoFileName;
    VideoInfo       _videoInfo;
    double          _bufferTarget;
    int             _forcedAudioChannels;
    
    bool                        _isPlaying;
    bool                        _isPaused;
    bool                        _isWaitingForBuffers;
    double                      _audioPlaybackTime;
    double                      _videoPlaybackTime;
    bool                        _videoBuffersFilledWithBackup;
    bool                        _isDecoding;
    bool                        _isLooping;
    boost::thread*              _currentDecodingThread;
    boost::mutex*               _playerMutex;
    boost::condition_variable*  _playerCondVar;
    boost::mutex*               _decodingMutex;
    boost::condition_variable*  _decodingCondVar;
    
    double                      _currentAudioStorage;
    double                      _currentAudioBackupStorage;
    std::deque<AudioFrame*>     _audioFrames; 
    std::deque<AudioFrame*>     _backupAudioFrames;
    
    double                      _currentVideoStorage;
    double                      _currentVideoBackupStorage;
    double                      _lastVideoFrameTimeRemaining;   // How much time remains until the next 
                                                                // frame in the queue must be used
    Ogre::TexturePtr            _texturePtr;
    Ogre::String                _originalTextureName;
    Ogre::TextureUnitState*     _originalTextureUnitState;
    std::deque<VideoFrame*>     _videoFrames;
    std::deque<VideoFrame*>     _backupVideoFrames;
    
    int _framesPopped;
    
    Ogre::Log*  _log;
    LogLevel    _logLevel;
};

    
//------------------------------------------------------------------------------
inline 
Ogre::Log* 
FFmpegVideoPlayer::getLog()
{
    return _log;
}

//------------------------------------------------------------------------------
inline
void 
FFmpegVideoPlayer::setLogLevel(LogLevel p_level)
{
    _logLevel = p_level;
}

//------------------------------------------------------------------------------
inline
LogLevel 
FFmpegVideoPlayer::getLogLevel() const
{
    return _logLevel;
}

//------------------------------------------------------------------------------
inline
const Ogre::String& 
FFmpegVideoPlayer::getMaterialName() const
{
    return _materialName;
}

//------------------------------------------------------------------------------
inline
const Ogre::String& 
FFmpegVideoPlayer::getTextureName() const
{
    return _textureName;
}

//------------------------------------------------------------------------------
inline
const Ogre::String& 
FFmpegVideoPlayer::getVideoFilename() const
{
    return _videoFileName;
}

//------------------------------------------------------------------------------
inline
VideoInfo& 
FFmpegVideoPlayer::getVideoInfo()
{
    return _videoInfo;
}

//------------------------------------------------------------------------------
inline
float 
FFmpegVideoPlayer::getBufferTarget() const
{
    return _bufferTarget;
}

//------------------------------------------------------------------------------
inline
void 
FFmpegVideoPlayer::setForcedAudioChannels(int p_numChannels)
{
    _forcedAudioChannels = p_numChannels;
}

//------------------------------------------------------------------------------
inline
bool 
FFmpegVideoPlayer::getIsPlaying() const
{
    return _isPlaying;
}

//------------------------------------------------------------------------------
inline
bool 
FFmpegVideoPlayer::getIsPaused() const
{
    return _isPaused;
}

//------------------------------------------------------------------------------
inline
bool 
FFmpegVideoPlayer::getIsWaitingForBuffers() const
{
    return _isWaitingForBuffers;
}

//------------------------------------------------------------------------------
inline
void 
FFmpegVideoPlayer::setIsLooping(bool p_looping)
{
    _isLooping = p_looping;
}
   
//------------------------------------------------------------------------------
inline
bool 
FFmpegVideoPlayer::getIsLooping() const
{
    return _isLooping;
}

//------------------------------------------------------------------------------
inline 
void 
FFmpegVideoPlayer::pauseVideo()
{
    if (_isPlaying)
    {
        _isPaused = true;
    }
}
  
//------------------------------------------------------------------------------  
inline 
void 
FFmpegVideoPlayer::resumeVideo()
{
    
    if (_isPlaying)
    {
        _isPaused = false;
    }
}

#endif	/* FFMPEGVIDEOPLAYER_H */

