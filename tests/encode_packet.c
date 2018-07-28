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
    Buffer *buffer = buffer_allocate(sizeof(data));
    variable_length_int_encode(0, buffer);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        buffer
    );
}

TestResult test_vl_int_data_127(void) {
    char data[] = { 127 };
    Buffer *buffer = buffer_allocate(sizeof(data));
    variable_length_int_encode(127, buffer);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        buffer
    );
}

TestResult test_vl_int_data_128(void) {
    char data[] = { 0x80, 0x01 };
    Buffer *buffer = buffer_allocate(sizeof(data));
    variable_length_int_encode(128, buffer);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        buffer
    );
}

TestResult test_vl_int_data_16383(void) {
    char data[] = { 0xff, 0x7f };
    Buffer *buffer = buffer_allocate(sizeof(data));
    variable_length_int_encode(16383, buffer);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        buffer
    );
}

TestResult test_vl_int_data_16384(void) {
    char data[] = { 0x80, 0x80, 0x01 };
    Buffer *buffer = buffer_allocate(sizeof(data));
    variable_length_int_encode(16384, buffer);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        buffer
    );
}

TestResult test_vl_int_data_32767(void) {
    char data[] = { 0xff, 0xff, 0x01 };
    Buffer *buffer = buffer_allocate(sizeof(data));
    variable_length_int_encode(32767, buffer);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        buffer
    );
}

// UTF-8 String encoding

TestResult test_utf8_string_null(void) {
    char data[] = { 0x00, 0x00 };
    Buffer *buffer = buffer_allocate(sizeof(data));
    size_t sz = utf8_string_encode(NULL, buffer);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        buffer
    );
}

TestResult test_utf8_string_empty(void) {
    char data[] = { 0x00, 0x00 };
    Buffer *buffer = buffer_allocate(sizeof(data));
    size_t sz = utf8_string_encode("", buffer);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        buffer
    );
}

TestResult test_utf8_string_hello(void) {
    char data[] = { 0x00, 0x05, 'h', 'e', 'l', 'l', 'o' };
    Buffer *buffer = buffer_allocate(sizeof(data));
    size_t sz = utf8_string_encode("hello", buffer);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        buffer
    );
}

// make header
TestResult test_make_header(void) {
    char data[] = { 0x10, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00 };
    Buffer *buffer = make_buffer_for_header(5, PacketTypeConnect);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        buffer
    );
}

TestResult test_encode_connect_simple(void) {
    char data[] = {
        0x10, 0x10, // header
        0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04, 0x02, 0x00, 0x0a, // var header
        0x00, 0x04, 't', 'e', 's', 't' // client id
    };
    ConnectPayload *payload = calloc(1, sizeof(ConnectPayload));

    payload->client_id = "test";
    payload->protocol_level = 4;
    payload->keepalive_interval = 10;
    payload->clean_session = 1;

    Buffer *encoded = encode_connect(payload);
    free(payload);
    
    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        encoded
    );
}

TestResult test_encode_connect_will(void) {
    char data[] = {
        0x10, 0x2d, // header
        0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04, 0x2e, 0x00, 0x0a, // var header
        0x00, 0x04, 't', 'e', 's', 't', // client id
        0x00, 0x0d, 't', 'e', 's', 't', '/', 'l', 'a', 's', 't', 'w', 'i', 'l', 'l',
        0x00, 0x0c, 'd', 'i', 's', 'c', 'o', 'n', 'n', 'e', 'c', 't', 'e', 'd',

    };
    ConnectPayload *payload = calloc(1, sizeof(ConnectPayload));

    payload->client_id = "test";
    payload->protocol_level = 4;
    payload->keepalive_interval = 10;
    payload->clean_session = 1;

    payload->will_topic = "test/lastwill";
    payload->will_message = "disconnected";
    payload->will_qos = MQTT_QOS_1;
    payload->retain_will = true;

    Buffer *encoded = encode_connect(payload);
    free(payload);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        encoded
    );
}

TestResult test_encode_connect_auth(void) {
    char data[] = {
        0x10, 0x39, // header
        0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04, 0xee, 0x00, 0x0a, // var header
        0x00, 0x04, 't', 'e', 's', 't', // client id
        0x00, 0x0d, 't', 'e', 's', 't', '/', 'l', 'a', 's', 't', 'w', 'i', 'l', 'l',
        0x00, 0x0c, 'd', 'i', 's', 'c', 'o', 'n', 'n', 'e', 'c', 't', 'e', 'd',
        0x00, 0x04, 'a', 'n', 'o', 'n', // username
        0x00, 0x04, 't', 'e', 's', 't' // password
    };
    ConnectPayload *payload = calloc(1, sizeof(ConnectPayload));

    payload->client_id = "test";
    payload->protocol_level = 4;
    payload->keepalive_interval = 10;
    payload->clean_session = 1;

    payload->will_topic = "test/lastwill";
    payload->will_message = "disconnected";
    payload->will_qos = MQTT_QOS_1;
    payload->retain_will = true;

    payload->username = "anon";
    payload->password = "test";

    Buffer *encoded = encode_connect(payload);
    free(payload);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        encoded
    );
}

// not implemented placeholder

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
    TEST("Encode Connect simple", test_encode_connect_simple),
    TEST("Encode Connect with will", test_encode_connect_will),
    TEST("Encode Connect with auth", test_encode_connect_auth),
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
