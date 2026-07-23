#include <stdio.h>
#include <stdint.h>
#include "axiomtrace.h"
#include "axiom_metadata_id_generated.h"

void emit_a(void);
void emit_b(void);

static const char *s_trace_path;

static int write_frame(const uint8_t *data, uint16_t length, void *context) {
    FILE *trace;
    (void)context;
    trace = fopen(s_trace_path, "ab");
    if (trace == NULL) {
        return -1;
    }
    if (fwrite(data, 1u, length, trace) != length) {
        (void)fclose(trace);
        return -1;
    }
    (void)fclose(trace);
    return 0;
}

int main(int argc, char **argv) {
    static const axiom_backend_t backend = AXIOM_BACKEND_INIT(.name = "trace", .write = write_frame);
    FILE *trace;
    if (argc != 2) {
        return 2;
    }
    s_trace_path = argv[1];
    trace = fopen(s_trace_path, "wb");
    if (trace == NULL) {
        return 3;
    }
    (void)fclose(trace);
    axiom_init();
    if (axiom_backend_register(&backend) != AXIOM_BACKEND_OK) {
        return 4;
    }
    AXIOM_EMIT_METADATA_ID();
    emit_a();
    emit_b();
    axiom_flush();
    return 0;
}
