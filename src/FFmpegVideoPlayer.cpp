/* 
 * File:   FFmpegVideoPlayer.cpp
 * Author: TheSHEEEP
 * 
 * Created on 10. Januar 2014, 10:57
 */

#include "FFmpegVideoPlayer.h"

#include <OgreLogManager.h>
#include <OgreMaterialManager.h>
#include <OgreMaterial.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreHardwarePixelBuffer.h>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>

//------------------------------------------------------------------------------
VideoInfo::VideoInfo()
    : infoFilled(false)
    , decodingDone(false)
    , decodingAborted(false)
    , audioDuration(0.0) 
    , audioDecodedDuration(0.0)
    , audioSampleRate(0)
    , audioBitRate(0)
    , audioNumChannels(0)
    , videoDecodedDuration(0.0)
    , videoDuration(0.0) 
    , videoWidth(0)
    , videoHeight(0)
    , longerDuration(0.0)
    , error("")
{ 
}

FFmpegVideoPlayer* FFmpegVideoPlayer::_instance = NULL;  
//------------------------------------------------------------------------------
FFmpegVideoPlayer::FFmpegVideoPlayer() 
    : _materialName("")
    , _textureUnitName("")
    , _videoFileName("")
    , _isPlaying(false)
    , _isPaused(false)
    , _isWaitingForBuffers(false)
    , _audioPlaybackTime(0.0)
    , _videoPlaybackTime(0.0)
    , _videoBuffersFilledWithBackup(false)
    , _isDecoding(false)
    , _isLooping(false)
    , _bufferTarget(1.5)
    , _forcedAudioChannels(0)
    , _currentDecodingThread(NULL)
    , _playerMutex(NULL)
    , _playerCondVar(NULL)
    , _decodingMutex(NULL)
    , _decodingCondVar(NULL)
    , _currentAudioStorage(0.0)
    , _currentAudioBackupStorage(0.0)
    , _currentVideoStorage(0.0)
    , _currentVideoBackupStorage(0.0)
    , _lastVideoFrameTimeRemaining(0.0)
    , _originalTextureName("")
    , _originalTextureUnitState(NULL)
    , _framesPopped(0)
    , _log(NULL)
    , _logLevel(LOGLEVEL_NORMAL)
{
    _playerMutex = new boost::mutex();
    _playerCondVar = new boost::condition_variable();
    _decodingMutex = new boost::mutex();
    _decodingCondVar = new boost::condition_variable();
}

//------------------------------------------------------------------------------
FFmpegVideoPlayer::~FFmpegVideoPlayer() 
{
    // Delete old thread and thread info object
    if (_currentDecodingThread != NULL)
    {
        _currentDecodingThread->join();
        delete _currentDecodingThread;
        _currentDecodingThread = NULL;
    }
    
    // Delete old frames
    for (unsigned int i = 0; i < _audioFrames.size(); ++i)
    {
        delete _audioFrames[i];
    }
    _audioFrames.clear();
    for (unsigned int i = 0; i < _videoFrames.size(); ++i)
    {
        delete _videoFrames[i];
    }
    _videoFrames.clear();
    
    // Delete sync objects
    if (_playerMutex != NULL)
    {
        delete _playerMutex;
        delete _playerCondVar;
        delete _decodingMutex;
        delete _decodingCondVar;
    }
}

//------------------------------------------------------------------------------
void 
FFmpegVideoPlayer::setLog(Ogre::Log* p_log)
{
    boost::mutex::scoped_lock lock(*_playerMutex);
    _log = p_log;
}

//------------------------------------------------------------------------------
void 
FFmpegVideoPlayer::setMaterialName(const Ogre::String& p_name)
{
    if (!_isPlaying)
    {
        _materialName = p_name;
    }
}

//------------------------------------------------------------------------------
void 
FFmpegVideoPlayer::setTextureUnitName(const Ogre::String& p_name)
{
    if (!_isPlaying)
    {
        _textureUnitName = p_name;
    }
}

//------------------------------------------------------------------------------
void 
FFmpegVideoPlayer::setVideoFilename(const Ogre::String& p_name)
{
    if (!_isDecoding)
    {
        _videoFileName = p_name;
    }
}

//------------------------------------------------------------------------------
void 
FFmpegVideoPlayer::setBufferTarget(double p_targetSeconds)
{
    if (!_isDecoding)
    {
        _bufferTarget = p_targetSeconds;
    }
}

//------------------------------------------------------------------------------
void 
FFmpegVideoPlayer::addAudioFrame(AudioFrame* p_frame)
{
    boost::mutex::scoped_lock lock(*_playerMutex);
    _audioFrames.push_back(p_frame);
    _currentAudioStorage += p_frame->lifeTime;
    
    // Create backup for first 0.5 seconds if looping
    if (_isLooping && _currentAudioBackupStorage < 0.5)
    {
        _backupAudioFrames.push_back(new AudioFrame(*p_frame));
        _currentAudioBackupStorage += p_frame->lifeTime;
        
        if (_currentAudioBackupStorage > 1.0)
        {
            int i = 0;
            ++i;
        }
    }
}

//------------------------------------------------------------------------------
void 
FFmpegVideoPlayer::addVideoFrame(VideoFrame* p_frame)
{
    boost::mutex::scoped_lock lock(*_playerMutex);
    _videoFrames.push_back(p_frame);
    _currentVideoStorage += p_frame->lifeTime;
    
    // Create backup for first 0.5 seconds if looping
    if (_isLooping && _currentVideoBackupStorage < 0.5)
    {
        _backupVideoFrames.push_back(new VideoFrame(*p_frame));
        _currentVideoBackupStorage += p_frame->lifeTime;
    }
}

//------------------------------------------------------------------------------
bool 
FFmpegVideoPlayer::getAudioBufferIsFull()
{
    boost::mutex::scoped_lock lock(*_playerMutex);
    return _currentAudioStorage >= _bufferTarget;
}

//------------------------------------------------------------------------------
bool 
FFmpegVideoPlayer::getVideoBufferIsFull()
{
    boost::mutex::scoped_lock lock(*_playerMutex);
    return _currentVideoStorage >= _bufferTarget;
}

//------------------------------------------------------------------------------
void 
FFmpegVideoPlayer::setAudioPlaybackDone()
{
    boost::mutex::scoped_lock lock(*_playerMutex);
    
    // set time to maximum
    _audioPlaybackTime = _videoInfo.audioDuration;
    
    if (_log && _logLevel >= LOGLEVEL_NORMAL) 
    {
        _log->logMessage("Audio playback reported done. ");
    }
    
    // Update the longerDuration as it is possible that the audio duration changed
    _videoInfo.longerDuration = _videoInfo.videoDuration > _videoInfo.audioDuration ? 
                                _videoInfo.videoDuration : _videoInfo.audioDuration;
    
    // If looping, apply the backup audio buffer here
    if (_isLooping)
    {
        for (unsigned int i = 0; i < _audioFrames.size(); ++i)
        {
            delete _audioFrames[i];
        }
        _audioFrames.clear();
        
        for (unsigned int i = 0; i < _backupAudioFrames.size(); ++i)
        {
            _currentAudioStorage += _backupAudioFrames[i]->lifeTime;
            _audioFrames.push_back(new AudioFrame(*_backupAudioFrames[i]));
        }
        
        if (_log && _logLevel >= LOGLEVEL_NORMAL) 
            _log->logMessage("Added audio backup to storage.");
    }
}


//------------------------------------------------------------------------------
bool  
FFmpegVideoPlayer::startPlaying()
{
    // Start decoding first
    bool decoding = startDecoding();

    // Sanity checks
    if (!decoding)
    {
        if (_log && _logLevel >= LOGLEVEL_MINIMAL) 
             _log->logMessage("Can't play video. Decoding failed.", Ogre::LML_CRITICAL);
        return false;
    }
    if (_isPlaying)
    {
        if (_log && _logLevel >= LOGLEVEL_MINIMAL) 
            _log->logMessage("Can't play another video. Video is already playing.", Ogre::LML_CRITICAL);
        return false;
    }
    if (_materialName == "" || _textureUnitName == "")
    {
        if (_log && _logLevel >= LOGLEVEL_MINIMAL) 
            _log->logMessage("Can't play video. No material, texture or resource group specified.", Ogre::LML_CRITICAL);
        return false;
    }
    
    // Get the material
    Ogre::MaterialPtr matPtr = Ogre::MaterialManager::getSingleton().getByName(_materialName);
    if (matPtr.isNull())
    {
        if (_log && _logLevel >= LOGLEVEL_MINIMAL) 
            _log->logMessage("Can't play video. Material " + _materialName + " not found.", Ogre::LML_CRITICAL);
        return false;
    }
    
    // Create a new texture for our video
    Ogre::TextureManager::getSingleton().remove("FFmpegVideoTexture");
    _texturePtr = Ogre::TextureManager::getSingleton().createManual(
                    "FFmpegVideoTexture",
                    Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
                    Ogre::TEX_TYPE_2D,
                    _videoInfo.videoWidth, _videoInfo.videoWidth,
                    0,
                    Ogre::PF_BYTE_RGBA,
                    Ogre::TU_DYNAMIC_WRITE_ONLY_DISCARDABLE);
    
    // Now look for the texture
    bool found = false;
    unsigned short numTechniques = matPtr->getNumTechniques();
    for (unsigned short i = 0; i < numTechniques; ++i)
    {
        Ogre::Technique* tech = matPtr->getTechnique(i);
        unsigned short numPasses = tech->getNumPasses();
        for (unsigned short j = 0; j < numPasses; ++j)
        {
            Ogre::Pass* pass = tech->getPass(j);
            unsigned short numTUs = pass->getNumTextureUnitStates();
            for (unsigned short k = 0; k < numTUs; ++k)
            {
                Ogre::TextureUnitState* tu = pass->getTextureUnitState(k);
                
                // Is this our texture?
                if (tu->getName() == _textureUnitName)
                {
                    _originalTextureUnitState = tu;
                    _originalTextureName = tu->getTextureName();
                    found = true;
                    
                    if (_log && _logLevel >= LOGLEVEL_NORMAL)
                        _log->logMessage("Successfully found texture unit " 
                                        + _textureUnitName + " inside material " 
                                        + _materialName + ".", Ogre::LML_NORMAL);
                }
            }
            
            if (found) break;
        }
        
        if (found) break;
    }
    
    _isWaitingForBuffers = true;
    return true;
}

//------------------------------------------------------------------------------
bool 
FFmpegVideoPlayer::startDecoding(bool p_leaveFramesIntact)
{
    // Sanity checks
    if (_isDecoding)
    {
        if (_log && _logLevel >= LOGLEVEL_MINIMAL) 
             _log->logMessage("Can't decode another video. Video is already decoding.", Ogre::LML_CRITICAL);
        return false;
    }
    if (_videoFileName == "")
    {
        if (_log && _logLevel >= LOGLEVEL_MINIMAL) 
             _log->logMessage("Can't decode video. No video filename specified.", Ogre::LML_CRITICAL);
        return false;
    }
    
    // Delete old thread and thread info object
    if (_currentDecodingThread != NULL)
    {
        _currentDecodingThread->join();
        delete _currentDecodingThread;
        _currentDecodingThread = NULL;
    }
    
    // Delete remaining frames
    if (!p_leaveFramesIntact)
    {
        for (unsigned int i = 0; i < _audioFrames.size(); ++i)
        {
            delete _audioFrames[i];
        }
        _audioFrames.clear();
        for (unsigned int i = 0; i < _videoFrames.size(); ++i)
        {
            delete _videoFrames[i];
        }
        _videoFrames.clear();
        
        _currentAudioStorage = 0.0;
        _currentVideoStorage = 0.0;
    }
    
    // Reset variables
    _videoInfo.audioNumChannels = _forcedAudioChannels > 0 ? _forcedAudioChannels : 0;
    _lastVideoFrameTimeRemaining = 0.0;
    _videoBuffersFilledWithBackup = false;
    _audioPlaybackTime = 0.0;
    _videoPlaybackTime = 0.0;
    _videoInfo.decodingDone = false;
    _videoInfo.decodingAborted = false;
    _videoInfo.audioDecodedDuration = 0.0;
    _videoInfo.videoDecodedDuration = 0.0;
    _isDecoding = false;
    _isPaused = false;
    
    // If we are in looping mode and currently playing, this means that this is loop X
    // So we do not need to reset all values
    if (!_isLooping && !_isPlaying)
    {
        _videoInfo.audioDuration = 0.0;
        _videoInfo.videoDuration = 0.0;
        _videoInfo.infoFilled = false;
        _originalTextureName = 0.0;
        _originalTextureUnitState = NULL;
    }
    
    // Create thread info object - it is deleted inside the decoding thread
    ThreadInfo* threadInfo = new ThreadInfo();
    threadInfo->playerMutex = _playerMutex;
    threadInfo->playerCondVar = _playerCondVar;
    threadInfo->videoPlayer = this;
    threadInfo->decodingMutex = _decodingMutex;
    threadInfo->decodingCondVar = _decodingCondVar;
    threadInfo->isLoop = _isLooping && _isPlaying;
    
    // Start decoding thread, then wait until the VideoInfo object was filled
    _currentDecodingThread = new boost::thread(videoDecodingThread, threadInfo);
    {
        boost::mutex::scoped_lock lock(*_playerMutex);
        while (!_videoInfo.infoFilled || _videoInfo.error.length() > 0)
        {
            boost::chrono::steady_clock::time_point const timeOut = 
                boost::chrono::steady_clock::now() + boost::chrono::milliseconds(3000);
            _playerCondVar->wait_until(lock, timeOut);
        }
        
        // Do we have an error?
        if (_videoInfo.error.length() > 0)
        {
            if (_log && _logLevel >= LOGLEVEL_MINIMAL) 
             _log->logMessage("Decoding error: " + _videoInfo.error, Ogre::LML_CRITICAL);
            return false;
        }
    }
    
    _isDecoding = true;
    return true;
}

//------------------------------------------------------------------------------
void 
FFmpegVideoPlayer::stopVideo()
{
    // Abort decoding
    _videoInfo.decodingAborted = true;
    _decodingCondVar->notify_all();
    _currentDecodingThread->join();
    
    // Restore original texture
    _originalTextureUnitState->setTextureName(_originalTextureName);
    
    // Stop playback
    _isDecoding = false;
    _isPlaying = false;
    
    // Clear frames
    for (unsigned int i = 0; i < _audioFrames.size(); ++i)
    {
        delete _audioFrames[i];
    }
    _audioFrames.clear();
    for (unsigned int i = 0; i < _videoFrames.size(); ++i)
    {
        delete _videoFrames[i];
    }
    _videoFrames.clear();
}

//------------------------------------------------------------------------------
int 
FFmpegVideoPlayer::distributeDecodedAudioFrames(unsigned int p_numBuffers, 
                                                std::vector<uint8_t*>& p_outAudioBuffers, 
                                                std::vector<unsigned int>& p_outAudioBufferSizes,
                                                double& p_outTotalBuffersTime)
{
    boost::mutex::scoped_lock lock(*_playerMutex);
    
    // Get the actual number of buffers to fill
    unsigned int numBuffers = 
        _audioFrames.size() >= p_numBuffers? p_numBuffers : _audioFrames.size();
    
    if (numBuffers == 0) return 0;
    
    // Calculate the number of audio frames per buffer, and special for the
    // last buffer as there may be a rest
    int numFramesPerBuffer = _audioFrames.size() / numBuffers;
    int numFramesRest = _audioFrames.size() % numBuffers;
    
    // Fill each buffer
    double totalLifeTime;
    AudioFrame* frame;
    std::vector<AudioFrame*> frames;
    for (unsigned int i = 0; i < numBuffers; ++i)
    {
        // Get all frames for this buffer to count the lifeTime and data size
        unsigned int dataSize = 0;
        totalLifeTime = 0.0;
        for (unsigned int j = 0; j < numFramesPerBuffer; ++j)
        {
            frame = _audioFrames.front();
            _audioFrames.pop_front();
            frames.push_back(frame);
            
            totalLifeTime += frame->lifeTime;
            dataSize += frame->dataSize;
        }
        
        // Append the rest to the last buffer
        if (i == numBuffers - 1)
        {
            for (unsigned int j = 0; j < numFramesRest; ++j)
            {
                frame = _audioFrames.front();
                _audioFrames.pop_front();
                frames.push_back(frame);

                totalLifeTime += frame->lifeTime;
                dataSize += frame->dataSize;
            }
        }
        
        _currentAudioStorage -= totalLifeTime;
        p_outTotalBuffersTime += totalLifeTime;
        
        // Create the buffer
        uint8_t* buffer = new uint8_t[dataSize];
        
        // Concatenate frames into a single memory target
        uint8_t* destination = buffer;
        for (unsigned int j = 0; j < frames.size(); ++j)
        {
            memcpy(destination, frames[j]->data, frames[j]->dataSize);
            destination += frames[j]->dataSize;
        }
        
        // Delete used frames
        for (unsigned int j = 0; j < frames.size(); ++j)
        {
            delete frames[j];
        }
        frames.clear();
        
        // Store buffer and size in return values
        p_outAudioBuffers.push_back(buffer);
        p_outAudioBufferSizes.push_back(dataSize);
    }
        
    if (_log && _logLevel >= LOGLEVEL_MINIMAL) 
        _log->logMessage("Distributed " + boost::lexical_cast<std::string>(p_outTotalBuffersTime)
                            + " seconds to " + boost::lexical_cast<std::string>(numBuffers) 
                            + " buffers.", Ogre::LML_CRITICAL);
        
    // We got at least one new frame, wake up the decoder for more decoding
    _decodingCondVar->notify_all();
    
    return numBuffers;
}

//------------------------------------------------------------------------------
VideoFrame* 
FFmpegVideoPlayer::passVideoTimeAndGetFrame(double p_time)
{
    _lastVideoFrameTimeRemaining -= p_time;
    
    // If we do not need a new frame, just return NULL
    if (_lastVideoFrameTimeRemaining > 0.0)
    {
        return NULL;
    }
    // We have passed at least the last frame
    else
    {
        double timeToPass = - _lastVideoFrameTimeRemaining;
        
        // Get frames until we have passed the required time
        VideoFrame* frame = NULL;
        while (timeToPass > 0.0)
        {
            // Delete skipped frame
            if (frame != NULL)
            {
                delete frame;
                frame = NULL;
            }
            
            // No more frames? We're done!
            if (_videoFrames.empty())
            {
                if (_log && _logLevel >= LOGLEVEL_NORMAL) 
                    _log->logMessage("No more frames left in passVideoTime.", Ogre::LML_NORMAL);
                return NULL;
            }
            
            // Get new frame
            {
                boost::mutex::scoped_lock lock(*_playerMutex);
                frame = _videoFrames.front();
                _videoFrames.pop_front();
                _currentVideoStorage -= frame->lifeTime;
                _framesPopped++;
            }
            timeToPass -= frame->lifeTime;
        }
        
        // We got at least one new frame, wake up the decoder for more decoding
        _decodingCondVar->notify_all();
        
        // We got the correct frame, now set the lifetime to the frame's lifetime
        // minus what has already passed from it. Which just happens to be -timeToPass
        _lastVideoFrameTimeRemaining = -timeToPass;
        return frame;
    }
    
    // We actually should never get here
    return NULL;
}

//------------------------------------------------------------------------------
bool 
FFmpegVideoPlayer::frameStarted(const Ogre::FrameEvent& p_evt)
{
    double timeSinceLast = p_evt.timeSinceLastFrame;
    
    // Check for errors
    if (_isDecoding)
    {
        if (_videoInfo.error.size() > 0)
        {
            if (_log && _logLevel >= LOGLEVEL_MINIMAL) 
                _log->logMessage("Decoding error: " + _videoInfo.error, Ogre::LML_CRITICAL);
            _currentDecodingThread->join();
            return false;
        }
    }
    
    // Waiting for the buffers to be filled initially
    if (_isWaitingForBuffers)
    {
        // If the buffers are not yet filled, try again next frame
        if (!getAudioBufferIsFull() || !getVideoBufferIsFull())
        {
            return true;
        }
        
        // Buffers are filled, so replace the texture and start playing
        _originalTextureUnitState->setTextureName("FFmpegVideoTexture");
        if (_log && _logLevel >= LOGLEVEL_NORMAL) 
             _log->logMessage("Replacing texture " + _originalTextureName + " with video texture.");
        
        _isPlaying = true;
        _isPaused = false;
        _isWaitingForBuffers = false;
    }
    
    // Update the texture we play on, if we are in playback mode and not paused
    if (_isPlaying && !_isPaused)
    {
        VideoFrame* frame = passVideoTimeAndGetFrame(timeSinceLast);
        if (frame != NULL)
        {
            Ogre::PixelBox pb(_videoInfo.videoWidth, _videoInfo.videoHeight, 1, Ogre::PF_BYTE_RGBA, frame->data);
            Ogre::HardwarePixelBufferSharedPtr buffer = _texturePtr->getBuffer();
            buffer->blitFromMemory(pb);
            
            // We're done with the frame and need to delete it
            delete frame;
        }
        
        // Stop when we're done with the video
        _videoPlaybackTime += timeSinceLast;
        if (_videoPlaybackTime >= _videoInfo.longerDuration)
        {
            // Stop playing when not looping
            _isDecoding = false;
            if (!_isLooping)
            {
                // Restore the texture's original state
                _originalTextureUnitState->setTextureName(_originalTextureName);

                _isPlaying = false;
            }
            // If we loop, restart the decoding
            else
            {
                if (!_videoBuffersFilledWithBackup)
                {
                    // Clean up video frames
                    for (unsigned int i = 0; i < _videoFrames.size(); ++i)
                    {
                        delete _videoFrames[i];
                    }
                    _videoFrames.clear();

                    // Apply video backup buffer
                    for (unsigned int i = 0; i < _backupVideoFrames.size(); ++i)
                    {
                        _currentVideoStorage += _backupVideoFrames[i]->lifeTime;
                        _videoFrames.push_back(new VideoFrame(*_backupVideoFrames[i]));
                    }
                    
                    if (_log && _logLevel >= LOGLEVEL_NORMAL) 
                        _log->logMessage("Added video backup to storage.");
                    
                    _videoBuffersFilledWithBackup = true;
                }
                
                // Restart decoding if the audio is also finished playing
                if (_audioPlaybackTime >= _videoInfo.audioDuration)
                {
                    if (_log && _logLevel >= LOGLEVEL_NORMAL) 
                        _log->logMessage("Both video and audio playback finished. Restart decoding. ");
                    
                    // Restart decoding, keeping the audio and video frames intact (as we just refilled them with backup)
                    startDecoding(true);
                }
            }
        }
    }
    
    return true;
}

