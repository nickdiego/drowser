/*
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "GstreamerBackend.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

GstreamerBackend::GstreamerBackend(Nix::MediaPlayerClient* client)
    : GstreamerBackendBase(client)
{
}

GstreamerBackend::~GstreamerBackend()
{
    destroyAudioSink();
}

bool GstreamerBackend::createAudioSink()
{
    assert(!m_audioSink);

    m_audioSink = gst_element_factory_make("playbin", "play");
    if (!m_audioSink)
        return false;

    // Audio sink
    GstElement* scale = gst_element_factory_make("scaletempo", 0);
    if (!scale) {
        GST_WARNING("Failed to create scaletempo");
        return false;
    }

    GstElement* convert = gst_element_factory_make("audioconvert", 0);
    GstElement* resample = gst_element_factory_make("audioresample", 0);
    GstElement* sink = gst_element_factory_make("autoaudiosink", 0);

    GstElement* audioSink = gst_bin_new("audio-sink");
    gst_bin_add_many(GST_BIN(audioSink), scale, convert, resample, sink, NULL);

    if (!gst_element_link_many(scale, convert, resample, sink, NULL)) {
        GST_WARNING("Failed to link audio sink elements");
        gst_object_unref(audioSink);
        return false;
    }

    GstPad* pad = gst_element_get_static_pad(scale, "sink");
    gst_element_add_pad(audioSink, gst_ghost_pad_new("sink", pad));

    g_object_set(m_audioSink, "audio-sink", audioSink, NULL);
    gst_object_unref(pad);

    GstBus* bus = gst_element_get_bus(m_audioSink);
    gst_bus_add_signal_watch(bus);
    g_signal_connect(bus, "message", G_CALLBACK(onGstBusMessage), this);
    gst_object_unref(bus);

    return true;
}

void GstreamerBackend::destroyAudioSink()
{
    if (!m_audioSink)
        return;
    gst_element_set_state(m_audioSink, GST_STATE_NULL);
    gst_object_unref(m_audioSink);
}

void GstreamerBackend::load(const char* url)
{
    gst_init_check(0, 0, 0);

    if (!m_audioSink && !createAudioSink()) {
        // Could be a decode error as well, but just report a error is enough for now.
        m_playerClient->networkStateChanged(Nix::MediaPlayerClient::NetworkError);
        return;
    }

    g_object_set(m_audioSink, "uri", url, NULL);

    gst_element_set_state(m_audioSink, GST_STATE_PAUSED);
    setDownloadBuffering();
}

void GstreamerBackend::handleMessage(GstMessage* message)
{
    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR: {
          GError *err;
          gchar *debug;
          gst_message_parse_error(message, &err, &debug);
          GST_WARNING(err->message);
          g_error_free(err);
          g_free(debug);
          gst_element_set_state(m_audioSink, GST_STATE_READY);
          break;
        }
    case GST_MESSAGE_EOS:
          gst_element_set_state(m_audioSink, GST_STATE_READY);
          break;

    case GST_MESSAGE_BUFFERING: {
        if (m_isLive)
            return;

        int percent = 0;
        gst_message_parse_buffering(message, &percent);
        if (percent < 100) {
            gst_element_set_state(m_audioSink, GST_STATE_PAUSED);
        } else {
            // the duration is available now.
            m_playerClient->durationChanged();
            m_bufferingFinished = true;
            if (!m_paused)
               gst_element_set_state(m_audioSink, GST_STATE_PLAYING);
        }
        break;
    }
    case GST_MESSAGE_CLOCK_LOST:
        if (!m_paused) {
            gst_element_set_state(m_audioSink, GST_STATE_PAUSED);
            gst_element_set_state(m_audioSink, GST_STATE_PLAYING);
        }
        break;
    case GST_MESSAGE_DURATION_CHANGED:
        m_playerClient->durationChanged();
        break;
    case GST_MESSAGE_STATE_CHANGED:
    case GST_MESSAGE_ASYNC_DONE:
        // Ignore state changes from internal elements. They are
        // forwarded to playbin anyway.
        if (GST_MESSAGE_SRC(message) == reinterpret_cast<GstObject*>(m_audioSink))
            updateStates();
        break;
    default:
        break;
    }
}

void GstreamerBackend::updateStates()
{
    GstState state;
    GstState pending;

    GstStateChangeReturn ret = gst_element_get_state(m_audioSink,
        &state, &pending, 250 * GST_NSECOND);

    Nix::MediaPlayerClient::ReadyState oldReadyState = m_readyState;
    Nix::MediaPlayerClient::NetworkState oldNetworkState = m_networkState;

    switch (ret) {
    case GST_STATE_CHANGE_FAILURE:
        GST_WARNING("Failed to change state");
        break;
    case GST_STATE_CHANGE_SUCCESS:
        m_isLive = false;

        switch (state) {
        case GST_STATE_READY:
            m_readyState = Nix::MediaPlayerClient::HaveMetadata;
            m_networkState = Nix::MediaPlayerClient::Empty;
            break;
        case GST_STATE_PAUSED:
        case GST_STATE_PLAYING:
            if (m_seeking) {
                m_seeking = false;
                m_playerClient->currentTimeChanged();
            }

            if (m_bufferingFinished) {
                m_readyState = Nix::MediaPlayerClient::HaveEnoughData;
                m_networkState = Nix::MediaPlayerClient::Loaded;
            } else {
                m_readyState = Nix::MediaPlayerClient::HaveCurrentData;
                m_networkState = Nix::MediaPlayerClient::Loading;
            }

            if (m_pendingSeek) {
                m_pendingSeek = false;
                seek(m_seekTime);
            }
            break;
        default:
            break;
        }
        break;
    case GST_STATE_CHANGE_NO_PREROLL:
        m_isLive = true;
        setDownloadBuffering();

        if (state == GST_STATE_PAUSED) {
            m_readyState = Nix::MediaPlayerClient::HaveEnoughData;
            m_networkState = Nix::MediaPlayerClient::Loading;
        }
        break;
    default:
        break;
    }

    if (oldReadyState != m_readyState)
        m_playerClient->readyStateChanged(m_readyState);

    if (oldNetworkState != m_networkState)
        m_playerClient->networkStateChanged(m_networkState);
}

void GstreamerBackend::setDownloadBuffering()
{
    assert(m_audioSink);

    GstPlayFlags flags;
    g_object_get(m_audioSink, "flags", &flags, NULL);
    // TODO add support for "auto" downloading...
    if (m_isLive)
        g_object_set(m_audioSink, "flags", flags & ~GST_PLAY_FLAG_DOWNLOAD, NULL);
    else
        g_object_set(m_audioSink, "flags", flags | GST_PLAY_FLAG_DOWNLOAD, NULL);
}
