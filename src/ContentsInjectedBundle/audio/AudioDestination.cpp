/*
 *  Copyright (C) 2011 Igalia S.L
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "AudioDestination.h"
#include "WebKitWebAudioSourceGStreamer.h"

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

using namespace Nix;

static gboolean messageCallback(GstBus*, GstMessage* message, GstElement*);
static bool configureSinkDevice(GstElement* autoSink);

#ifndef GST_API_VERSION_1
static void onGStreamerWavparsePadAddedCallback(GstElement* element, GstPad* pad, AudioDestination* destination)
{
    destination->finishBuildingPipelineAfterWavParserPadReady(pad);
}
#endif

AudioDestination::AudioDestination(size_t bufferSize, unsigned numberOfInputChannels, unsigned numberOfChannels, double sampleRate, AudioDevice::RenderCallback* callback)
    : m_wavParserAvailable(false)
    , m_audioSinkAvailable(false)
    , m_pipeline(0)
    , m_sampleRate(sampleRate)
{
    // FIXME: NUMBER OF CHANNELS NOT USED??????????/ WHY??????????

    m_pipeline = gst_pipeline_new("play");

    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipeline));
    assert(bus);
    gst_bus_add_signal_watch(bus);
    g_signal_connect(bus, "message", G_CALLBACK(messageCallback), m_pipeline);

    GstElement* webkitAudioSrc = reinterpret_cast<GstElement*>(g_object_new(WEBKIT_TYPE_WEB_AUDIO_SRC,
                                                                            "rate", sampleRate,
                                                                            "handler", callback,
                                                                            "frames", bufferSize, NULL));

    GstElement* wavParser = gst_element_factory_make("wavparse", 0);

    m_wavParserAvailable = wavParser;

    if (!m_wavParserAvailable) {
        g_error("Failed to create GStreamer wavparse element");
        return;
    }

#ifndef GST_API_VERSION_1
    g_signal_connect(wavParser, "pad-added", G_CALLBACK(onGStreamerWavparsePadAddedCallback), this);
#endif
    gst_bin_add_many(GST_BIN(m_pipeline), webkitAudioSrc, wavParser, NULL);
    gst_element_link_pads_full(webkitAudioSrc, "src", wavParser, "sink", GST_PAD_LINK_CHECK_NOTHING);

    gst_element_sync_state_with_parent(webkitAudioSrc);
    gst_element_sync_state_with_parent(wavParser);

#ifdef GST_API_VERSION_1
    GstPad* srcPad = gst_element_get_static_pad(wavParser, "src");
    finishBuildingPipelineAfterWavParserPadReady(srcPad);
#endif
}

AudioDestination::~AudioDestination()
{
    gst_element_set_state(m_pipeline, GST_STATE_NULL);
    gst_object_unref(m_pipeline);
}

void AudioDestination::finishBuildingPipelineAfterWavParserPadReady(GstPad* pad)
{
    GstElement* audioSink = gst_element_factory_make("autoaudiosink", 0);
    m_audioSinkAvailable = audioSink;

    if (!audioSink)
        return;

    if (!configureSinkDevice(audioSink))
        GST_WARNING_OBJECT(audioSink, "Couldn't configure sink device");

    // Autoaudiosink does the real sink detection in the GST_STATE_NULL->READY transition
    // so it's best to roll it to READY as soon as possible to ensure the underlying platform
    // audiosink was loaded correctly.
    GstStateChangeReturn stateChangeReturn = gst_element_set_state(audioSink, GST_STATE_READY);
    if (stateChangeReturn == GST_STATE_CHANGE_FAILURE) {
        gst_element_set_state(audioSink, GST_STATE_NULL);
        m_audioSinkAvailable = false;
        return;
    }

    GstElement* audioConvert = gst_element_factory_make("audioconvert", 0);
    gst_bin_add_many(GST_BIN(m_pipeline), audioConvert, audioSink, NULL);

    // Link wavparse's src pad to audioconvert sink pad.
    GstPad* sinkPad = gst_element_get_static_pad(audioConvert, "sink");
    gst_pad_link_full(pad, sinkPad, GST_PAD_LINK_CHECK_NOTHING);

    // Link audioconvert to audiosink and roll states.
    gst_element_link_pads_full(audioConvert, "src", audioSink, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_sync_state_with_parent(audioConvert);
    gst_element_sync_state_with_parent(audioSink);
}

void AudioDestination::start()
{
    GST_WARNING("Input Ready, starting main pipeline...");
    if (!m_wavParserAvailable)
        return;

    gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
}

void AudioDestination::stop()
{
    if (!m_wavParserAvailable || !m_audioSinkAvailable)
        return;

    gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
}

static bool configureSinkDevice(GstElement* autoSink) {
    GstElement* deviceElement;

    GstChildProxy* childProxy = GST_CHILD_PROXY(autoSink);
    if (!childProxy)
        return false;

    GstStateChangeReturn stateChangeResult = gst_element_set_state(autoSink, GST_STATE_READY);
    if (stateChangeResult != GST_STATE_CHANGE_SUCCESS)
        return false;

    if (gst_child_proxy_get_children_count(childProxy))
        deviceElement = GST_ELEMENT(gst_child_proxy_get_child_by_index(childProxy, 0));

//#if 0
    // FIXME temp workaround for pulsesink underflow warning issues
    // probably something related to LATENCY event failing in "play"
    // pipeline on startup. This adds some latency to the audio rendering
    g_object_set(deviceElement, "buffer-time", (gint64)100000, NULL);
    g_object_set(deviceElement, "latency-time", (gint64)100000, NULL);
    g_object_set(deviceElement, "drift-tolerance", (gint64)1000000, NULL);
//#endif

    GST_WARNING_OBJECT(deviceElement, "configured.");
    gst_object_unref(GST_OBJECT(deviceElement));
    return true;
}

static gboolean messageCallback(GstBus*, GstMessage* message, GstElement* data)
{
    GError* error = 0;
    gchar* debug = 0;

    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_EOS:
        break;
    case GST_MESSAGE_WARNING:
        gst_message_parse_warning(message, &error, &debug);
        g_warning("Warning: %d, %s. Debug output: %s", error->code,  error->message, debug);
        break;
    case GST_MESSAGE_ERROR:
        gst_message_parse_error(message, &error, &debug);
        g_warning("Error: %d, %s. Debug output: %s", error->code,  error->message, debug);
        break;
    case GST_MESSAGE_STATE_CHANGED:
        GstState old_state, new_state;
        gst_message_parse_state_changed (message, &old_state, &new_state, NULL);
        g_warning ("[play] Element %s changed state from %s to %s.\n",
           GST_OBJECT_NAME (message->src),
           gst_element_state_get_name (old_state),
           gst_element_state_get_name (new_state));
        break;
    case GST_MESSAGE_STREAM_STATUS:
        GstStreamStatusType status;
        GstElement *owner;
        gst_message_parse_stream_status(message, &status, &owner);
        g_warning ("[play] Element %s(%s) changed stream status to %d.\n",
           GST_OBJECT_NAME (owner),
           GST_OBJECT_NAME (message->src), status);
        break;

    case GST_MESSAGE_LATENCY:
        g_warning ("[play] Element %s requested latency, recaltulating...\n",
           GST_OBJECT_NAME (message->src));
        gst_bin_recalculate_latency(GST_BIN(data));
        break;

    default:
        break;
    }

    //FIXME deref error and debug
    return TRUE;
}


