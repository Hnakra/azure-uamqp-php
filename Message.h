#ifndef UAMQP_PHP_MESSAGE_H
#define UAMQP_PHP_MESSAGE_H
#include <phpcpp.h>
#include "azure_uamqp_c/uamqp.h"

class Message : public Php::Base
{
private:
    std::string body;
    MESSAGE_HANDLE message;
    AMQP_VALUE application_properties;
    AMQP_VALUE annotations_map;
    Php::Value applicationPropertiesMap;
    // std::string* propertyKeys;

public:
    Message();
    virtual ~Message() = default;
    PROPERTIES_HANDLE properties_handle;

    void setMessageHandler(MESSAGE_HANDLE message);
    MESSAGE_HANDLE getMessageHandler();
    void setBody(std::string body);

    void __construct(Php::Parameters &params);
    Php::Value getBody();
    Php::Value getApplicationProperty(Php::Parameters &params);
    Php::Value getApplicationProperties();
    void setApplicationProperty(Php::Parameters &params);
    Php::Value getMessageAnnotation(Php::Parameters &params);
    void setMessageAnnotation(Php::Parameters &params);
    void setProperty(Php::Parameters &params);
    Php::Value getProperty(Php::Parameters &params);
    //Php::Value getPropertyKeys();
};

#endif
