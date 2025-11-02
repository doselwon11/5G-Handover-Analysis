#ifndef _FILESENDERUDP_H_
#define _FILESENDERUDP_H_

#include <string.h>
#include <omnetpp.h>

#include <inet/common/INETDefs.h>
#include <inet/transportlayer/contract/udp/UdpSocket.h>
#include <inet/networklayer/common/L3AddressResolver.h>

#include "FilePacket_m.h"

using namespace inet;

class FileSenderUDP : public omnetpp::cSimpleModule
{
    inet::UdpSocket _socket;
    
    // Self messages
    cMessage* _self_msg;
    
    // Configuration parameters
    int _local_port;
    int _dest_port;
    inet::L3Address _dest_address;

    unsigned int _total_frames;
    unsigned int _current_frame;
    int _payload_size; // one frame byte size
    
  public:
    FileSenderUDP();
    virtual ~FileSenderUDP();

  protected:
    virtual int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    void initialize(int stage) override;
    void handleMessage(omnetpp::cMessage *msg) override;
    void finish() override;
    
    void sendPacket();
};

#endif
