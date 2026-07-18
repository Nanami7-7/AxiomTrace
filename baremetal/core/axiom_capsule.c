#include "axiom_capsule.h"

#include "axiom_frame.h"
#include "axiom_port.h"

#include <string.h>

#if AXIOM_CAPSULE_ENABLED

#define AXIOM_CAPSULE_MAGIC_A 0x41u
#define AXIOM_CAPSULE_MAGIC_X 0x58u
#define AXIOM_CAPSULE_MAGIC_C 0x43u
#define AXIOM_CAPSULE_MAGIC_P 0x50u
#define AXIOM_CAPSULE_VERSION 1u
#define AXIOM_CAPSULE_HEADER_LEN 21u
#define AXIOM_CAPSULE_CRC_LEN 4u
#define AXIOM_CAPSULE_MIN_LEN (AXIOM_CAPSULE_HEADER_LEN + AXIOM_CAPSULE_CRC_LEN)
#define AXIOM_CAPSULE_IO_CHUNK 64u

typedef enum {
    AXIOM_CAPSULE_STATE_IDLE = 0,
    AXIOM_CAPSULE_STATE_CAPTURING,
    AXIOM_CAPSULE_STATE_FROZEN,
} axiom_capsule_state_t;

/* One record-aware byte ring owns all retained wire frames. Before a fault it
 * evicts complete oldest frames; after a fault it is append-only. */
static uint8_t s_frames[AXIOM_CAPSULE_RING_SIZE];
static uint16_t s_frame_head;
static uint16_t s_frame_used;
static uint8_t s_pre_window_count;
static uint8_t s_post_window_count;

static uint8_t s_snapshot[AXIOM_CAPSULE_MAX_SNAPSHOT_LEN];
static uint8_t s_snapshot_len;
static uint8_t s_reset_reason;
static uint8_t s_fault_type;
static uint32_t s_drop_count;
static axiom_capsule_state_t s_state;

static void capsule_u16_le(uint8_t *out, uint16_t value) {
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)(value >> 8u);
}

static uint16_t capsule_read_u16_le(const uint8_t *data) {
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8u));
}

static void capsule_u32_le(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)((value >> 8u) & 0xFFu);
    out[2] = (uint8_t)((value >> 16u) & 0xFFu);
    out[3] = (uint8_t)(value >> 24u);
}

static uint32_t capsule_read_u32_le(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8u) |
        ((uint32_t)data[2] << 16u) | ((uint32_t)data[3] << 24u);
}

static void capsule_hash_be(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)(value >> 24u);
    out[1] = (uint8_t)((value >> 16u) & 0xFFu);
    out[2] = (uint8_t)((value >> 8u) & 0xFFu);
    out[3] = (uint8_t)(value & 0xFFu);
}

static uint32_t capsule_crc32_update(uint32_t crc, const uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8u; ++bit) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1u) ^ (0xEDB88320u & mask);
        }
    }
    return crc;
}

static uint32_t capsule_total_len(void) {
    return AXIOM_CAPSULE_HEADER_LEN + (uint32_t)s_snapshot_len +
        (uint32_t)s_frame_used + AXIOM_CAPSULE_CRC_LEN;
}

static bool capsule_ram_available(void) {
    return s_state != AXIOM_CAPSULE_STATE_IDLE && s_post_window_count > 0u;
}

static void capsule_build_header(uint8_t header[AXIOM_CAPSULE_HEADER_LEN]) {
    uint16_t total = (uint16_t)capsule_total_len();
    header[0] = AXIOM_CAPSULE_MAGIC_A;
    header[1] = AXIOM_CAPSULE_MAGIC_X;
    header[2] = AXIOM_CAPSULE_MAGIC_C;
    header[3] = AXIOM_CAPSULE_MAGIC_P;
    header[4] = AXIOM_CAPSULE_VERSION;
    capsule_u16_le(header + 5u, total);
    header[7] = s_reset_reason;
    header[8] = s_fault_type;
    capsule_hash_be(header + 9u, AXIOM_CAPSULE_FIRMWARE_HASH);
    capsule_u32_le(header + 13u, s_drop_count);
    header[17] = s_pre_window_count;
    header[18] = s_post_window_count;
    header[19] = AXIOM_CAPSULE_SNAPSHOT_ID;
    header[20] = s_snapshot_len;
}

static void frame_ring_copy_out(uint16_t offset, uint8_t *out, uint16_t len) {
    uint16_t index = (uint16_t)((s_frame_head + offset) % AXIOM_CAPSULE_RING_SIZE);
    uint16_t first = (uint16_t)(AXIOM_CAPSULE_RING_SIZE - index);
    if (first > len) {
        first = len;
    }
    memcpy(out, s_frames + index, first);
    if (first < len) {
        memcpy(out + first, s_frames, (uint16_t)(len - first));
    }
}

static void frame_ring_append(const uint8_t *frame, uint16_t len) {
    uint16_t tail = (uint16_t)((s_frame_head + s_frame_used) % AXIOM_CAPSULE_RING_SIZE);
    uint16_t first = (uint16_t)(AXIOM_CAPSULE_RING_SIZE - tail);
    if (first > len) {
        first = len;
    }
    memcpy(s_frames + tail, frame, first);
    if (first < len) {
        memcpy(s_frames, frame + first, (uint16_t)(len - first));
    }
    s_frame_used = (uint16_t)(s_frame_used + len);
}

static bool frame_ring_evict_oldest(void) {
    uint8_t frame[AXIOM_MAX_FRAME_LEN];
    uint16_t available = s_frame_used;
    uint16_t frame_len = 0u;
    if (available > sizeof(frame)) {
        available = sizeof(frame);
    }
    frame_ring_copy_out(0u, frame, available);
    if (!axiom_frame_validate(frame, available, &frame_len)) {
        s_frame_head = 0u;
        s_frame_used = 0u;
        s_pre_window_count = 0u;
        return false;
    }
    s_frame_head = (uint16_t)((s_frame_head + frame_len) % AXIOM_CAPSULE_RING_SIZE);
    s_frame_used = (uint16_t)(s_frame_used - frame_len);
    if (s_pre_window_count > 0u) {
        s_pre_window_count--;
    }
    return true;
}

static bool retain_pre_frame(const uint8_t *frame, uint16_t len) {
    if (AXIOM_CAPSULE_PRE_EVENTS == 0u || !frame || len == 0u ||
        len > AXIOM_MAX_FRAME_LEN || len > AXIOM_CAPSULE_RING_SIZE) {
        return false;
    }
    while (s_pre_window_count >= AXIOM_CAPSULE_PRE_EVENTS ||
           (uint32_t)s_frame_used + len > AXIOM_CAPSULE_RING_SIZE) {
        if (!frame_ring_evict_oldest()) {
            break;
        }
    }
    if ((uint32_t)s_frame_used + len > AXIOM_CAPSULE_RING_SIZE) {
        return false;
    }
    frame_ring_append(frame, len);
    if (s_pre_window_count < 255u) {
        s_pre_window_count++;
    }
    return true;
}

static bool append_post_frame(const uint8_t *frame, uint16_t len) {
    if (!frame || len == 0u || len > AXIOM_MAX_FRAME_LEN ||
        (uint32_t)s_frame_used + len > AXIOM_CAPSULE_RING_SIZE) {
        s_state = AXIOM_CAPSULE_STATE_FROZEN;
        return false;
    }
    frame_ring_append(frame, len);
    if (s_post_window_count < 255u) {
        s_post_window_count++;
    }
    return true;
}

static bool flash_write_exact(uint32_t offset, const uint8_t *data, uint32_t len) {
    int result = axiom_port_flash_write(AXIOM_CAPSULE_FLASH_BASE + offset, data, len);
    return result == 0 || result == (int)len;
}

static bool capsule_write_frames(uint32_t *offset, uint32_t *crc) {
    uint8_t chunk[AXIOM_CAPSULE_IO_CHUNK];
    uint16_t copied = 0u;
    while (copied < s_frame_used) {
        uint16_t amount = (uint16_t)(s_frame_used - copied);
        if (amount > sizeof(chunk)) {
            amount = sizeof(chunk);
        }
        frame_ring_copy_out(copied, chunk, amount);
        *crc = capsule_crc32_update(*crc, chunk, amount);
        if (!flash_write_exact(*offset, chunk, amount)) {
            return false;
        }
        *offset += amount;
        copied = (uint16_t)(copied + amount);
    }
    return true;
}

static bool flash_capsule_header(uint8_t header[AXIOM_CAPSULE_HEADER_LEN], uint16_t *len) {
    if (axiom_port_flash_read(AXIOM_CAPSULE_FLASH_BASE, header,
                              AXIOM_CAPSULE_HEADER_LEN) < 0 ||
        header[0] != AXIOM_CAPSULE_MAGIC_A || header[1] != AXIOM_CAPSULE_MAGIC_X ||
        header[2] != AXIOM_CAPSULE_MAGIC_C || header[3] != AXIOM_CAPSULE_MAGIC_P ||
        header[4] != AXIOM_CAPSULE_VERSION) {
        return false;
    }
    *len = capsule_read_u16_le(header + 5u);
    return *len >= AXIOM_CAPSULE_MIN_LEN && *len <= AXIOM_CAPSULE_MAX_IMAGE_SIZE &&
        *len <= AXIOM_CAPSULE_FLASH_SIZE;
}

static bool flash_capsule_valid(uint16_t *valid_len) {
    uint8_t header[AXIOM_CAPSULE_HEADER_LEN];
    uint8_t chunk[AXIOM_CAPSULE_IO_CHUNK];
    uint16_t len = 0u;
    if (!flash_capsule_header(header, &len)) {
        return false;
    }
    uint32_t data_len = (uint32_t)len - AXIOM_CAPSULE_CRC_LEN;
    uint32_t offset = 0u;
    uint32_t crc = 0xFFFFFFFFu;
    while (offset < data_len) {
        uint32_t amount = data_len - offset;
        if (amount > sizeof(chunk)) {
            amount = sizeof(chunk);
        }
        if (axiom_port_flash_read(AXIOM_CAPSULE_FLASH_BASE + offset, chunk, amount) < 0) {
            return false;
        }
        crc = capsule_crc32_update(crc, chunk, amount);
        offset += amount;
    }
    if (axiom_port_flash_read(AXIOM_CAPSULE_FLASH_BASE + data_len, chunk,
                              AXIOM_CAPSULE_CRC_LEN) < 0 ||
        ~crc != capsule_read_u32_le(chunk)) {
        return false;
    }
    if (valid_len) {
        *valid_len = len;
    }
    return true;
}

static uint32_t build_ram_image(uint8_t *out, uint32_t max_len) {
    uint32_t total = capsule_total_len();
    uint8_t header[AXIOM_CAPSULE_HEADER_LEN];
    if (!out || !capsule_ram_available() || total > max_len || total > 0xFFFFu) {
        return 0u;
    }
    capsule_build_header(header);
    memcpy(out, header, sizeof(header));
    if (s_snapshot_len > 0u) {
        memcpy(out + AXIOM_CAPSULE_HEADER_LEN, s_snapshot, s_snapshot_len);
    }
    frame_ring_copy_out(0u, out + AXIOM_CAPSULE_HEADER_LEN + s_snapshot_len, s_frame_used);
    uint32_t crc = ~capsule_crc32_update(0xFFFFFFFFu, out, total - AXIOM_CAPSULE_CRC_LEN);
    capsule_u32_le(out + total - AXIOM_CAPSULE_CRC_LEN, crc);
    return total;
}

void axiom_capsule_init(void) {
    s_frame_head = 0u;
    s_frame_used = 0u;
    s_pre_window_count = 0u;
    s_post_window_count = 0u;
    s_snapshot_len = 0u;
    s_reset_reason = 0u;
    s_fault_type = 0u;
    s_drop_count = 0u;
    s_state = AXIOM_CAPSULE_STATE_IDLE;
}

void axiom_capsule_record_drops(uint32_t lost) {
    s_drop_count += lost;
}

void axiom_capsule_observe_frame(const uint8_t *frame, uint16_t len, axiom_level_t level) {
    if (!frame || len == 0u) {
        return;
    }
    if (s_state == AXIOM_CAPSULE_STATE_IDLE) {
        if (level != AXIOM_LEVEL_FAULT) {
            (void)retain_pre_frame(frame, len);
            return;
        }
        s_state = AXIOM_CAPSULE_STATE_CAPTURING;
        s_reset_reason = axiom_port_reset_reason();
        s_fault_type = frame[3];
        s_snapshot_len = axiom_port_fault_snapshot(s_snapshot, AXIOM_CAPSULE_MAX_SNAPSHOT_LEN);
        if (s_snapshot_len > AXIOM_CAPSULE_MAX_SNAPSHOT_LEN) {
            s_snapshot_len = AXIOM_CAPSULE_MAX_SNAPSHOT_LEN;
        }
        (void)append_post_frame(frame, len);
    } else if (s_state == AXIOM_CAPSULE_STATE_CAPTURING &&
               s_post_window_count < AXIOM_CAPSULE_POST_EVENTS) {
        (void)append_post_frame(frame, len);
    }
    if (s_state == AXIOM_CAPSULE_STATE_CAPTURING &&
        s_post_window_count >= AXIOM_CAPSULE_POST_EVENTS) {
        s_state = AXIOM_CAPSULE_STATE_FROZEN;
    }
}

bool axiom_capsule_commit(void) {
    uint8_t header[AXIOM_CAPSULE_HEADER_LEN];
    uint8_t crc_bytes[AXIOM_CAPSULE_CRC_LEN];
    uint32_t total = capsule_total_len();
    uint32_t offset = 0u;
    uint32_t crc = 0xFFFFFFFFu;
    if (!capsule_ram_available() || total > AXIOM_CAPSULE_MAX_IMAGE_SIZE || total > 0xFFFFu ||
        axiom_port_flash_erase(AXIOM_CAPSULE_FLASH_BASE, AXIOM_CAPSULE_FLASH_SIZE) < 0) {
        return false;
    }
    capsule_build_header(header);
    crc = capsule_crc32_update(crc, header, sizeof(header));
    if (!flash_write_exact(offset, header, sizeof(header))) {
        return false;
    }
    offset += sizeof(header);
    if (s_snapshot_len > 0u) {
        crc = capsule_crc32_update(crc, s_snapshot, s_snapshot_len);
        if (!flash_write_exact(offset, s_snapshot, s_snapshot_len)) {
            return false;
        }
        offset += s_snapshot_len;
    }
    if (!capsule_write_frames(&offset, &crc)) {
        return false;
    }
    capsule_u32_le(crc_bytes, ~crc);
    return flash_write_exact(offset, crc_bytes, sizeof(crc_bytes));
}

bool axiom_capsule_present(void) {
    if (capsule_ram_available()) {
        return true;
    }
    return flash_capsule_valid(NULL);
}

uint32_t axiom_capsule_read(uint8_t *out, uint32_t max_len) {
    if (!out || max_len == 0u) {
        return 0u;
    }
    if (capsule_ram_available()) {
        return build_ram_image(out, max_len);
    }
    uint16_t len = 0u;
    if (!flash_capsule_valid(&len) || len > max_len ||
        axiom_port_flash_read(AXIOM_CAPSULE_FLASH_BASE, out, len) < 0) {
        return 0u;
    }
    return len;
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
    return 0u;
}
void axiom_capsule_clear(void) { }

#endif
