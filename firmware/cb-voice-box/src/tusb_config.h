#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#define CFG_TUSB_MCU             OPT_MCU_RP2040
#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS              OPT_OS_NONE
#endif
#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT         0
#endif
#define CFG_TUD_ENABLED          1
#define CFG_TUD_MAX_SPEED        OPT_MODE_FULL_SPEED
#define CFG_TUD_ENDPOINT0_SIZE   64

#define CFG_TUD_CDC              0
#define CFG_TUD_MSC              0
#define CFG_TUD_MIDI             0
#define CFG_TUD_VENDOR           0
#ifndef CB_VOICE_BOX_DISABLE_HID
#define CB_VOICE_BOX_DISABLE_HID 0
#endif
#define CFG_TUD_HID              (CB_VOICE_BOX_DISABLE_HID ? 0 : 1)
#define CFG_TUD_HID_EP_BUFSIZE   16

// UAC2 microphone: four 16-bit channels at 16 kHz. Channel 1 will carry CB audio;
// the other channels are reserved while the PCM1808 capture path is brought up.
#define CFG_TUD_AUDIO                        1
#define CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE     16000
#define CFG_TUD_AUDIO_ENABLE_EP_IN           1
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX 2
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX   4
#define CFG_TUD_AUDIO_EP_SZ_IN TUD_AUDIO_EP_SIZE(TUD_OPT_HIGH_SPEED, CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE, CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX, CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX)
#define CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL     1
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX    CFG_TUD_AUDIO_EP_SZ_IN
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ (4 * CFG_TUD_AUDIO_EP_SZ_IN)

#endif
