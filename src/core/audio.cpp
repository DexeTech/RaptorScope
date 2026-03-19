/*═══════════════════════════════════════════════════════════════════
 *  RaptorScope  -  VAG ADPCM Audio Decoder
 *═══════════════════════════════════════════════════════════════════*/
#include "core/audio.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*─── VAG prediction coefficients ────────────────────────────────*/
static const f64 VAG_COEFF[5][2] = {
    { 0.0,          0.0         },
    { 60.0 / 64.0,  0.0         },
    {115.0 / 64.0, -52.0 / 64.0 },
    { 98.0 / 64.0, -55.0 / 64.0 },
    {122.0 / 64.0, -60.0 / 64.0 },
};

VagBlockResult decode_vag_block(const u8* data, size_t offset, f64 s1, f64 s2)
{
    VagBlockResult r;
    r.s1 = s1;
    r.s2 = s2;

    u8 shift_filter = data[offset];
    r.flags = data[offset + 1];

    int shift    = shift_filter & 0x0F;
    int pred_idx = (shift_filter >> 4) & 0x0F;
    if (pred_idx > 4) pred_idx = 4;

    f64 c1 = VAG_COEFF[pred_idx][0];
    f64 c2 = VAG_COEFF[pred_idx][1];

    int si = 0;
    for (int i = 2; i < 16; i++) {
        u8 byte = data[offset + i];
        for (int nibble = 0; nibble < 2; nibble++) {
            int d = (nibble == 0) ? (byte & 0x0F) : ((byte >> 4) & 0x0F);
            if (d >= 8) d -= 16;

            f64 sample = (f64)(d << (12 - shift)) + (r.s1 * c1) + (r.s2 * c2);
            if (sample < -32768.0) sample = -32768.0;
            if (sample >  32767.0) sample =  32767.0;

            r.s2 = r.s1;
            r.s1 = sample;
            r.samples[si++] = (s16)sample;
        }
    }
    return r;
}

int decode_vag(const u8* data, size_t start, size_t end,
               s16* out, int max_samples)
{
    f64 s1 = 0.0, s2 = 0.0;
    int count = 0;
    size_t off = start;

    while (off + 16 <= end && count + 28 <= max_samples) {
        VagBlockResult r = decode_vag_block(data, off, s1, s2);
        for (int i = 0; i < 28 && count < max_samples; i++)
            out[count++] = r.samples[i];
        s1 = r.s1;
        s2 = r.s2;
        off += 16;
        if (r.flags & 1) break;   /* end flag */
    }
    return count;
}

int split_vag_samples(const u8* data, size_t data_size,
                      VagRegion* regions, int max_regions)
{
    int count = 0;
    size_t sample_start = 0;
    size_t i = 0;

    while (i + 15 < data_size && count < max_regions) {
        u8 flags = data[i + 1];
        if (flags & 1) {  /* end bit */
            size_t sample_end = i + 16;
            /* Check for flag=7 terminator padding */
            if (sample_end + 16 <= data_size && data[sample_end + 1] == 7)
                sample_end += 16;
            if (sample_end - sample_start >= 16) {
                regions[count].start = sample_start;
                regions[count].end   = sample_end;
                count++;
            }
            sample_start = sample_end;
            i = sample_end;
        } else {
            i += 16;
        }
    }
    return count;
}

/*─── WAV file output ────────────────────────────────────────────*/

static void write_wav_header(u8* buf, int num_samples, int sample_rate)
{
    int data_size = num_samples * 2;   /* 16-bit mono */
    int file_size = 36 + data_size;

    memcpy(buf, "RIFF", 4);
    wr_u32(buf + 4,  file_size);
    memcpy(buf + 8,  "WAVE", 4);
    memcpy(buf + 12, "fmt ", 4);
    wr_u32(buf + 16, 16);             /* chunk size */
    wr_u16(buf + 20, 1);              /* PCM format */
    wr_u16(buf + 22, 1);              /* mono */
    wr_u32(buf + 24, sample_rate);
    wr_u32(buf + 28, sample_rate * 2); /* byte rate */
    wr_u16(buf + 32, 2);              /* block align */
    wr_u16(buf + 34, 16);             /* bits per sample */
    memcpy(buf + 36, "data", 4);
    wr_u32(buf + 40, data_size);
}

bool write_wav_file(const char* path, const s16* pcm, int num_samples,
                    int sample_rate)
{
    FILE* f = fopen(path, "wb");
    if (!f) return false;

    u8 header[44];
    write_wav_header(header, num_samples, sample_rate);
    fwrite(header, 1, 44, f);

    /* Write PCM data as little-endian 16-bit */
    for (int i = 0; i < num_samples; i++) {
        u8 lo = (u8)(pcm[i] & 0xFF);
        u8 hi = (u8)((pcm[i] >> 8) & 0xFF);
        fwrite(&lo, 1, 1, f);
        fwrite(&hi, 1, 1, f);
    }

    fclose(f);
    return true;
}

size_t build_wav_memory(const s16* pcm, int num_samples,
                        u8* out_buf, size_t buf_size,
                        int sample_rate)
{
    size_t needed = 44 + (size_t)num_samples * 2;
    if (buf_size < needed) return 0;

    write_wav_header(out_buf, num_samples, sample_rate);
    for (int i = 0; i < num_samples; i++) {
        out_buf[44 + i * 2]     = (u8)(pcm[i] & 0xFF);
        out_buf[44 + i * 2 + 1] = (u8)((pcm[i] >> 8) & 0xFF);
    }
    return needed;
}

/*─── GIAN header parsing ────────────────────────────────────────*/
bool parse_gian_header(const u8* data, size_t size, GianHeader& out)
{
    memset(&out, 0, sizeof(out));
    out.valid = false;
    if (size < 16) return false;
    if (memcmp(data, "Gian", 4) != 0) return false;

    memcpy(out.magic, data, 4); out.magic[4] = 0;
    out.num_programs = rd_u16(data + 6);
    out.num_tones    = rd_u16(data + 8);
    out.num_vag      = rd_u16(data + 10);

    int np = out.num_programs;
    if (np > 16) np = 16;
    int nt = out.num_tones;
    if (nt > 16) nt = 16;

    /* Header: 16 bytes
       Programs: 16 slots x 8 bytes = 128 bytes (at offset 16)
         byte 0: num_tones_in_program
         byte 1: volume
         byte 2-3: priority (0xFFFF unused)
         byte 4: pan
         byte 5-7: padding
       Tones: np * nt * 32 bytes (at offset 144)
         Standard Sony VH tone layout, 32 bytes each */

    for (int p = 0; p < np && p < 16; p++) {
        size_t poff = 16 + (size_t)p * 8;
        if (poff + 8 > size) break;
        out.programs[p].volume = data[poff + 1];
        out.programs[p].pan    = data[poff + 4];
    }

    /* Collect unique SPU addresses to build VAG index map */
    u32 spu_addrs[128];
    int spu_count = 0;

    size_t tone_base = 144;
    out.tone_count = 0;
    for (int p = 0; p < np && out.tone_count < 128; p++) {
        for (int t = 0; t < nt; t++) {
            size_t toff = tone_base + (size_t)(p * nt + t) * 32;
            if (toff + 32 > size) break;
            u8 vol = data[toff + 2];
            u8 pitch = data[toff + 4];
            u8 key_lo = data[toff + 6];
            u8 key_hi = data[toff + 7];
            /* Skip empty tones and SPU address entries (inverted key range) */
            if ((vol == 0 && pitch == 0 && key_hi == 0) || (key_lo > key_hi && key_hi != 0))
                continue;

            GianTone& tn = out.tones[out.tone_count++];
            tn.program   = (u8)p;
            tn.tone_idx  = (u8)t;
            tn.volume    = vol;
            tn.pan       = data[toff + 3];
            tn.pitch     = pitch;
            tn.pitch_fine= data[toff + 5];
            tn.key_lo    = data[toff + 6];
            tn.key_hi    = key_hi;
            tn.adsr1     = rd_u16(data + toff + 14);
            tn.adsr2     = rd_u16(data + toff + 16);
            u32 spu_raw  = (u32)rd_u16(data + toff + 22) * 8;
            tn.spu_addr  = spu_raw;

            /* Track unique SPU addresses for VAG index mapping */
            bool found = false;
            for (int s = 0; s < spu_count; s++) {
                if (spu_addrs[s] == spu_raw) { found = true; break; }
            }
            if (!found && spu_count < 128) spu_addrs[spu_count++] = spu_raw;

            /* vag_bank will be resolved after sorting SPU addresses */
            tn.vag_bank = 0;
        }
    }

    /* Sort SPU addresses and assign VAG bank indices:
       lowest SPU address = VAG 0, next = VAG 1, etc. */
    for (int i = 0; i < spu_count - 1; i++)
        for (int j = i + 1; j < spu_count; j++)
            if (spu_addrs[j] < spu_addrs[i]) {
                u32 tmp = spu_addrs[i]; spu_addrs[i] = spu_addrs[j]; spu_addrs[j] = tmp;
            }

    for (int ti = 0; ti < out.tone_count; ti++) {
        for (int s = 0; s < spu_count; s++) {
            if (out.tones[ti].spu_addr == spu_addrs[s]) {
                out.tones[ti].vag_bank = (u8)s;
                break;
            }
        }
    }

    out.valid = true;
    return true;
}

/*─── SEQ header parsing (Capcom/Gian format) ───────────────────*/
bool parse_seq_header(const u8* data, size_t size, SeqHeader& out)
{
    memset(&out, 0, sizeof(out));
    out.valid = false;
    if (size < 15) return false;

    /* Capcom/Gian SEQ format (no magic):
       u16 version    @ 0
       u16 ppqn       @ 2
       u32 tempo_usec @ 4
       ...
       MIDI data      @ 15 */
    out.version       = rd_u16(data + 0);
    out.resolution    = rd_u16(data + 2);
    out.initial_tempo = rd_u32(data + 4);
    out.rhythm_n = data[13];
    out.rhythm_d = data[14];

    if (out.resolution == 0 || out.initial_tempo == 0) return false;
    if (out.initial_tempo > 10000000) return false;

    out.bpm = 60000000.0f / (f32)out.initial_tempo;
    out.valid = true;
    return true;
}

/*─── SEQ note parser for piano roll ────────────────────────────*/
int parse_seq_notes(const u8* data, size_t size, int midi_offset,
                    u32 tempo_usec, u16 ppqn,
                    SeqNote* out, int max_notes,
                    f32* out_duration, int* out_channels_mask)
{
    if (!data || (int)size <= midi_offset) return 0;
    int pos = midi_offset;
    int n = 0;
    f32 time_sec = 0.0f;
    f32 sec_per_tick = (f32)tempo_usec / 1000000.0f / (f32)(ppqn ? ppqn : 1);
    u8  running = 0;
    int ch_mask = 0;

    /* Track active notes: (ch*128 + note) -> start_time */
    f32 active[2048]; /* 16 channels * 128 notes */
    for (int i = 0; i < 2048; i++) active[i] = -1.0f;

    while (pos < (int)size && n < max_notes) {
        u8 delta = data[pos++];
        time_sec += delta * sec_per_tick;
        if (pos >= (int)size) break;

        u8 b = data[pos];
        if (b >= 0x80) { running = b; pos++; }

        u8 hi = (running >> 4) & 0xF;
        u8 ch = running & 0xF;

        if (hi == 0x8 || hi == 0x9) {
            if (pos + 1 >= (int)size) break;
            u8 note = data[pos]; u8 vel = data[pos + 1]; pos += 2;
            bool on = (hi == 0x9 && vel > 0);
            int key = ch * 128 + note;
            ch_mask |= (1 << ch);

            if (on) {
                active[key] = time_sec;
            } else {
                if (active[key] >= 0.0f && n < max_notes) {
                    out[n].start_time = active[key];
                    out[n].end_time = time_sec;
                    out[n].channel = ch;
                    out[n].note = note;
                    out[n].velocity = vel;
                    n++;
                }
                active[key] = -1.0f;
            }
        } else if (hi == 0xB || hi == 0xE) {
            if (pos + 1 >= (int)size) break;
            pos += 2;
        } else if (hi == 0xC || hi == 0xD) {
            if (pos >= (int)size) break;
            pos += 1;
        } else if (running == 0xFF) {
            /* Meta event */
            if (pos >= (int)size) break;
            u8 mtype = data[pos++];
            if (pos >= (int)size) break;
            u8 mlen = data[pos++];
            if (mtype == 0x51 && mlen == 3 && pos + 3 <= (int)size) {
                tempo_usec = ((u32)data[pos] << 16) | ((u32)data[pos+1] << 8) | data[pos+2];
                sec_per_tick = (f32)tempo_usec / 1000000.0f / (f32)(ppqn ? ppqn : 1);
            }
            if (mtype == 0x2F) break; /* end of track */
            pos += mlen;
        } else {
            pos++; /* skip unknown */
        }
    }

    /* Close remaining active notes */
    for (int i = 0; i < 2048 && n < max_notes; i++) {
        if (active[i] >= 0.0f) {
            out[n].start_time = active[i];
            out[n].end_time = time_sec;
            out[n].channel = (u8)(i / 128);
            out[n].note = (u8)(i % 128);
            out[n].velocity = 64;
            n++;
        }
    }

    if (out_duration) *out_duration = time_sec + 0.5f;
    if (out_channels_mask) *out_channels_mask = ch_mask;
    return n;
}

/*═══════════════════════════════════════════════════════════════════
 *  SEQ Synthesizer — renders Capcom/Gian sequences to stereo PCM
 *  using VAG instrument samples with pitch shifting, ADSR, and pan.
 *═══════════════════════════════════════════════════════════════════*/

/* Internal note event with full channel state at trigger time */
struct SynthNote {
    f32 time;
    u8  type;     /* 0 = on, 1 = off */
    u8  ch, note, vel;
    u8  program, volume, pan, expression;
};

int render_seq_to_stereo(
    const u8* seq_data, size_t seq_size,
    const GianHeader& gian,
    DecodedSample* vag_samples, int num_vag,
    s16* out_pcm, int max_frames,
    int sample_rate, f32 max_duration)
{
    if (!seq_data || seq_size < 15 || !out_pcm) return 0;

    /* Parse header */
    u16 ppqn = rd_u16(seq_data + 2);
    u32 tempo_usec = rd_u32(seq_data + 4);
    if (ppqn == 0 || tempo_usec == 0) return 0;

    /* First pass: parse MIDI events into SynthNote list */
    const int MAX_NOTES = 16384;
    SynthNote* notes = (SynthNote*)malloc(sizeof(SynthNote) * MAX_NOTES);
    if (!notes) return 0;
    int nn = 0;

    u8 ch_prog[16] = {0};
    u8 ch_vol[16], ch_pan[16], ch_expr[16];
    for (int i = 0; i < 16; i++) { ch_vol[i] = 127; ch_pan[i] = 64; ch_expr[i] = 127; }

    int pos = 15;
    f32 time_sec = 0.0f;
    f32 spt = (f32)tempo_usec / 1000000.0f / (f32)ppqn;
    u8 running = 0;

    while (pos < (int)seq_size && nn < MAX_NOTES) {
        u8 delta = seq_data[pos++];
        time_sec += delta * spt;
        if (pos >= (int)seq_size) break;

        u8 b = seq_data[pos];
        if (b >= 0x80) { running = b; pos++; }
        u8 hi = (running >> 4) & 0xF;
        u8 ch = running & 0xF;

        if (hi == 0x8 || hi == 0x9) {
            if (pos + 1 >= (int)seq_size) break;
            u8 note = seq_data[pos]; u8 vel = seq_data[pos+1]; pos += 2;
            bool on = (hi == 0x9 && vel > 0);
            notes[nn].time = time_sec;
            notes[nn].type = on ? 0 : 1;
            notes[nn].ch = ch; notes[nn].note = note; notes[nn].vel = vel;
            notes[nn].program = ch_prog[ch];
            notes[nn].volume = ch_vol[ch];
            notes[nn].pan = ch_pan[ch];
            notes[nn].expression = ch_expr[ch];
            nn++;
        } else if (hi == 0xB) {
            if (pos + 1 >= (int)seq_size) break;
            u8 cc = seq_data[pos]; u8 val = seq_data[pos+1]; pos += 2;
            if (cc == 7) ch_vol[ch] = val;
            else if (cc == 10) ch_pan[ch] = val;
            else if (cc == 11) ch_expr[ch] = val;
        } else if (hi == 0xC) {
            if (pos >= (int)seq_size) break;
            ch_prog[ch] = seq_data[pos++];
        } else if (hi == 0xE) {
            if (pos + 1 >= (int)seq_size) break;
            pos += 2;
        } else if (running == 0xFF) {
            if (pos >= (int)seq_size) break;
            u8 mt = seq_data[pos++];
            if (pos >= (int)seq_size) break;
            u8 ml = seq_data[pos++];
            if (mt == 0x51 && ml == 3 && pos + 3 <= (int)seq_size) {
                tempo_usec = ((u32)seq_data[pos] << 16) | ((u32)seq_data[pos+1] << 8) | seq_data[pos+2];
                spt = (f32)tempo_usec / 1000000.0f / (f32)ppqn;
            }
            if (mt == 0x2F) break;
            pos += ml;
        } else {
            pos++;
        }
    }

    /* Match note-on/off pairs */
    struct RenderNote {
        f32 start, duration;
        u8  note, vel, program, volume, pan, expression;
    };
    RenderNote* rnotes = (RenderNote*)malloc(sizeof(RenderNote) * nn);
    int rn = 0;

    /* active[ch*128+note] = index into notes[], or -1 */
    int active[2048];
    for (int i = 0; i < 2048; i++) active[i] = -1;

    for (int i = 0; i < nn; i++) {
        SynthNote& e = notes[i];
        int key = e.ch * 128 + e.note;
        if (e.type == 0) { /* note on */
            if (active[key] >= 0 && rn < nn) {
                /* close previous */
                SynthNote& prev = notes[active[key]];
                rnotes[rn].start = prev.time;
                rnotes[rn].duration = e.time - prev.time;
                rnotes[rn].note = prev.note; rnotes[rn].vel = prev.vel;
                rnotes[rn].program = prev.program; rnotes[rn].volume = prev.volume;
                rnotes[rn].pan = prev.pan; rnotes[rn].expression = prev.expression;
                rn++;
            }
            active[key] = i;
        } else { /* note off */
            if (active[key] >= 0 && rn < nn) {
                SynthNote& prev = notes[active[key]];
                rnotes[rn].start = prev.time;
                rnotes[rn].duration = e.time - prev.time;
                rnotes[rn].note = prev.note; rnotes[rn].vel = prev.vel;
                rnotes[rn].program = prev.program; rnotes[rn].volume = prev.volume;
                rnotes[rn].pan = prev.pan; rnotes[rn].expression = prev.expression;
                rn++;
                active[key] = -1;
            }
        }
    }
    /* Close remaining */
    for (int i = 0; i < 2048 && rn < nn; i++) {
        if (active[i] >= 0) {
            SynthNote& prev = notes[active[i]];
            rnotes[rn].start = prev.time;
            rnotes[rn].duration = 2.0f < (time_sec - prev.time) ? 2.0f : (time_sec - prev.time);
            rnotes[rn].note = prev.note; rnotes[rn].vel = prev.vel;
            rnotes[rn].program = prev.program; rnotes[rn].volume = prev.volume;
            rnotes[rn].pan = prev.pan; rnotes[rn].expression = prev.expression;
            rn++;
        }
    }

    /* Determine output duration */
    f32 duration = time_sec + 2.0f;
    if (duration > max_duration) duration = max_duration;
    int total_frames = (int)(duration * sample_rate);
    if (total_frames > max_frames) total_frames = max_frames;

    /* Allocate float mixing buffers */
    f32* buf_l = (f32*)calloc(total_frames, sizeof(f32));
    f32* buf_r = (f32*)calloc(total_frames, sizeof(f32));
    if (!buf_l || !buf_r) { free(notes); free(rnotes); free(buf_l); free(buf_r); return 0; }

    /* Build program → tone lookup */
    /* For each note, find matching tone by program + key range */
    const f32 PI_2 = 3.14159265f / 2.0f;
    const f32 release_time = 0.15f;
    int release_samps = (int)(release_time * sample_rate);

    for (int ri = 0; ri < rn; ri++) {
        RenderNote& rn_note = rnotes[ri];

        /* Find tone */
        const GianTone* tone = 0;
        const GianTone* fallback = 0;
        for (int ti = 0; ti < gian.tone_count; ti++) {
            if (gian.tones[ti].program == rn_note.program) {
                if (!fallback) fallback = &gian.tones[ti];
                if (rn_note.note >= gian.tones[ti].key_lo &&
                    rn_note.note <= gian.tones[ti].key_hi) {
                    tone = &gian.tones[ti];
                    break;
                }
            }
        }
        if (!tone) tone = fallback;
        if (!tone) continue;

        /* Get VAG sample */
        int vi = tone->vag_bank;
        if (vi >= num_vag || !vag_samples[vi].pcm || vag_samples[vi].count <= 1)
            continue;

        s16* pcm = vag_samples[vi].pcm;
        int pcm_len = vag_samples[vi].count;

        /* Pitch ratio */
        f32 semitone_diff = (f32)rn_note.note - (f32)tone->pitch;
        f32 pitch_ratio = powf(2.0f, semitone_diff / 12.0f);

        /* Amplitude */
        f32 amp = (rn_note.vel / 127.0f) * (tone->volume / 127.0f)
                * (rn_note.volume / 127.0f) * (rn_note.expression / 127.0f);
        amp *= 0.25f; /* master reduction */

        /* Stereo pan */
        f32 pan_norm = rn_note.pan / 127.0f;
        f32 pan_l = cosf(pan_norm * PI_2);
        f32 pan_r = sinf(pan_norm * PI_2);

        /* Envelope: simple attack (5ms) + sustain + release */
        int attack_samps = (int)(0.005f * sample_rate);
        int start_frame = (int)(rn_note.start * sample_rate);
        int note_frames = (int)(rn_note.duration * sample_rate);
        int total_note = note_frames + release_samps;

        f32 src_pos = 0.0f;
        for (int j = 0; j < total_note; j++) {
            int out_idx = start_frame + j;
            if (out_idx >= total_frames) break;

            int si = (int)src_pos;
            if (si >= pcm_len - 1) break;

            /* Linear interpolation */
            f32 frac = src_pos - (f32)si;
            f32 s = pcm[si] * (1.0f - frac) + pcm[si + 1] * frac;

            /* Envelope */
            f32 env;
            if (j < attack_samps)
                env = (f32)j / (f32)(attack_samps > 0 ? attack_samps : 1);
            else if (j < note_frames)
                env = 1.0f;
            else {
                f32 rel = (f32)(j - note_frames) / (f32)(release_samps > 0 ? release_samps : 1);
                env = 1.0f - rel;
                if (env < 0.0f) env = 0.0f;
            }

            f32 val = s * amp * env;
            buf_l[out_idx] += val * pan_l;
            buf_r[out_idx] += val * pan_r;

            src_pos += pitch_ratio;
        }
    }

    /* Convert to interleaved s16 stereo */
    for (int i = 0; i < total_frames; i++) {
        f32 l = buf_l[i], r = buf_r[i];
        if (l > 32767.0f) l = 32767.0f; if (l < -32768.0f) l = -32768.0f;
        if (r > 32767.0f) r = 32767.0f; if (r < -32768.0f) r = -32768.0f;
        out_pcm[i * 2]     = (s16)l;
        out_pcm[i * 2 + 1] = (s16)r;
    }

    free(notes); free(rnotes); free(buf_l); free(buf_r);
    return total_frames;
}

/*─── Stereo WAV writer ─────────────────────────────────────────*/
bool write_wav_stereo(const char* path, const s16* pcm, int num_frames,
                      int sample_rate)
{
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    u32 data_size = num_frames * 4; /* 2 channels * 2 bytes */
    u32 file_size = 36 + data_size;
    u8 hdr[44];
    memcpy(hdr, "RIFF", 4);
    hdr[4]=(u8)file_size; hdr[5]=(u8)(file_size>>8);
    hdr[6]=(u8)(file_size>>16); hdr[7]=(u8)(file_size>>24);
    memcpy(hdr+8, "WAVEfmt ", 8);
    hdr[16]=16;hdr[17]=0;hdr[18]=0;hdr[19]=0;
    hdr[20]=1;hdr[21]=0; /* PCM */
    hdr[22]=2;hdr[23]=0; /* stereo */
    hdr[24]=(u8)sample_rate; hdr[25]=(u8)(sample_rate>>8);
    hdr[26]=(u8)(sample_rate>>16); hdr[27]=(u8)(sample_rate>>24);
    u32 bps = sample_rate * 4;
    hdr[28]=(u8)bps; hdr[29]=(u8)(bps>>8);
    hdr[30]=(u8)(bps>>16); hdr[31]=(u8)(bps>>24);
    hdr[32]=4;hdr[33]=0; /* block align */
    hdr[34]=16;hdr[35]=0; /* bits per sample */
    memcpy(hdr+36, "data", 4);
    hdr[40]=(u8)data_size; hdr[41]=(u8)(data_size>>8);
    hdr[42]=(u8)(data_size>>16); hdr[43]=(u8)(data_size>>24);
    fwrite(hdr, 1, 44, f);
    fwrite(pcm, 2, num_frames * 2, f);
    fclose(f);
    return true;
}
