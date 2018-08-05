#include "test.h"
#include "packet.h"

extern size_t variable_length_int_encode(uint16_t value, Buffer *buffer);
extern size_t variable_length_int_size(uint16_t value);
extern size_t utf8_string_encode(char *string, Buffer *buffer);
extern Buffer *make_buffer_for_header(size_t sz, MQTTControlPacketType type);

// Variable length int length calculation
TestResult test_vl_int_0(void) {
    TESTASSERT(variable_length_int_size(0) == 1, "Values < 128 should fit in one byte");
    TEST_OK();
}

TestResult test_vl_int_127(void) {
    TESTASSERT(variable_length_int_size(127) == 1, "Values < 128 should fit in one byte");
    TEST_OK();
}

TestResult test_vl_int_128(void) {
    TESTASSERT(variable_length_int_size(128) == 2, "Values < 16384 should fit in two bytes");
    TEST_OK();
}

TestResult test_vl_int_16383(void) {
    TESTASSERT(variable_length_int_size(16383) == 2, "Values < 16384 should fit in two bytes");
    TEST_OK();
}

TestResult test_vl_int_16384(void) {
    TESTASSERT(variable_length_int_size(16384) == 3, "Values < 2097151 should fit in three bytes");
    TEST_OK();
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
    (void)utf8_string_encode(NULL, buffer);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        buffer
    );
}

TestResult test_utf8_string_empty(void) {
    char data[] = { 0x00, 0x00 };
    Buffer *buffer = buffer_allocate(sizeof(data));
    (void)utf8_string_encode("", buffer);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        buffer
    );
}

TestResult test_utf8_string_hello(void) {
    char data[] = { 0x00, 0x05, 'h', 'e', 'l', 'l', 'o' };
    Buffer *buffer = buffer_allocate(sizeof(data));
    (void)utf8_string_encode("hello", buffer);

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
    ConnectPayload *payload = (ConnectPayload *)calloc(1, sizeof(ConnectPayload));

    payload->client_id = "test";
    payload->protocol_level = 4;
    payload->keepalive_interval = 10;
    payload->clean_session = 1;

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypeConnect, payload });
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
    ConnectPayload *payload = (ConnectPayload *)calloc(1, sizeof(ConnectPayload));

    payload->client_id = "test";
    payload->protocol_level = 4;
    payload->keepalive_interval = 10;
    payload->clean_session = 1;

    payload->will_topic = "test/lastwill";
    payload->will_message = "disconnected";
    payload->will_qos = MQTT_QOS_1;
    payload->retain_will = true;

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypeConnect, payload });
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
    ConnectPayload *payload = (ConnectPayload *)calloc(1, sizeof(ConnectPayload));

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

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypeConnect, payload });
    free(payload);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        encoded
    );
}

TestResult test_encode_connack(void) {
    char data[] = {
        0x20, 0x02, // header
        0x01, // session present
        0x00 // accepted
    };
    ConnAckPayload *payload = (ConnAckPayload *)calloc(1, sizeof(ConnAckPayload));

    payload->session_present = true;
    payload->status = ConnAckStatusAccepted;

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypeConnAck, payload });
    free(payload);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        encoded
    );
}

TestResult test_encode_publish_no_msg(void) {
    char data[] = {
        0x33, 0x0e, // header, qos1, retain
        0x00, 0x0a, 't', 'e', 's', 't', '/', 't', 'o', 'p', 'i', 'c',
        0x00, 0x0a // packet id
    };
    PublishPayload *payload = (PublishPayload *)calloc(1, sizeof(PublishPayload));

    payload->qos = MQTT_QOS_1;
    payload->retain = true;
    payload->topic = "test/topic";
    payload->packet_id = 10;

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypePublish, payload });
    free(payload);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        encoded
    );
}

TestResult test_encode_publish_dup_qos0(void) {
    PublishPayload *payload = (PublishPayload *)calloc(1, sizeof(PublishPayload));

    payload->qos = MQTT_QOS_0;
    payload->duplicate = true;
    payload->retain = true;
    payload->topic = "test/topic";
    payload->packet_id = 10;

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypePublish, payload });
    free(payload);
    TESTASSERT(encoded == NULL, "DUP and QoS level 0 is an incompatible combination");
    TEST_OK();
}

TestResult test_encode_publish_with_msg(void) {
    char data[] = {
        0x3b, 0x15, // header, qos1, retain
        0x00, 0x0a, 't', 'e', 's', 't', '/', 't', 'o', 'p', 'i', 'c',
        0x00, 0x0a, // packet id
        'p', 'a', 'y', 'l', 'o', 'a', 'd'
    };
    PublishPayload *payload = (PublishPayload *)calloc(1, sizeof(PublishPayload));

    payload->qos = MQTT_QOS_1;
    payload->retain = true;
    payload->duplicate = true;
    payload->topic = "test/topic";
    payload->packet_id = 10;
    payload->message = "payload";

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypePublish, payload });
    free(payload);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        encoded
    );
}

TestResult test_encode_publish_with_msg_qos0(void) {
    char data[] = {
        0x31, 0x13, // header, qos1, retain
        0x00, 0x0a, 't', 'e', 's', 't', '/', 't', 'o', 'p', 'i', 'c',
        'p', 'a', 'y', 'l', 'o', 'a', 'd'
    };
    PublishPayload *payload = (PublishPayload *)calloc(1, sizeof(PublishPayload));

    payload->qos = MQTT_QOS_0;
    payload->retain = true;
    payload->duplicate = false;
    payload->topic = "test/topic";
    payload->packet_id = 10;
    payload->message = "payload";

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypePublish, payload });
    free(payload);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        encoded
    );
}

TestResult test_encode_puback(void) {
    char data[] = {
        0x40, 0x02, // header
        0x00, 0x0a  // packet id
    };
    PubAckPayload *payload = (PubAckPayload *)calloc(1, sizeof(PubAckPayload));

    payload->packet_id = 10;

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypePubAck, payload });
    free(payload);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        encoded
    );
}

TestResult test_encode_pubrec(void) {
    char data[] = {
        0x50, 0x02, // header
        0x00, 0x0a  // packet id
    };
    PubRecPayload *payload = (PubRecPayload *)calloc(1, sizeof(PubRecPayload));

    payload->packet_id = 10;

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypePubRec, payload });
    free(payload);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        encoded
    );
}

TestResult test_encode_pubrel(void) {
    char data[] = {
        0x62, 0x02, // header
        0x00, 0x0a  // packet id
    };
    PubRelPayload *payload = (PubRelPayload *)calloc(1, sizeof(PubRelPayload));

    payload->packet_id = 10;

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypePubRel, payload });
    free(payload);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        encoded
    );
}

TestResult test_encode_pubcomp(void) {
    char data[] = {
        0x70, 0x02, // header
        0x00, 0x0a  // packet id
    };
    PubCompPayload *payload = (PubCompPayload *)calloc(1, sizeof(PubCompPayload));

    payload->packet_id = 10;

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypePubComp, payload });
    free(payload);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        encoded
    );
}

TestResult test_encode_subscribe(void) {
    char data[] = {
        0x82, 0x0f, // header
        0x00, 0x0a,  // packet id
        0x00, 0x0a, 't', 'e', 's', 't', '/', 't', 'o', 'p', 'i', 'c',
        0x01 // qos
    };
    SubscribePayload *payload = (SubscribePayload *)calloc(1, sizeof(SubscribePayload));

    payload->packet_id = 10;
    payload->topic = "test/topic";
    payload->qos = MQTT_QOS_1;

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypeSubscribe, payload });
    free(payload);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        encoded
    );
}

TestResult test_encode_suback(void) {
    char data[] = {
        0x90, 0x03, // header
        0x00, 0x0a,  // packet id,
        0x02 // status
    };
    SubAckPayload *payload = (SubAckPayload *)calloc(1, sizeof(SubAckPayload));

    payload->packet_id = 10;
    payload->status = SubAckStatusQoS2;

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypeSubAck, payload });
    free(payload);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        encoded
    );
}

TestResult test_encode_unsubscribe(void) {
    char data[] = {
        0xa2, 0x0e, // header
        0x00, 0x0a, // packet id
        0x00, 0x0a, 't', 'e', 's', 't', '/', 't', 'o', 'p', 'i', 'c',
    };
    UnsubscribePayload *payload = (UnsubscribePayload *)calloc(1, sizeof(UnsubscribePayload));

    payload->packet_id = 10;
    payload->topic = "test/topic";

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypeUnsubscribe, payload });
    free(payload);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        encoded
    );
}

TestResult test_encode_unsuback(void) {
    char data[] = {
        0xb0, 0x02, // header
        0x00, 0x0a  // packet id,
    };
    UnsubAckPayload *payload = (UnsubAckPayload *)calloc(1, sizeof(UnsubAckPayload));

    payload->packet_id = 10;

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypeUnsubAck, payload });
    free(payload);

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        encoded
    );
}

TestResult test_encode_pingreq(void) {
    char data[] = {
        0xc0, 0x00 // header
    };

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypePingReq, NULL });

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        encoded
    );
}

TestResult test_encode_pingresp(void) {
    char data[] = {
        0xd0, 0x00 // header
    };

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypePingResp, NULL });

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        encoded
    );
}

TestResult test_encode_disconnect(void) {
    char data[] = {
        0xe0, 0x00 // header
    };

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypeDisconnect, NULL });

    return TESTMEMCMP(
        buffer_from_data_copy(data, sizeof(data)),
        encoded
    );
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
    TEST("Encode ConnAck", test_encode_connack),
    TEST("Encode Publish with no message", test_encode_publish_no_msg),
    TEST("Encode Publish with invalid flags", test_encode_publish_dup_qos0),
    TEST("Encode Publish with message", test_encode_publish_with_msg),
    TEST("Encode Publish with message on QoS 0", test_encode_publish_with_msg_qos0),
    TEST("Encode PubAck", test_encode_puback),
    TEST("Encode PubRec", test_encode_pubrec),
    TEST("Encode PubRel", test_encode_pubrel),
    TEST("Encode PubComp", test_encode_pubcomp),
    TEST("Encode Subscribe", test_encode_subscribe),
    TEST("Encode SubAck", test_encode_suback),
    TEST("Encode Unsubscribe", test_encode_unsubscribe),
    TEST("Encode UnsubAck", test_encode_unsuback),
    TEST("Encode PingReq", test_encode_pingreq),
    TEST("Encode PingResp", test_encode_pingresp),
    TEST("Encode Disconnect", test_encode_disconnect)
);
