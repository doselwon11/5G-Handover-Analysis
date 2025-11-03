#ifndef _FILESENDERTCP_H_
#define _FILESENDERTCP_H_

#include <string.h>
#include <omnetpp.h>
#include <inet/common/INETDefs.h>
#include <inet/transportlayer/contract/tcp/TcpSocket.h>
#include <inet/networklayer/common/L3AddressResolver.h>

#include "FilePacket_m.h"

using namespace inet;

// file sender class for TCP protocol
class FileSenderTCP : public omnetpp::cSimpleModule, public TcpSocket::ICallback
{
  private:
    TcpSocket socket;
    cMessage* selfMsg = nullptr;

    // parameters
    int localPort;
    int destPort;
    L3Address destAddress;
    int payloadSize;
    int totalFrames;
    int currentFrame = 0;

  protected:
    virtual int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    void initialize(int stage) override;
    void handleMessage(omnetpp::cMessage *msg) override;
    void finish() override;

    // TCP call backs
    virtual void socketAvailable(inet::TcpSocket *socket, inet::TcpAvailableInfo *availableInfo) override;
    virtual void socketEstablished(inet::TcpSocket *socket) override;
    virtual void socketDataArrived(inet::TcpSocket *socket, inet::Packet *msg, bool urgent) override;
    virtual void socketPeerClosed(inet::TcpSocket *socket) override;
    virtual void socketClosed(inet::TcpSocket *socket) override;
    virtual void socketFailure(inet::TcpSocket *socket, int code) override;
    virtual void socketStatusArrived(inet::TcpSocket *socket, inet::TcpStatusInfo *status) override;
    virtual void socketDeleted(inet::TcpSocket *socket) override;

    void sendPacket();

  public:
    FileSenderTCP() {}
    virtual ~FileSenderTCP();
};

#endif
