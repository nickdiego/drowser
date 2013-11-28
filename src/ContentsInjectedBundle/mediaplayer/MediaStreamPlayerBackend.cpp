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
#include "Logging.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

MediaStreamPlayerBackend::MediaStreamPlayerBackend(Nix::MediaPlayerClient* client, Nix::MediaStream* stream)
    : MediaPlayerBackendBase(client, true)
    , m_stream(stream)
    , m_audioSourceId()
    , m_audioSinkBin(nullptr)
    , m_stopped(true)
{
}

MediaStreamPlayerBackend::~MediaStreamPlayerBackend()
{
    destroyAudioSink();
}

GstElement* MediaStreamPlayerBackend::pipeline() const
{
    return CentralPipelineUnit::instance().pipeline();
}

bool MediaStreamPlayerBackend::createAudioSink()
{
    // FIXME: check errors
    assert(!m_audioSinkBin);
    m_audioSinkBin = gst_bin_new(0);

    GstElement* volume = gst_element_factory_make("volume", "volume");

    GstElement* audioSink = gst_element_factory_make("autoaudiosink", 0);
    gst_bin_add_many(GST_BIN(m_audioSinkBin), volume, audioSink, NULL);
    gst_element_link_many(volume, audioSink, NULL);

    GstPad* pad = gst_element_get_static_pad(volume, "sink");
    gst_element_add_pad(m_audioSinkBin, gst_ghost_pad_new("sink", pad));

    LOG(Media, "created sink bin %p", m_audioSinkBin);
    return true;
}

void MediaStreamPlayerBackend::destroyAudioSink()
{
    if (!m_audioSinkBin)
        return;
    gst_element_set_state(m_audioSinkBin, GST_STATE_NULL);
    gst_object_unref(m_audioSinkBin);
}

void MediaStreamPlayerBackend::load()
{
    gst_init_check(0, 0, 0);

    LOG(Media, "loading..");

    if (!m_audioSinkBin && !createAudioSink()) {
        // Could be a decode error as well, but just report a error is enough for now.
        setNetworkState(Nix::MediaPlayerClient::NetworkError);
        return;
    }

    LOG(Media, "playbin ok..");

    if (!m_stream || m_stream->ended()) {
        loadingFailed(Nix::MediaPlayerClient::NetworkError);
        return;
    }

    LOG(Media, "stream not ended..");

    setReadyState(Nix::MediaPlayerClient::HaveNothing);
    setNetworkState(Nix::MediaPlayerClient::Loading);

    if (!internalLoad())
        return;

    LOG(Media, "internal load ok..");
}

void MediaStreamPlayerBackend::setDownloadBuffering()
{
    assert(m_audioSinkBin);

    GstPlayFlags flags;
    g_object_get(m_audioSinkBin, "flags", &flags, NULL);
    // TODO add support for "auto" downloading...
    if (m_isLive)
        g_object_set(m_audioSinkBin, "flags", flags & ~GST_PLAY_FLAG_DOWNLOAD, NULL);
    else
        g_object_set(m_audioSinkBin, "flags", flags | GST_PLAY_FLAG_DOWNLOAD, NULL);
}


/* Nix::MediaStreamSource::Observer methods */
void MediaStreamPlayerBackend::sourceReadyStateChanged()
{
    CentralPipelineUnit& cpu = CentralPipelineUnit::instance();
    if (!m_stream || m_stream->ended())
        stop();

    // check if the source should be ended
    if (!m_audioSourceId.empty()) {
        std::vector<Nix::MediaStreamSource*> audioSources = m_stream->audioSources();
        for (auto& source: audioSources) {
            if (!source->enabled() && m_audioSourceId.compare(source->id()) == 0) {
                cpu.disconnectFromSource(m_audioSourceId, m_audioSinkBin);
                m_audioSourceId = "";
                break;
            }
        }
    }
}

// FIXME implement
void MediaStreamPlayerBackend::sourceMutedChanged()
{
}

// FIXME implement
void MediaStreamPlayerBackend::sourceEnabledChanged()
{
}

// FIXME implement
bool MediaStreamPlayerBackend::observerIsEnabled()
{
    return true;
}
/**/


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
        cpu.disconnectFromSource(m_audioSourceId, m_audioSinkBin);
    m_audioSourceId = "";
}

void MediaStreamPlayerBackend::setMediaStream(Nix::MediaStream* stream)
{
    assert(stream);
    if (m_stream && m_stream == stream)
        return;

    // FIXME: Incomplete implementation
    // handle when some stream was already playing..
    m_stream = stream;
}

void MediaStreamPlayerBackend::play()
{
    LOG(Media, "Play");
    if (!m_stream || m_stream->ended()) {
        m_readyState = Nix::MediaPlayerClient::HaveNothing;
        loadingFailed(Nix::MediaPlayerClient::Empty);
        return;
    }

    m_paused = false;
    internalLoad();
}

void MediaStreamPlayerBackend::pause()
{
    LOG(Media, "Pause");
    m_paused = true;
    stop();
}

bool MediaStreamPlayerBackend::internalLoad()
{
    if (!m_stopped)
        return false;

    LOG(Media, " currently stopped");

    m_stopped = false;
    if (!m_stream || m_stream->ended()) {
        loadingFailed(Nix::MediaPlayerClient::NetworkError);
        return false;
    }

    LOG(Media, " let's connect to medistream pipeline");
    return connectToGSTLiveStream(m_stream);
}

bool MediaStreamPlayerBackend::connectToGSTLiveStream(Nix::MediaStream* mediaStream)
{
    LOG(Media, "Connecting to live stream, descriptor: %p", mediaStream);
    if (!mediaStream)
        return false;

    // FIXME: Error handling.. this could fail.. and this method never returns false.

    CentralPipelineUnit& cpu = CentralPipelineUnit::instance();

    if (!m_audioSourceId.empty()) {
        cpu.disconnectFromSource(m_audioSourceId, m_audioSinkBin);
        m_audioSourceId = "";
    }

    std::vector<Nix::MediaStreamSource*> audioSources = mediaStream->audioSources();
    LOG(Media, "Stream descriptor has %lu audio streams.", audioSources.size());
    for (auto& source: audioSources) {
        if (!source->enabled())
            continue;
        if (source->type() == Nix::MediaStreamSource::Audio) {
            if (cpu.connectToSource(source->id(), m_audioSinkBin)) {
                m_audioSourceId = source->id();
                source->addObserver(this);
                break;
            }
        }
    }

    return true;
}

void MediaStreamPlayerBackend::loadingFailed(Nix::MediaPlayerClient::NetworkState networkState)
{
    setNetworkState(networkState);
    setReadyState(Nix::MediaPlayerClient::HaveNothing);
}

