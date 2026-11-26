#include <xsdk/audio.h>
#include "xsdk_sound_sink.h"
int lcs_sound_open(void) { return NT_SUCCESS(xsdk_audio_open(8)); }
int lcs_sound_next(float **frame) { return NT_SUCCESS(xsdk_audio_next_frame(100,frame)); }
void lcs_sound_queue(void) { xsdk_audio_queue_frame(); }
void lcs_sound_close(void) { xsdk_audio_close(); }
void lcs_sound_stats(unsigned *played,unsigned *silent) { XsdkAudioStats s; xsdk_audio_stats(&s); *played=s.played; *silent=s.silent; }
