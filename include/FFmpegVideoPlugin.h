/* 
 * File:   FFmpegPlugin.h
 * Author: TheSHEEEP
 *
 * Created on 10. Januar 2014, 10:32
 * 
 * This plugin allows you to play a video on any material.
 * It also offers an interface to access the video frames in an an Ogre compatible format
 * as well as the audio frames. How you handle the audio is up to you, the plugin only
 * offers the audio data.
 * 
 * The decoding of the video happens buffered. You can specify how many seconds the video
 * should be buffered (default 5 seconds). 
 * 
 * License:
 * This plugin uses the same license as Ogre, namely MIT.
 */

#ifndef FFMPEGPLUGIN_H
#define	FFMPEGPLUGIN_H

#include <Ogre.h>
#include <OgrePlugin.h>

#include "FFmpegVideoPlayer.h"
 
class FFmpegVideoPlugin : public Ogre::Plugin
{
public:
      /**
       * Constructor.
       */
    FFmpegVideoPlugin();
 
    /**
     * @return The name of this plugin.
     */
    const Ogre::String& getName() const;
 
    /**
     * Registers this plugin.
     */
    void install();
 
    /**
     * Initializes this plugin.
     */
    void initialise();
 
    /**
     * Shuts down this plugin.
     */
    void shutdown();
 
    /**
     * Uninstalls this plugin.
     */
    void uninstall();
    
private:
    FFmpegVideoPlayer*  _videoPlayer;
};

#endif	/* FFMPEGPLUGIN_H */

