/*
 * aria_audio_shim.c — Software audio synthesis for Aria virtual console
 *
 * All functions return int32_t (0=success, -1=error, or value as documented).
 *
 * Design: "dry mode" (no hardware output) is the default. Call
 * aria_audio_init(1) for dry mode (tests), aria_audio_init(0) to attempt
 * actual output via /dev/dsp (OSS) or stdout pipe in the future.
 *
 * For the virtual console prototype, all playback is synchronous and
 * in-memory. A real implementation would spin a mixing thread; that is
 * intentionally left for a future LIB-19+ milestone.
 *
 * Waveform codes:
 *   0 = square      1 = triangle    2 = sawtooth    3 = noise (LFSR)
 *
 * Channel layout: 4 independent channels (0-3), like classic 8-bit hardware.
 *
 * MIDI note numbers: middle C = 60, A4 = 69 (440 Hz).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define AUDIO_CHANNELS   4
#define AUDIO_SAMPLE_RATE 44100

/* ---- per-channel state ---- */

typedef struct {
    int      active;       /* 1 if "playing" */
    int32_t  freq_hz;
    int32_t  waveform;     /* 0-3 */
    int32_t  duration_ms;
    int32_t  volume;       /* 0-15 */
    uint32_t lfsr;         /* noise state */
} Channel;

static int     g_initialized = 0;
static int     g_dry_mode    = 1;  /* default: dry */
static Channel g_channels[AUDIO_CHANNELS];

/* ---- note frequency table (MIDI 0-127 -> Hz, rounded) ----
 * Generated from: round(440.0 * 2^((n-69)/12.0))
 * Only the commonly used range 21-108 is accurate; outside that
 * the table uses clamped values.
 */
static const int32_t NOTE_FREQ[128] = {
     8,    9,    9,   10,   10,   11,   12,   12,   13,   14,   15,   15,  /* 0-11 */
    16,   17,   18,   19,   21,   22,   23,   25,   26,   28,   29,   31,  /* 12-23 */
    33,   35,   37,   39,   41,   44,   46,   49,   52,   55,   58,   62,  /* 24-35 */
    65,   69,   73,   78,   82,   87,   92,   98,  104,  110,  117,  123,  /* 36-47 */
   131,  139,  147,  156,  165,  175,  185,  196,  208,  220,  233,  247,  /* 48-59 */
   262,  277,  294,  311,  330,  349,  370,  392,  415,  440,  466,  494,  /* 60-71 */
   523,  554,  587,  622,  659,  698,  740,  784,  831,  880,  932,  988,  /* 72-83 */
  1047, 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661, 1760, 1865, 1976, /* 84-95 */
  2093, 2217, 2349, 2489, 2637, 2794, 2960, 3136, 3322, 3520, 3729, 3951, /* 96-107 */
  4186, 4435, 4699, 4978, 5274, 5588, 5920, 6272, 6645, 7040, 7459, 7902, /* 108-119 */
  8372, 8870, 9397, 9956,10548,11175,11840,12544                           /* 120-127 */
};

/* ---- public API ---- */

/*
 * Initialize audio subsystem.
 *   dry_mode=1: silent mode for tests (always succeeds)
 *   dry_mode=0: attempt hardware output (falls back to dry if unavailable)
 * Returns 0 on success, -1 on error.
 */
int32_t aria_audio_init(int32_t dry_mode) {
    if (g_initialized) return 0;
    g_dry_mode = (dry_mode != 0) ? 1 : 0;
    memset(g_channels, 0, sizeof(g_channels));
    for (int i = 0; i < AUDIO_CHANNELS; i++) {
        g_channels[i].volume = 15;
        g_channels[i].lfsr   = (uint32_t)(0xACE1u + i);
    }
    g_initialized = 1;
    return 0;
}

/* Shutdown and reset state. Returns 0. */
int32_t aria_audio_shutdown(void) {
    memset(g_channels, 0, sizeof(g_channels));
    g_initialized = 0;
    return 0;
}

/* Returns 1 if initialized, 0 otherwise. */
int32_t aria_audio_is_init(void) {
    return g_initialized ? 1 : 0;
}

/* Returns 1 if in dry mode, 0 if in hardware mode. */
int32_t aria_audio_is_dry(void) {
    return g_dry_mode ? 1 : 0;
}

/* Returns the fixed sample rate (44100). */
int32_t aria_audio_sample_rate(void) {
    return AUDIO_SAMPLE_RATE;
}

/*
 * Calculate sample count for a given duration in milliseconds.
 * samples = ms * 44100 / 1000
 */
int32_t aria_audio_samples_for_ms(int32_t ms) {
    if (ms < 0) return 0;
    /* Avoid overflow: ms * 44100 can overflow int32 around 48000ms */
    int64_t s = (int64_t)ms * (int64_t)AUDIO_SAMPLE_RATE / 1000LL;
    if (s > 0x7FFFFFFF) s = 0x7FFFFFFF;
    return (int32_t)s;
}

/*
 * Return frequency in Hz for a MIDI note number (0-127).
 * Returns -1 if out of range.
 */
int32_t aria_audio_note_freq(int32_t midi_note) {
    if (midi_note < 0 || midi_note > 127) return -1;
    return NOTE_FREQ[midi_note];
}

/*
 * Play a tone on a channel.
 *   channel   : 0-3
 *   freq_hz   : frequency in Hz (1-20000)
 *   waveform  : 0=square 1=triangle 2=sawtooth 3=noise
 *   duration_ms: 0 = play until stopped; >0 = play for N ms then stop
 * In dry mode, just marks the channel as active.
 * Returns 0 on success, -1 on bad args.
 */
int32_t aria_audio_play_tone(int32_t channel, int32_t freq_hz,
                              int32_t waveform, int32_t duration_ms) {
    if (!g_initialized) return -1;
    if (channel < 0 || channel >= AUDIO_CHANNELS) return -1;
    if (freq_hz < 1 || freq_hz > 20000) return -1;
    if (waveform < 0 || waveform > 3) return -1;
    if (duration_ms < 0) return -1;
    Channel *ch = &g_channels[channel];
    ch->active      = 1;
    ch->freq_hz     = freq_hz;
    ch->waveform    = waveform;
    ch->duration_ms = duration_ms;
    return 0;
}

/* Stop a channel. Returns 0 on success, -1 if bad channel. */
int32_t aria_audio_stop_channel(int32_t channel) {
    if (channel < 0 || channel >= AUDIO_CHANNELS) return -1;
    g_channels[channel].active = 0;
    return 0;
}

/* Stop all channels. Returns 0. */
int32_t aria_audio_stop_all(void) {
    for (int i = 0; i < AUDIO_CHANNELS; i++)
        g_channels[i].active = 0;
    return 0;
}

/*
 * Set channel volume (0=silent, 15=max).
 * Returns 0 on success, -1 on bad args.
 */
int32_t aria_audio_set_volume(int32_t channel, int32_t vol) {
    if (channel < 0 || channel >= AUDIO_CHANNELS) return -1;
    if (vol < 0 || vol > 15) return -1;
    g_channels[channel].volume = vol;
    return 0;
}

/* Returns 1 if channel is active, 0 if not, -1 on bad channel. */
int32_t aria_audio_is_playing(int32_t channel) {
    if (channel < 0 || channel >= AUDIO_CHANNELS) return -1;
    return g_channels[channel].active ? 1 : 0;
}

/* Returns current volume for channel (0-15), or -1 on bad channel. */
int32_t aria_audio_get_volume(int32_t channel) {
    if (channel < 0 || channel >= AUDIO_CHANNELS) return -1;
    return g_channels[channel].volume;
}

/*
 * Generate one square wave sample for the given frequency.
 * Returns +1 or -1 (as int32, representing the sign of the square wave
 * at a given phase offset).
 *   phase_sample: absolute sample index
 *   freq_hz     : frequency
 * Returns 1 for first half of period, -1 for second half.
 */
int32_t aria_audio_square_sample(int32_t phase_sample, int32_t freq_hz) {
    if (freq_hz <= 0) return 1;
    int32_t period = AUDIO_SAMPLE_RATE / freq_hz;
    if (period < 1) period = 1;
    int32_t pos = phase_sample % period;
    return (pos < period / 2) ? 1 : (0 - 1);
}

/*
 * Advance a 16-bit Galois LFSR one step and return the output bit (0 or 1).
 * Polynomial: x^16 + x^14 + x^13 + x^11 + 1 (taps: bits 0,2,3,5)
 * Pass a pointer to a uint32 holding the LFSR state (seed must be != 0).
 * Returns 0 or 1.
 */
int32_t aria_audio_lfsr_step(int32_t state) {
    uint32_t s = (uint32_t)state;
    if (s == 0) s = 0xACE1u;
    /* galois LFSR step */
    uint32_t bit = s & 1u;
    s >>= 1;
    if (bit) s ^= 0xB400u; /* taps for maximal 16-bit sequence */
    return (int32_t)s;
}
