#ifndef PDAFT_FRAME_PROTOCOL_H
#define PDAFT_FRAME_PROTOCOL_H
#include <stdint.h>
#define PDAFT_FRAME_MAGIC 0x50444658u
#define PDAFT_FRAME_SLOTS 3
#define PDAFT_FRAME_MAP_SIZE 4096
// Cross-process ABI; same layout on x86_64 and arm64. States use atomic builtins.
typedef struct {
    uint32_t surface_id;
    uint32_t state; // 0 free, 1 ready, 2 GPU/producer owned
    uint64_t sequence;
    uint64_t time_ns;
    int32_t game_state;
    int32_t game_substate;
} PDAFTFrameSlot;
typedef struct {
    uint32_t magic, version, producer_pid, width, height, stopped;
    uint64_t produced, dropped;
    PDAFTFrameSlot slots[PDAFT_FRAME_SLOTS];
    uint32_t auto_play, input_active, stop_requested, input_hooked;
    uint64_t autoplay_pulses;
    uint8_t keys[256];
    uint64_t key_events,key_polls,manual_seen,foreground_calls,expected_window,returned_window;
    int32_t mouse_x,mouse_y;
    uint32_t mouse_enabled;
    // Version 2 appends metadata without changing the established input ABI.
    uint32_t hdr_mv_only;
    uint8_t pending_taps[256];
    uint32_t ui_mask_surface_ids[PDAFT_FRAME_SLOTS];
    uint64_t ui_mask_sequence[PDAFT_FRAME_SLOTS];
} PDAFTFrameControl;
#endif
