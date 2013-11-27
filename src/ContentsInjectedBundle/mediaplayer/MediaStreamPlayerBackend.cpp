/*
 * Copyright (C) 2012 Collabora Ltd. All rights reserved.
 * Copyright (C) 2013 Igalia S.L. All rights reserved.
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

#include "MediaStreamPlayerBackend.h"
#include "CentralPipelineUnit.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

MediaStreamPlayerBackend::MediaStreamPlayerBackend(Nix::MediaPlayerClient* client)
    : MediaPlayerBackendBase(client)
    , m_stopped(false)
{
}

MediaStreamPlayerBackend::~MediaStreamPlayerBackend()
{
    destroyAudioSink();
}

bool MediaStreamPlayerBackend::createAudioSink()
{
    // FIXME: check errors
    assert(!m_audioSink);
    m_audioSink = gst_bin_new(0);

    GstElement* volume = gst_element_factory_make("volume", "volume");

    GstElement* audioSink = gst_element_factory_make("autoaudiosink", 0);
    gst_bin_add_many(GST_BIN(m_audioSink), volume, audioSink, NULL);
    gst_element_link_many(volume, audioSink, NULL);

    GstPad* pad = gst_element_get_static_pad(volume, "sink");
    gst_element_add_pad(m_audioSink, gst_ghost_pad_new("sink", pad));

    return true;
}

void MediaStreamPlayerBackend::destroyAudioSink()
{
    if (!m_audioSink)
        return;
    gst_element_set_state(m_audioSink, GST_STATE_NULL);
    gst_object_unref(m_audioSink);
}

void MediaStreamPlayerBackend::load(const char* url)
{
    gst_init_check(0, 0, 0);

    if (!m_audioSink && !createAudioSink()) {
        // Could be a decode error as well, but just report a error is enough for now.
        setNetworkState(Nix::MediaPlayerClient::NetworkError);
        return;
    }

    if (!m_stream /*|| m_stream->ended()*/) { // FIXME
        loadingFailed(Nix::MediaPlayerClient::NetworkError);
        return;
    }

    setReadyState(Nix::MediaPlayerClient::HaveNothing);
    setNetworkState(Nix::MediaPlayerClient::Loading);

    if (!internalLoad())
        return;
}

void MediaStreamPlayerBackend::handleMessage(GstMessage* message)
{
}

void MediaStreamPlayerBackend::setDownloadBuffering()
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

void MediaStreamPlayerBackend::sourceReadyStateChanged()
{
    CentralPipelineUnit& cpu = CentralPipelineUnit::instance();
    if (!m_stream|| m_stream->ended())
        stop();

#if 0 //FIXME export MediaStreamTracks
    // check if the source should be ended
    if (!m_audioSourceId.empty()) {
        for (unsigned i = 0; i < m_stream->numberOfAudioStreams(); i++) {
            Nix::MediaStreamSource* source = m_stream->audioStreams(i);
            if (!source->enabled() && m_audioSourceId.compare(source->id()) == 0) {
                cpu.disconnectFromSource(m_audioSourceId, m_audioSink);
                m_audioSourceId = "";
                break;
            }
        }
    }
#endif
}

void MediaStreamPlayerBackend::sourceMutedChanged()
{
}

void MediaStreamPlayerBackend::sourceEnabledChanged()
{
}

bool MediaStreamPlayerBackend::stopped()
{
    return m_stopped;
}

void MediaStreamPlayerBackend::stop()
{
    if (m_stopped)
        return;

    m_stopped = true;
    CentralPipelineUnit& cpu = CentralPipelineUnit::instance();
    if (!m_audioSourceId.empty())
        cpu.disconnectFromSource(m_audioSourceId, m_audioSink);
    m_audioSourceId = "";
}

void MediaStreamPlayerBackend::play()
{
    if (!m_stream/* || m_streamDescriptor->ended()*/) { //FIXME
        m_readyState = Nix::MediaPlayerClient::HaveNothing;
        loadingFailed(Nix::MediaPlayerClient::Empty);
        return;
    }

    m_paused = false;
    internalLoad();
}

void MediaStreamPlayerBackend::pause()
{
    m_paused = true;
    stop();
}

bool MediaStreamPlayerBackend::internalLoad()
{
    if (!m_stopped)
        return false;

    m_stopped = false;
    if (!m_stream/* || m_stream->ended()*/) { //FIXME
        loadingFailed(Nix::MediaPlayerClient::NetworkError);
        return false;
    }
    return connectToGSTLiveStream(m_stream);
}

// FIXME implement
bool MediaStreamPlayerBackend::connectToGSTLiveStream(Nix::MediaStream* streamDescriptor)
{
    return true;
}

// FIXME implement
void MediaStreamPlayerBackend::loadingFailed(Nix::MediaPlayerClient::NetworkState networkState)
{
}
