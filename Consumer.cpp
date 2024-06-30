#include "Consumer.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/tlsio.h"
#include "azure_c_shared_utility/socketio.h"
#include "azure_uamqp_c/uamqp.h"
#include "Session.h"
#include "Message.h"

static Php::Value *callbackFn;
static bool consumerStopRunning = false;
static std::string consumerExceptionMessage = "";

static void on_link_detach_received_consumer(void* context, ERROR_HANDLE error)
{
    (void)error;
    const char* condition = NULL;
    const char* description = NULL;
    error_get_condition(error, &condition);
    error_get_description(error, &description);
    consumerExceptionMessage += "(" + std::string(condition) + ") " + std::string(description);
    consumerStopRunning = true;
}

static AMQP_VALUE on_message_received(const void* context, MESSAGE_HANDLE message)
{
    (void)context;
    Message *msg = new Message();
    msg->setMessageHandler(message);
    (*callbackFn)(Php::Object("Azure\\uAMQP\\Message", msg));

    return messaging_delivery_accepted();
}

Consumer::Consumer(Session *session, std::string resourceName)
{
    this->session = session;
    this->resourceName = resourceName;

    // source = messaging_create_source(("amqps://" + session->getConnection()->getHost() + "/" + resourceName).c_str());
    // source = messaging_create_source((resourceName).c_str());

    auto filterSet = amqpvalue_create_filter_set(amqpvalue_create_map());

    // std::uint64_t timeNow = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    // std::string filterString = "amqp.annotation.x-opt-enqueuedtimeutc > " + std::to_string(timeNow);
    std::string filterString = "amqp.correlation_id='123'";
    auto selectorFilterKey = amqpvalue_create_symbol("apache.org:selector-filter:string");
    auto selectorKey = amqpvalue_create_symbol("apache.org:selector-filter:string");
    auto filterEntryValue = amqpvalue_create_string(filterString.c_str());
    auto filterEntry =  amqpvalue_create_described(selectorFilterKey, filterEntryValue);
    amqpvalue_set_map_value(filterSet, selectorKey, filterEntry);

    auto newSource = source_create();
    source_set_address(newSource, amqpvalue_create_string((resourceName).c_str()));
    source_set_filter(newSource, filterSet);
    source = amqpvalue_create_source(newSource);

    target = messaging_create_target("ingress-rx");
    link = link_create(session->getSessionHandler(), "receiver-link", role_receiver, source, target);
    link_set_rcv_settle_mode(link, receiver_settle_mode_first);
    link_subscribe_on_link_detach_received(link, on_link_detach_received_consumer, session);

    amqpvalue_destroy(source);
    amqpvalue_destroy(target);

    /* create a message receiver */
    message_receiver = messagereceiver_create(link, NULL, NULL);

    if (message_receiver == NULL) {
        throw Php::Exception("Could not create message receiver");
    }

/*    AMQP_VALUE filter = amqpvalue_create_composite(amqpvalue_create_symbol("apache.org:selector-filter:string"), amqpvalue_create_string("correlation-id = '123'"));
    source_set_filter(message_receiver, filter);*/



    if (session->getConnection()->isDebugOn()) {
        messagereceiver_set_trace(message_receiver, true);
    }
}

void Consumer::setCallback(Php::Value &callback, Php::Value &loopFn)
{
    callbackFn = &callback;

    if (messagereceiver_open(message_receiver, on_message_received, message_receiver) != 0) {
        throw Php::Exception("Could not open the message receiver");
    }

    loopFn();
}

void Consumer::consume()
{
    if (consumerStopRunning) {
        close();
    } else {
        session->getConnection()->doWork();
    }
}

void Consumer::close()
{
    closeRequested = true;
    messagereceiver_destroy(message_receiver);
    link_destroy(link);
    session->close();
    session->getConnection()->close();
    if (!consumerExceptionMessage.empty()) {
        throw Php::Exception(consumerExceptionMessage);
    }
}

bool Consumer::wasCloseRequested()
{
    return closeRequested;
}
