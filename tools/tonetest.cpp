// Play a 440 Hz tone for two seconds with BSoundPlayer, to check that
// sound output works at all.
#include <Application.h>
#include <SoundPlayer.h>
#include <math.h>
#include <stdio.h>

static double phase;

static void
fill(void *, void *buffer, size_t size, const media_raw_audio_format &fmt)
{
  float *out = (float *) buffer;
  size_t frames = size / (sizeof(float) * fmt.channel_count);
  for (size_t i = 0; i < frames; i++)
    {
      float v = 0.3f * sin(phase);
      phase += 2 * M_PI * 440 / fmt.frame_rate;
      for (uint32 c = 0; c < fmt.channel_count; c++)
        *out++ = v;
    }
}

int
main()
{
  BApplication app("application/x-vnd.test-tone");
  media_raw_audio_format f = media_raw_audio_format::wildcard;
  f.frame_rate = 44100;
  f.channel_count = 2;
  f.format = media_raw_audio_format::B_AUDIO_FLOAT;
  f.byte_order = B_MEDIA_HOST_ENDIAN;
  BSoundPlayer player(&f, "tone", fill);
  printf("init %s\n", strerror(player.InitCheck()));
  player.Start();
  player.SetHasData(true);
  snooze(2000000);
  player.Stop();
  printf("format rate %.0f channels %u format 0x%x\n",
         player.Format().frame_rate, (unsigned) player.Format().channel_count,
         (unsigned) player.Format().format);
  return 0;
}
