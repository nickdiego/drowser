#ifndef GstAudioDevice_h
#define GstAudioDevice_h

#include <gst/gst.h>
#include <NixPlatform/Platform.h>

class GstAudioDevice : public Nix::AudioDevice {
public:
    GstAudioDevice(const char* inputDeviceId, size_t bufferSize, unsigned numberOfInputChannels,
        unsigned numberOfOutputChannels, double sampleRate, Nix::AudioDevice::RenderCallback* renderCallback);
    virtual ~GstAudioDevice();

    virtual void start();
    virtual void stop();

    double sampleRate() { return m_sampleRate; }
    bool providesLiveInput() { return m_providesLiveInput; }

private:
    void finishBuildingPipelineAfterWavParserPadReady(GstPad*);

    bool m_wavParserAvailable;
    bool m_audioSinkAvailable;
    GstElement* m_pipeline;
    GstElement* m_audioSink;
    double m_sampleRate;
    size_t m_bufferSize;
    bool m_providesLiveInput;
    char* m_inputDeviceId;
    Nix::AudioDevice::RenderCallback* m_renderCallback;
};

#endif
