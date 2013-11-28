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

#ifndef MediaStreamPlayerBackend_h
#define MediaStreamPlayerBackend_h

#include "MediaPlayerBackendBase.h"

#include <NixPlatform/MediaStream.h>
#include <NixPlatform/MediaStreamSource.h>

#include <string>

class MediaStreamPlayerBackend : public MediaPlayerBackendBase, private Nix::MediaStreamSource::Observer
{
public:
    MediaStreamPlayerBackend(Nix::MediaPlayerClient *client);
    virtual ~MediaStreamPlayerBackend();

    // MediaPlayerBackendBase methods
    virtual void load(const char* url) override;
    virtual void play() override;
    virtual void pause() override;

    virtual float duration() const override { return 0; }
    virtual float currentTime() const override { return 0; }
    virtual void seek(float) override { }
    virtual bool seeking() const override { return false; }
    virtual float maxTimeSeekable() const override { return 0; }
    virtual void setPlaybackRate(float) override { }

    // MediaPlayerBackendBase
    virtual GstElement* pipeline() const override;

    // Nix::MediaStreamSource::Observer methods
    virtual void sourceReadyStateChanged() override;
    virtual void sourceMutedChanged() override;
    virtual void sourceEnabledChanged() override;
protected:
    virtual bool createAudioSink() override;
    virtual void destroyAudioSink() override;
    void stop();
    bool stopped();

    void loadingFailed(Nix::MediaPlayerClient::NetworkState networkState); //FIXME Move to MediaPlayerBackendBase ??
    bool internalLoad();
    bool connectToGSTLiveStream(Nix::MediaStream* stream);

private:
    Nix::MediaStream* m_stream;
    std::string m_audioSourceId;
    GstElement* m_audioSinkBin;
    bool m_stopped;

    void setDownloadBuffering();
};

#endif // MediaStreamPlayerBackend_h
