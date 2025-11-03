#ifndef _FILESENDERQUIC_H_
#define _FILESENDERQUIC_H_

#include <string.h>
#include <omnetpp.h>
#include <inet/common/INETDefs.h>
#include <inet/transportlayer/contract/udp/UdpSocket.h>
#include <inet/networklayer/common/L3AddressResolver.h>
#include "FilePacket_m.h"

using namespace inet;

// file sender class for QUIC protocol
class FileSenderQUIC : public omnetpp::cSimpleModule
{
  private:
    inet::UdpSocket socket;
    cMessage *selfMsg = nullptr;

    int localPort;
    int destPort;
    inet::L3Address destAddress;

    unsigned int totalFrames;
    unsigned int currentFrame = 0;
    int payloadSize;

  protected:
    virtual int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    void sendPacket();
};

#endif
