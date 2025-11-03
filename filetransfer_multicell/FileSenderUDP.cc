#include "FileSenderUDP.h"

#include <cmath>
#include <inet/common/TimeTag_m.h>

Define_Module(FileSenderUDP);
using namespace inet;
using namespace omnetpp;

FileSenderUDP::FileSenderUDP()
{
    _self_msg = nullptr;
}

FileSenderUDP::~FileSenderUDP()
{
    cancelAndDelete(_self_msg);
}

// initialise file sender for UDP protocol
void FileSenderUDP::initialize(int stage)
{
    cSimpleModule::initialize(stage);

    if (stage == INITSTAGE_LOCAL) // initialisation
    {
        _self_msg = new cMessage("sendTimer");
        _local_port = par("localPort");
        _dest_port = par("destPort");

        _payload_size = par("payloadSize");
        _total_frames = par("nFrames");
        _current_frame = 0;

        EV_INFO << "FileSender initialized. Total packets: " << _total_frames << ", Payload: " << _payload_size << "B" << endl;
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) // when socket connect
    {
        const char *destAddrs = par("destAddress");
        _dest_address = L3AddressResolver().resolve(destAddrs);
        _socket.setOutputGate(gate("socketOut"));
        _socket.bind(_local_port);

        // transmission start
        simtime_t start_time = par("startTime");
        scheduleAt(simTime() + start_time, _self_msg);
    }
}

void FileSenderUDP::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage())
    {
        if (_current_frame < _total_frames)
        {
            sendPacket();
            simtime_t interval = par("samplingTime");
            scheduleAt(simTime() + interval, _self_msg);
        }
        else
        {
            cancelAndDelete(_self_msg);
        }
    }
}

void FileSenderUDP::sendPacket()
{
    Packet* packet = new Packet("File");
    auto fileData = makeShared<FilePacket>();
    
    fileData->setNframes(_total_frames);
    fileData->setIDframe(_current_frame++);
    fileData->setPayloadTimestamp(simTime());
    fileData->setPayloadSize(_payload_size);
    fileData->setChunkLength(B(_payload_size));
    
    packet->insertAtBack(fileData);

    _socket.sendTo(packet, _dest_address, _dest_port);
}

void FileSenderUDP::finish()
{
    recordScalar("Total Frames Sent", _current_frame);
}
