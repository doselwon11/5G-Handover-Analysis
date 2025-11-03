#ifndef _FILERECEIVERQUIC_H_
#define _FILERECEIVERQUIC_H_

#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <omnetpp.h>
#include <inet/common/INETDefs.h>
#include <inet/transportlayer/contract/udp/UdpSocket.h>
#include <inet/networklayer/common/L3AddressResolver.h>
#include "FilePacket_m.h"

#define WINDOW_SIZE 0.1

using namespace inet;
using namespace omnetpp;

// file receiver class for QUIC protocol
class FileReceiverQUIC : public cSimpleModule
{
  private:
    inet::UdpSocket socket;

    unsigned long rxCount = 0;
    unsigned long rxByteSumWindow = 0;
    unsigned long txSeqId = 0;
    unsigned long rxSeqId = 0;
    unsigned long lossSeqCnt = 0;
    int lastPayloadSize = 0;   // stores last received payload size for throughput calculation

    bool firstRxSeen = false;

    SimTime timeDelaySum = 0;
    SimTime timePacketLast = 0;
    SimTime timeLastInstTpCal = 0;
    SimTime timeWindowSize = WINDOW_SIZE;

    unsigned int tempStrCounter = 0;
    std::ostringstream tempStr;
    std::ofstream resultCsv;

  protected:
    virtual int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
};

#endif
