#pragma once
#ifdef __cplusplus
extern "C" {
#endif
int lcs_sound_open(void);
int lcs_sound_next(float **frame);
void lcs_sound_queue(void);
void lcs_sound_close(void);
void lcs_sound_stats(unsigned *played,unsigned *silent);
#ifdef __cplusplus
}
#endif
