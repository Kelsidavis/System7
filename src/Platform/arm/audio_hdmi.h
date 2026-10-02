#ifndef ARM_AUDIO_HDMI_H
#define ARM_AUDIO_HDMI_H

#include <stdint.h>

int audio_hdmi_init(void);
int audio_hdmi_enable(void);
int audio_hdmi_disable(void);
int audio_hdmi_write_samples(const int16_t *samples, uint32_t sample_count);
int audio_hdmi_flush(void);
uint32_t audio_hdmi_get_buffer_size(void);
uint32_t audio_hdmi_get_buffer_used(void);
uint32_t audio_hdmi_get_buffer_free(void);
void audio_hdmi_reset_buffer(void);
uint32_t audio_hdmi_get_sample_rate(void);
uint32_t audio_hdmi_get_channels(void);
uint32_t audio_hdmi_get_bits_per_sample(void);
void audio_hdmi_shutdown(void);
int audio_hdmi_is_enabled(void);
void audio_hdmi_test_tone(void);

#endif /* ARM_AUDIO_HDMI_H */
