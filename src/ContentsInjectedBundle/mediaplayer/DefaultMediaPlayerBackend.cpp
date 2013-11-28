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

#include "DefaultMediaPlayerBackend.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

DefaultMediaPlayerBackend::DefaultMediaPlayerBackend(Nix::MediaPlayerClient* client, const std::string& url)
    : MediaPlayerBackendBase(client)
    , m_url(url)
    , m_audioSink(nullptr)
    , m_seeking(false)
    , m_pendingSeek(false)
    , m_bufferingFinished(false)
    , m_playbackRate(1)
{
}

DefaultMediaPlayerBackend::~DefaultMediaPlayerBackend()
{
    destroyAudioSink();
}

GstElement* DefaultMediaPlayerBackend::pipeline() const
{
    return m_audioSink;
}

/* MediaPlayerBackend */
float DefaultMediaPlayerBackend::duration() const
{
    gint64 duration = GST_CLOCK_TIME_NONE;
    gst_element_query_duration(m_audioSink, GST_FORMAT_TIME, &duration);
    if (GST_CLOCK_TIME_IS_VALID(duration))
        return static_cast<double>(duration) / GST_SECOND;
    return 0;
}

float DefaultMediaPlayerBackend::currentTime() const
{
    gint64 current = GST_CLOCK_TIME_NONE;
    gst_element_query_position(m_audioSink, GST_FORMAT_TIME, &current);
    if (GST_CLOCK_TIME_IS_VALID(current))
        return static_cast<double>(current) / GST_SECOND;
    return 0;
}

void DefaultMediaPlayerBackend::seek(float time)
{
    if (!m_audioSink)
        return;

    m_seekTime = time;

    if (m_seeking) {
        m_pendingSeek = true;
        return;
    }

    GstState state;
    gst_element_get_state(m_audioSink, &state, 0, 0);
    if (state != GST_STATE_PAUSED && state != GST_STATE_PLAYING)
        return;

    float seconds;
    float microSeconds = modff(time, &seconds) * 1000000;
    GTimeVal timeValue;
    timeValue.tv_sec = static_cast<glong>(seconds);
    timeValue.tv_usec = static_cast<glong>(roundf(microSeconds / 10000) * 10000);

    if (!gst_element_seek(m_audioSink, m_playbackRate, GST_FORMAT_TIME, static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
                          GST_SEEK_TYPE_SET, GST_TIMEVAL_TO_TIME(timeValue), GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
        return;
    }

    m_seeking = true;
}

bool DefaultMediaPlayerBackend::seeking() const
{
    return m_seeking;
}

float DefaultMediaPlayerBackend::maxTimeSeekable() const
{
    if (m_isLive)
        return std::numeric_limits<float>::infinity();

    return duration();
}

void DefaultMediaPlayerBackend::setPlaybackRate(float playbackRate)
{
    m_playbackRate = playbackRate;
}
/**/

bool DefaultMediaPlayerBackend::createAudioSink()
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

void DefaultMediaPlayerBackend::destroyAudioSink()
{
    if (!m_audioSink)
        return;
    gst_element_set_state(m_audioSink, GST_STATE_NULL);
    gst_object_unref(m_audioSink);
}

void DefaultMediaPlayerBackend::load()
{
    gst_init_check(0, 0, 0);

    if (!m_audioSink && !createAudioSink()) {
        // Could be a decode error as well, but just report a error is enough for now.
        setNetworkState(Nix::MediaPlayerClient::NetworkError);
        return;
    }

    g_object_set(m_audioSink, "uri", m_url.c_str(), NULL);

    gst_element_set_state(m_audioSink, GST_STATE_PAUSED);
    setDownloadBuffering();
}

void DefaultMediaPlayerBackend::handleMessage(GstMessage* message)
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

void DefaultMediaPlayerBackend::updateStates()
{
    GstState state;
    GstState pending;

    GstStateChangeReturn ret = gst_element_get_state(m_audioSink,
        &state, &pending, 250 * GST_NSECOND);

    switch (ret) {
    case GST_STATE_CHANGE_FAILURE:
        GST_WARNING("Failed to change state");
        break;
    case GST_STATE_CHANGE_SUCCESS:
        m_isLive = false;

        switch (state) {
        case GST_STATE_READY:
            setReadyState(Nix::MediaPlayerClient::HaveMetadata);
            setNetworkState(Nix::MediaPlayerClient::Empty);
            break;
        case GST_STATE_PAUSED:
        case GST_STATE_PLAYING:
            if (m_seeking) {
                m_seeking = false;
                m_playerClient->currentTimeChanged();
            }

            if (m_bufferingFinished) {
                setReadyState(Nix::MediaPlayerClient::HaveEnoughData);
                setNetworkState(Nix::MediaPlayerClient::Loaded);
            } else {
                setReadyState(Nix::MediaPlayerClient::HaveCurrentData);
                setNetworkState(Nix::MediaPlayerClient::Loading);
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
            setReadyState(Nix::MediaPlayerClient::HaveEnoughData);
            setNetworkState(Nix::MediaPlayerClient::Loading);
        }
        break;
    default:
        break;
    }
}

void DefaultMediaPlayerBackend::setDownloadBuffering()
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
