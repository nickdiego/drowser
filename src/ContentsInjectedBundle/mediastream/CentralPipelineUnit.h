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

#ifndef CentralPipelineUnit_h
#define CentralPipelineUnit_h

#include <map>
#include <string>

typedef struct _GstElement GstElement;
typedef struct _GstMessage GstMessage;
typedef struct _GstPad GstPad;

class SourceFactory;

class CentralPipelineUnit {
protected:
    CentralPipelineUnit();
public:
    virtual ~CentralPipelineUnit();

    static CentralPipelineUnit& instance();

    bool handleMessage(GstMessage*);

    void registerSourceFactory(SourceFactory*, const std::string& sourceId);
    void deregisterSourceFactory(const std::string& sourceId);
    bool connectToSource(const std::string& sourceId, GstElement* sink, GstPad* sinkPad = 0);
    bool disconnectFromSource(const std::string& sourceId, GstElement* sink, GstPad* sinkPad = 0);

    // ideally this should be private, but keeping it public to avoid requiring
    // the C callbacks to be static members (and thus exposing internal
    // gstreamer API in this header)
    void disconnectSinkFromPipelinePadBlocked(GstElement* sink, GstPad* sourcePad);
    void disconnectUnusedSource(GstElement* tee);

    GstElement* pipeline() const;

private:
    class Source {
    public:
        Source() : m_sourceElement(0), m_sourcePad(0), m_sourceFactory(0), m_removeWhenNotUsed(true) { }
        Source(GstElement* sourceElement, GstPad* sourcePad, SourceFactory* sourceFactory, bool removeWhenNotUsed)
            : m_sourceElement(sourceElement)
            , m_sourcePad(sourcePad)
            , m_sourceFactory(sourceFactory)
            , m_removeWhenNotUsed(removeWhenNotUsed)
        { }

        GstElement* m_sourceElement;
        GstPad* m_sourcePad;
        SourceFactory* m_sourceFactory;
        bool m_removeWhenNotUsed;
    };

    typedef std::map<std::string, Source> PipelineMap;

    typedef std::map<std::string, std::string> SourceIdLookupMap;
    typedef std::map<std::string, SourceFactory*> SourceFactoryMap;

    bool connectAndGetSourceElement(const std::string& sourceId, GstElement*& sourceElement, GstPad*& sourcePad);

    void disconnectSinkFromTee(GstElement* tee, GstElement* sink, GstPad*);

    GstElement* m_pipeline;
    PipelineMap m_pipelineMap;
    SourceIdLookupMap m_sourceIdLookupMap;
    SourceFactoryMap m_sourceFactoryMap;
    unsigned long m_probeId;
};

#endif // CentralPipelineUnit_h
