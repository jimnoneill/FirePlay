#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void fireplay_video_set_surface(void *anativewindow);
void fireplay_video_set_codec(int is_h265);
void fireplay_video_set_sps_pps(const uint8_t *sps, int sps_len, const uint8_t *pps, int pps_len);
void fireplay_video_push_nal(const uint8_t *data, int data_len, uint64_t pts_ntp);
void fireplay_video_shutdown();

#ifdef __cplusplus
}
#endif
