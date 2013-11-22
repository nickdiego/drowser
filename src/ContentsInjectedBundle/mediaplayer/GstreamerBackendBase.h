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

#ifndef GstreamerBackendBase_h
#define GstreamerBackendBase_h

#include "MediaPlayerBackend.h"

#include <gst/gst.h>

// GstPlayFlags flags from playbin. It is the policy of GStreamer to
// not publicly expose element-specific enums. That's why this
// GstPlayFlags enum has been copied here.
typedef enum {
    GST_PLAY_FLAG_VIDEO         = (1 << 0),
    GST_PLAY_FLAG_AUDIO         = (1 << 1),
    GST_PLAY_FLAG_TEXT          = (1 << 2),
    GST_PLAY_FLAG_VIS           = (1 << 3),
    GST_PLAY_FLAG_SOFT_VOLUME   = (1 << 4),
    GST_PLAY_FLAG_NATIVE_AUDIO  = (1 << 5),
    GST_PLAY_FLAG_NATIVE_VIDEO  = (1 << 6),
    GST_PLAY_FLAG_DOWNLOAD      = (1 << 7),
    GST_PLAY_FLAG_BUFFERING     = (1 << 8),
    GST_PLAY_FLAG_DEINTERLACE   = (1 << 9),
    GST_PLAY_FLAG_SOFT_COLORBALANCE = (1 << 10)
} GstPlayFlags;

class GstreamerBackendBase : public MediaPlayerBackend
{
public:
    GstreamerBackendBase(Nix::MediaPlayerClient* client);
    virtual ~GstreamerBackendBase();

    virtual void play();
    virtual void pause();
    virtual void seek(float);
    virtual float duration() const;
    virtual float currentTime() const;
    virtual void setVolume(float);
    virtual void setMuted(bool);
    virtual bool seeking() const;
    virtual float maxTimeSeekable() const;
    virtual void setPlaybackRate(float);
    virtual bool isLiveStream() const;
    virtual bool isPaused() const;

protected:
    GstElement* m_audioSink;
    bool m_paused;
    bool m_isLive;
    bool m_seeking;
    bool m_pendingSeek;
    bool m_bufferingFinished;
    float m_playbackRate;
    float m_seekTime;
    Nix::MediaPlayerClient::ReadyState m_readyState;
    Nix::MediaPlayerClient::NetworkState m_networkState;

    static void onGstBusMessage(GstBus*, GstMessage*, GstreamerBackendBase* backend);
    virtual bool createAudioSink() { return false; }
    virtual void destroyAudioSink() {}
    virtual void handleMessage(GstMessage *message) {}
};

#endif // GstreamerBackendBase_h
