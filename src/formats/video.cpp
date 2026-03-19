/*═══════════════════════════════════════════════════════════════════
 *  RaptorScope  -  MPEG-1 Video + Audio Decoder
 *
 *  Uses pl_mpeg (MIT license) by Dominic Szablewski for decoding.
 *  https://github.com/phoboslab/pl_mpeg
 *═══════════════════════════════════════════════════════════════════*/
#define PL_MPEG_IMPLEMENTATION
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include "ext/pl_mpeg.h"
#include "formats/video.h"
#include <string.h>

/* ── Detection ── */
bool is_mpeg_ps(const u8* data, u32 size) {
    if (size < 12) return false;
    return data[0]==0 && data[1]==0 && data[2]==1 && data[3]==0xBA;
}

bool mpeg_get_info(const u8* data, u32 size, int* w, int* h, float* fps) {
    static const float ft[]={0,23.976f,24.f,25.f,29.97f,30.f,50.f,59.94f,60.f};
    for (u32 i=0; i+7<size; i++) {
        if (data[i]==0 && data[i+1]==0 && data[i+2]==1 && data[i+3]==0xB3) {
            if (w) *w = (data[i+4]<<4)|(data[i+5]>>4);
            if (h) *h = ((data[i+5]&0xF)<<8)|data[i+6];
            if (fps) { int c=data[i+7]&0xF; *fps=(c<9)?ft[c]:24.f; }
            return true;
        }
    }
    return false;
}

/* ── Decode all video frames + audio ── */
bool decode_mpeg_video(const u8* data, u32 size, DecodedVideo& out) {
    out.clear();
    if (!is_mpeg_ps(data, size)) return false;

    /* pl_mpeg needs a non-const copy it can own */
    u8* copy = (u8*)malloc(size);
    if (!copy) return false;
    memcpy(copy, data, size);

    plm_t* plm = plm_create_with_memory(copy, size, 1);
    if (!plm) { free(copy); return false; }

    int w = plm_get_width(plm);
    int h = plm_get_height(plm);
    double fps = plm_get_framerate(plm);
    if (w <= 0 || h <= 0) { plm_destroy(plm); return false; }

    out.width = w;
    out.height = h;
    out.fps = (float)fps;

    /* Enable both video and audio decoding */
    plm_set_video_enabled(plm, 1);
    plm_set_audio_enabled(plm, 1);

    int samplerate = plm_get_samplerate(plm);
    out.audio_samplerate = (samplerate > 0) ? samplerate : 44100;
    out.has_audio = (samplerate > 0);

    /* Pre-allocate frame array */
    int vcap = 256;
    out.frames = new VideoFrame[vcap];
    out.frame_count = 0;

    /* Pre-allocate audio buffer (stereo s16, estimate ~10 sec) */
    int acap = out.audio_samplerate * 2 * 30; /* 30 sec worth */
    s16* audio_buf = (s16*)malloc(acap * sizeof(s16));
    int audio_pos = 0; /* sample count (individual s16 values, L+R interleaved) */

    /* Decode loop: pull video frames, interleave audio */
    while (!plm_has_ended(plm)) {
        /* Decode one video frame */
        plm_frame_t* frame = plm_decode_video(plm);
        if (!frame) break;

        /* Grow video array if needed */
        if (out.frame_count >= vcap) {
            vcap *= 2;
            VideoFrame* nf = new VideoFrame[vcap];
            for (int i = 0; i < out.frame_count; i++) {
                nf[i].rgba = out.frames[i].rgba;
                nf[i].width = out.frames[i].width;
                nf[i].height = out.frames[i].height;
                out.frames[i].rgba = NULL;
            }
            delete[] out.frames;
            out.frames = nf;
        }

        u8* rgba = (u8*)malloc(w * h * 4);
        if (!rgba) break;
        plm_frame_to_rgba(frame, rgba, w * 4);

        out.frames[out.frame_count].rgba = rgba;
        out.frames[out.frame_count].width = w;
        out.frames[out.frame_count].height = h;
        out.frame_count++;

        /* Decode any audio up to this video frame's time */
        if (out.has_audio) {
            plm_samples_t* samples;
            while ((samples = plm_decode_audio(plm)) != NULL) {
                int nf_samples = samples->count; /* frames (each = L+R) */
                int needed = audio_pos + nf_samples * 2;

                /* Grow audio buffer if needed */
                if (needed > acap) {
                    acap = needed * 2;
                    audio_buf = (s16*)realloc(audio_buf, acap * sizeof(s16));
                    if (!audio_buf) { out.has_audio = false; break; }
                }

                /* Convert float interleaved → s16 interleaved */
                for (int i = 0; i < (int)nf_samples; i++) {
                    float l = samples->interleaved[i * 2 + 0];
                    float r = samples->interleaved[i * 2 + 1];
                    int li = (int)(l * 32767.0f);
                    int ri = (int)(r * 32767.0f);
                    if (li > 32767) li = 32767; if (li < -32768) li = -32768;
                    if (ri > 32767) ri = 32767; if (ri < -32768) ri = -32768;
                    audio_buf[audio_pos++] = (s16)li;
                    audio_buf[audio_pos++] = (s16)ri;
                }

                /* Don't decode audio past the current video timestamp */
                if (samples->time >= frame->time) break;
            }
        }
    }

    plm_destroy(plm);

    /* Finalize audio */
    if (out.has_audio && audio_pos > 0) {
        out.audio_pcm = (s16*)malloc(audio_pos * sizeof(s16));
        if (out.audio_pcm) {
            memcpy(out.audio_pcm, audio_buf, audio_pos * sizeof(s16));
            out.audio_samples = audio_pos / 2; /* frames, not individual values */
        }
    }
    free(audio_buf);

    return out.frame_count > 0;
}
