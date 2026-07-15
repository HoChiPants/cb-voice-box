#include <string.h>
#include "bsp/board_api.h"
#include "tusb.h"

#if CB_VOICE_BOX_DOUBLE_PCM_SAMPLES
#define USB_PID 0x4061
#elif CB_VOICE_BOX_SCK_WIRE_TEST
#define USB_PID 0x4021
#elif CB_VOICE_BOX_PCM_MASTER_CAPTURE
#define USB_PID 0x4051
#elif CB_VOICE_BOX_PCM_MASTER_TEST
#define USB_PID 0x4041
#elif CB_VOICE_BOX_PTT_AUDIO_DIAGNOSTIC
#define USB_PID 0x4031
#else
#define USB_PID (0x4001 | (CB_VOICE_BOX_DISABLE_HID ? 0 : (1 << 2)) | (1 << 4))
#endif
#define EPNUM_AUDIO 0x01
#define EPNUM_HID   0x82

static tusb_desc_device_t const desc_device = {
  .bLength = sizeof(tusb_desc_device_t), .bDescriptorType = TUSB_DESC_DEVICE,
  .bcdUSB = 0x0200, .bDeviceClass = TUSB_CLASS_MISC,
  .bDeviceSubClass = MISC_SUBCLASS_COMMON, .bDeviceProtocol = MISC_PROTOCOL_IAD,
  .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE, .idVendor = 0xCafe,
  .idProduct = USB_PID, .bcdDevice = 0x0101, .iManufacturer = 1,
  .iProduct = 2, .iSerialNumber = 3, .bNumConfigurations = 1,
};

uint8_t const *tud_descriptor_device_cb(void) { return (uint8_t const *) &desc_device; }

#if !CB_VOICE_BOX_DISABLE_HID
uint8_t const desc_hid_report[] = { TUD_HID_REPORT_DESC_KEYBOARD() };
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
  (void) instance;
  return desc_hid_report;
}
#endif

enum { ITF_AUDIO_CONTROL = 0, ITF_AUDIO_STREAMING,
#if !CB_VOICE_BOX_DISABLE_HID
  ITF_HID,
#endif
  ITF_TOTAL };
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_AUDIO20_MIC_FOUR_CH_DESC_LEN + (CB_VOICE_BOX_DISABLE_HID ? 0 : TUD_HID_DESC_LEN))

uint8_t const desc_configuration[] = {
  TUD_CONFIG_DESCRIPTOR(1, ITF_TOTAL, 0, CONFIG_TOTAL_LEN, 0x80, 100),
  TUD_AUDIO20_MIC_FOUR_CH_DESCRIPTOR(ITF_AUDIO_CONTROL, 0,
    CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX,
    CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX * 8,
    0x80 | EPNUM_AUDIO, CFG_TUD_AUDIO_EP_SZ_IN),
  #if !CB_VOICE_BOX_DISABLE_HID
  TUD_HID_DESCRIPTOR(ITF_HID, 0, HID_ITF_PROTOCOL_KEYBOARD,
    sizeof(desc_hid_report), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 10),
  #endif
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void) index;
  return desc_configuration;
}

enum { STRID_LANGID = 0, STRID_MANUFACTURER, STRID_PRODUCT, STRID_SERIAL };
static char const *string_desc_arr[] = {
  (const char[]) { 0x09, 0x04 }, "CB Voice Box",
#if CB_VOICE_BOX_DOUBLE_PCM_SAMPLES
  "CB Voice Box Clock Corrected",
#elif CB_VOICE_BOX_SCK_WIRE_TEST
  "CB Voice Box SCK Wire Test",
#elif CB_VOICE_BOX_PCM_MASTER_CAPTURE
  "CB Voice Box Master Capture",
#elif CB_VOICE_BOX_PCM_MASTER_TEST
  "CB Voice Box PCM Master Test",
#elif CB_VOICE_BOX_PTT_AUDIO_DIAGNOSTIC
  "CB Voice Box PTT Diagnostic",
#else
  "CB Voice Box SCK Fix",
#endif
  NULL,
};
static uint16_t desc_str[32 + 1];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) langid;
  size_t count;
  if (index == STRID_LANGID) {
    memcpy(&desc_str[1], string_desc_arr[0], 2);
    count = 1;
  } else if (index == STRID_SERIAL) {
    count = board_usb_get_serial(desc_str + 1, 32);
  } else {
    if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) return NULL;
    const char *text = string_desc_arr[index];
    count = strlen(text);
    if (count > 32) count = 32;
    for (size_t i = 0; i < count; i++) desc_str[1 + i] = text[i];
  }
  desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * count + 2);
  return desc_str;
}
