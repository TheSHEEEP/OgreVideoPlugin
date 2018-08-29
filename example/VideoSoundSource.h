/**
 * This is an example class that shows how the OgreVideoPlugin can be used 
 * with OpenAL to play sound from a video.
 * This class is used in a slightly different form in our own project, so I do not guarantee it will work for you.
 * But you should be able to get any info you need from it to create your own openAL playback.
 * @author TheSHEEEP
 */
#ifndef _VideoSoundSource_
#define _VideoSoundSource_

#include <AL/alc.h>
#include <AL/al.h>
#define AL_ALEXT_PROTOTYPES
#include <AL/efx.h>
#undef AL_ALEXT_PROTOTYPES
#include <AL/alc.h>
#include <AL/alext.h>
#define TEST_ERROR(_msg)		\
	error = alGetError();		\
	if (error != AL_NO_ERROR) {	\
		std::cout<<_msg<< "\n";	\
	}



class VideoSoundSource
{
    enum State
    {
        InvalidState = -1,
        Idle,
        Playing,
        Paused,
        NumStates
    };
    
public:
    ALfloat listenerOri[6];
	/**
	 * Constructor.
	 * @param source 	The OpenAL source to use.
	 */
    VideoSoundSource();
	
	/**
	 * Destructor.
	 */
    virtual ~VideoSoundSource(void);

	/**
	 * Starts playing the video.
	 */
    void play();
	
	/**
	 * Stops video playback.
	 */
    void stop();

	/**
	 * Sets the pitch to the passed value.
	 */
    void setPitch(float factor);
	
	/**
	 * Sets the volume to the passed value.
	 */
    void setVolume(float gain);
    
    /**
     * Pause the source.
     */
    void pause();
    
    /**
     * Resume the source.
     */
    void resume();
    
    /**
     * Updates the streaming sounds. Does nothing for non-streaming sounds.
     */
    void update(float timeSinceLastFrame);
    
protected:
    ALuint          _source;
    State           _state;
    
    ALenum  _streamingFormat;
    ALsizei _streamingFrequency;
    double  _streamingBufferTime;
    bool    _delayStreamingPlay;
    double  _playbackTime;

       /**
     * Resume the source.
     */
    
    ALCdevice *device;
	ALvoid *data;

	ALCcontext *context;
	ALsizei size, freq;
	ALuint buffer, source;
	
	ALboolean loop = AL_FALSE;
	ALCenum error;
    ALboolean enumeration;
	const ALCchar *devices;
	const ALCchar *defaultDeviceName ="";

    void setup();
    void list_audio_devices(const ALCchar *devices);
    ALenum to_al_format(short channels, short samples);
};

#endif // #ifndef _VideoSoundSource_
