/*
 *  Copyright (C) 2012 Collabora Ltd. All rights reserved.
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

#ifndef SourceFactory_h
#define SourceFactory_h

#include <string>

typedef struct _GstPad GstPad;
typedef struct _GstElement GstElement;

class SourceFactory {
public:
    SourceFactory() { }
    virtual GstElement* createSource(const std::string& sourceId, GstPad*& srcPad) = 0;

protected:
    virtual ~SourceFactory() { }
};

#endif // SourceFactory_h

