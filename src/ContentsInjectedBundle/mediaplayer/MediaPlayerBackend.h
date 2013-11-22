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

#ifndef MediaPlayerBackend_h
#define MediaPlayerBackend_h

#include <NixPlatform/MediaPlayer.h>

class MediaPlayerBackend
{
public:
    MediaPlayerBackend(Nix::MediaPlayerClient* client);
    virtual ~MediaPlayerBackend();

    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void seek(float) = 0;
    virtual float duration() const = 0;
    virtual float currentTime() const = 0;
    virtual void setVolume(float) = 0;
    virtual void setMuted(bool) = 0;
    virtual void load(const char* url) = 0;
    virtual bool seeking() const = 0;
    virtual float maxTimeSeekable() const = 0;
    virtual void setPlaybackRate(float) = 0;
    virtual bool isLiveStream() const = 0;
    virtual bool isPaused() const = 0;

protected:
    Nix::MediaPlayerClient *m_playerClient;
};

#endif // MediaPlayerBackend_h
