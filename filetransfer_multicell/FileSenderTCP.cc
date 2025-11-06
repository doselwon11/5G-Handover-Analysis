#include "FileSenderTCP.h"
#include <inet/common/TimeTag_m.h>
#include <inet/common/packet/chunk/ByteCountChunk.h>

Define_Module(FileSenderTCP);
using namespace inet;
using namespace omnetpp;

FileSenderTCP::~FileSenderTCP()
{
    cancelAndDelete(selfMsg);
}

// initialise file sender for TCP protocol
void FileSenderTCP::initialize(int stage)
{
    cSimpleModule::initialize(stage);

    if (stage == INITSTAGE_LOCAL)
    {
        selfMsg = new cMessage("sendTimer");
        localPort   = par("localPort");
        destPort    = par("destPort");
        payloadSize = par("payloadSize");
        totalFrames = par("nFrames");
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER)
    {
        const char *destAddrStr = par("destAddress");
        destAddress = L3AddressResolver().resolve(destAddrStr);

        socket.setOutputGate(gate("socketOut"));
        socket.bind(localPort);
        socket.setCallback(this);
        socket.connect(destAddress, destPort);

        // NOTE: do NOT schedule the timer here.
        // We start sending after TCP handshake in socketEstablished().
    }
}

void FileSenderTCP::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage())
    {
        if (socket.isOpen())
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
                socket.close();
            }
        }
        else
        {
            // not connected yet; retry shortly
            scheduleAt(simTime() + 0.05, selfMsg);
        }
    }
    else
    {
        socket.processMessage(msg);
    }
}

void FileSenderTCP::sendPacket()
{
    // build a payload as raw bytes for TCP stream semantics
    auto packet = new Packet("FileTCP");
    auto chunk  = makeShared<ByteCountChunk>(B(payloadSize));
    packet->insertAtBack(chunk);

    EV_INFO << "TCP/IP send frame " << currentFrame
            << " (" << payloadSize << "B) at " << simTime() << endl;

    ++currentFrame;
    socket.send(packet);
}

/*** TCP call backs ***/

void FileSenderTCP::socketAvailable(TcpSocket * /*socket*/, TcpAvailableInfo *availableInfo)
{
    // not used on the active side (client); safe to ignore.
    delete availableInfo;
}

void FileSenderTCP::socketEstablished(TcpSocket * /*socket*/)
{
    EV_INFO << "TCP/IP connection established (sender) at " << simTime() << "\n";

    // respect configured startTime; schedule first send now that TCP is up
    simtime_t startDelay = par("startTime");
    if (selfMsg && !selfMsg->isScheduled())
        scheduleAt(simTime() + startDelay, selfMsg);
}

void FileSenderTCP::socketDataArrived(TcpSocket * /*socket*/, Packet *msg, bool /*urgent*/)
{
    // sender does not expect data
    delete msg;
}

void FileSenderTCP::socketPeerClosed(TcpSocket *socket)
{
    EV_INFO << "Peer closed TCP/IP connection (sender)\n";
    socket->close();
}

void FileSenderTCP::socketClosed(TcpSocket * /*socket*/)
{
    EV_INFO << "Socket closed (sender)\n";
}

void FileSenderTCP::socketFailure(TcpSocket * /*socket*/, int code)
{
    EV_WARN << "TCP/IP connection FAILED at " << simTime() << ", code=" << code << endl;
}

void FileSenderTCP::socketStatusArrived(TcpSocket * /*socket*/, TcpStatusInfo *status)
{
    // not needed for this app
    delete status;
}

void FileSenderTCP::socketDeleted(TcpSocket * /*socket*/)
{
    EV_INFO << "TCP/IP socket deleted (sender)\n";
}

/*** finish ***/
void FileSenderTCP::finish()
{
    recordScalar("Total Frames Sent", currentFrame);
}
