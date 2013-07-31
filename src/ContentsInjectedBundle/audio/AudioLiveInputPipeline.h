#ifndef AudioLiveInputPipeline_h
#define AudioLiveInputPipeline_h

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/pbutils/pbutils.h>

#ifdef GST_API_VERSION_1
#include <gst/audio/audio.h>
#else
#include <gst/audio/multichannel.h>
#endif

class AudioLiveInputPipeline {
public:
    typedef void (*InputReadyCallback)();

    AudioLiveInputPipeline(float sampleRate);
    ~AudioLiveInputPipeline();

    GstStateChangeReturn start();
    gboolean sendQuery(GstQuery* query);
    bool isReady() { return m_ready; }

    int pullChannelBuffers(GSList** bufferList);

    void handleNewDeinterleavePad(GstPad*);
    void deinterleavePadsConfigured();
    GstFlowReturn handlePullRequest(GstAppSink* sink);

private:
    void buildInputPipeline();
    GstBuffer* pullNewBuffer(GstAppSink* sink);

    GstElement* m_pipeline;
    GstElement* m_source;
    GstElement* m_deInterleave;
    GSList* m_sinkList;
    float m_sampleRate;
    bool m_ready;
};

#endif //AudioLiveInputPipeline_h

