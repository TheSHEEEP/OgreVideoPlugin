#include "FFmpegVideoPlugin.h"

#include <OgreLogManager.h>

const Ogre::String sPluginName = "FFmpeg Video Plugin";

//------------------------------------------------------------------------------
FFmpegVideoPlugin::FFmpegVideoPlugin()
    : _videoPlayer(NULL)
{
    
}

//------------------------------------------------------------------------------
const Ogre::String& 
FFmpegVideoPlugin::getName() const
{
  return sPluginName;
}

//------------------------------------------------------------------------------
void 
FFmpegVideoPlugin::install()
{
    _videoPlayer = FFmpegVideoPlayer::getSingletonPtr();
}

//------------------------------------------------------------------------------
void 
FFmpegVideoPlugin::initialise()
{
    // Add as a frame listener
    Ogre::Root::getSingletonPtr()->addFrameListener(_videoPlayer);
    
    // Create and attach the log
    _videoPlayer->setLog(Ogre::LogManager::getSingletonPtr()->createLog("FFmpegVideoPlayer.log"));
}

//------------------------------------------------------------------------------
void 
FFmpegVideoPlugin::shutdown()
{
    Ogre::LogManager::getSingletonPtr()->destroyLog("FFmpegVideoPlayer.log");
    Ogre::Root::getSingletonPtr()->removeFrameListener(_videoPlayer);
}

//------------------------------------------------------------------------------
void 
FFmpegVideoPlugin::uninstall()
{
}
