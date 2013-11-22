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

#include "GstreamerBackendBase.h"

#include <gst/audio/streamvolume.h>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

using namespace std;

GstreamerBackendBase::GstreamerBackendBase(Nix::MediaPlayerClient* client)
    : MediaPlayerBackend(client)
    , m_audioSink(nullptr)
    , m_paused(true)
    , m_isLive(false)
    , m_seeking(false)
    , m_pendingSeek(false)
    , m_bufferingFinished(false)
    , m_playbackRate(1)
    , m_readyState(Nix::MediaPlayerClient::HaveNothing)
    , m_networkState(Nix::MediaPlayerClient::Empty)
{
}

GstreamerBackendBase::~GstreamerBackendBase()
{
}

void GstreamerBackendBase::play()
{
    gst_element_set_state(m_audioSink, GST_STATE_PLAYING);
    m_paused = false;
}

void GstreamerBackendBase::pause()
{
    gst_element_set_state(m_audioSink, GST_STATE_PAUSED);
    m_paused = true;
}

float GstreamerBackendBase::duration() const
{
    gint64 duration = GST_CLOCK_TIME_NONE;
    gst_element_query_duration(m_audioSink, GST_FORMAT_TIME, &duration);
    if (GST_CLOCK_TIME_IS_VALID(duration))
        return static_cast<double>(duration) / GST_SECOND;
    return 0;
}

float GstreamerBackendBase::currentTime() const
{
    gint64 current = GST_CLOCK_TIME_NONE;
    gst_element_query_position(m_audioSink, GST_FORMAT_TIME, &current);
    if (GST_CLOCK_TIME_IS_VALID(current))
        return static_cast<double>(current) / GST_SECOND;
    return 0;
}

void GstreamerBackendBase::seek(float time)
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

bool GstreamerBackendBase::seeking() const
{
    return m_seeking;
}

float GstreamerBackendBase::maxTimeSeekable() const
{
    if (m_isLive)
        return numeric_limits<float>::infinity();

    return duration();
}

void GstreamerBackendBase::setPlaybackRate(float playbackRate)
{
    m_playbackRate = playbackRate;
}

void GstreamerBackendBase::setVolume(float volume)
{
    gst_stream_volume_set_volume(GST_STREAM_VOLUME(m_audioSink), GST_STREAM_VOLUME_FORMAT_LINEAR, volume);
}

void GstreamerBackendBase::setMuted(bool mute)
{
    gst_stream_volume_set_mute(GST_STREAM_VOLUME(m_audioSink), mute);
}

bool GstreamerBackendBase::isLiveStream() const
{
    return m_isLive;
}

void GstreamerBackendBase::onGstBusMessage(GstBus* bus, GstMessage* msg, GstreamerBackendBase* backend)
{
    backend->handleMessage(msg);
}

bool GstreamerBackendBase::isPaused() const
{
    return m_paused;
}
