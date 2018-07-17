<h1>Discontinued!!</h1>
This repo is not maintained any more and I did not use Ogre in years. Meanwhile, FFmpeg has changed a lot recently.<br />
Feel free to create your own repo based on this once, but know that this repo will not see support any more.

<h1>Ogre Video Plugin</h1>
This is an actual Ogre plugin.<br />
It will install the FFmpegVideoPlayer as a frame listener so all you have to do i interact with the singleton.

<h2>What does it do?</h2>
The video player will replace an existing texture in an existing texture unit in an existing material with the video.<br />
This means that you can use a video as input for your already existing shader pipeline!<br />
<br />
As soon as the buffer is filled, playback starts and the original texture unit's texture is replaced by the video.<br />
When the video is finished, the original texture is restored.

But you don't have to use the video player for video playback. you can use it to decode only and take part of the playback all by yourself. <br />See <b>Usage</b> for more. 

<h2>Usage</h2>
Using the video player is rather simple. <br />
First of all, make sure the header is included:
```c++
#include "FFmpegVideoPlayer.h"
```

The basic usage is to first set some options, and then start playing the video.<br />
You can use the <b>FFMPEG_PLAYER</b> define to have a shorter access to the singleton:
```c++
// Set the material name to play on
// IMPORTANT: The material has to exist!
FFMPEG_PLAYER->setMaterialName("VideoMaterial");

// Set the texture unit name to play on
// IMPORTANT: The material above must have a texture unit with that name!
FFMPEG_PLAYER->setTextureUnitName("VideoTextureUnit");

// Set the buffer target for the video player.
// This is the time the buffer will fill before starting playback, and also the time it will always 
// try to hold in the buffers.
// As soon as both the audio and the video buffers hold the target time, decoding is stopped until 
// at least one of the buffers
// hold less than the target time.
FFMPEG_PLAYER->setBufferTarget(1.5);

// Set the name of the video file
FFMPEG_PLAYER->setVideoFilename("MyVideo.avi");


// Should the video loop?
FFMPEG_PLAYER->setIsLooping(false);

// Set the forced number of audio channels
// If this is not set, the number of channels of the video file will be used (which will in most 
// cases be stereo)
// You can set this to 0 manually to force using the video's number of channels.
// Any number higher than 2 will most likely break something
// IMPORTANT: OpenAL needs to have mono sound for 3D audio effects
FFMPEG_PLAYER->setForcedAudioChannels(1);

// Set the log level of the player's own log file.
FFMPEG_PLAYER->setLogLevel(LOGLEVEL_NORMAL);

// Play the video
// Remember: this is video only, no audio
if (!FFMPEG_PLAYER->startPlaying())
{
    // print error
}
```

<h2>What about audio?</h2>
The video player itself does only decode the audio frames and encode them into non-planar float format (AV_SAMPLE_FMT_FLT in FFmpeg).<br />
It does not play the audio in any way. You will have to take care of that.<br />
The player offers a function that will fill some buffers with decoded audio frames for you:
```c++
int distributeDecodedAudioFrames(   unsigned int p_numBuffers, 
                                    std::vector<uint8_t*>& p_outAudioBuffers, 
                                    std::vector<unsigned int>& p_outAudioBufferSizes,
                                    double& p_outTotalBuffersTime);
```

I've written the player with OpenAL in mind and it works fine with that, so I put the class we use in our project (slightly changed) as an example into the repository. <br />
But it should also be possible to use the above function to play the audio with another library.

<h2>What video formats are supported?</h2>
That really depends on how you built FFmpeg.<br />
As we will be using it to play OGG (vorbis & theora) files, I've added those to the CMake script for easier access. <br />
But you can use your own additions to FFmpeg.<br />
<br />
I have tested a video in the following container formats successfully:<br />
avi, flv, ogv, mp4, 3gp, webm, mpg<br />
<br />
The following did not work in my tests:<br />
wmv (but who needs that, anyway?)

<h2>Can multiple videos be played at once?</h2>
Not by using the FFMPEG_PLAYER define. Each class of the FFmpegVideoPlayer can only play one video at a time.<br />
But I have made the constructor of that class public (it is not really a singleton, I know). <br />
So you should be able to create as many FFmpegVideoPlayers as you want to play videos. <br />
I have not tested this, though, so I do not guarantee anything.

<h2>License - MIT</h2>
The MIT License (MIT)

Copyright (c) 2014 Jan "TheSHEEEP" Drabner (jan@jdrabner.eu)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
