#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include <string.h>

#include "buffer.h"

typedef enum {
    TestStatusSkipped = 0,
    TestStatusFailure,
    TestStatusOk,
    TestStatusFailureHexdump
} TestStatus;

typedef struct {
    TestStatus status;
    char *message;
    Buffer *buffer;
    Buffer *valid;
} TestResult;

typedef TestResult (*TestPointer)(void);

typedef struct {
    char *name;
    TestPointer run;
    char *file;
    int line;
} DefinedTest;

extern DefinedTest defined_tests[];

#define TEST(_name, _function) (DefinedTest){ _name, _function, __FILE__, __LINE__ }

#define TESTS(...) \
    DefinedTest defined_tests[] = { \
        __VA_ARGS__, \
        TEST(NULL, NULL) \
    }

#define TEST_OK() return (TestResult){ TestStatusOk, NULL, NULL, NULL }
#define TESTRESULT(_status, _message) return (TestResult){ _status, _message, NULL, NULL }
#define TESTRESULT_BUFFER(_status, _message, _buffer, _valid) return (TestResult){ _status, _message, _buffer, _valid }
#define TESTASSERT(_assertion, _message) if (!(_assertion)) { return (TestResult){ TestStatusFailure, _message, NULL, NULL }; }


static inline TestResult TESTMEMCMP(Buffer *valid, Buffer *check) {
    if (valid->len != check->len) {
        TESTRESULT_BUFFER(TestStatusFailureHexdump, "Buffer size differs from valid", check, valid);
    }
    if (memcmp(valid->data, check->data, valid->len) == 0) {
        buffer_release(check);
        buffer_release(valid);
        TESTRESULT(TestStatusOk, "Buffer matches valid");
    } else {
        TESTRESULT_BUFFER(TestStatusFailureHexdump, "Buffer and valid differ", check, valid);
    }
}

// not implemented placeholder

#if 0
static TestResult not_implemented(void) {
    TESTRESULT(TestStatusSkipped, "Not implemented");
}
#endif

int main(int argc, char **argv) {
    uint16_t successes = 0;
    uint16_t skips = 0;
    uint16_t failures = 0;

    setvbuf(stdout, NULL, _IOLBF, 128);
    setvbuf(stderr, NULL, _IOLBF, 128);

    for(DefinedTest *test = defined_tests; test->run != NULL; test++) {
        TestResult result = test->run();
        switch (result.status) {
            case TestStatusOk:
                successes++;
                fprintf(stdout, "info: Test %s suceeded\n", test->name);
                break;
            case TestStatusSkipped:
                skips++;
                fprintf(stderr, "%s:%d: warning: Skipped test %s\n", test->file, test->line, test->name);
                if (result.message) {
                    fprintf(stderr, "  -> %s\n", result.message);
                }
                break;
            case TestStatusFailure:
                failures++;
                fprintf(stderr, "%s:%d: error: Test %s failed\n", test->file, test->line, test->name);
                if (result.message) {
                    fprintf(stderr, "  -> %s\n", result.message);
                }
                break;
            case TestStatusFailureHexdump:
                failures++;
                fprintf(stderr, "%s:%d: error: Test %s failed\n", test->file, test->line, test->name);
                if (result.message) {
                    fprintf(stderr, "  -> %s\n", result.message);
                }
                if (result.valid) {
                    fprintf(stderr, "  -> Template (%zu bytes)\n", result.valid->len);
                    buffer_hexdump(result.valid, 5);
                }
                if (result.buffer) {
                    fprintf(stderr, "  -> Buffer (%zu bytes)\n", result.buffer->len);
                    buffer_hexdump(result.buffer, 5);
                }
                break;
        }
    }

    fprintf(stderr, "\n%d Tests, %d skipped, %d succeeded, %d failed\n\n",
        skips + successes + failures,
        skips,
        successes,
        failures
    );

    return failures > 0;
}
