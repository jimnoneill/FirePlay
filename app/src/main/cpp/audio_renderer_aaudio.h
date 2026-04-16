#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void fireplay_audio_init();
void fireplay_audio_shutdown();
void fireplay_audio_push_alac(const uint8_t *encoded, int encoded_len);
void fireplay_audio_push_aac_eld(const uint8_t *encoded, int encoded_len);

#ifdef __cplusplus
}
#endif
