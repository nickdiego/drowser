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

#ifndef MediaStreamCenterGStreamer_h
#define MediaStreamCenterGStreamer_h

#include "SourceFactory.h"

#include <NixPlatform/MediaStreamAudioSource.h>

#include <map>
#include <string>
#include <vector>

typedef struct _GstElementFactory GstElementFactory;

class MediaStreamSourceGStreamer: public Nix::MediaStreamAudioSource {
public:
MediaStreamSourceGStreamer(const std::string& id, const std::string& name, const std::string& device, const std::string& factoryKey)
    : Nix::MediaStreamAudioSource(id.c_str(), name.c_str())
    , m_factoryKey(factoryKey)
    , m_device(device)
    {
        LOG_MEDIA_MESSAGE("'%s' '%s'", m_factoryKey.c_str(), m_device.c_str());
        LOG_MEDIA_MESSAGE("id '%s'", this->id());
    }

    virtual ~MediaStreamSourceGStreamer()
    {
        LOG_MEDIA_MESSAGE("%p", this);
    }

    const std::string& factoryKey() const { return m_factoryKey; }
    const std::string& device() const { return m_device; }

private:

    std::string m_factoryKey;
    std::string m_device;
};

typedef std::map<std::string, MediaStreamSourceGStreamer*> MediaStreamSourceGStreamerMap;

class MediaStreamCenterGStreamer: private SourceFactory {

public:
    static MediaStreamCenterGStreamer* create();

    MediaStreamSourceGStreamerMap& sourceMap() { return m_sourceMap; }
    Nix::MediaStreamSource* firstSource(Nix::MediaStreamSource::Type);

private:
    typedef std::map<std::string, GstElementFactory*> ElementFactoryMap;

private:
    MediaStreamCenterGStreamer();

    void registerSourceFactories(const std::vector<MediaStreamSourceGStreamer*>&);
    std::string storeElementFactoryForElement(GstElement* source);
    GstElementFactory* storedElementFactory(const std::string& key);
    std::string storeSourceInfo(MediaStreamSourceGStreamer*);

    // SourceFactory
    virtual GstElement* createSource(const std::string& sourceId, GstPad*& srcPad) override;

    ElementFactoryMap m_elementFactoryMap;
    MediaStreamSourceGStreamerMap m_sourceMap;
};

#endif // MediaStreamCenterGStreamer_h
