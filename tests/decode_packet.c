#include "test.h"
#include "packet.h"

// Variable length int data check

TestResult test_vl_int_data_0(void) {
    char data[] = { 0 };
    Buffer *buffer = buffer_from_data_copy(data, sizeof(data));
    uint16_t result = variable_length_int_decode(buffer);
    TESTASSERT(result == 0, "Should decode to 0");
    TEST_OK();
}

TestResult test_vl_int_data_127(void) {
    char data[] = { 127 };
    Buffer *buffer = buffer_from_data_copy(data, sizeof(data));
    uint16_t result = variable_length_int_decode(buffer);
    TESTASSERT(result == 127, "Should decode to 127");
    TEST_OK();
}

TestResult test_vl_int_data_128(void) {
    char data[] = { 0x80, 0x01 };
    Buffer *buffer = buffer_from_data_copy(data, sizeof(data));
    uint16_t result = variable_length_int_decode(buffer);
    TESTASSERT(result == 128, "Should decode to 128");
    TEST_OK();
}

TestResult test_vl_int_data_16383(void) {
    char data[] = { 0xff, 0x7f };
    Buffer *buffer = buffer_from_data_copy(data, sizeof(data));
    uint16_t result = variable_length_int_decode(buffer);
    TESTASSERT(result == 16383, "Should decode to 16383");
    TEST_OK();
}

TestResult test_vl_int_data_16384(void) {
    char data[] = { 0x80, 0x80, 0x01 };
    Buffer *buffer = buffer_from_data_copy(data, sizeof(data));
    uint16_t result = variable_length_int_decode(buffer);
    TESTASSERT(result == 16384, "Should decode to 16384");
    TEST_OK();
}

TestResult test_vl_int_data_32767(void) {
    char data[] = { 0xff, 0xff, 0x01 };
    Buffer *buffer = buffer_from_data_copy(data, sizeof(data));
    uint16_t result = variable_length_int_decode(buffer);
    TESTASSERT(result == 32767, "Should decode to 32767");
    TEST_OK();
}

// UTF-8 String decoding

TestResult test_utf8_string_empty(void) {
    char data[] = { 0x00, 0x00 };
    Buffer *buffer = buffer_from_data_copy(data, sizeof(data));
    char *string = utf8_string_decode(buffer);

    TESTASSERT(strlen(string) == 0, "Should decode empty string");
    TEST_OK();
}


TestResult test_utf8_string_hello(void) {
    char data[] = { 0x00, 0x05, 'h', 'e', 'l', 'l', 'o' };
    Buffer *buffer = buffer_from_data_copy(data, sizeof(data));
    char *string = utf8_string_decode(buffer);

    TESTASSERT(strncmp("hello", string, 5) == 0, "Should decode to 'hello' string");
    TEST_OK();
}

// packet decoder

TestResult test_decode_connect_simple(void) {
    char data[] = {
        0x10, 0x10, // header
        0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04, 0x02, 0x00, 0x0a, // var header
        0x00, 0x04, 't', 'e', 's', 't' // client id
    };
    Buffer *buffer = buffer_from_data_copy(data, sizeof(data));
    MQTTPacket *packet = mqtt_packet_decode(buffer);

    TESTASSERT(packet != NULL, "Packet should be valid");
    TESTASSERT(packet->packet_type == PacketTypeConnect, "Should be connect packet");

    ConnectPayload *payload = (ConnectPayload *)packet->payload;
    TESTASSERT(strncmp("test", payload->client_id, 4) == 0, "Client id should be 'test'");
    TESTASSERT(payload->protocol_level == 4, "Protocol level should be 4");
    TESTASSERT(payload->keepalive_interval == 10, "Keepalive should be 10");
    TESTASSERT(payload->clean_session == 1, "Clean session should be 1");

    free_MQTTPacket(packet);
    buffer_release(buffer);

    TEST_OK();
}

TestResult test_decode_connect_will(void) {
    char data[] = {
        0x10, 0x2d, // header
        0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04, 0x2e, 0x00, 0x0a, // var header
        0x00, 0x04, 't', 'e', 's', 't', // client id
        0x00, 0x0d, 't', 'e', 's', 't', '/', 'l', 'a', 's', 't', 'w', 'i', 'l', 'l',
        0x00, 0x0c, 'd', 'i', 's', 'c', 'o', 'n', 'n', 'e', 'c', 't', 'e', 'd',

    };
    Buffer *buffer = buffer_from_data_copy(data, sizeof(data));
    MQTTPacket *packet = mqtt_packet_decode(buffer);

    TESTASSERT(packet != NULL, "Packet should be valid");
    TESTASSERT(packet->packet_type == PacketTypeConnect, "Should be connect packet");

    ConnectPayload *payload = (ConnectPayload *)packet->payload;
    TESTASSERT(strncmp("test", payload->client_id, 4) == 0, "Client id should be 'test'");
    TESTASSERT(payload->protocol_level == 4, "Protocol level should be 4");
    TESTASSERT(payload->keepalive_interval == 10, "Keepalive should be 10");
    TESTASSERT(payload->clean_session == 1, "Clean session should be 1");

    TESTASSERT(strncmp("test/lastwill", payload->will_topic, 14) == 0, "Last will topic should be 'test/lastwill'");
    TESTASSERT(strncmp("disconnected", payload->will_message, 13) == 0, "Last will message should be 'disconnected'");
    TESTASSERT(payload->will_qos == MQTT_QOS_1, "Last will QoS should be 1");
    TESTASSERT(payload->retain_will == true, "Last will retain flag should be true");

    free_MQTTPacket(packet);
    buffer_release(buffer);

    TEST_OK();
}

TestResult test_decode_connect_auth(void) {
    char data[] = {
        0x10, 0x39, // header
        0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04, 0xee, 0x00, 0x0a, // var header
        0x00, 0x04, 't', 'e', 's', 't', // client id
        0x00, 0x0d, 't', 'e', 's', 't', '/', 'l', 'a', 's', 't', 'w', 'i', 'l', 'l',
        0x00, 0x0c, 'd', 'i', 's', 'c', 'o', 'n', 'n', 'e', 'c', 't', 'e', 'd',
        0x00, 0x04, 'a', 'n', 'o', 'n', // username
        0x00, 0x04, 't', 'e', 's', 't' // password
    };
    Buffer *buffer = buffer_from_data_copy(data, sizeof(data));
    MQTTPacket *packet = mqtt_packet_decode(buffer);

    TESTASSERT(packet != NULL, "Packet should be valid");
    TESTASSERT(packet->packet_type == PacketTypeConnect, "Should be connect packet");

    ConnectPayload *payload = (ConnectPayload *)packet->payload;
    TESTASSERT(strncmp("test", payload->client_id, 4) == 0, "Client id should be 'test'");
    TESTASSERT(payload->protocol_level == 4, "Protocol level should be 4");
    TESTASSERT(payload->keepalive_interval == 10, "Keepalive should be 10");
    TESTASSERT(payload->clean_session == 1, "Clean session should be 1");

    TESTASSERT(strncmp("test/lastwill", payload->will_topic, 14) == 0, "Last will topic should be 'test/lastwill'");
    TESTASSERT(strncmp("disconnected", payload->will_message, 13) == 0, "Last will message should be 'disconnected'");
    TESTASSERT(payload->will_qos == MQTT_QOS_1, "Last will QoS should be 1");
    TESTASSERT(payload->retain_will == true, "Last will retain flag should be true");

    TESTASSERT(strncmp("anon", payload->username, 4) == 0, "Username should be 'anon'");
    TESTASSERT(strncmp("test", payload->password, 4) == 0, "Password should be 'test'");

    free_MQTTPacket(packet);
    buffer_release(buffer);

    TEST_OK();
}

TestResult test_decode_connack(void) {
    char data[] = {
        0x20, 0x02, // header
        0x01, // session present
        0x00 // accepted
    };
    Buffer *buffer = buffer_from_data_copy(data, sizeof(data));
    MQTTPacket *packet = mqtt_packet_decode(buffer);

    TESTASSERT(packet != NULL, "Packet should be valid");
    TESTASSERT(packet->packet_type == PacketTypeConnAck, "Should be connack packet");

    ConnAckPayload *payload = (ConnAckPayload *)packet->payload;
    TESTASSERT(payload->session_present == true, "Session should be present");
    TESTASSERT(payload->status == ConnAckStatusAccepted, "Connection status should be accepted");

    free_MQTTPacket(packet);
    buffer_release(buffer);

    TEST_OK();
}

TestResult test_decode_publish_no_msg(void) {
    char data[] = {
        0x33, 0x0e, // header, qos1, retain
        0x00, 0x0a, 't', 'e', 's', 't', '/', 't', 'o', 'p', 'i', 'c',
        0x00, 0x0a // packet id
    };
    Buffer *buffer = buffer_from_data_copy(data, sizeof(data));
    MQTTPacket *packet = mqtt_packet_decode(buffer);

    TESTASSERT(packet != NULL, "Packet should be valid");
    TESTASSERT(packet->packet_type == PacketTypePublish, "Should be publish packet");

    PublishPayload *payload = (PublishPayload *)packet->payload;
    TESTASSERT(payload->qos == MQTT_QOS_1, "QoS should be 1");
    TESTASSERT(payload->retain == true, "Retain should be true");
    TESTASSERT(payload->packet_id == 10, "Packet ID should be 10");
    TESTASSERT(strncmp("test/topic", payload->topic, 11) == 0, "Topic should match");
    TESTASSERT(payload->message == NULL, "Message should be NULL");

    free_MQTTPacket(packet);
    buffer_release(buffer);

    TEST_OK();
}

TestResult test_decode_publish_with_msg(void) {
    char data[] = {
        0x33, 0x15, // header, qos1, retain
        0x00, 0x0a, 't', 'e', 's', 't', '/', 't', 'o', 'p', 'i', 'c',
        0x00, 0x0a, // packet id
        'p', 'a', 'y', 'l', 'o', 'a', 'd'
    };
    Buffer *buffer = buffer_from_data_copy(data, sizeof(data));
    MQTTPacket *packet = mqtt_packet_decode(buffer);

    TESTASSERT(packet != NULL, "Packet should be valid");
    TESTASSERT(packet->packet_type == PacketTypePublish, "Should be publish packet");

    PublishPayload *payload = (PublishPayload *)packet->payload;
    TESTASSERT(payload->qos == MQTT_QOS_1, "QoS should be 1");
    TESTASSERT(payload->retain == true, "Retain should be true");
    TESTASSERT(payload->packet_id == 10, "Packet ID should be 10");
    TESTASSERT(strncmp("test/topic", payload->topic, 11) == 0, "Topic should match");
    TESTASSERT(strncmp("payload", payload->message, 8) == 0, "Message should be 'payload'");

    free_MQTTPacket(packet);
    buffer_release(buffer);

    TEST_OK();
}

TestResult test_decode_puback(void) {
    char data[] = {
        0x40, 0x02, // header
        0x00, 0x0a  // packet id 
    };
    Buffer *buffer = buffer_from_data_copy(data, sizeof(data));
    MQTTPacket *packet = mqtt_packet_decode(buffer);

    TESTASSERT(packet != NULL, "Packet should be valid");
    TESTASSERT(packet->packet_type == PacketTypePubAck, "Should be puback packet");

    PubAckPayload *payload = (PubAckPayload *)packet->payload;
    TESTASSERT(payload->packet_id == 10, "Packet id should be 10");

    free_MQTTPacket(packet);
    buffer_release(buffer);

    TEST_OK();
}

TestResult test_decode_pubrec(void) {
    char data[] = {
        0x50, 0x02, // header
        0x00, 0x0a  // packet id 
    };
    Buffer *buffer = buffer_from_data_copy(data, sizeof(data));
    MQTTPacket *packet = mqtt_packet_decode(buffer);

    TESTASSERT(packet != NULL, "Packet should be valid");
    TESTASSERT(packet->packet_type == PacketTypePubRec, "Should be pubrec packet");

    PubRecPayload *payload = (PubRecPayload *)packet->payload;
    TESTASSERT(payload->packet_id == 10, "Packet id should be 10");

    free_MQTTPacket(packet);
    buffer_release(buffer);

    TEST_OK();
}

TestResult test_decode_pubrel(void) {
    char data[] = {
        0x60, 0x02, // header
        0x00, 0x0a  // packet id 
    };
    Buffer *buffer = buffer_from_data_copy(data, sizeof(data));
    MQTTPacket *packet = mqtt_packet_decode(buffer);

    TESTASSERT(packet != NULL, "Packet should be valid");
    TESTASSERT(packet->packet_type == PacketTypePubRel, "Should be pubrel packet");

    PubRelPayload *payload = (PubRelPayload *)packet->payload;
    TESTASSERT(payload->packet_id == 10, "Packet id should be 10");

    free_MQTTPacket(packet);
    buffer_release(buffer);

    TEST_OK();
}

TestResult test_decode_pubcomp(void) {
    char data[] = {
        0x70, 0x02, // header
        0x00, 0x0a  // packet id 
    };
    Buffer *buffer = buffer_from_data_copy(data, sizeof(data));
    MQTTPacket *packet = mqtt_packet_decode(buffer);

    TESTASSERT(packet != NULL, "Packet should be valid");
    TESTASSERT(packet->packet_type == PacketTypePubComp, "Should be pubcomp packet");

    PubCompPayload *payload = (PubCompPayload *)packet->payload;
    TESTASSERT(payload->packet_id == 10, "Packet id should be 10");

    free_MQTTPacket(packet);
    buffer_release(buffer);

    TEST_OK();
}

TestResult test_decode_subscribe(void) {
    char data[] = {
        0x80, 0x0f, // header
        0x00, 0x0a,  // packet id
        0x00, 0x0a, 't', 'e', 's', 't', '/', 't', 'o', 'p', 'i', 'c',
        0x01 // qos
    };
    Buffer *buffer = buffer_from_data_copy(data, sizeof(data));
    MQTTPacket *packet = mqtt_packet_decode(buffer);

    TESTASSERT(packet != NULL, "Packet should be valid");
    TESTASSERT(packet->packet_type == PacketTypeSubscribe, "Should be subscribe packet");

    SubscribePayload *payload = (SubscribePayload *)packet->payload;
    TESTASSERT(payload->qos == MQTT_QOS_1, "QoS should be 1");
    TESTASSERT(payload->packet_id == 10, "Packet ID should be 10");
    TESTASSERT(strncmp("test/topic", payload->topic, 11) == 0, "Topic should match");

    free_MQTTPacket(packet);
    buffer_release(buffer);

    TEST_OK();
}

TestResult test_decode_suback(void) {
    char data[] = {
        0x90, 0x03, // header
        0x00, 0x0a,  // packet id,
        0x02 // status
    };
    Buffer *buffer = buffer_from_data_copy(data, sizeof(data));
    MQTTPacket *packet = mqtt_packet_decode(buffer);

    TESTASSERT(packet != NULL, "Packet should be valid");
    TESTASSERT(packet->packet_type == PacketTypeSubAck, "Should be suback packet");

    SubAckPayload *payload = (SubAckPayload *)packet->payload;
    TESTASSERT(payload->packet_id == 10, "Packet ID should be 10");
    TESTASSERT(payload->status == SubAckStatusQoS2, "Status should be QoS 2 ack");

    free_MQTTPacket(packet);
    buffer_release(buffer);

    TEST_OK();
}

TestResult test_decode_unsubscribe(void) {
    char data[] = {
        0xa0, 0x0e, // header
        0x00, 0x0a, // packet id
        0x00, 0x0a, 't', 'e', 's', 't', '/', 't', 'o', 'p', 'i', 'c',
    };
    Buffer *buffer = buffer_from_data_copy(data, sizeof(data));
    MQTTPacket *packet = mqtt_packet_decode(buffer);

    TESTASSERT(packet != NULL, "Packet should be valid");
    TESTASSERT(packet->packet_type == PacketTypeUnsubscribe, "Should be unsubscribe packet");

    UnsubscribePayload *payload = (UnsubscribePayload *)packet->payload;
    TESTASSERT(payload->packet_id == 10, "Packet ID should be 10");
    TESTASSERT(strncmp("test/topic", payload->topic, 11) == 0, "Topic should match");

    free_MQTTPacket(packet);
    buffer_release(buffer);

    TEST_OK();
}

TestResult test_decode_unsuback(void) {
    char data[] = {
        0xb0, 0x02, // header
        0x00, 0x0a  // packet id,
    };
    Buffer *buffer = buffer_from_data_copy(data, sizeof(data));
    MQTTPacket *packet = mqtt_packet_decode(buffer);

    TESTASSERT(packet != NULL, "Packet should be valid");
    TESTASSERT(packet->packet_type == PacketTypeUnsubAck, "Should be unsuback packet");

    UnsubAckPayload *payload = (UnsubAckPayload *)packet->payload;
    TESTASSERT(payload->packet_id == 10, "Packet ID should be 10");

    free_MQTTPacket(packet);
    buffer_release(buffer);

    TEST_OK();
}

TestResult test_decode_pingreq(void) {
    char data[] = {
        0xc0, 0x00 // header
    };
    Buffer *buffer = buffer_from_data_copy(data, sizeof(data));
    MQTTPacket *packet = mqtt_packet_decode(buffer);

    TESTASSERT(packet != NULL, "Packet should be valid");
    TESTASSERT(packet->packet_type == PacketTypePingReq, "Should be pingreq packet");

    free_MQTTPacket(packet);
    buffer_release(buffer);

    TEST_OK();
}

TestResult test_decode_pingresp(void) {
    char data[] = {
        0xd0, 0x00 // header
    };
    Buffer *buffer = buffer_from_data_copy(data, sizeof(data));
    MQTTPacket *packet = mqtt_packet_decode(buffer);

    TESTASSERT(packet != NULL, "Packet should be valid");
    TESTASSERT(packet->packet_type == PacketTypePingResp, "Should be pingresp packet");

    free_MQTTPacket(packet);
    buffer_release(buffer);

    TEST_OK();
}

TestResult test_decode_disconnect(void) {
    char data[] = {
        0xe0, 0x00 // header
    };
    Buffer *buffer = buffer_from_data_copy(data, sizeof(data));
    MQTTPacket *packet = mqtt_packet_decode(buffer);

    TESTASSERT(packet != NULL, "Packet should be valid");
    TESTASSERT(packet->packet_type == PacketTypeDisconnect, "Should be disconnect packet");

    free_MQTTPacket(packet);
    buffer_release(buffer);

    TEST_OK();
}

// not implemented placeholder

TestResult not_implemented(void) {
    TESTRESULT(TestStatusSkipped, "Not implemented");
}


TESTS(
    TEST("Variable length int decode for 0", test_vl_int_data_0),
    TEST("Variable length int decode for 127", test_vl_int_data_127),
    TEST("Variable length int decode for 128", test_vl_int_data_128),
    TEST("Variable length int decode for 16383", test_vl_int_data_16383),
    TEST("Variable length int decode for 16384", test_vl_int_data_16384),
    TEST("Variable length int decode for 32767", test_vl_int_data_32767),
    TEST("UTF-8 string decode empty string", test_utf8_string_empty),
    TEST("UTF-8 string decode \"hello\"", test_utf8_string_hello),
    TEST("Decode Connect simple", test_decode_connect_simple),
    TEST("Decode Connect with will", test_decode_connect_will),
    TEST("Decode Connect with auth", test_decode_connect_auth),
    TEST("Decode ConnAck", test_decode_connack),
    TEST("Decode Publish with no message", test_decode_publish_no_msg),
    TEST("Decode Publish with message", test_decode_publish_with_msg),
    TEST("Decode PubAck", test_decode_puback),
    TEST("Decode PubRec", test_decode_pubrec),
    TEST("Decode PubRel", test_decode_pubrel),
    TEST("Decode PubComp", test_decode_pubcomp),
    TEST("Decode Subscribe", test_decode_subscribe),
    TEST("Decode SubAck", test_decode_suback),
    TEST("Decode Unsubscribe", test_decode_unsubscribe),
    TEST("Decode UnsubAck", test_decode_unsuback),
    TEST("Decode PingReq", test_decode_pingreq),
    TEST("Decode PingResp", test_decode_pingresp),
    TEST("Decode Disconnect", test_decode_disconnect)
);
