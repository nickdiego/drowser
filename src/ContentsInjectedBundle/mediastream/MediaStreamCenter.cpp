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

#include "MediaStreamCenter.h"
#include "MediaStreamCenterGStreamer.h"

#include <NixPlatform/MediaStreamAudioSource.h>
#include <NixPlatform/MediaStreamCenter.h>
#include <NixPlatform/MediaStreamSource.h>

#include <cassert>
#include <cstdio>
#include <glib.h>
#include <gst/gst.h>

MediaStreamCenter::MediaStreamCenter(): m_impl(MediaStreamCenterGStreamer::create())
{
}

MediaStreamCenter::~MediaStreamCenter()
{
    // FIXME use std::unique_ptr instead of manual memory management ??
    delete m_impl;
}

const char* MediaStreamCenter::validateRequestConstraints(Nix::MediaConstraints& audioConstraints, Nix::MediaConstraints& videoConstraints)
{
    // FIXME: Check if constraints are ok.
    return nullptr;
}

Nix::MediaStream MediaStreamCenter::createMediaStream(Nix::MediaConstraints& audioConstraints, Nix::MediaConstraints& videoConstraints)
{
    std::vector<Nix::MediaStreamSource*> audioSources;
    std::vector<Nix::MediaStreamSource*> videoSources;

    if (!audioConstraints.isNull()) {
        Nix::MediaStreamSource* audioSource = m_impl->firstSource(Nix::MediaStreamSource::Audio);

        assert(audioSource != nullptr);
        LOG(Media, "%s got audio source id='%s'", __PRETTY_FUNCTION__, audioSource->id());

        audioSources.push_back(audioSource);
    }

    if (!videoConstraints.isNull()) {
        // Not supported.
    }

    Nix::MediaStream mediaStream;
    mediaStream.initialize(audioSources, videoSources);
    return mediaStream;
}
