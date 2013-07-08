#include "PlatformClient.h"
#include "AudioFileReader.h"
#include "GstAudioDevice.h"

#include <NixPlatform/AudioBus.h>

#include <cstdio>

using namespace Nix;

bool initializeAudioBackend()
{
    bool didInitialize = true;
    if (!gst_is_initialized())
        didInitialize = gst_init_check(0, 0, 0);

    // If gstreamer was already initialized, then we return a success.
    return didInitialize;
}

bool PlatformClient::loadAudioResource(AudioBus* destinationBus, const char* audioFileData, size_t dataSize, double sampleRate)
{
    return AudioFileReader(audioFileData, dataSize).createBus(destinationBus, sampleRate);
}

Data PlatformClient::loadResource(const char* name)
{
    return AudioFileReader::loadResource(name);
}

AudioDevice* PlatformClient::createAudioDevice(const Nix::String& inputDeviceId, size_t bufferSize, unsigned numberOfInputChannels, unsigned numberOfChannels, double sampleRate, AudioDevice::RenderCallback* renderCallback)
{
    printf("[%s] {%s}\n", __PRETTY_FUNCTION__, inputDeviceId.utf8().data());
    return new GstAudioDevice(inputDeviceId, bufferSize, numberOfInputChannels, numberOfChannels, sampleRate, renderCallback);
}
