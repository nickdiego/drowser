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

#include "MediaPlayerBackendBase.h"

#include <gst/audio/streamvolume.h>

MediaPlayerBackendBase::MediaPlayerBackendBase(Nix::MediaPlayerClient* client, bool isLive)
    : MediaPlayerBackend(client)
    , m_paused(true)
    , m_isLive(isLive)
    , m_readyState(Nix::MediaPlayerClient::HaveNothing)
    , m_networkState(Nix::MediaPlayerClient::Empty)
{
}

MediaPlayerBackendBase::~MediaPlayerBackendBase()
{
}

void MediaPlayerBackendBase::play()
{
    gst_element_set_state(pipeline(), GST_STATE_PLAYING);
    m_paused = false;
}

void MediaPlayerBackendBase::pause()
{
    gst_element_set_state(pipeline(), GST_STATE_PAUSED);
    m_paused = true;
}

void MediaPlayerBackendBase::setVolume(float volume)
{
    gst_stream_volume_set_volume(GST_STREAM_VOLUME(pipeline()), GST_STREAM_VOLUME_FORMAT_LINEAR, volume);
}

void MediaPlayerBackendBase::setMuted(bool mute)
{
    gst_stream_volume_set_mute(GST_STREAM_VOLUME(pipeline()), mute);
}

bool MediaPlayerBackendBase::isLiveStream() const
{
    return m_isLive;
}

void MediaPlayerBackendBase::setReadyState(Nix::MediaPlayerClient::ReadyState readyState)
{
    if (m_readyState == readyState)
        return;
    m_readyState = readyState;
    m_playerClient->readyStateChanged(m_readyState);
}

void MediaPlayerBackendBase::setNetworkState(Nix::MediaPlayerClient::NetworkState networkState)
{
    if (m_networkState == networkState)
        return;
    m_networkState = networkState;
    m_playerClient->networkStateChanged(m_networkState);
}

void MediaPlayerBackendBase::onGstBusMessage(GstBus* bus, GstMessage* msg, MediaPlayerBackendBase* backend)
{
    backend->handleMessage(msg);
}

bool MediaPlayerBackendBase::isPaused() const
{
    return m_paused;
}
