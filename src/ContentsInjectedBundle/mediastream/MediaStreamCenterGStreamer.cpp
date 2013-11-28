/*
 *  Copyright (C) 2012 Collabora Ltd. All rights reserved.
 *  Copyright (C) 2013 Igalia S.L. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "MediaStreamCenterGStreamer.h"
#include "CentralPipelineUnit.h"

#include <gst/gst.h>

using std::string;
using std::vector;

MediaStreamCenterGStreamer* MediaStreamCenterGStreamer::create()
{
    return new MediaStreamCenterGStreamer();
}

std::string createUniqueName(const char* prefix)
{
    static int count = 0;
    return prefix + std::to_string(count++);
}

static GstElement* findDeviceSource(const char* elementName)
{
    GstElement* deviceSrc = nullptr;
    GstElement* autoSrc = gst_element_factory_make(elementName, 0);

    if (!autoSrc)
        return deviceSrc;

    GstChildProxy* childProxy = GST_CHILD_PROXY(autoSrc);
    if (!childProxy)
        return deviceSrc;

    GstStateChangeReturn stateChangeResult = gst_element_set_state(autoSrc, GST_STATE_READY);
    if (stateChangeResult != GST_STATE_CHANGE_SUCCESS)
        return deviceSrc;

    if (gst_child_proxy_get_children_count(childProxy))
        deviceSrc = GST_ELEMENT(gst_child_proxy_get_child_by_index(childProxy, 0));

    gst_element_set_state(autoSrc, GST_STATE_NULL);
    return deviceSrc;
}

static void probeSource(GstElement* source, const string& key, Nix::MediaStreamSource::Type type, vector<MediaStreamSourceGStreamer*>& sources)
{
    // FIXME: gstreamer 1.0 doesn't have an equivalent to GstPropertyProbe,
    // so we don't support choosing a particular device yet.
    // see https://bugzilla.gnome.org/show_bug.cgi?id=678402

    string deviceId = key;
    deviceId.append(";default");

    // FIXME: g_object_get is putting a null pointer into deviceNameBuffer.
    // Investigate it later
    char* deviceNameBuffer;
    g_object_get(source, "device-name", &deviceNameBuffer, NULL);
    string deviceName = (deviceNameBuffer) ? deviceNameBuffer: "Default Audio Source";

    LOG_MEDIA_MESSAGE("deviceID:'%s' deviceName:'%s'", deviceId.c_str(), deviceName.c_str());
    MediaStreamSourceGStreamer* mediaStreamSource = new MediaStreamSourceGStreamer(deviceId, deviceName, "default", key);

    // TODO: fill source capabilities and states.

    sources.push_back(mediaStreamSource);
}

static string getDeviceStringFromDeviceIdString(const string& deviceId)
{
    size_t index = deviceId.find(";");
    string deviceStr = deviceId.substr(index + 1, deviceId.length() - index);
    return deviceStr;
}

static string getFactoryNameFromDeviceIdString(const string& deviceId)
{
    size_t index = deviceId.find(";");
    string deviceStr = deviceId.substr(0, index);
    return deviceStr;
}

static GstElement* createAudioSourceBin(GstElement* source)
{
    GstElement* audioconvert = gst_element_factory_make("audioconvert", 0);
    if (!audioconvert) {
        LOG_MEDIA_MESSAGE("ERROR, Got no audioconvert element for audio source pipeline");
        return nullptr;
    }

    GstCaps* audiocaps;
    audiocaps = gst_caps_new_simple("audio/x-raw", "channels", G_TYPE_INT, 1, NULL);
    if (!audiocaps) {
        LOG_MEDIA_MESSAGE("ERROR, Unable to create filter caps for audio source pipeline");
        return nullptr;
    }

    string binName(createUniqueName("StreamDevice_AudioTestSourceBin"));
    GstElement* audioSourceBin = gst_bin_new(binName.c_str());

    gst_bin_add_many(GST_BIN(audioSourceBin), source, audioconvert, NULL);

    if (!gst_element_link_filtered(source, audioconvert, audiocaps)) {
        LOG_MEDIA_MESSAGE("ERROR, Cannot link audio source elements");
        return nullptr;
    }

    GstPad* srcPad = gst_element_get_static_pad(audioconvert, "src");
    gst_element_add_pad(audioSourceBin, gst_ghost_pad_new("src", srcPad));

    return audioSourceBin;
}

static GstElement* createVideoSourceBin(GstElement* source)
{
    GstElement* colorspace = gst_element_factory_make("videoconvert", 0);
    if (!colorspace) {
        LOG_MEDIA_MESSAGE("ERROR, Got no videoconvert element for video source pipeline");
        return nullptr;
    }
    GstElement* videoscale = gst_element_factory_make("videoscale", 0);
    if (!videoscale) {
        LOG_MEDIA_MESSAGE("ERROR, Got no videoscale element for video source pipeline");
        return nullptr;
    }

    GstCaps* videocaps = gst_caps_new_simple("video/x-raw", "width", G_TYPE_INT, 320, "height", G_TYPE_INT, 240, NULL);
    if (!videocaps) {
        LOG_MEDIA_MESSAGE("ERROR, Unable to create filter caps for video source pipeline");
        return nullptr;
    }

    string binName(createUniqueName("StreamDevice_VideoTestSourceBin"));
    GstElement* videoSourceBin = gst_bin_new(binName.c_str());

    gst_bin_add_many(GST_BIN(videoSourceBin), source, videoscale, colorspace, NULL);

    if (!gst_element_link_many(source, videoscale, NULL)
        || !gst_element_link_filtered(videoscale, colorspace, videocaps)) {
        LOG_MEDIA_MESSAGE("ERROR, Cannot link video source elements");
        gst_object_unref(videoSourceBin);
        return nullptr;
    }

    GstPad* srcPad = gst_element_get_static_pad(colorspace, "src");
    gst_element_add_pad(videoSourceBin, gst_ghost_pad_new("src", srcPad));

    return videoSourceBin;
}

//---- MediaStreamCenterGStreamer's function

MediaStreamCenterGStreamer::MediaStreamCenterGStreamer()
{
    //initializeGStreamer(); // FIXME really needed??

    LOG_MEDIA_MESSAGE("Discovering media source devices");
    vector<MediaStreamSourceGStreamer*> sources;

    GstElement* audioSrc = findDeviceSource("autoaudiosrc");
    if (audioSrc) {
        string key = storeElementFactoryForElement(audioSrc);
        probeSource(audioSrc, key, Nix::MediaStreamSource::Audio, sources);
    }

    //FIXME we don't support video yet
    /*GstElement* videoSrc = findDeviceSource("autovideosrc");
    if (videoSrc) {
        string key = storeElementFactoryForElement(videoSrc);
        probeSource(videoSrc, key, Nix::MediaStreamSource::Video, sources);
    }*/

    registerSourceFactories(sources);
}

void MediaStreamCenterGStreamer::registerSourceFactories(const std::vector<MediaStreamSourceGStreamer*>& sources)
{
    CentralPipelineUnit& cpu = CentralPipelineUnit::instance();

    for (size_t i = 0; i < sources.size(); i++) {
        const string& sourceId = storeSourceInfo(sources[i]);
        LOG_MEDIA_MESSAGE("Registering source factory for source with id=\"%s\"", sourceId.c_str());
        cpu.registerSourceFactory(this, sourceId);
    }
}

std::string MediaStreamCenterGStreamer::storeElementFactoryForElement(GstElement* source)
{
    GstElementFactory* elementFactory = gst_element_get_factory(source);
    string strFactoryName(gst_element_get_name(elementFactory));

    ElementFactoryMap::iterator elementFactoryIt = m_elementFactoryMap.find(strFactoryName);
    if (elementFactoryIt == m_elementFactoryMap.end())
        m_elementFactoryMap.insert(std::make_pair(strFactoryName, elementFactory));

    return strFactoryName;
}

GstElementFactory* MediaStreamCenterGStreamer::storedElementFactory(const string& key)
{
    GstElementFactory* elementFactory = 0;
    ElementFactoryMap::iterator elementFactoryIt = m_elementFactoryMap.find(key);
    if (elementFactoryIt != m_elementFactoryMap.end())
        elementFactory = elementFactoryIt->second;
    return elementFactory;
}

string MediaStreamCenterGStreamer::storeSourceInfo(MediaStreamSourceGStreamer* source)
{
    string id = g_strdup(source->id());
    string deviceString = getDeviceStringFromDeviceIdString(id);
    string factoryString = getFactoryNameFromDeviceIdString(id);

    string key(factoryString);
    key += ";" + deviceString;

    LOG_MEDIA_MESSAGE("Registering source details - id='%s' device='%s' factory='%s' key='%s'",
        id.c_str(), deviceString.c_str(), factoryString.c_str(), key.c_str());

    MediaStreamSourceGStreamerMap::iterator sourceIterator = m_sourceMap.find(key);
    if (sourceIterator != m_sourceMap.end())
        return sourceIterator->first;

    m_sourceMap.insert(std::make_pair(key, source));
    return key;
}

// SourceFactory
GstElement* MediaStreamCenterGStreamer::createSource(const string& sourceId, GstPad*& srcPad)
{
    LOG_MEDIA_MESSAGE("Creating source with id='%s'", sourceId.c_str());

    MediaStreamSourceGStreamerMap::iterator sourceIterator = m_sourceMap.find(sourceId);
    if (sourceIterator == m_sourceMap.end())
        return nullptr;

    MediaStreamSourceGStreamer* mediaStreamSource = sourceIterator->second;

    GstElementFactory* factory = storedElementFactory(mediaStreamSource->factoryKey());
    if (!factory)
        return nullptr;

    GstElement* source = gst_element_factory_create(factory, "device_source");
    if (!source)
        return nullptr;

    LOG_MEDIA_MESSAGE("sourceInfo.m_device=%s", mediaStreamSource->device().c_str());
    // device choosing is not implemented for gstreamer 1.0 yet

    if (mediaStreamSource->type() == Nix::MediaStreamSource::Audio)
        source = createAudioSourceBin(source);
    else if (mediaStreamSource->type() == Nix::MediaStreamSource::Video)
        source = createVideoSourceBin(source);

    srcPad = gst_element_get_static_pad(source, "src");

    return source;
}

Nix::MediaStreamSource* MediaStreamCenterGStreamer::firstSource(Nix::MediaStreamSource::Type type)
{
    for (auto iter = m_sourceMap.begin(); iter != m_sourceMap.end(); ++iter) {
        LOG_MEDIA_MESSAGE("findSource xxx");
        Nix::MediaStreamSource* source = iter->second;
        if (source->type() == type)
            return source;
    }

    return nullptr;
}

