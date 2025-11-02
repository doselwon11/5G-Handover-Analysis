#include "FileSenderQUIC.h"
#include <inet/common/TimeTag_m.h>

Define_Module(FileSenderQUIC);
using namespace inet;
using namespace omnetpp;

void FileSenderQUIC::initialize(int stage)
{
    cSimpleModule::initialize(stage);

    if (stage == INITSTAGE_LOCAL)
    {
        selfMsg = new cMessage("sendTimer");
        localPort = par("localPort");
        destPort = par("destPort");
        payloadSize = par("payloadSize");
        totalFrames = par("nFrames");
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER)
    {
        const char *destAddrStr = par("destAddress");
        destAddress = L3AddressResolver().resolve(destAddrStr);

        socket.setOutputGate(gate("socketOut"));
        socket.bind(localPort);

        simtime_t startTime = par("startTime");
        scheduleAt(simTime() + startTime, selfMsg);
    }
}

void FileSenderQUIC::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage())
    {
        if (currentFrame < totalFrames)
        {
            sendPacket();
            simtime_t interval = par("samplingTime");
            scheduleAt(simTime() + interval, selfMsg);
        }
        else
        {
            cancelAndDelete(selfMsg);
            selfMsg = nullptr;
        }
    }
}

void FileSenderQUIC::sendPacket()
{
    auto packet = new Packet("QUIC_File");
    auto fileData = makeShared<FilePacket>();
    fileData->setNframes(totalFrames);
    fileData->setIDframe(currentFrame++);
    fileData->setPayloadTimestamp(simTime());
    fileData->setPayloadSize(payloadSize);
    fileData->setChunkLength(B(payloadSize));
    packet->insertAtBack(fileData);

    socket.sendTo(packet, destAddress, destPort);
}

void FileSenderQUIC::finish()
{
    recordScalar("Total Frames Sent", currentFrame);
}
