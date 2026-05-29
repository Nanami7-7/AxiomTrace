#include <stdio.h>
#include <stdint.h>
#include "axiomtrace.h"

static const char *s_output_dir;
static unsigned int s_frame_index;

static int write_frame(const uint8_t *buf, uint16_t len, void *ctx) {
    char path[512];
    static const char *const names[] = {
        "frame_01_minimal.bin",
        "frame_02_u16.bin",
        "frame_03_metadata_identity.bin",
        "frame_04_location_file_id.bin",
        "frame_05_system_probe.bin"
    };
    FILE *output;
    (void)ctx;
    if (s_frame_index >= (sizeof(names) / sizeof(names[0]))) {
        return -1;
    }
    (void)snprintf(path, sizeof(path), "%s/%s", s_output_dir, names[s_frame_index]);
#if defined(_MSC_VER)
    if (fopen_s(&output, path, "wb") != 0) {
        output = NULL;
    }
#else
    output = fopen(path, "wb");
#endif
    if (output == NULL) {
        return -1;
    }
    if (fwrite(buf, 1u, len, output) != len) {
        (void)fclose(output);
        return -1;
    }
    (void)fclose(output);
    s_frame_index++;
    return 0;
}

int main(int argc, char **argv) {
    static const axiom_backend_t backend = AXIOM_BACKEND_INIT(.name = "golden", .write = write_frame);
    static const uint8_t metadata_id[AXIOM_METADATA_ID_LEN] = {
        0x01u, 0x23u, 0x45u, 0x67u, 0x89u, 0xABu, 0xCDu, 0xEFu
    };
    uint8_t payload[AXIOM_MAX_PAYLOAD_LEN];
    uint8_t position = 0u;
    if (argc != 2) {
        return 2;
    }
    s_output_dir = argv[1];
    axiom_init();
    if (axiom_backend_register(&backend) != AXIOM_BACKEND_OK) {
        return 3;
    }
    AX_EVT(INFO, 0x03u, 0x0000u);
    axiom_flush();
    AX_EVT(INFO, 0x03u, 0x0001u, (uint16_t)1000u);
    axiom_flush();
    axiom_emit_metadata_id(metadata_id);
    axiom_flush();
    axiom_enc_u8(payload, &position, 42u);
    axiom_enc_meta_location_file_id(payload, &position, 0x1234u, 77u);
    axiom_write(AXIOM_LEVEL_INFO, 0x03u, 0x0002u, payload, position);
    axiom_flush();
    AX_PROBE("sample", (uint8_t)9u);
    axiom_flush();
    return s_frame_index == 5u ? 0 : 4;
}
