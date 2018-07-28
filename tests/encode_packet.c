#include "test.h"
#include "packet.h"

// Variable length int length calculation
TestResult test_vl_int_0(void) {
    TESTASSERT(variable_length_int_size(0) == 1, "Values < 128 should fit in one byte");
}

TestResult test_vl_int_127(void) {
    TESTASSERT(variable_length_int_size(127) == 1, "Values < 128 should fit in one byte");
}

TestResult test_vl_int_128(void) {
    TESTASSERT(variable_length_int_size(128) == 2, "Values < 16384 should fit in two bytes");
}

TestResult test_vl_int_16383(void) {
    TESTASSERT(variable_length_int_size(16383) == 2, "Values < 16384 should fit in two bytes");
}

TestResult test_vl_int_16384(void) {
    TESTASSERT(variable_length_int_size(16384) == 3, "Values < 2097151 should fit in three bytes");
}

// Variable length int data check

TestResult test_vl_int_data_0(void) {
    char data[] = { 0 };
    Buffer *buffer = buffer_allocate(5);
    variable_length_int_encode(0, buffer);

    TESTASSERT(
        (
            (buffer->position == 1)
            &&
            (memcmp(buffer->data, data, 1) == 0)
        ),
    "Variable length int of 0 should result in [0x00]");
}

TestResult test_vl_int_data_127(void) {
    char data[] = { 127 };
    Buffer *buffer = buffer_allocate(5);
    variable_length_int_encode(127, buffer);

    TESTASSERT(
        (
            (buffer->position == 1)
            &&
            (memcmp(buffer->data, data, 1) == 0)
        ),
    "Variable length int of 127 should result in [0x7f]");
}

TestResult test_vl_int_data_128(void) {
    char data[] = { 0x80, 0x01 };
    Buffer *buffer = buffer_allocate(5);
    variable_length_int_encode(128, buffer);

    TESTASSERT(
        (
            (buffer->position == 2)
            &&
            (memcmp(buffer->data, data, 2) == 0)
        ),
    "Variable length int of 128 should result in [0x80, 0x01]");
}

TestResult test_vl_int_data_16383(void) {
    char data[] = { 0xff, 0x7f };
    Buffer *buffer = buffer_allocate(5);
    variable_length_int_encode(16383, buffer);

    TESTASSERT(
        (
            (buffer->position == 2)
            &&
            (memcmp(buffer->data, data, 2) == 0)
        ),
    "Variable length int of 16383 should result in [0xff, 0x7f]");
}

TestResult test_vl_int_data_16384(void) {
    char data[] = { 0x80, 0x80, 0x01 };
    Buffer *buffer = buffer_allocate(5);
    variable_length_int_encode(16384, buffer);

    TESTASSERT(
        (
            (buffer->position == 3)
            &&
            (memcmp(buffer->data, data, 3) == 0)
        ),
    "Variable length int of 16384 should result in [0x80, 0x80, 0x01]");
}

TestResult test_vl_int_data_32767(void) {
    char data[] = { 0xff, 0xff, 0x01 };
    Buffer *buffer = buffer_allocate(5);
    variable_length_int_encode(32767, buffer);

    TESTASSERT(
        (
            (buffer->position == 3)
            &&
            (memcmp(buffer->data, data, 3) == 0)
        ),
    "Variable length int of 32767 should result in [0xff, 0xff, 0x01]");
}

// UTF-8 String encoding

TestResult test_utf8_string_null(void) {
    char data[] = { 0x00, 0x00 };
    Buffer *buffer = buffer_allocate(5);
    size_t sz = utf8_string_encode(NULL, buffer);
    TESTASSERT(
        (
            (buffer->position == 2)
            &&
            (sz == 2)
            &&
            (memcmp(buffer->data, data, 2) == 0)
        ),
    "NULL String should result in [0x00, 0x00]");
}

TestResult test_utf8_string_empty(void) {
    char data[] = { 0x00, 0x00 };
    Buffer *buffer = buffer_allocate(5);
    size_t sz = utf8_string_encode("", buffer);
    TESTASSERT(
        (
            (buffer->position == 2)
            &&
            (sz == 2)
            &&
            (memcmp(buffer->data, data, 2) == 0)
        ),
    "Empty String should result in [0x00, 0x00]");    
}

TestResult test_utf8_string_hello(void) {
    char data[] = { 0x00, 0x05, 'h', 'e', 'l', 'l', 'o' };
    Buffer *buffer = buffer_allocate(10);
    size_t sz = utf8_string_encode("hello", buffer);
    TESTASSERT(
        (
            (buffer->position == 7)
            &&
            (sz == 7)
            &&
            (memcmp(buffer->data, data, 7) == 0)
        ),
    "\"hello\" String should result in [0x00, 0x05, 'h', 'e', 'l', 'l', 'o']");
}

// make header
TestResult test_make_header(void) {
    char data[] = { 0x10, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00 };
    Buffer *buffer = make_buffer_for_header(5, PacketTypeConnect);

    TESTASSERT(
        (
            (buffer->len == 7)
            &&
            (memcmp(buffer->data, data, 7) == 0)
            &&
            (buffer->position == 2)
        ),
    "Header should be valid");
}

TestResult not_implemented(void) {
    TESTRESULT(TestStatusSkipped, "Not implemented");
}


TESTS(
    TEST("Variable length int size for 0", test_vl_int_0),
    TEST("Variable length int size for 127", test_vl_int_127),
    TEST("Variable length int size for 128", test_vl_int_128),
    TEST("Variable length int size for 16383", test_vl_int_16383),
    TEST("Variable length int size for 16384", test_vl_int_16384),
    TEST("Variable length int data for 0", test_vl_int_data_0),
    TEST("Variable length int data for 127", test_vl_int_data_127),
    TEST("Variable length int data for 128", test_vl_int_data_128),
    TEST("Variable length int data for 16383", test_vl_int_data_16383),
    TEST("Variable length int data for 16384", test_vl_int_data_16384),
    TEST("Variable length int data for 32767", test_vl_int_data_32767),
    TEST("UTF-8 string encode NULL", test_utf8_string_null),
    TEST("UTF-8 string encode empty string", test_utf8_string_empty),
    TEST("UTF-8 string encode \"hello\"", test_utf8_string_hello),
    TEST("Make header", test_make_header),
    TEST("Encode Connect", not_implemented),
    TEST("Encode ConnAck", not_implemented),
    TEST("Encode Publish", not_implemented),
    TEST("Encode PubAck", not_implemented),
    TEST("Encode PubRec", not_implemented),
    TEST("Encode PubRel", not_implemented),
    TEST("Encode PubComp", not_implemented),
    TEST("Encode Subscribe", not_implemented),
    TEST("Encode SubAck", not_implemented),
    TEST("Encode Unsubscribe", not_implemented),
    TEST("Encode UnsubAck", not_implemented),
    TEST("Encode PingReq", not_implemented),
    TEST("Encode PingResp", not_implemented),
    TEST("Encode Disconnect", not_implemented)
);
