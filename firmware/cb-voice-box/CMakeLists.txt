cmake_minimum_required(VERSION 3.20)

set(PICO_BOARD pico CACHE STRING "RP2040-Zero is Pico-compatible")
include(../pico-sdk/external/pico_sdk_import.cmake)

project(cb_voice_box C CXX ASM)
pico_sdk_init()

add_executable(cb_voice_box
  src/main.c
  src/usb_descriptors.c
)

pico_generate_pio_header(cb_voice_box ${CMAKE_CURRENT_LIST_DIR}/src/pcm1808_i2s.pio)

target_include_directories(cb_voice_box PRIVATE src)
target_link_libraries(cb_voice_box PRIVATE pico_stdlib tinyusb_device tinyusb_board hardware_clocks hardware_dma hardware_pio)

option(CB_VOICE_BOX_TONE_TEST "Send a known 440 Hz USB-microphone tone instead of PCM1808 audio" OFF)
if(CB_VOICE_BOX_TONE_TEST)
  target_compile_definitions(cb_voice_box PRIVATE CB_VOICE_BOX_TONE_TEST=1)
endif()
option(CB_VOICE_BOX_DISABLE_HID "Build an audio-only diagnostic image with no keyboard interface" OFF)
if(CB_VOICE_BOX_DISABLE_HID)
  target_compile_definitions(cb_voice_box PRIVATE CB_VOICE_BOX_DISABLE_HID=1)
endif()
option(CB_VOICE_BOX_SCK_WIRE_TEST "Toggle GPIO 13 slowly to verify the complete SCK wire with a multimeter" OFF)
if(CB_VOICE_BOX_SCK_WIRE_TEST)
  target_compile_definitions(cb_voice_box PRIVATE CB_VOICE_BOX_SCK_WIRE_TEST=1)
endif()
option(CB_VOICE_BOX_PTT_AUDIO_DIAGNOSTIC "Put a PTT marker tone on USB audio channel 2" OFF)
if(CB_VOICE_BOX_PTT_AUDIO_DIAGNOSTIC)
  target_compile_definitions(cb_voice_box PRIVATE CB_VOICE_BOX_PTT_AUDIO_DIAGNOSTIC=1)
endif()
option(CB_VOICE_BOX_PCM_MASTER_TEST "Let PCM1808 drive BCK/LRCK to prove its digital core responds to SCK" OFF)
if(CB_VOICE_BOX_PCM_MASTER_TEST)
  target_compile_definitions(cb_voice_box PRIVATE CB_VOICE_BOX_PCM_MASTER_TEST=1)
endif()
option(CB_VOICE_BOX_PCM_MASTER_CAPTURE "Capture I2S while PCM1808 drives BCK/LRCK" OFF)
if(CB_VOICE_BOX_PCM_MASTER_CAPTURE)
  target_compile_definitions(cb_voice_box PRIVATE CB_VOICE_BOX_PCM_MASTER_CAPTURE=1)
endif()
option(CB_VOICE_BOX_DOUBLE_PCM_SAMPLES "Repeat each PCM frame to correct an 8 kHz source for 16 kHz USB" OFF)
if(CB_VOICE_BOX_DOUBLE_PCM_SAMPLES)
  target_compile_definitions(cb_voice_box PRIVATE CB_VOICE_BOX_DOUBLE_PCM_SAMPLES=1)
endif()

pico_enable_stdio_usb(cb_voice_box 0)
pico_enable_stdio_uart(cb_voice_box 0)
pico_add_extra_outputs(cb_voice_box)
