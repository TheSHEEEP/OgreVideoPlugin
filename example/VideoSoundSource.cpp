/**
 * @author Jan "TheSHEEEP" Drabner
 */
#include "VideoSoundSource.h"

#include <vector>

#include "FFmpegVideoPlayer.h"
#include <AL/alext.h>

using namespace Ogre;

//--------------------------------------------------------------------------------------------------
VideoSoundSource::VideoSoundSource(ALuint source)
    : _source(source)
    , _state(Idle)
    , _streamingFormat(-1)
    , _streamingFrequency(-1)
    , _streamingBufferTime(0.0)
    , _delayStreamingPlay(false)
    , _playbackTime(0.0)
{
	 alGenSources(1, &source);
}

//--------------------------------------------------------------------------------------------------
VideoSoundSource::~VideoSoundSource(void)
{
    // Clear buffers
	// Stop so that we can remove the buffers
	alSourceRewind(_source);
	ALenum success = alGetError();
	if(success != AL_NO_ERROR)
	{
		std::cout<<"Error in :~VideoSoundSource(void)"<<std::endl;
		return;
	}
	alSourceStop(_source);
	success = alGetError();
	if(success != AL_NO_ERROR)
	{
		std::cout<<"Error in :~VideoSoundSource(void)2"<<std::endl;
		return;
	}

	// Remove the buffers
	alSourcei(_source, AL_BUFFER, 0);
	success = alGetError();
	if(success != AL_NO_ERROR)
	{
		std::cout<<"Error in :~VideoSoundSource(void)3"<<std::endl;
		return;
	}
	
    alDeleteSources(1, &_source);
}

//--------------------------------------------------------------------------------------------------
void
VideoSoundSource::play()
{
	// Get the format and the frequency.
	if (_streamingFormat == -1 && _streamingFrequency == -1)
	{
		// Get the target format
		switch(FFMPEG_PLAYER->getVideoInfo().audioNumChannels)
		{
		case 1:
			_streamingFormat = AL_FORMAT_MONO16;
			break;
		case 2:
			_streamingFormat = AL_FORMAT_STEREO16;
			break;
		}
		
		// Use Float32 if possible!
		if (alIsExtensionPresent("AL_EXT_FLOAT32"))
		{
			std::cout<<"FLOAT32 extension is present."<<std::endl;
			switch(FFMPEG_PLAYER->getVideoInfo().audioNumChannels)
			{
			case 1:
				_streamingFormat = AL_FORMAT_MONO_FLOAT32;
				break;
			case 2:
				_streamingFormat = AL_FORMAT_STEREO_FLOAT32;
				break;
			}
		}

		// Get the frequency 
		// (this is the sample rate, why does OpenAL call it different than the rest of the world?)
		_streamingFrequency = FFMPEG_PLAYER->getVideoInfo().audioSampleRate;
	}

	// If the video player's buffers are not filled, we delay the playback
	if (FFMPEG_PLAYER->getIsWaitingForBuffers())
	{
		_delayStreamingPlay = true;
		return;
	}
	
	_playbackTime = 0.0;
	_streamingBufferTime = 0.0;
	
	// Decoded buffers are filled, so let's init the OpenAL buffers
	// Generate the buffers, 4 seems a fine number
	int numBuffers = 4;
	ALuint buffers[4];
	alGenBuffers(numBuffers, buffers);
	ALenum success = alGetError();
	if(success != AL_NO_ERROR)
	{
		std::cout<<"Error in :play()"<<std::endl;
		return;
	}
	
	// Fill a number of data buffers with audio from the stream
	std::vector<uint8_t*> audioBuffers;
	std::vector<unsigned int> audioBufferSizes;
	double buffersPlayTime = 0.0;
	unsigned int numFilled = 
		FFMPEG_PLAYER->distributeDecodedAudioFrames(numBuffers, audioBuffers, audioBufferSizes, buffersPlayTime);
	_streamingBufferTime = buffersPlayTime;
	
	// Assign the data buffers to the OpenAL buffers
	for (unsigned int i = 0; i < numFilled; ++i)
	{
		// Fill the buffer with data
		alBufferData(buffers[i], _streamingFormat, audioBuffers[i], audioBufferSizes[i], _streamingFrequency);
		
		// Free the temporary buffer - the data was copied to OpenAL
		 delete [] audioBuffers[i];
		
		success = alGetError();
		if(success != AL_NO_ERROR)
		{
			std::cout<<"Error in :play()1"<<std::endl;
			return;
		}
	}
	
	// Queue the buffers into OpenAL
	alSourceQueueBuffers(_source, numFilled, buffers);
	success = alGetError();
	if(success != AL_NO_ERROR)
	{
			std::cout<<"Error in :play()2"<<std::endl;
		return;
	}
    
    alSourcePlay(_source);
    _state = Playing;
}

//--------------------------------------------------------------------------------------------------
void
VideoSoundSource::stop()
{
    alSourceStop(_source);
    _state = Idle;
}

//--------------------------------------------------------------------------------------------------
void
VideoSoundSource::setPitch(float factor)
{
    alSourcef(_source, AL_PITCH, factor);
}

//--------------------------------------------------------------------------------------------------
void
VideoSoundSource::setVolume(float gain)
{
    alSourcef(_source, AL_GAIN, gain);
}

//--------------------------------------------------------------------------------------------------
void 
VideoSoundSource::pause()
{
    alSourcePause(_source);
    _state = Paused;
}
    
//--------------------------------------------------------------------------------------------------
void 
VideoSoundSource::resume()
{
    if (_state == Paused)
    {
        alSourcePlay(_source);
        _state = Playing;
    }
}

//--------------------------------------------------------------------------------------------------
void 
VideoSoundSource::update(float timeSinceLastFrame)
{
	// Make sure no further decoding is done and pause the source
	// This is a fallback.
	// It is much better (especially for synchronization) to call pause()/resume() on the source
	if (FFMPEG_PLAYER->getIsPaused() && _state != Paused)
	{
		alSourcePause(_source);
		_state = Paused;
	}
	else if (FFMPEG_PLAYER->getIsPlaying() && !FFMPEG_PLAYER->getIsPaused() && _state == Paused)
	{
		alSourcePlay(_source);
		_state = Playing;
	}
	
	// Do not update when paused
	if (_state == Paused)
	{
		return;
	}
	
	// It is possible that we are still waiting for the streaming buffers of the
	// video player to be filled. If so, start playing here once they are filled.
	if (_delayStreamingPlay)
	{
		// The video player will only start playing initially when the buffers are 
		// filled, so check for that
		if (FFMPEG_PLAYER->getIsPlaying())
		{
			_delayStreamingPlay = false;
			play();
		}
		return;
	}
	
	_playbackTime += timeSinceLastFrame;
	
	// It is possible that we are done
	if (_playbackTime >= FFMPEG_PLAYER->getVideoInfo().audioDuration)
	{
		// Notify the video player that sound playback is finished
		// This is required so that the player can restart decoding if in looping mode
		FFMPEG_PLAYER->setAudioPlaybackDone();
		
		// If this is not looping, stop the sound
		if (!FFMPEG_PLAYER->getIsLooping())
		{
			alSourceStop(_source);
			return;
		}
		// If we are looping, destroy the buffers
		else
		{
			// Stop so that we can remove the buffers
			alSourceRewind(_source);
			ALenum success = alGetError();
			if(success != AL_NO_ERROR)
			{
				// print error
				return;
			}
			alSourceStop(_source);
			success = alGetError();
			if(success != AL_NO_ERROR)
			{
				// print error
				return;
			}
			
			// Remove the buffers
			alSourcei(_source, AL_BUFFER, 0);
			success = alGetError();
			if(success != AL_NO_ERROR)
			{
				// print error
				return;
			}
			
			// Start playing again
			play();
			return;
		}
	}
	
	// If we still have more than 1.5 second of additional time in the OpenAL buffers
	// do not try to refill them.
	// IMPORTANT: If you do not do this, you will waste a lot of memory, because OpenAL 
	// requests buffers much faster than it plays them, so the audio buffer of the video
	// player will be emptied MUCH faster than the video buffer, which will lead to the video
	// buffer becoming pretty gigantic -> wasting memory
	if ((_streamingBufferTime - _playbackTime) > 1.50)
	{
		return;
	}
	
	ALint numBuffersProcessed;

	// Check if OpenAL is done with any of the queued buffers
	alGetSourcei(_source, AL_BUFFERS_PROCESSED, &numBuffersProcessed);
	if(numBuffersProcessed <= 0)
		return;
	
	// Fill a number of data buffers with audio from the stream
	std::vector<uint8_t*> audioBuffers;
	std::vector<unsigned int> audioBufferSizes;
	double buffersPlayTime = 0.0;
	unsigned int numFilled = 
		FFMPEG_PLAYER->distributeDecodedAudioFrames(numBuffersProcessed, audioBuffers, audioBufferSizes, buffersPlayTime);
	_streamingBufferTime += buffersPlayTime;
	
	// Assign the data buffers to the OpenAL buffers
	ALuint buffer;
	for (unsigned int i = 0; i < numFilled; ++i)
	{
		// Pop the oldest queued buffer from the source, 
		// fill it with the new data, then re-queue it
		alSourceUnqueueBuffers(_source, 1, &buffer);
		
		ALenum success = alGetError();
		if(success != AL_NO_ERROR)
		{
				std::cout<<"Error in :alSourceUnqueueBuffers"<<std::endl;
			return;
		}
		
		alBufferData(buffer, _streamingFormat, audioBuffers[i], audioBufferSizes[i], _streamingFrequency);
		
		success = alGetError();
		if(success != AL_NO_ERROR)
		{
				std::cout<<"Error in :alBufferData"<<std::endl;
			return;
		}
		
		alSourceQueueBuffers(_source, 1, &buffer);
		
		// Free the temporary buffer - the data was copied to OpenAL
		delete [] audioBuffers[i];
		
		success = alGetError();
		if(success != AL_NO_ERROR)
		{
				std::cout<<"Error in :alSourceQueueBuffers"<<std::endl;
			return;
		}
	}
	
	// Make sure the source is still playing, 
	// and restart it if needed.
	ALint playStatus;
	alGetSourcei(_source, AL_SOURCE_STATE, &playStatus);
	if(playStatus != AL_PLAYING)
		alSourcePlay(_source);
}

