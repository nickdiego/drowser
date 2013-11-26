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

#ifndef GstreamerStreamBackend_h
#define GstreamerStreamBackend_h

#include "GstreamerBackendBase.h"

#include <NixPlatform/MediaStream.h>
#include <NixPlatform/MediaStreamSource.h>

#include <string>

class GstreamerStreamBackend : public GstreamerBackendBase, private Nix::MediaStreamSource::Observer
{
public:
    GstreamerStreamBackend(Nix::MediaPlayerClient *client);
    virtual ~GstreamerStreamBackend();

    virtual void load(const char* url);
    virtual void sourceStateChanged();
    virtual void sourceMutedChanged();
    virtual void sourceEnabledChanged();
    virtual bool stopped();
    void stop();

protected:
    virtual bool createAudioSink() override;
    virtual void destroyAudioSink() override;
    virtual void handleMessage(GstMessage *message);

private:
    Nix::MediaStream* m_stream;
    std::string m_audioSourceId;
    bool m_stopped;

    void setDownloadBuffering();
    void updateStates();
};

#endif // GstreamerStreamBackend_h
