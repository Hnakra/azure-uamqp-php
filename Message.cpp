#include "Message.h"

static Php::Value get_value_from_map(AMQP_VALUE map, const char* key, const char type)
{
    AMQP_VALUE amqp_value_value = amqpvalue_get_map_value(map, amqpvalue_create_string(key));
    Php::Value result;

    const char* valueString = NULL;
    int64_t valueTimestamp;
    int32_t valueInt;
    double valueDouble;
    int status = 0;

    switch (type) {
        case 'S':
            status = amqpvalue_get_string(amqp_value_value, &valueString);
            result = valueString;
            break;
        case 'T':
            status = amqpvalue_get_timestamp(amqp_value_value, &valueTimestamp);
            result = valueTimestamp;
            break;
        case 'I':
            status = amqpvalue_get_int(amqp_value_value, &valueInt);
            result = valueInt;
            break;
        case 'D':
            status = amqpvalue_get_double(amqp_value_value, &valueDouble);
            result = valueDouble;
            break;
    }

    if (status != 0) {
        throw Php::Exception("Could not parse key '" + std::string(key) + "'");
    }

    return result;
}
static void add_map_item(AMQP_VALUE map, const char* name, AMQP_VALUE amqp_value_value, bool isApplicationProperty)
{
    if (isApplicationProperty) {
        AMQP_VALUE amqp_value_name = amqpvalue_create_string(name);
        amqpvalue_set_map_value(map, amqp_value_name, amqp_value_value);
        amqpvalue_destroy(amqp_value_name);
    } else {
        AMQP_VALUE amqp_value_name = amqpvalue_create_symbol(name);
        amqpvalue_set_map_value(map, amqp_value_name, amqp_value_value);
        amqpvalue_destroy(amqp_value_name);
    }
}
static void add_map_string(AMQP_VALUE map, const char* name, const char* value, bool isApplicationProperty)
{
    AMQP_VALUE amqp_value_value = amqpvalue_create_string(value);
    add_map_item(map, name, amqp_value_value, isApplicationProperty);
    amqpvalue_destroy(amqp_value_value);
}
static void add_map_timestamp(AMQP_VALUE map, const char* name, int64_t value, bool isApplicationProperty)
{
    AMQP_VALUE amqp_value_value = amqpvalue_create_timestamp(value);
    add_map_item(map, name, amqp_value_value, isApplicationProperty);
    amqpvalue_destroy(amqp_value_value);
}
static void add_map_int(AMQP_VALUE map, const char* name, int32_t value, bool isApplicationProperty)
{
    AMQP_VALUE amqp_value_value = amqpvalue_create_int(value);
    add_map_item(map, name, amqp_value_value, isApplicationProperty);
    amqpvalue_destroy(amqp_value_value);
}
static void add_map_double(AMQP_VALUE map, const char* name, double value, bool isApplicationProperty)
{
    AMQP_VALUE amqp_value_value = amqpvalue_create_double(value);
    add_map_item(map, name, amqp_value_value, isApplicationProperty);
    amqpvalue_destroy(amqp_value_value);
}
static void add_map_value(AMQP_VALUE map, const char* key, const char type, Php::Value value, bool isApplicationProperty)
{
    switch (type) {
        case 'I':
            add_map_int(map, key, static_cast<int32_t>(value), isApplicationProperty);
            break;
        case 'S':
            add_map_string(map, key, value.stringValue().c_str(), isApplicationProperty);
            break;
        case 'T':
            add_map_timestamp(map, key, static_cast<int64_t>(value), isApplicationProperty);
            break;
        case 'D':
            add_map_double(map, key, static_cast<double>(value), isApplicationProperty);
            break;
    }
}

Message::Message()
{
    message = message_create();
    application_properties = amqpvalue_create_map();
    message_set_application_properties(message, application_properties);
    properties_handle = properties_create();
}

Message::Message(MESSAGE_HANDLE msg)
{
    message = msg;
    message_get_application_properties(message, &application_properties);
    application_properties_map = amqpvalue_get_inplace_described_value(application_properties);
    properties_handle = properties_create();
}

void Message::__construct(Php::Parameters &params)
{
    setBody(params[0].stringValue());
}

void Message::__destruct()
{
    properties_destroy(properties_handle);
    amqpvalue_destroy(application_properties);
}

Php::Value Message::getBody()
{
    if (body.empty()) {

        MESSAGE_BODY_TYPE body_type;
        if (message_get_body_type(message, &body_type) != 0 && body_type != MESSAGE_BODY_TYPE_VALUE) {

            BINARY_DATA body_data;
            if (message_get_body_amqp_data_in_place(message, 0, &body_data) != 0 ) {
                throw Php::Exception("Unsupported body type");
            }
            for (size_t i = 0; i < body_data.length; ++i) {
                body += (unsigned char)body_data.bytes[i];
            }
            return body;
        }

        AMQP_VALUE body_value;
        message_get_body_amqp_value_in_place(message, &body_value);

        AMQP_TYPE amqp_type = amqpvalue_get_type(body_value);
        if (amqp_type == AMQP_TYPE_SYMBOL || amqp_type == AMQP_TYPE_STRING) {
            char* result = amqpvalue_to_string(body_value);
            body = (std::string)result;
            free(result);

            return body;
        }

        if (amqp_type == AMQP_TYPE_BINARY) {
            amqp_binary binary_value;
            if (amqpvalue_get_binary(body_value, &binary_value) != 0) {
                throw Php::Exception("Unsupported body type");
            }
            for (uint64_t i = 0; i < binary_value.length; ++i) {
                body += ((char*)binary_value.bytes)[i];
            }

            return body;
        }
    }

}

void Message::setBody(std::string body)
{
    this->body = body;
    AMQP_VALUE amqp_value = amqpvalue_create_string(body.c_str());
    message_set_body_amqp_value(message, amqp_value);
    amqpvalue_destroy(amqp_value);
}

Php::Value Message::getApplicationProperty(Php::Parameters &params)
{
    std::string key = params[0].stringValue();


    auto result = get_value_from_map(application_properties_map, key.c_str(), params[1].stringValue().at(0));


    return result;
}

Php::Value Message::getApplicationProperties()
{
    Php::Value applicationPropertiesMap;

    uint32_t property_count = 0;

    amqpvalue_get_map_pair_count(application_properties_map, &property_count);
    for (uint32_t i = 0; i < property_count; i++) {
        AMQP_VALUE map_key_name = NULL;
        AMQP_VALUE map_key_value = NULL;
        const char* key_name;
        const char* valueString = NULL;
        int64_t valueTimestamp;
        int32_t valueInt;
        double valueDouble;

        amqpvalue_get_map_key_value_pair(application_properties_map, i, &map_key_name, &map_key_value);
        amqpvalue_get_string(map_key_name, &key_name);
        switch (amqpvalue_get_type(map_key_value)) {
            default:
                LogError("Unknown AMQP type");
                break;
            case AMQP_TYPE_INT:
                amqpvalue_get_int(map_key_value, &valueInt);
                applicationPropertiesMap[key_name] = valueInt;
                break;
            case AMQP_TYPE_DOUBLE:
                amqpvalue_get_double(map_key_value, &valueDouble);
                applicationPropertiesMap[key_name] = valueDouble;
                break;
            case AMQP_TYPE_TIMESTAMP:
                amqpvalue_get_timestamp(map_key_value, &valueTimestamp);
                applicationPropertiesMap[key_name] = valueTimestamp;
                break;
            case AMQP_TYPE_STRING:
                amqpvalue_get_string(map_key_value, &valueString);
                applicationPropertiesMap[key_name] = valueString;
                break;
        }
       amqpvalue_destroy(map_key_name);
       amqpvalue_destroy(map_key_value);
    }

    return applicationPropertiesMap;
}

void Message::setApplicationProperty(Php::Parameters &params)
{
    add_map_value(application_properties, params[0].stringValue().c_str(), params[1].stringValue().at(0), params[2], true);
}

MESSAGE_HANDLE Message::getMessageHandler()
{
    return message;
}

PROPERTIES_HANDLE Message::getPropertiesHandle()
{
    return properties_handle;
}

void Message::setMessageHandler(MESSAGE_HANDLE message)
{
    this->message = message;
}

void Message::setProperty(Php::Parameters &params)
{
    int numProperty = -1;
    std::string key = params[0].stringValue();

    std::string* propertyKeys = new std::string[13] {
        "message_id",
        "user_id",
        "to",
        "subject",
        "reply_to",
        "correlation_id",
        "content_type",
        "content_encoding",
        "absolute_expiry_time",
        "creation_time",
        "group_id",
        "group_sequence",
        "reply_to_group_id"
    };

    for (int i = 0; i < 13; i++) {
        if (propertyKeys[i] == key) {
            numProperty = i;
            break;
        }
    }

    AMQP_VALUE amqp_value = NULL;

    switch (numProperty) {
        case 0:
            //properties_set_message_id(properties_handle, amqpvalue_create_int(static_cast<int32_t>(params[1])));
            amqp_value = amqpvalue_create_string(params[1].stringValue().c_str());
            properties_set_message_id(properties_handle, amqp_value);
            break;
        case 1:
            throw Php::Exception("Property key user_id is not supported, because this property need implementation amqp_binary type");
            break;
        case 2:
            amqp_value = amqpvalue_create_string(params[1].stringValue().c_str());
            properties_set_to(properties_handle, amqp_value);
            break;
        case 3:
            properties_set_subject(properties_handle, params[1].stringValue().c_str());
            break;
        case 4:
            amqp_value = amqpvalue_create_string(params[1].stringValue().c_str());
            properties_set_reply_to(properties_handle, amqp_value);
            break;
        case 5:
            //properties_set_correlation_id(properties_handle, amqpvalue_create_int(static_cast<int32_t>(params[1])));
            amqp_value = amqpvalue_create_string(params[1].stringValue().c_str());
            properties_set_correlation_id(properties_handle, amqp_value);
            break;
        case 6:
            properties_set_content_type(properties_handle, params[1].stringValue().c_str());
            break;
        case 7:
            properties_set_content_encoding(properties_handle, params[1].stringValue().c_str());
            break;
        case 8:
            properties_set_absolute_expiry_time(properties_handle, static_cast<int64_t>(params[1]));
            break;
        case 9:
            properties_set_creation_time(properties_handle, static_cast<int64_t>(params[1]));
            break;
        case 10:
            properties_set_group_id(properties_handle, params[1].stringValue().c_str());
            break;
        case 11:
            throw Php::Exception("Property key group_sequence is not supported, because this property need implementation sequence_no type");
            break;
        case 12:
            properties_set_reply_to_group_id(properties_handle, params[1].stringValue().c_str());
            break;
        default:
            properties_destroy(properties_handle);
            throw Php::Exception("Property key is not exist");
    }
    amqpvalue_destroy(amqp_value);
    delete[] propertyKeys;
}

Php::Value Message::getProperty(Php::Parameters &params)
{
    std::string key = params[0].stringValue();

    std::string* propertyKeys = new std::string[13] {
        "message_id",
        "user_id",
        "to",
        "subject",
        "reply_to",
        "correlation_id",
        "content_type",
        "content_encoding",
        "absolute_expiry_time",
        "creation_time",
        "group_id",
        "group_sequence",
        "reply_to_group_id"
    };

    int numProperty = -1;
    for (int i = 0; i < 13; i++) {
        if (propertyKeys[i] == key) {
            numProperty = i;
            break;
        }
    }

    AMQP_VALUE amqp_value;
    const char* string_value;
    int64_t timestamp_value;
    std::string result;

    PROPERTIES_HANDLE properties;

    message_get_properties(message, &properties);

    switch (numProperty) {
        case 0:
            if (properties_get_message_id(properties, &amqp_value) == 0) {
                amqpvalue_get_string(amqp_value, &string_value);
                result = string_value;
            }
            break;
        case 1:
            throw Php::Exception("Property key user_id is not supported, because this property need implementation amqp_binary type");
            break;
        case 2:
            if (properties_get_to(properties, &amqp_value) == 0) {
                amqpvalue_get_string(amqp_value, &string_value);
                result = string_value;
            }
            break;
        case 3:
            if (properties_get_subject(properties, &string_value) == 0) {
                result = string_value;
            }
            break;
        case 4:
            if (properties_get_reply_to(properties, &amqp_value) == 0) {
                amqpvalue_get_string(amqp_value, &string_value);
                result = string_value;
            }
            break;
        case 5:
            if (properties_get_correlation_id(properties, &amqp_value) == 0) {
                amqpvalue_get_string(amqp_value, &string_value);
                result = string_value;
            }
            break;
        case 6:
            if (properties_get_content_type(properties, &string_value) == 0) {
                result = string_value;
            }
            break;
        case 7:
            if (properties_get_content_encoding(properties, &string_value) == 0) {
                result = string_value;
            }
            break;
        case 8:
            if (properties_get_absolute_expiry_time(properties, &timestamp_value) == 0) {
                result = std::to_string(timestamp_value);
            }
            break;
        case 9:
            if (properties_get_creation_time(properties, &timestamp_value) == 0) {
                result = std::to_string(timestamp_value);
            }
            break;
        case 10:
            if (properties_get_group_id(properties, &string_value) == 0) {
                result = string_value;
            }
            break;
        case 11:
            throw Php::Exception("Property key group_sequence is not supported, because this property need implementation sequence_no type");
            break;
        case 12:
            if (properties_get_reply_to_group_id(properties, &string_value) == 0) {
                result = string_value;
            }
            break;
        default:
            properties_destroy(properties);
            throw Php::Exception("Property key is not exist");
    }

    properties_destroy(properties);
    delete[] propertyKeys;
    return result;
}

