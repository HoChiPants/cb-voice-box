#include <stdbool.h>
#include <stdint.h>

#include "bsp/board_api.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "tusb.h"
#include "pcm1808_i2s.pio.h"

#define PTT_PIN 2
#define BCK_PIN 10
#define LRCK_PIN 11
#define DOUT_PIN 12
#define SCK_PIN 13
#define SAMPLES_PER_MS (CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE / 1000)
#define I2S_WORDS 256

#ifndef CB_VOICE_BOX_DOUBLE_PCM_SAMPLES
#define CB_VOICE_BOX_DOUBLE_PCM_SAMPLES 0
#endif

static uint32_t sample_rate = CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE;
static uint8_t clock_valid = 1;
static int32_t i2s_words[I2S_WORDS] __attribute__((aligned(I2S_WORDS * sizeof(int32_t))));
static int dma_channel;

static void pcm1808_init(void) {
  // 96 MHz makes both PIO clock dividers exact. The USB controller remains on
  // its dedicated 48 MHz clock.
  set_sys_clock_khz(96000, true);

  PIO pio = pio0;
#if CB_VOICE_BOX_PCM_MASTER_CAPTURE
  // The PCM1808 supplies BCK/LRCK in 256-fs master mode (MD1=MD0=HIGH).
  // A PIO receiver aligns to LRCK, samples DOUT on BCK rising edges, and DMA
  // keeps the same left/right ring-buffer layout used by the USB audio task.
  uint rx_sm = pio_claim_unused_sm(pio, true);
  uint sck_sm = pio_claim_unused_sm(pio, true);
  uint rx_offset = pio_add_program(pio, &pcm1808_i2s_master_rx_program);
  uint sck_offset = pio_add_program(pio, &pcm1808_sck_program);

  pio_sm_config rx_config = pcm1808_i2s_master_rx_program_get_default_config(rx_offset);
  sm_config_set_in_pins(&rx_config, DOUT_PIN);
  sm_config_set_in_shift(&rx_config, false, true, 32);
  sm_config_set_clkdiv(&rx_config, 1.0f);
  pio_gpio_init(pio, BCK_PIN);
  pio_gpio_init(pio, LRCK_PIN);
  pio_gpio_init(pio, DOUT_PIN);
  pio_sm_set_consecutive_pindirs(pio, rx_sm, BCK_PIN, 3, false);
  pio_sm_init(pio, rx_sm, rx_offset, &rx_config);

  pio_sm_config sck_config = pcm1808_sck_program_get_default_config(sck_offset);
  sm_config_set_sideset_pins(&sck_config, SCK_PIN);
  sm_config_set_clkdiv(&sck_config, 11.71875f);
  pio_gpio_init(pio, SCK_PIN);
  pio_sm_set_consecutive_pindirs(pio, sck_sm, SCK_PIN, 1, true);
  pio_sm_init(pio, sck_sm, sck_offset, &sck_config);

  dma_channel = dma_claim_unused_channel(true);
  dma_channel_config dma_config = dma_channel_get_default_config(dma_channel);
  channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
  channel_config_set_read_increment(&dma_config, false);
  channel_config_set_write_increment(&dma_config, true);
  channel_config_set_ring(&dma_config, true, 10);
  channel_config_set_dreq(&dma_config, pio_get_dreq(pio, rx_sm, false));
  dma_channel_configure(dma_channel, &dma_config, i2s_words, &pio->rxf[rx_sm], 0xffffffff, true);
  pio_enable_sm_mask_in_sync(pio, (1u << rx_sm) | (1u << sck_sm));
  return;
#elif CB_VOICE_BOX_PCM_MASTER_TEST
  // Diagnostic only: MD1=MD0=HIGH makes the PCM1808 the I2S master. Leave
  // BCK/LRCK as inputs so the Pico cannot contend with the ADC's outputs.
  gpio_init(BCK_PIN);
  gpio_set_dir(BCK_PIN, GPIO_IN);
  gpio_disable_pulls(BCK_PIN);
  gpio_init(LRCK_PIN);
  gpio_set_dir(LRCK_PIN, GPIO_IN);
  gpio_disable_pulls(LRCK_PIN);
  gpio_init(DOUT_PIN);
  gpio_set_dir(DOUT_PIN, GPIO_IN);
  gpio_disable_pulls(DOUT_PIN);

  uint sck_sm = pio_claim_unused_sm(pio, true);
  uint sck_offset = pio_add_program(pio, &pcm1808_sck_program);
  pio_sm_config sck_config = pcm1808_sck_program_get_default_config(sck_offset);
  sm_config_set_sideset_pins(&sck_config, SCK_PIN);
  sm_config_set_clkdiv(&sck_config, 11.71875f);
  pio_gpio_init(pio, SCK_PIN);
  pio_sm_set_consecutive_pindirs(pio, sck_sm, SCK_PIN, 1, true);
  pio_sm_init(pio, sck_sm, sck_offset, &sck_config);
  pio_sm_set_enabled(pio, sck_sm, true);
  return;
#else
  uint sm = pio_claim_unused_sm(pio, true);
  uint sck_sm = pio_claim_unused_sm(pio, true);
  uint offset = pio_add_program(pio, &pcm1808_i2s_program);
  uint sck_offset = pio_add_program(pio, &pcm1808_sck_program);
  pio_sm_config config = pcm1808_i2s_program_get_default_config(offset);
  sm_config_set_sideset_pins(&config, BCK_PIN);
  sm_config_set_in_pins(&config, DOUT_PIN);
  sm_config_set_in_shift(&config, false, true, 32);
  sm_config_set_clkdiv(&config, 46.875f); // 2.048 MHz PIO clock

  pio_gpio_init(pio, BCK_PIN);
  pio_gpio_init(pio, LRCK_PIN);
  pio_gpio_init(pio, DOUT_PIN);
  pio_sm_set_consecutive_pindirs(pio, sm, BCK_PIN, 2, true);
  pio_sm_set_consecutive_pindirs(pio, sm, DOUT_PIN, 1, false);
  pio_sm_init(pio, sm, offset, &config);

  // The RP2040's dedicated clock outputs are unavailable on GPIO 13, so a
  // second PIO state machine generates SCK there. 96 MHz / 11.71875 gives an
  // 8.192 MHz instruction rate; two instructions make one 4.096 MHz cycle.
#if CB_VOICE_BOX_SCK_WIRE_TEST
  gpio_init(SCK_PIN);
  gpio_set_dir(SCK_PIN, GPIO_OUT);
  gpio_put(SCK_PIN, false);
#else
  pio_sm_config sck_config = pcm1808_sck_program_get_default_config(sck_offset);
  sm_config_set_sideset_pins(&sck_config, SCK_PIN);
  sm_config_set_clkdiv(&sck_config, 11.71875f);
  pio_gpio_init(pio, SCK_PIN);
  pio_sm_set_consecutive_pindirs(pio, sck_sm, SCK_PIN, 1, true);
  pio_sm_init(pio, sck_sm, sck_offset, &sck_config);
#endif

  dma_channel = dma_claim_unused_channel(true);
  dma_channel_config dma_config = dma_channel_get_default_config(dma_channel);
  channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
  channel_config_set_read_increment(&dma_config, false);
  channel_config_set_write_increment(&dma_config, true);
  channel_config_set_ring(&dma_config, true, 10); // 256 × 32-bit words
  channel_config_set_dreq(&dma_config, pio_get_dreq(pio, sm, false));
  dma_channel_configure(dma_channel, &dma_config, i2s_words, &pio->rxf[sm], 0xffffffff, true);
  // Start SCK and the I2S clocks together so LRCK/BCK remain synchronized to
  // the PCM1808 system clock as required in slave mode.
#if CB_VOICE_BOX_SCK_WIRE_TEST
  pio_sm_set_enabled(pio, sm, true);
#else
  pio_enable_sm_mask_in_sync(pio, (1u << sm) | (1u << sck_sm));
#endif
#endif
}

static void sck_wire_test_task(void) {
#if CB_VOICE_BOX_SCK_WIRE_TEST
  static uint32_t previous_ms;
  const uint32_t now = tusb_time_millis_api();
  if (now - previous_ms >= 1000) {
    previous_ms = now;
    gpio_xor_mask(1u << SCK_PIN);
  }
#endif
}

static void ptt_task(void) {
#if !CB_VOICE_BOX_DISABLE_HID
  static bool was_pressed = false;
  const bool ptt = !gpio_get(PTT_PIN);
  if (ptt == was_pressed || !tud_hid_ready()) return;
  const uint8_t modifiers = KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_LEFTALT | KEYBOARD_MODIFIER_LEFTSHIFT;
  const uint8_t keys[6] = { ptt ? HID_KEY_P : 0 };
  tud_hid_keyboard_report(0, ptt ? modifiers : 0, keys);
  was_pressed = ptt;
#endif
}

static void audio_task(void) {
  static uint32_t previous_ms;
  static uint32_t read_index;
  const uint32_t now = tusb_time_millis_api();
  if (now == previous_ms || !tud_mounted()) return;
  previous_ms = now;

  static int16_t frames[SAMPLES_PER_MS][CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX];
#if CB_VOICE_BOX_TONE_TEST
  // Temporary diagnostic: a 440 Hz tone proves macOS and the USB audio
  // descriptor independently of the preamp, ADC, I2S clocks, and wiring.
  static uint32_t phase;
  const uint32_t phase_step = (440u << 16) / CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE;
  for (uint32_t frame = 0; frame < SAMPLES_PER_MS; frame++) {
    const int16_t sample = (phase & 0x8000) ? 12000 : -12000;
    phase += phase_step;
    frames[frame][0] = sample;
    frames[frame][1] = 0;
    frames[frame][2] = 0;
    frames[frame][3] = 0;
  }
#else
  const uint32_t writer_index = ((uint32_t) dma_hw->ch[dma_channel].write_addr - (uint32_t) i2s_words) / sizeof(i2s_words[0]);
  // This PCM1808 module has been measured producing eight stereo frames per
  // millisecond even though USB must remain at 16 kHz for the desktop app.
  // In the corrected build, consume eight source frames and repeat each one.
  const uint32_t source_frames_needed = CB_VOICE_BOX_DOUBLE_PCM_SAMPLES
      ? SAMPLES_PER_MS / 2
      : SAMPLES_PER_MS;
  if (((writer_index - read_index) & (I2S_WORDS - 1)) < source_frames_needed * 2) return;

#if CB_VOICE_BOX_PTT_AUDIO_DIAGNOSTIC
  static uint32_t ptt_phase;
  const uint32_t ptt_phase_step = (880u << 16) / CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE;
  const bool ptt_pressed = !gpio_get(PTT_PIN);
#endif
  int16_t left = 0;
  int16_t right = 0;
  for (uint32_t frame = 0; frame < SAMPLES_PER_MS; frame++) {
    // Keep the PCM1808 left channel. Its 24-bit I2S sample is sign-extended
    // here to the 16-bit USB microphone format expected by Whisper.
    if (!CB_VOICE_BOX_DOUBLE_PCM_SAMPLES || (frame & 1u) == 0) {
      left = (int16_t) (i2s_words[read_index] >> 16);
      read_index = (read_index + 1) & (I2S_WORDS - 1);
      right = (int16_t) (i2s_words[read_index] >> 16);
      read_index = (read_index + 1) & (I2S_WORDS - 1);
    }
    frames[frame][0] = left;
#if CB_VOICE_BOX_PTT_AUDIO_DIAGNOSTIC
    // Channel 2 is an out-of-band PTT marker for diagnostics and, later, the
    // desktop app. It replaces keyboard shortcuts with a signal carried by
    // the existing USB microphone interface.
    frames[frame][1] = ptt_pressed ? ((ptt_phase & 0x8000) ? 12000 : -12000) : 0;
    ptt_phase += ptt_phase_step;
    frames[frame][2] = right;
#else
    frames[frame][1] = 0;
    frames[frame][2] = 0;
#endif
    frames[frame][3] = 0;
  }
#endif
  tud_audio_write(frames, sizeof(frames));
}

int main(void) {
  board_init();
  gpio_init(PTT_PIN);
  gpio_set_dir(PTT_PIN, GPIO_IN);
  gpio_pull_up(PTT_PIN);
  pcm1808_init();
  tusb_rhport_init_t init = { .role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_FULL };
  tusb_init(BOARD_TUD_RHPORT, &init);

  while (true) {
    tud_task();
    sck_wire_test_task();
    ptt_task();
    audio_task();
  }
}

#if !CB_VOICE_BOX_DISABLE_HID
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) {
  (void) instance; (void) report_id; (void) report_type; (void) buffer; (void) bufsize;
}
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) {
  (void) instance; (void) report_id; (void) report_type; (void) buffer; (void) reqlen;
  return 0;
}
#endif

bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *request) {
  const uint8_t control = TU_U16_HIGH(request->wValue);
  const uint8_t entity = TU_U16_HIGH(request->wIndex);
  if (entity == 4 && control == AUDIO20_CS_CTRL_SAM_FREQ) {
    if (request->bRequest == AUDIO20_CS_REQ_CUR)
      return tud_control_xfer(rhport, request, &sample_rate, sizeof(sample_rate));
    if (request->bRequest == AUDIO20_CS_REQ_RANGE) {
      audio20_control_range_4_n_t(1) range = { .wNumSubRanges = 1, .subrange = {{ sample_rate, sample_rate, 0 }} };
      return tud_control_xfer(rhport, request, &range, sizeof(range));
    }
  }
  if (entity == 4 && control == AUDIO20_CS_CTRL_CLK_VALID)
    return tud_control_xfer(rhport, request, &clock_valid, sizeof(clock_valid));
  return false;
}
bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *request, uint8_t *buffer) {
  (void) rhport; (void) request; (void) buffer;
  return false;
}
bool tud_audio_get_req_itf_cb(uint8_t rhport, tusb_control_request_t const *request) { (void) rhport; (void) request; return false; }
bool tud_audio_set_req_itf_cb(uint8_t rhport, tusb_control_request_t const *request, uint8_t *buffer) { (void) rhport; (void) request; (void) buffer; return false; }
bool tud_audio_get_req_ep_cb(uint8_t rhport, tusb_control_request_t const *request) { (void) rhport; (void) request; return false; }
bool tud_audio_set_req_ep_cb(uint8_t rhport, tusb_control_request_t const *request, uint8_t *buffer) { (void) rhport; (void) request; (void) buffer; return false; }
