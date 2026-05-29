#include "axiom_capsule.h"

#include "axiom_port.h"

#include <string.h>

#if AXIOM_CAPSULE_ENABLED

#define AXIOM_CAPSULE_MAGIC_A 0x41u /* A */
#define AXIOM_CAPSULE_MAGIC_X 0x58u /* X */
#define AXIOM_CAPSULE_MAGIC_C 0x43u /* C */
#define AXIOM_CAPSULE_MAGIC_P 0x50u /* P */
#define AXIOM_CAPSULE_VERSION 1u
#define AXIOM_CAPSULE_HEADER_LEN 21u
#define AXIOM_CAPSULE_CRC_LEN 4u
#define AXIOM_CAPSULE_MIN_LEN (AXIOM_CAPSULE_HEADER_LEN + AXIOM_CAPSULE_CRC_LEN)

#if AXIOM_CAPSULE_PRE_EVENTS > 0
#define AXIOM_CAPSULE_PRE_STORAGE_SLOTS AXIOM_CAPSULE_PRE_EVENTS
#else
#define AXIOM_CAPSULE_PRE_STORAGE_SLOTS 1
#endif

typedef enum {
    AXIOM_CAPSULE_STATE_IDLE = 0,
    AXIOM_CAPSULE_STATE_CAPTURING,
    AXIOM_CAPSULE_STATE_FROZEN,
} axiom_capsule_state_t;

static uint8_t s_pre_frames[AXIOM_CAPSULE_PRE_STORAGE_SLOTS][AXIOM_MAX_FRAME_LEN];
static uint16_t s_pre_lens[AXIOM_CAPSULE_PRE_STORAGE_SLOTS];
static uint8_t s_pre_next;
static uint8_t s_pre_count;

static uint8_t s_frame_stream[AXIOM_CAPSULE_RING_SIZE];
static uint16_t s_frame_stream_len;
static uint8_t s_pre_window_count;
static uint8_t s_post_window_count;

static uint8_t s_snapshot[AXIOM_CAPSULE_MAX_SNAPSHOT_LEN];
static uint8_t s_snapshot_len;
static uint8_t s_reset_reason;
static uint8_t s_fault_type;
static uint32_t s_drop_count;
static axiom_capsule_state_t s_state;

static uint8_t s_ram_capsule[AXIOM_CAPSULE_MAX_IMAGE_SIZE];
static uint16_t s_ram_capsule_len;

static void capsule_u16_le(uint8_t *out, uint16_t value) {
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)(value >> 8);
}

static uint16_t capsule_read_u16_le(const uint8_t *data) {
    uint16_t value = data[0];
    value = (uint16_t)(value | (uint16_t)((uint16_t)data[1] << 8u));
    return value;
}

static void capsule_u32_le(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)((value >> 8) & 0xFFu);
    out[2] = (uint8_t)((value >> 16) & 0xFFu);
    out[3] = (uint8_t)(value >> 24);
}

static void capsule_hash_be(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)(value >> 24);
    out[1] = (uint8_t)((value >> 16) & 0xFFu);
    out[2] = (uint8_t)((value >> 8) & 0xFFu);
    out[3] = (uint8_t)(value & 0xFFu);
}

static uint32_t capsule_crc32(const uint8_t *data, uint32_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8u; ++bit) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

static bool capsule_valid_image(const uint8_t *data, uint16_t len) {
    if (len < AXIOM_CAPSULE_MIN_LEN) {
        return false;
    }
    if (data[0] != AXIOM_CAPSULE_MAGIC_A || data[1] != AXIOM_CAPSULE_MAGIC_X ||
        data[2] != AXIOM_CAPSULE_MAGIC_C || data[3] != AXIOM_CAPSULE_MAGIC_P) {
        return false;
    }
    uint16_t encoded_len = capsule_read_u16_le(data + 5);
    if (encoded_len != len) {
        return false;
    }
    uint32_t expected = (uint32_t)data[len - 4u] |
        ((uint32_t)data[len - 3u] << 8) |
        ((uint32_t)data[len - 2u] << 16) |
        ((uint32_t)data[len - 1u] << 24);
    return capsule_crc32(data, (uint32_t)len - AXIOM_CAPSULE_CRC_LEN) == expected;
}

static bool append_frame(const uint8_t *frame, uint16_t len, bool pre_window) {
    if (!frame || len == 0u || len > AXIOM_MAX_FRAME_LEN) {
        return false;
    }
    if ((uint32_t)s_frame_stream_len + len > AXIOM_CAPSULE_RING_SIZE) {
        s_state = AXIOM_CAPSULE_STATE_FROZEN;
        return false;
    }
    memcpy(s_frame_stream + s_frame_stream_len, frame, len);
    s_frame_stream_len = (uint16_t)(s_frame_stream_len + len);
    if (pre_window) {
        if (s_pre_window_count < 255u) {
            s_pre_window_count++;
        }
    } else {
        if (s_post_window_count < 255u) {
            s_post_window_count++;
        }
    }
    return true;
}

static void store_pre_frame(const uint8_t *frame, uint16_t len) {
    if (AXIOM_CAPSULE_PRE_EVENTS == 0u || !frame || len == 0u || len > AXIOM_MAX_FRAME_LEN) {
        return;
    }
    memcpy(s_pre_frames[s_pre_next], frame, len);
    s_pre_lens[s_pre_next] = len;
    s_pre_next = (uint8_t)((s_pre_next + 1u) % AXIOM_CAPSULE_PRE_STORAGE_SLOTS);
    if (s_pre_count < AXIOM_CAPSULE_PRE_EVENTS) {
        s_pre_count++;
    }
}

static void freeze_pre_window(void) {
    uint8_t start = (s_pre_count == AXIOM_CAPSULE_PRE_EVENTS) ? s_pre_next : 0u;
    for (uint8_t i = 0; i < s_pre_count; ++i) {
        uint8_t index = (uint8_t)((start + i) % AXIOM_CAPSULE_PRE_STORAGE_SLOTS);
        if (!append_frame(s_pre_frames[index], s_pre_lens[index], true)) {
            break;
        }
    }
}

static bool build_ram_capsule(void) {
    uint32_t total_len = AXIOM_CAPSULE_HEADER_LEN + (uint32_t)s_snapshot_len +
        (uint32_t)s_frame_stream_len + AXIOM_CAPSULE_CRC_LEN;
    if (total_len > AXIOM_CAPSULE_MAX_IMAGE_SIZE || total_len > 0xFFFFu) {
        return false;
    }

    uint16_t pos = 0;
    s_ram_capsule[pos++] = AXIOM_CAPSULE_MAGIC_A;
    s_ram_capsule[pos++] = AXIOM_CAPSULE_MAGIC_X;
    s_ram_capsule[pos++] = AXIOM_CAPSULE_MAGIC_C;
    s_ram_capsule[pos++] = AXIOM_CAPSULE_MAGIC_P;
    s_ram_capsule[pos++] = AXIOM_CAPSULE_VERSION;
    capsule_u16_le(s_ram_capsule + pos, (uint16_t)total_len);
    pos = (uint16_t)(pos + 2u);
    s_ram_capsule[pos++] = s_reset_reason;
    s_ram_capsule[pos++] = s_fault_type;
    capsule_hash_be(s_ram_capsule + pos, AXIOM_CAPSULE_FIRMWARE_HASH);
    pos = (uint16_t)(pos + 4u);
    capsule_u32_le(s_ram_capsule + pos, s_drop_count);
    pos = (uint16_t)(pos + 4u);
    s_ram_capsule[pos++] = s_pre_window_count;
    s_ram_capsule[pos++] = s_post_window_count;
    s_ram_capsule[pos++] = AXIOM_CAPSULE_SNAPSHOT_ID;
    s_ram_capsule[pos++] = s_snapshot_len;
    if (s_snapshot_len > 0u) {
        memcpy(s_ram_capsule + pos, s_snapshot, s_snapshot_len);
        pos = (uint16_t)(pos + s_snapshot_len);
    }
    if (s_frame_stream_len > 0u) {
        memcpy(s_ram_capsule + pos, s_frame_stream, s_frame_stream_len);
        pos = (uint16_t)(pos + s_frame_stream_len);
    }
    uint32_t crc = capsule_crc32(s_ram_capsule, pos);
    capsule_u32_le(s_ram_capsule + pos, crc);
    pos = (uint16_t)(pos + AXIOM_CAPSULE_CRC_LEN);
    s_ram_capsule_len = pos;
    return true;
}

static bool read_flash_capsule(uint8_t *out, uint32_t max_len, uint32_t *copied) {
    uint8_t header[AXIOM_CAPSULE_HEADER_LEN];
    if (axiom_port_flash_read(AXIOM_CAPSULE_FLASH_BASE, header, sizeof(header)) < 0) {
        return false;
    }
    uint16_t len = capsule_read_u16_le(header + 5);
    if (len < AXIOM_CAPSULE_MIN_LEN || len > AXIOM_CAPSULE_MAX_IMAGE_SIZE || len > max_len) {
        return false;
    }
    if (axiom_port_flash_read(AXIOM_CAPSULE_FLASH_BASE, out, len) < 0) {
        return false;
    }
    if (!capsule_valid_image(out, len)) {
        return false;
    }
    if (copied) {
        *copied = len;
    }
    return true;
}

void axiom_capsule_init(void) {
    s_pre_next = 0;
    s_pre_count = 0;
    s_frame_stream_len = 0;
    s_pre_window_count = 0;
    s_post_window_count = 0;
    s_snapshot_len = 0;
    s_reset_reason = 0;
    s_fault_type = 0;
    s_drop_count = 0;
    s_state = AXIOM_CAPSULE_STATE_IDLE;
    s_ram_capsule_len = 0;
    memset(s_pre_lens, 0, sizeof(s_pre_lens));
}

void axiom_capsule_record_drops(uint32_t lost) {
    s_drop_count += lost;
}

void axiom_capsule_observe_frame(const uint8_t *frame, uint16_t len, axiom_level_t level) {
    if (!frame || len == 0u) {
        return;
    }

    if (s_state == AXIOM_CAPSULE_STATE_IDLE) {
        if (level == AXIOM_LEVEL_FAULT) {
            s_state = AXIOM_CAPSULE_STATE_CAPTURING;
            s_reset_reason = axiom_port_reset_reason();
            s_fault_type = frame[3];
            s_snapshot_len = axiom_port_fault_snapshot(s_snapshot, AXIOM_CAPSULE_MAX_SNAPSHOT_LEN);
            if (s_snapshot_len > AXIOM_CAPSULE_MAX_SNAPSHOT_LEN) {
                s_snapshot_len = AXIOM_CAPSULE_MAX_SNAPSHOT_LEN;
            }
            freeze_pre_window();
            (void)append_frame(frame, len, false);
            if (s_post_window_count >= AXIOM_CAPSULE_POST_EVENTS) {
                s_state = AXIOM_CAPSULE_STATE_FROZEN;
            }
        } else {
            store_pre_frame(frame, len);
        }
        return;
    }

    if (s_state == AXIOM_CAPSULE_STATE_CAPTURING) {
        if (s_post_window_count < AXIOM_CAPSULE_POST_EVENTS) {
            (void)append_frame(frame, len, false);
        }
        if (s_post_window_count >= AXIOM_CAPSULE_POST_EVENTS) {
            s_state = AXIOM_CAPSULE_STATE_FROZEN;
        }
    }
}

bool axiom_capsule_commit(void) {
    if (s_frame_stream_len == 0u) {
        return false;
    }
    if (!build_ram_capsule()) {
        return false;
    }
    int erase = axiom_port_flash_erase(AXIOM_CAPSULE_FLASH_BASE, AXIOM_CAPSULE_FLASH_SIZE);
    if (erase < 0) {
        return false;
    }
    int write = axiom_port_flash_write(AXIOM_CAPSULE_FLASH_BASE, s_ram_capsule, s_ram_capsule_len);
    return write == 0 || write == (int)s_ram_capsule_len;
}

bool axiom_capsule_present(void) {
    if (s_ram_capsule_len > 0u && capsule_valid_image(s_ram_capsule, s_ram_capsule_len)) {
        return true;
    }
    uint8_t scratch[AXIOM_CAPSULE_MAX_IMAGE_SIZE];
    uint32_t copied = 0;
    return read_flash_capsule(scratch, sizeof(scratch), &copied);
}

uint32_t axiom_capsule_read(uint8_t *out, uint32_t max_len) {
    if (!out || max_len == 0u) {
        return 0;
    }
    if (s_ram_capsule_len > 0u && s_ram_capsule_len <= max_len &&
        capsule_valid_image(s_ram_capsule, s_ram_capsule_len)) {
        memcpy(out, s_ram_capsule, s_ram_capsule_len);
        return s_ram_capsule_len;
    }
    uint32_t copied = 0;
    if (read_flash_capsule(out, max_len, &copied)) {
        return copied;
    }
    return 0;
}

void axiom_capsule_clear(void) {
    axiom_capsule_init();
    (void)axiom_port_flash_erase(AXIOM_CAPSULE_FLASH_BASE, AXIOM_CAPSULE_FLASH_SIZE);
}

#else

void axiom_capsule_init(void) { }
void axiom_capsule_observe_frame(const uint8_t *frame, uint16_t len, axiom_level_t level) {
    (void)frame;
    (void)len;
    (void)level;
}
void axiom_capsule_record_drops(uint32_t lost) { (void)lost; }
bool axiom_capsule_commit(void) { return false; }
bool axiom_capsule_present(void) { return false; }
uint32_t axiom_capsule_read(uint8_t *out, uint32_t max_len) {
    (void)out;
    (void)max_len;
    return 0;
}
void axiom_capsule_clear(void) { }

#endif
