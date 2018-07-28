#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <libgen.h>

#include <string.h>

#include "cputime.h"

typedef enum {
    TestStatusSkipped = 0,
    TestStatusFailure,
    TestStatusOk,
} TestStatus;

typedef struct {
    TestStatus status;
    char *message;
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

#define TESTRESULT(_status, _message) return (TestResult){ _status, _message }

#ifdef TIMETRIAL
# define TESTASSERT(_assertion, _message) return (TestResult){ (_assertion) ? TestStatusOk : TestStatusFailure, NULL }
#else
# define TESTASSERT(_assertion, _message) return (TestResult){ (_assertion) ? TestStatusOk : TestStatusFailure, _message }
#endif

void timetrial(DefinedTest *test);

int main(int argc, char **argv) {
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
                fprintf(stderr, "%s:%d: error: Test %s failed\n", test->file, test->line, test->name);
                if (result.message) {
                    fprintf(stderr, "  -> %s\n", result.message);
                }
                return 1;
                break;
        }
    }

    return 0;
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
