#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void fireplay_video_set_surface(void *anativewindow);  // ANativeWindow*; NULL clears
void fireplay_video_set_codec(int is_h265);            // 0 = H.264, 1 = H.265
void fireplay_video_push_nal(const uint8_t *data, int data_len, uint64_t pts_ntp);
void fireplay_video_shutdown();

#ifdef __cplusplus
}
#endif
