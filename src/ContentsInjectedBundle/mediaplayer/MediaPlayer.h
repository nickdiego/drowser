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

#ifndef MediaPlayer_h
#define MediaPlayer_h

#include <NixPlatform/MediaPlayer.h>

class MediaPlayerBackend;

namespace Nix {
class MediaStream;
}

class MediaPlayer : public Nix::MediaPlayer
{
public:
    MediaPlayer(Nix::MediaPlayerClient*);
    virtual ~MediaPlayer();

    virtual void setSrc(const char* url) override;
    virtual void setSrc(Nix::MediaStream* stream) override;
    virtual void load() override;
    virtual void play() override;
    virtual void pause() override;
    virtual float duration() const override;
    virtual float currentTime() const override;
    virtual void seek(float) override;
    virtual void setVolume(float) override;
    virtual void setMuted(bool) override;
    virtual bool seeking() const override;
    virtual float maxTimeSeekable() const override;
    virtual void setPlaybackRate(float) override;
    virtual bool isLiveStream() const override;

private:
    MediaPlayerBackend *m_backend;
};

#endif // MediaPlayer_h
