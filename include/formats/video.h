/*═══════════════════════════════════════════════════════════════════
 *  RaptorScope  -  Video (MPEG-1 PS) format support
 *  Uses pl_mpeg (MIT license) for decoding video + audio.
 *═══════════════════════════════════════════════════════════════════*/
#ifndef DC_FORMATS_VIDEO_H
#define DC_FORMATS_VIDEO_H

#include "core/types.h"
#include <windows.h>

/* Check if raw data starts with MPEG-1 Program Stream pack header */
bool is_mpeg_ps(const u8* data, u32 size);

/* Video frame storage */
struct VideoFrame {
    u8*  rgba;      /* RGBA pixel data */
    int  width;
    int  height;
    VideoFrame() : rgba(0), width(0), height(0) {}
    ~VideoFrame() { free(rgba); }
};

/* Decoded video  -  array of frames + interleaved audio PCM */
struct DecodedVideo {
    VideoFrame* frames;
    int         frame_count;
    int         width, height;
    float       fps;

    /* Audio: interleaved stereo s16 PCM */
    s16*        audio_pcm;
    int         audio_samples;    /* total sample frames (L+R = 1 frame) */
    int         audio_samplerate;
    bool        has_audio;

    DecodedVideo() : frames(0), frame_count(0),
                     width(0), height(0), fps(24.0f),
                     audio_pcm(0), audio_samples(0),
                     audio_samplerate(44100), has_audio(false) {}
    ~DecodedVideo() { clear(); }

    void clear() {
        delete[] frames; frames = 0; frame_count = 0;
        free(audio_pcm); audio_pcm = 0; audio_samples = 0;
        has_audio = false;
    }
};

/* Decode MPEG-1 PS file  -  all video frames + audio PCM */
bool decode_mpeg_video(const u8* data, u32 size, DecodedVideo& out);

/* Parse MPEG-1 sequence header to get resolution and fps (no decoding) */
bool mpeg_get_info(const u8* data, u32 size, int* w, int* h, float* fps);

#endif /* DC_FORMATS_VIDEO_H */
