#ifndef _FILERECEIVERTCP_H_
#define _FILERECEIVERTCP_H_

#define protected public
#include <inet/transportlayer/contract/tcp/TcpSocket.h>
#undef protected

#include <string.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <omnetpp.h>
#include <inet/common/INETDefs.h>
#include <inet/transportlayer/contract/tcp/TcpSocket.h>
#include <inet/networklayer/common/L3AddressResolver.h>

#include "FilePacket_m.h"

#define WINDOW_SIZE 0.1

// file receiver class for TCP protocol
class FileReceiverTCP : public omnetpp::cSimpleModule, public inet::TcpSocket::ICallback
{
  private:
    inet::TcpSocket socket;
    std::map<int, inet::TcpSocket*> socketMap;

    unsigned long _rx_count = 0;
    unsigned long _rx_byte_sum_window = 0;
    unsigned long _tx_seq_id = 0;
    unsigned long _rx_seq_id = 0;
    unsigned long _loss_seq_id_cnt = 0;
    int _last_payload_size = 0;   // tracks most recent payload size for throughput calculation

    bool _first_rx_seen = false;

    omnetpp::SimTime _time_delay_sum = 0;
    omnetpp::SimTime _time_packet_last = 0;
    omnetpp::SimTime _time_last_inst_tp_cal = 0;
    omnetpp::SimTime _time_window_size = WINDOW_SIZE;

    unsigned int _temp_str_counter = 0;
    std::ostringstream _temp_str;
    std::ofstream _result_csv;

  protected:
      virtual int numInitStages() const override { return inet::NUM_INIT_STAGES; }
      virtual void initialize(int stage) override;
      virtual void handleMessage(omnetpp::cMessage *msg) override;
      virtual void finish() override;

      // TCP call backs
      virtual void socketAvailable(inet::TcpSocket *socket, inet::TcpAvailableInfo *availableInfo) override;
      virtual void socketEstablished(inet::TcpSocket *socket) override;
      virtual void socketDataArrived(inet::TcpSocket *socket, inet::Packet *pk, bool urgent) override;
      virtual void socketPeerClosed(inet::TcpSocket *socket) override;
      virtual void socketClosed(inet::TcpSocket *socket) override;
      virtual void socketFailure(inet::TcpSocket *socket, int code) override;
      virtual void socketStatusArrived(inet::TcpSocket *socket, inet::TcpStatusInfo *status) override;
      virtual void socketDeleted(inet::TcpSocket *socket) override;
};

#endif
