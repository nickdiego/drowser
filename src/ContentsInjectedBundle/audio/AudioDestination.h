#ifndef AudioDestination_h
#define AudioDestination_h

#include <gst/gst.h>
#include <NixPlatform/Platform.h>

class AudioDestination : public Nix::AudioDevice {
public:
    AudioDestination(const char* inputDeviceId, size_t bufferSize, unsigned numberOfInputChannels, unsigned numberOfChannels, double sampleRate, Nix::AudioDevice::RenderCallback* renderCallback);
    virtual ~AudioDestination();

    virtual void start();
    virtual void stop();

    double sampleRate() { return m_sampleRate; }
    void finishBuildingPipelineAfterWavParserPadReady(GstPad*);

private:
    bool m_wavParserAvailable;
    bool m_audioSinkAvailable;
    GstElement* m_pipeline;
    GstElement* m_audioSink;
    double m_sampleRate;
    size_t m_bufferSize;
    bool m_isDevice;
    const char* m_inputDeviceId;
    Nix::AudioDevice::RenderCallback* m_renderCallback;
};

#endif
