#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <libgen.h>

#include <string.h>

#include "cputime.h"
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
    Buffer *template;
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

#define TESTRESULT(_status, _message) return (TestResult){ _status, _message, NULL, NULL }
#define TESTRESULT_BUFFER(_status, _message, _buffer, _template) return (TestResult){ _status, _message, _buffer, _template }

#ifdef TIMETRIAL
# define TESTASSERT(_assertion, _message) return (TestResult){ (_assertion) ? TestStatusOk : TestStatusFailure, NULL }
#else
# define TESTASSERT(_assertion, _message) return (TestResult){ (_assertion) ? TestStatusOk : TestStatusFailure, _message }
#endif

static inline TestResult TESTMEMCMP(Buffer *template, Buffer *check) {
    if (template->len != check->len) {
        TESTRESULT_BUFFER(TestStatusFailureHexdump, "Buffer size differs from template", check, template);
    }
    if (memcmp(template->data, check->data, template->len) == 0) {
        TESTRESULT(TestStatusOk, "Buffer matches template");
    } else {
        TESTRESULT_BUFFER(TestStatusFailureHexdump, "Buffer and template differ", check, template);
    }
}

void timetrial(DefinedTest *test);

int main(int argc, char **argv) {
    bool failure_seen = false;

    for(DefinedTest *test = defined_tests; test->run != NULL; test++) {
        TestResult result = test->run();
        switch (result.status) {
            case TestStatusOk:
                fprintf(stdout, "info: Test %s suceeded ", test->name);
                #ifdef TIMETRIAL
                timetrial(test);
                #else
                fprintf(stdout, "\n");
                #endif
                break;
            case TestStatusSkipped:
                fprintf(stderr, "%s:%d: warning: Skipped test %s\n", test->file, test->line, test->name);
                if (result.message) {
                    fprintf(stderr, "  -> %s\n", result.message);
                }
                break;
            case TestStatusFailure:
                failure_seen = true;
                fprintf(stderr, "%s:%d: error: Test %s failed\n", test->file, test->line, test->name);
                if (result.message) {
                    fprintf(stderr, "  -> %s\n", result.message);
                }
                break;
            case TestStatusFailureHexdump:
                failure_seen = true;
                fprintf(stderr, "%s:%d: error: Test %s failed\n", test->file, test->line, test->name);
                if (result.message) {
                    fprintf(stderr, "  -> %s\n", result.message);
                }
                if (result.template) {
                    fprintf(stderr, "  -> Template (%d bytes)\n", result.template->len);
                    buffer_hexdump(result.template, 5);
                }
                if (result.buffer) {
                    fprintf(stderr, "  -> Buffer (%d bytes)\n", result.buffer->len);
                    buffer_hexdump(result.buffer, 5);
                }
                break;
        }
    }

    return failure_seen;
}

#ifdef TIMETRIAL
#define ITER 10000000
void timetrial(DefinedTest *test) {
    double start, end;

    start = getCPUTime();
    for(uint64_t i = 0; i < ITER; i++) {
        volatile TestResult result = test->run();
        (void)result;
    }
    end = getCPUTime();

	double time = (end - start) * 1000.0 /* ms */ * 1000.0 /* us */ * 1000.0 /* ns */ / (double)ITER;
    fprintf(stdout, "[ %0.3f ns ]\n", time);
}
#endif
