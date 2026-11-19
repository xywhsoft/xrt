#ifndef XMQTT_EXAMPLE_RECEIVE_SLOT_H
#define XMQTT_EXAMPLE_RECEIVE_SLOT_H

#include <xmqtt/client.h>
#include <xrt/atomic.h>

/* One APP slot for the single echo in this example. The controller's
 * client worker is its sole producer. The APP releases the reference
 * after it has joined the controller and all of its connections. */
static inline void mqttExampleReceiveOne(xmqttclient* Client,ptr User)
{
    xatomicptr* Slot=User;
    if(xrtAtomicPtrLoad(Slot,XMEMORY_ACQUIRE))return;
    xmqttmessage* Message=NULL;
    if(xmqttClientTakeMessage(Client,&Message))
        xrtAtomicPtrStore(Slot,Message,XMEMORY_RELEASE);
}

#endif
