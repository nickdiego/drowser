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

#include "CentralPipelineUnit.h"
#include "SourceFactory.h"

#include <gst/gst.h>

using std::string;
using std::map;

template <class T>
GstIteratorResult webkitGstIteratorNext(GstIterator* iterator, T*& element)
{
    GValue item = G_VALUE_INIT;
    GstIteratorResult result = gst_iterator_next(iterator, &item);
    if (result == GST_ITERATOR_OK) {
        element = static_cast<T*>(g_value_get_object(&item));
    }
    return result;
}

static string generateElementPadId(GstElement* element, GstPad* pad)
{
    // FIXME long ???
    string id(std::to_string((unsigned long)(element)));
    id.append("_");
    id.append(std::to_string((unsigned long)(pad)));
    return id;
}

static bool messageCallback(GstBus*, GstMessage* message, CentralPipelineUnit* cpu)
{
    return cpu->handleMessage(message);
}

CentralPipelineUnit& CentralPipelineUnit::instance()
{
//    ASSERT(isMainThread()); ??? :(
    static CentralPipelineUnit instance;
    return instance;
}

CentralPipelineUnit::CentralPipelineUnit()
{
//    initializeGStreamer(); ??
    m_pipeline = gst_pipeline_new("mediastream_pipeline");
    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipeline));
    if (bus) {
        gst_bus_add_signal_watch(bus);
        gst_bus_set_sync_handler(bus, gst_bus_sync_signal_handler, 0, 0);
        g_signal_connect(bus, "sync-message", G_CALLBACK(messageCallback), this);
    }
    gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
}

CentralPipelineUnit::~CentralPipelineUnit()
{
    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipeline));
    if (bus)
        gst_bus_remove_signal_watch(bus);
}

void CentralPipelineUnit::registerSourceFactory(SourceFactory* factory, const string& sourceId)
{
    m_sourceFactoryMap.insert(std::make_pair(sourceId, factory));
}

void CentralPipelineUnit::deregisterSourceFactory(const string& sourceId)
{
    m_sourceFactoryMap.erase(sourceId);
}

bool CentralPipelineUnit::connectAndGetSourceElement(const string& sourceId, GstElement*& sourceElement, GstPad*& sourcePad)
{
    LOG_MEDIA_MESSAGE("Connecting to source element %s", sourceId.c_str());

    CentralPipelineUnit::PipelineMap::iterator sourceIterator = m_pipelineMap.find(sourceId);
    if (sourceIterator != m_pipelineMap.end()) {
        Source& source = sourceIterator->second;
        LOG_MEDIA_MESSAGE("Source element %s already in pipeline, using it.", sourceId.c_str());
        sourceElement = source.m_sourceElement;
        sourcePad = source.m_sourcePad;
        return true;
    }

    CentralPipelineUnit::SourceFactoryMap::iterator sourceFactoryIterator = m_sourceFactoryMap.find(sourceId);
    if (sourceFactoryIterator != m_sourceFactoryMap.end()) {
        LOG_MEDIA_MESSAGE("Found a SourceFactory. Creating source element %s", sourceId.c_str());
        SourceFactory* sourceFactory = sourceFactoryIterator->second;
        sourceElement = sourceFactory->createSource(sourceId, sourcePad);
        if (!sourceElement) {
            LOG_MEDIA_MESSAGE("ERROR, unable to create source element");
            return false;
        }

        if (!sourcePad) {
            LOG_MEDIA_MESSAGE("SourceFactory could not create a source pad, trying the element static \"src\" pad");
            sourcePad = gst_element_get_static_pad(sourceElement, "src");
        }

        if (!sourcePad) {
            LOG_MEDIA_MESSAGE("ERROR, unable to retrieve element source pad");
            gst_object_unref(sourceElement);
            sourceElement = nullptr;
            return false;
        }

        GstElement* tee = gst_element_factory_make("tee", 0);
        if (!tee) {
            LOG_MEDIA_MESSAGE("ERROR, Got no tee element");
            gst_object_unref(sourceElement);
            gst_object_unref(sourcePad);
            sourceElement = nullptr;
            sourcePad = nullptr;
            return false;
        }

        gst_bin_add_many(GST_BIN(m_pipeline), sourceElement, tee, NULL);

        gst_element_sync_state_with_parent(sourceElement);
        gst_element_sync_state_with_parent(tee);

        GstPad* sinkpad = gst_element_get_static_pad(tee, "sink");
        gst_pad_link(sourcePad, sinkpad);
    }
    return false;
}

bool CentralPipelineUnit::connectToSource(const string& sourceId, GstElement* sink, GstPad* sinkPad)
{
    GstPad* refSinkPad = sinkPad;
    LOG_MEDIA_MESSAGE("Connecting to source with id=%s, sink=%p, sinkpad=%p", sourceId.c_str(), sink, sinkPad);

    if (!sink || sourceId.empty()) {
        LOG_MEDIA_MESSAGE("ERROR, No sink provided or empty source id");
        return false;
    }

    if (!refSinkPad) {
        LOG_MEDIA_MESSAGE("No pad was given as argument, trying the element static \"sink\" pad.");
        refSinkPad = gst_element_get_static_pad(sink, "sink");
        if (!refSinkPad) {
            LOG_MEDIA_MESSAGE("ERROR, Unable to retrieve element sink pad");
            return false;
        }
    }

    GstElement* sourceElement;
    GstPad* sourcePad;

    bool haveSources = connectAndGetSourceElement(sourceId, sourceElement, sourcePad);
    if (!sourceElement) {
        LOG_MEDIA_MESSAGE("ERROR, Unable to get source element");
        return false;
    }

    GstElement* queue = gst_element_factory_make("queue", 0);
    if (!queue) {
        LOG_MEDIA_MESSAGE("ERROR, Got no queue element");
        return false;
    }

    GstElement* sinkParent = GST_ELEMENT(gst_element_get_parent(sink));
    if (!sinkParent) {
        LOG_MEDIA_MESSAGE("Sink not in pipeline, adding.");
        gst_bin_add(GST_BIN(m_pipeline), sink);
    } else if (sinkParent != m_pipeline) {
        LOG_MEDIA_MESSAGE("ERROR, Sink already added to another element. Pipeline is now broken!");
        return false;
    }

    gst_bin_add(GST_BIN(m_pipeline), queue);

    GstPad* teeSinkPad = gst_pad_get_peer(sourcePad);
    GstElement* tee = gst_pad_get_parent_element(teeSinkPad);

    gst_element_sync_state_with_parent(sink);
    gst_element_sync_state_with_parent(queue);

    gst_element_link_pads_full(tee, 0, queue, "sink", GST_PAD_LINK_CHECK_DEFAULT);
    GstPad* queueSrcPad = gst_element_get_static_pad(queue, "src");
    gst_pad_link(queueSrcPad, refSinkPad);

    if (!haveSources) {
        m_pipelineMap.insert(std::make_pair(sourceId, Source(sourceElement, sourcePad, 0, true)));
        m_sourceIdLookupMap.insert(std::make_pair(generateElementPadId(sourceElement, sourcePad), sourceId));
    }

    gst_element_set_state(m_pipeline, GST_STATE_PLAYING);

    return true;
}
bool CentralPipelineUnit::disconnectFromSource(const string& sourceId, GstElement* sink, GstPad* sinkPad)
{
    GstPad* refSinkPad = sinkPad;
    LOG_MEDIA_MESSAGE("Disconnecting from source with id=%s, sink=%p, sinkpad=%p", sourceId.c_str(), sink, sinkPad);

    if (!sink || sourceId.empty()) {
        LOG_MEDIA_MESSAGE("ERROR, No sink provided or empty source id");
        return false;
    }

    PipelineMap::iterator sourceIterator = m_pipelineMap.find(sourceId);
    if (sourceIterator == m_pipelineMap.end()) {
        LOG_MEDIA_MESSAGE("Could not find source with id=%s", sourceId.c_str());
        return false;
    }

    if (!refSinkPad) {
        LOG_MEDIA_MESSAGE("No pad was given as argument, trying the element static \"sink\" pad.");
        refSinkPad = gst_element_get_static_pad(sink, "sink");
        if (!refSinkPad) {
            LOG_MEDIA_MESSAGE("ERROR, Unable to retrieve element sink pad");
            return false;
        }
    }

    GstPad* lQueueSrcPad = gst_pad_get_peer(refSinkPad);
    GstElement* lQueue = gst_pad_get_parent_element(lQueueSrcPad);
    GstPad* lQueueSinkPad = gst_element_get_static_pad(lQueue, "sink");
    GstPad* lTeeSrcPad = gst_pad_get_peer(lQueueSinkPad);
    GstElement* lTee = gst_pad_get_parent_element(lTeeSrcPad);

    disconnectSinkFromTee(lTee, sink, refSinkPad);
    return true;
}

void CentralPipelineUnit::disconnectUnusedSource(GstElement* tee)
{
    GstPad* teeSinkPad = gst_element_get_static_pad(tee, "sink");
    GstPad* sourceSrcPad = gst_pad_get_peer(teeSinkPad);
    GstElement* source = gst_pad_get_parent_element(sourceSrcPad);

    SourceIdLookupMap::iterator sourceIdIterator = m_sourceIdLookupMap.find(generateElementPadId(source, sourceSrcPad));
    if (sourceIdIterator != m_sourceIdLookupMap.end()) {
        string sourceId = sourceIdIterator->second;

        PipelineMap::iterator sourceInfoIterator = m_pipelineMap.find(sourceId);
        if (sourceInfoIterator != m_pipelineMap.end()) {
            Source sourceInfo = sourceInfoIterator->second;

            if (sourceInfo.m_removeWhenNotUsed) {
                LOG_MEDIA_MESSAGE("Removing source %p with id=%s", source, sourceId.c_str());
                gst_element_set_state(source, GST_STATE_NULL);
                gst_element_set_state(tee, GST_STATE_NULL);
                gst_bin_remove(GST_BIN(m_pipeline), source);
                gst_bin_remove(GST_BIN(m_pipeline), tee);

                m_pipelineMap.erase(sourceId);
                m_sourceIdLookupMap.erase(generateElementPadId(source, sourceInfo.m_sourcePad));
            }
        }
    }
}

class DisconnectSinkJob {
public:
    DisconnectSinkJob(CentralPipelineUnit* cpu, GstElement* sink, GstPad* pad) :
        m_cpu(cpu)
        , m_sink(sink)
        , m_pad(pad)
    { }

    CentralPipelineUnit* m_cpu;
    GstElement* m_sink;
    GstPad* m_pad;
};

static gboolean dropElement(gpointer userData)
{
    DisconnectSinkJob* job = reinterpret_cast<DisconnectSinkJob*>(userData);
    job->m_cpu->disconnectSinkFromPipelinePadBlocked(job->m_sink, job->m_pad);
    delete job;
    return false;
}

static GstPadProbeReturn elementDrainedCallback(GstPad* pad, GstPadProbeInfo* info, gpointer userData)
{
    g_idle_add(dropElement, userData);
    return GST_PAD_PROBE_DROP;
}

GstElement* CentralPipelineUnit::pipeline() const
{
    return m_pipeline;
}

static GstPadProbeReturn disconnectSinkFromPipelineProbeAddedProxy(GstPad* pad, GstPadProbeInfo* info, gpointer userData)
{
    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));

    DisconnectSinkJob* job = reinterpret_cast<DisconnectSinkJob*>(userData);
    GstPad* queueSinkPad = gst_pad_get_peer(job->m_pad);
    GstElement* queue = gst_pad_get_parent_element(queueSinkPad);
    GstPad* queueSourcePad = gst_element_get_static_pad(queue, "src");

    // Install new probe for EOS.
    gst_pad_add_probe(queueSourcePad, static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_BLOCK | GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM),
        elementDrainedCallback, userData, 0);

    // Push EOS into the element, the probe will be fired when the
    // EOS leaves the queue and it has thus drained all of its data
    gst_pad_send_event(queueSinkPad, gst_event_new_eos());

    return GST_PAD_PROBE_OK;
}

void CentralPipelineUnit::disconnectSinkFromPipelinePadBlocked(GstElement* sink, GstPad* teeSourcePad)
{
    GstElement* tee = gst_pad_get_parent_element(teeSourcePad);

    GstPad* queueSinkPad = gst_pad_get_peer(teeSourcePad);
    GstElement* queue = gst_pad_get_parent_element(queueSinkPad);
    GstPad* queueSourcePad = gst_element_get_static_pad(queue, "src");

    gst_pad_set_active(teeSourcePad, FALSE);
    gst_pad_set_active(queueSourcePad, FALSE);
    gst_element_set_state(queue, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(m_pipeline), queue);
    gst_element_release_request_pad(tee, teeSourcePad);

    GstIterator* padsIterator = gst_element_iterate_sink_pads(sink);
    bool done = false;
    bool doRemoveSink = true;
    while (!done) {
        GstPad* pad;
        GstIteratorResult it = webkitGstIteratorNext(padsIterator, pad);
        switch (it) {
        case GST_ITERATOR_OK: {
            GstPad* peer = gst_pad_get_peer(pad);
            if (peer) {
                doRemoveSink = false;
                done = true;
            }
            break;
        }
        case GST_ITERATOR_RESYNC:
            gst_iterator_resync(padsIterator);
            break;
        case GST_ITERATOR_DONE:
            done = true;
            break;
        case GST_ITERATOR_ERROR:
            LOG_MEDIA_MESSAGE("ERROR, Iterating!");
            done = true;
            break;
        default:
            break;
        }
    }
    gst_iterator_free(padsIterator);

    string sourceAndPadId = generateElementPadId(sink, 0);
    SourceIdLookupMap::iterator sourceIdIterator = m_sourceIdLookupMap.find(generateElementPadId(sink, 0));
    if (sourceIdIterator != m_sourceIdLookupMap.end()) {
        string sourceId = sourceIdIterator->second;
        PipelineMap::iterator sourceInfoIterator = m_pipelineMap.find(sourceId);
        if (sourceInfoIterator != m_pipelineMap.end()) {
            Source sourceInfo = sourceInfoIterator->second;
            doRemoveSink = sourceInfo.m_removeWhenNotUsed;
        }
    }

    if (doRemoveSink) {
        gst_element_set_state(sink, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(m_pipeline), sink);
        LOG_MEDIA_MESSAGE("Sink removed");
        //GstPad* teeSinkPad = gst_element_get_static_pad(tee, "sink");
        //GstPad* sourceSrcPad = gst_pad_get_peer(teeSinkPad);
        //GstElement* source = gst_pad_get_parent_element(sourceSrcPad);
        disconnectUnusedSource(tee);
    } else
        LOG_MEDIA_MESSAGE("Did not remove the sink");

}

void CentralPipelineUnit::disconnectSinkFromTee(GstElement* tee, GstElement* sink, GstPad* pad)
{
    GstPad* sinkSinkPad = pad;
    if (!pad)
        sinkSinkPad = gst_element_get_static_pad(sink, "sink");

    LOG_MEDIA_MESSAGE("disconnecting sink %s from tee", GST_OBJECT_NAME(sink));

    GstPad* queueSourcePad = gst_pad_get_peer(sinkSinkPad);
    GstElement* queue = gst_pad_get_parent_element(queueSourcePad);
    GstPad* queueSinkPad = gst_element_get_static_pad(queue, "sink");
    GstPad* teeSourcePad = gst_pad_get_peer(queueSinkPad);

    DisconnectSinkJob* job = new DisconnectSinkJob(this, sink, teeSourcePad);

    gst_pad_add_probe(teeSourcePad, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
        disconnectSinkFromPipelineProbeAddedProxy, job, 0);
}

bool CentralPipelineUnit::handleMessage(GstMessage* message)
{
    GError* error;
    char* debug;

    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR: {
        gst_message_parse_error(message, &error, &debug);
        LOG_MEDIA_MESSAGE("Media error: %d, %s", error->code, error->message);
        //GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, "webkit-mediastream.error");
        gst_object_unref(error);
        break;
    }
    default:
        break;
    }

    return true;
}


