#ifndef ARM_HID_INPUT_H
#define ARM_HID_INPUT_H

#include <stdint.h>

int hid_process_keyboard_report(const uint8_t *report, uint32_t report_len);
int hid_process_mouse_report(const uint8_t *report, uint32_t report_len);
int hid_attach_keyboard(void);
void hid_detach_keyboard(void);
int hid_attach_mouse(void);
void hid_detach_mouse(void);
int hid_keyboard_attached(void);
int hid_mouse_attached(void);

#endif /* ARM_HID_INPUT_H */
