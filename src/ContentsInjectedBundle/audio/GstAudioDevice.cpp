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

#include "GstAudioDevice.h"
#include "WebKitWebAudioSourceGStreamer.h"

#include <NixPlatform/CString.h>
#include <NixPlatform/String.h>
#include <NixPlatform/Vector.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <cstring>

using namespace Nix;

static bool configureSinkDevice(GstElement* autoSink);

#ifndef GST_API_VERSION_1
static void onGStreamerWavparsePadAddedCallback(GstElement* element, GstPad* pad, GstAudioDevice* destination)
{
    destination->finishBuildingPipelineAfterWavParserPadReady(pad);
}
#endif

GstAudioDevice::GstAudioDevice(const Nix::String& inputDeviceId, size_t bufferSize, unsigned, unsigned, double sampleRate, AudioDevice::RenderCallback* renderCallback)
    : m_wavParserAvailable(false)
    , m_audioSinkAvailable(false)
    , m_pipeline(0)
    , m_sampleRate(sampleRate)
    , m_bufferSize(bufferSize)
    , m_isDevice(false)
    , m_loopId(0)
    , m_inputDeviceId(inputDeviceId)
    , m_renderCallback(renderCallback)
{
    printf("[%s] %p {%s}\n", __PRETTY_FUNCTION__, this, m_inputDeviceId.utf8().data());

    if (!std::strcmp(m_inputDeviceId.utf8().data(), "autoaudiosrc;default"))
        m_isDevice = true;

    // FIXME: NUMBER OF CHANNELS NOT USED??????????/ WHY??????????

    m_pipeline = gst_pipeline_new("play");

    GstElement* webkitAudioSrc = reinterpret_cast<GstElement*>(g_object_new(WEBKIT_TYPE_WEB_AUDIO_SRC,
                                                                            "rate", sampleRate,
                                                                            "handler", renderCallback,
                                                                            "frames", bufferSize, NULL));

    GstElement* wavParser = gst_element_factory_make("wavparse", 0);

    m_wavParserAvailable = wavParser;

    if (!m_wavParserAvailable) {
        g_error("Failed to create GStreamer wavparse element");
        return;
    }

    gst_bin_add_many(GST_BIN(m_pipeline), webkitAudioSrc, wavParser, NULL);
    gst_element_link_pads_full(webkitAudioSrc, "src", wavParser, "sink", GST_PAD_LINK_CHECK_NOTHING);

    gst_element_sync_state_with_parent(webkitAudioSrc);
    gst_element_sync_state_with_parent(wavParser);

    GstPad* srcPad = gst_element_get_static_pad(wavParser, "src");
    finishBuildingPipelineAfterWavParserPadReady(srcPad);
}

GstAudioDevice::~GstAudioDevice()
{
    printf("[%s] %p\n", __PRETTY_FUNCTION__, this);
    gst_element_set_state(m_pipeline, GST_STATE_NULL);
    gst_object_unref(m_pipeline);
    g_source_remove(m_loopId);
}

void GstAudioDevice::finishBuildingPipelineAfterWavParserPadReady(GstPad* pad)
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

void GstAudioDevice::start()
{
    GST_WARNING("Input Ready, starting main pipeline...");
    printf("[%s] %p {%s}\n", __PRETTY_FUNCTION__, this, m_inputDeviceId.utf8().data());
    if (!m_wavParserAvailable)
        return;

    gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
}

void GstAudioDevice::stop()
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
