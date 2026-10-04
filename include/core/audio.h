/*═══════════════════════════════════════════════════════════════════
 *  RaptorScope  -  PS1 Audio (VAG ADPCM, GIAN, SEQ)
 *═══════════════════════════════════════════════════════════════════*/
#ifndef DC_CORE_AUDIO_H
#define DC_CORE_AUDIO_H

#include "types.h"

/*─── VAG ADPCM ──────────────────────────────────────────────────*/

struct VagBlockResult {
    s16 samples[28];
    f64 s1, s2;
    u8  flags;
};

/* Decode one 16-byte VAG ADPCM block */
VagBlockResult decode_vag_block(const u8* data, size_t offset, f64 s1, f64 s2);

/* Decode VAG stream → PCM.  Returns number of samples written.
   out must be large enough (est: (end-start)/16 * 28 samples). */
int decode_vag(const u8* data, size_t start, size_t end,
               s16* out, int max_samples);

/* Split SNDB body into individual sample regions.
   Returns number of regions found.  regions[i] = {start, end}. */
struct VagRegion { size_t start, end; };
int split_vag_samples(const u8* data, size_t data_size,
                      VagRegion* regions, int max_regions);

/*─── WAV output ─────────────────────────────────────────────────*/

/* Write PCM samples to WAV file. Returns true on success. */
bool write_wav_file(const char* path, const s16* pcm, int num_samples,
                    int sample_rate = 22050);

/* Build WAV in memory.  Returns size written to out_buf. */
size_t build_wav_memory(const s16* pcm, int num_samples,
                        u8* out_buf, size_t buf_size,
                        int sample_rate = 22050);

/*─── GIAN Sound Header ──────────────────────────────────────────*/
struct GianProgram {
    u8 volume;
    u8 pan;
};

struct GianTone {
    u8  program;
    u8  tone_idx;
    u8  volume;
    u8  pan;
    u8  pitch;      /* center note: keying it plays the VAG at 44100 Hz */
    u8  pitch_fine; /* 1/128 semitone added to the center note */
    u8  key_lo, key_hi;
    u16 adsr1, adsr2;
    u8  vag_bank;
    u32 spu_addr;
};

struct GianHeader {
    char magic[5];  /* "Gian\0" */
    int  num_programs;
    int  num_tones;
    int  num_vag;
    GianProgram programs[16];
    GianTone    tones[128];
    int         tone_count;  /* actual parsed tones */
    bool        valid;
};

bool parse_gian_header(const u8* data, size_t size, GianHeader& out);

/* Rate (Hz) the SPU plays a tone's VAG at when the tone is keyed at note. */
f32 gian_tone_rate(const GianTone& tone, int note);

/* Rate the VAG is normally heard at: keyed at the tone's own key (sound
   effects use one key per tone), or at the key nearest middle C when the
   tone spans a range of keys. */
int gian_tone_sample_rate(const GianTone& tone);

/*─── SEQ Music ──────────────────────────────────────────────────*/
struct SeqNote {
    f32 start_time;
    f32 end_time;
    u8  channel;
    u8  note;
    u8  velocity;
};

struct SeqHeader {
    u32 version;
    u16 resolution;      /* ticks per quarter note (ppqn) */
    u32 initial_tempo;   /* microseconds per quarter note */
    u16 rhythm_n, rhythm_d;
    f32 bpm;
    bool valid;
};

bool parse_seq_header(const u8* data, size_t size, SeqHeader& out);

/* Parse SEQ events into note list for piano roll display.
   Returns number of notes. */
int parse_seq_notes(const u8* data, size_t size, int midi_offset,
                    u32 tempo_usec, u16 ppqn,
                    SeqNote* out, int max_notes,
                    f32* out_duration, int* out_channels_mask);

/*─── SEQ Synthesizer ────────────────────────────────────────────*/

/* Decoded VAG sample for synthesis */
struct DecodedSample {
    s16* pcm;
    int  count;
};

/* Render a SEQ sequence to stereo PCM using GIAN instruments + SNDB samples.
   out_pcm: interleaved stereo (L,R,L,R...), must be at least max_frames*2.
   Returns number of stereo frames written. */
int render_seq_to_stereo(
    const u8* seq_data, size_t seq_size,
    const GianHeader& gian,
    DecodedSample* vag_samples, int num_vag_samples,
    s16* out_pcm, int max_frames,
    int sample_rate = 22050,
    f32 max_duration = 300.0f);

/* Write stereo PCM to WAV file */
bool write_wav_stereo(const char* path, const s16* pcm, int num_frames,
                      int sample_rate = 22050);

#endif /* DC_CORE_AUDIO_H */
