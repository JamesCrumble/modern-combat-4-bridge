/*
 * android/media/AudioTrack - the game's only audio output.
 *
 * libModernCombat4.so does not link OpenSL ES: vox's Android driver creates a Java
 * AudioTrack through JNI (constructor "(IIIIII)V", getMinBufferSize, play,
 * write([BII)I - the names sit at aa50e4 in the library) and pushes mixed PCM
 * into it from its own thread. AudioTrack.write blocks while the device buffer
 * is full, and that block is the only thing pacing the mixer thread, so
 * whatever stands in for it has to block the same way.
 *
 * Two outputs:
 *
 *   <PREFIX>_AUDIO_FIFO=<path>  the Android wrapper: raw 44.1 kHz stereo s16
 *                               into a small pipe that a real AudioTrack reads;
 *                               the pipe filling up is the pacing.
 *   otherwise                   SDL's queue, held to about 100 ms.
 *
 * With neither usable the samples are dropped at the rate they would have
 * played, so the mixer thread does not spin.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <SDL2/SDL.h>

#include "platform.h"
#include "jni.h"
#include "jni_internals.h"
#include "port_env.h"
#include "trace.h"

class AndroidAudioTrack : public Object {
public:
    static Class clazz;
    Class *_getClass() { return &clazz; }

    int  rate;
    int  channels;
    int  bits;
    bool playing;
    int  fifo_fd;
    SDL_AudioDeviceID device;
    long long written;      /* bytes accepted */
};

enum {
    CHANNEL_OUT_MONO   = 4,
    CHANNEL_OUT_STEREO = 12,
    ENCODING_PCM_16BIT = 2,
    ENCODING_PCM_8BIT  = 3,
    PLAYSTATE_STOPPED  = 1,
    PLAYSTATE_PAUSED   = 2,
    PLAYSTATE_PLAYING  = 3,
};

static int frame_bytes(const AndroidAudioTrack *t)
{
    return t->channels * (t->bits / 8);
}

static void sleep_ms(long ms)
{
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static jobject AudioTrack_init(JNIEnv *env, jobject self, jclass clazz,
                               jint stream, jint rate, jint channel_config,
                               jint format, jint buffer_bytes, jint mode)
{
    (void)env; (void)clazz;
    AndroidAudioTrack *t = (AndroidAudioTrack *)self;
    t->rate     = rate > 0 ? rate : 44100;
    t->channels = channel_config == CHANNEL_OUT_MONO ? 1 : 2;
    t->bits     = format == ENCODING_PCM_8BIT ? 8 : 16;
    t->playing  = false;
    t->fifo_fd  = -1;
    t->device   = 0;
    t->written  = 0;

    trace("AudioTrack(stream=%d rate=%d channels=%d bits=%d buffer=%d mode=%d)",
          stream, t->rate, t->channels, t->bits, buffer_bytes, mode);

    const char *fifo = port_getenv("AUDIO_FIFO");
    if (fifo && *fifo) {
        if (t->rate != 44100 || t->channels != 2 || t->bits != 16) {
            trace("AudioTrack: " PORT_ENV_PREFIX "_AUDIO_FIFO wants 44100 Hz/2 ch/16-bit - "
                  "this track will be silent");
        } else {
            t->fifo_fd = open(fifo, O_WRONLY);
            if (t->fifo_fd < 0)
                trace("AudioTrack: cannot open %s: %s", fifo, strerror(errno));
            else
                trace("AudioTrack: direct output to %s", fifo);
        }
        return self;
    }

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        trace("AudioTrack: SDL audio unavailable (%s) - samples are dropped", SDL_GetError());
        return self;
    }
    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof(want));
    want.freq     = t->rate;
    want.format   = t->bits == 8 ? AUDIO_U8 : AUDIO_S16LSB;
    want.channels = (Uint8)t->channels;
    want.samples  = 1024;
    t->device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!t->device)
        trace("AudioTrack: SDL_OpenAudioDevice failed (%s) - samples are dropped", SDL_GetError());
    else
        trace("AudioTrack: SDL driver '%s'", SDL_GetCurrentAudioDriver());
    return self;
}

static jint AudioTrack_getMinBufferSize(JNIEnv *env, jclass clazz, jint rate, jint channel_config, jint format)
{
    (void)env; (void)clazz;
    int channels = channel_config == CHANNEL_OUT_MONO ? 1 : 2;
    int bytes    = format == ENCODING_PCM_8BIT ? 1 : 2;
    /* About 46 ms, the order of what a phone of the game's time answered. */
    int frames = rate > 0 ? rate * 46 / 1000 : 2048;
    return frames * channels * bytes;
}

static jint AudioTrack_getNativeOutputSampleRate(JNIEnv *env, jclass clazz, jint stream)
{
    (void)env; (void)clazz; (void)stream;
    return 44100;
}

static void AudioTrack_play(JNIEnv *env, jobject self)
{
    (void)env;
    AndroidAudioTrack *t = (AndroidAudioTrack *)self;
    t->playing = true;
    if (t->device)
        SDL_PauseAudioDevice(t->device, 0);
    trace("AudioTrack.play()");
}

static void AudioTrack_pause(JNIEnv *env, jobject self)
{
    (void)env;
    AndroidAudioTrack *t = (AndroidAudioTrack *)self;
    t->playing = false;
    if (t->device)
        SDL_PauseAudioDevice(t->device, 1);
}

static void AudioTrack_stop(JNIEnv *env, jobject self)
{
    AudioTrack_pause(env, self);
    AndroidAudioTrack *t = (AndroidAudioTrack *)self;
    if (t->device)
        SDL_ClearQueuedAudio(t->device);
}

static void AudioTrack_release(JNIEnv *env, jobject self)
{
    AudioTrack_stop(env, self);
    AndroidAudioTrack *t = (AndroidAudioTrack *)self;
    if (t->device) {
        SDL_CloseAudioDevice(t->device);
        t->device = 0;
    }
    if (t->fifo_fd >= 0) {
        close(t->fifo_fd);
        t->fifo_fd = -1;
    }
}

static jint AudioTrack_getPlayState(JNIEnv *env, jobject self)
{
    (void)env;
    return ((AndroidAudioTrack *)self)->playing ? PLAYSTATE_PLAYING : PLAYSTATE_STOPPED;
}

static jint AudioTrack_write(JNIEnv *env, jobject self, jbyteArray data, jint offset, jint size)
{
    (void)env;
    AndroidAudioTrack *t = (AndroidAudioTrack *)self;
    ArrayObject *arr = (ArrayObject *)data;
    if (!arr || !arr->elements || size <= 0 || offset < 0 || offset + size > arr->count)
        return 0;
    const unsigned char *p = (const unsigned char *)arr->elements + offset;

    if (t->written == 0)
        trace("AudioTrack.write: first block, %d bytes", size);
    t->written += size;

    if (t->fifo_fd >= 0) {
        int left = size;
        while (left > 0) {
            ssize_t n = write(t->fifo_fd, p, left);
            if (n < 0) {
                if (errno == EINTR)
                    continue;
                trace("AudioTrack: pipe write failed (%s) - output closed", strerror(errno));
                close(t->fifo_fd);
                t->fifo_fd = -1;
                break;
            }
            p += n;
            left -= (int)n;
        }
        return size;
    }

    if (t->device) {
        /* Block like the real thing: no more than ~100 ms waiting in the queue. */
        const Uint32 limit = (Uint32)(t->rate * frame_bytes(t) / 10);
        while (t->playing && SDL_GetQueuedAudioSize(t->device) > limit)
            sleep_ms(2);
        SDL_QueueAudio(t->device, p, (Uint32)size);
        return size;
    }

    /* Nowhere to send it: take as long as it would have played. */
    sleep_ms((long)size * 1000 / (t->rate * frame_bytes(t)));
    return size;
}

const ManagedMethod androidAudioTrackMethods[] = {
    REGISTER_INIT_METHOD(AndroidAudioTrack, AudioTrack_init, "(IIIIII)V"),
    ManagedMethod::RegisterStatic<&AudioTrack_getMinBufferSize>(
        AndroidAudioTrack::clazz, "getMinBufferSize", "(III)I"),
    ManagedMethod::RegisterStatic<&AudioTrack_getNativeOutputSampleRate>(
        AndroidAudioTrack::clazz, "getNativeOutputSampleRate", "(I)I"),
    ManagedMethod::Register<&AudioTrack_play>(AndroidAudioTrack::clazz, "play", "()V"),
    ManagedMethod::Register<&AudioTrack_pause>(AndroidAudioTrack::clazz, "pause", "()V"),
    ManagedMethod::Register<&AudioTrack_stop>(AndroidAudioTrack::clazz, "stop", "()V"),
    ManagedMethod::Register<&AudioTrack_release>(AndroidAudioTrack::clazz, "release", "()V"),
    ManagedMethod::Register<&AudioTrack_write>(AndroidAudioTrack::clazz, "write", "([BII)I"),
    ManagedMethod::Register<&AudioTrack_getPlayState>(AndroidAudioTrack::clazz, "getPlayState", "()I"),
    {NULL},
};

Class AndroidAudioTrack::clazz = {
    .classpath       = "android/media/AudioTrack",
    .classname       = "AudioTrack",
    .managed_methods = androidAudioTrackMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(AndroidAudioTrack),
};

static const int registered = ClassRegistry::register_class(AndroidAudioTrack::clazz);
