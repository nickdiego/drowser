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

#ifndef MediaStreamPlayer_h
#define MediaStreamPlayer_h

#include <NixPlatform/MediaStream.h>
#include <NixPlatform/MediaPlayer.h>
#include <gst/gst.h>

#include <string>
#include <pair>
#include <set>

namespace WebCore {

// class MediaStreamDescriptor; TODO

class MediaStreamPlayer: public Nix::MediaPlayer /*, private MediaStreamSource::Observer*/ {
//    friend void mediaPlayerPrivateRepaintCallback(WebKitVideoSink*, GstBuffer*, StreamMediaPlayerPrivateGStreamer*);
public:
    ~StreamMediaPlayerPrivateGStreamer();
    static void registerMediaEngine(MediaEngineRegistrar);

    virtual void load(const std::string&);
    virtual void cancelLoad() { }

    virtual void prepareToPlay() { }
    void play();
    void pause();

    bool hasVideo() const;
    bool hasAudio() const;

    virtual float duration() const { return 0; }

    virtual float currentTime() const { return 0; }
    virtual void seek(float) { }
    virtual bool seeking() const { return false; }

    virtual void setRate(float) { }
    virtual void setPreservesPitch(bool) { }
    virtual bool paused() const { return m_paused; }

    virtual bool hasClosedCaptions() const { return false; }
    virtual void setClosedCaptionsVisible(bool) { };

    virtual float maxTimeSeekable() const { return 0; }
    virtual std::pair<int, int> buffered() const { return std::make_pair(0, 0); }
    bool didLoadingProgress() const;

    virtual unsigned totalBytes() const { return 0; }
    virtual unsigned bytesLoaded() const { return 0; }

    virtual bool canLoadPoster() const { return false; }
    virtual void setPoster(const std::string&) { }

    virtual bool isLiveStream() const { return true; }

    gboolean handleMessage(GstMessage*);

    // FIXME MediaStreamSource::Observer implementation
    //virtual void sourceStateChanged() override final;
    //virtual void sourceMutedChanged() override final;
    //virtual void sourceEnabledChanged() override final;
    //virtual bool stopped() override final;


private:
    StreamMediaPlayerPrivateGStreamer(MediaPlayer*);

    static PassOwnPtr<MediaPlayerPrivateInterface> create(MediaPlayer*);

    static void getSupportedTypes(HashSet<std::string>&);
    static MediaPlayer::SupportsType supportsType(const std::string& type, const std::string& codecs, const URL&);
    static bool isAvailable();
    void createGSTAudioSinkBin();
    bool connectToGSTLiveStream(MediaStreamDescriptor*);
    void loadingFailed(MediaPlayer::NetworkState error);
    bool internalLoad();
    void stop();
    virtual GstElement* audioSink() const { return m_audioSinkBin.get(); }

private:
    bool m_paused;
    bool m_stopped;
    std::string m_videoSourceId;
    std::string m_audioSourceId;
    GRefPtr<GstElement> m_audioSinkBin;
    RefPtr<MediaStreamDescriptor> m_streamDescriptor;
};

#endif // MediaStreamPlayer_h

