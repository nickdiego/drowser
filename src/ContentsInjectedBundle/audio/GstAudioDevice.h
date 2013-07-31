#ifndef GstAudioDevice_h
#define GstAudioDevice_h

#include <gst/gst.h>
#include <NixPlatform/Platform.h>

class GstAudioDevice : public Nix::AudioDevice {
public:
    GstAudioDevice(const Nix::String& inputDeviceId, size_t bufferSize, unsigned numberOfInputChannels, unsigned numberOfChannels, double sampleRate, Nix::AudioDevice::RenderCallback* renderCallback);
    virtual ~GstAudioDevice();

    virtual void start();
    virtual void stop();

    double sampleRate() { return m_sampleRate; }

    static gboolean s_audioProcessLoop(gpointer userData) { return static_cast<AudioDestination*>(userData)->audioProcessLoop(); }

    virtual gboolean audioProcessLoop();
    void finishBuildingPipelineAfterWavParserPadReady(GstPad*);

private:
    bool m_wavParserAvailable;
    bool m_audioSinkAvailable;
    GstElement* m_pipeline;
    GstElement* m_audioSink;
    double m_sampleRate;
    size_t m_bufferSize;
    bool m_isDevice;
    guint m_loopId;
    const Nix::String& m_inputDeviceId;
    Nix::AudioDevice::RenderCallback* m_renderCallback;
};

#endif