#ifndef _FILERECEIVERUDP_H_
#define _FILERECEIVERUDP_H_

#include <string.h>
#include <fstream>
#include <cmath>
#include <iostream>
#include <vector>

#include <omnetpp.h>
#include <inet/common/INETDefs.h>
#include <inet/transportlayer/contract/udp/UdpSocket.h>
#include <inet/networklayer/common/L3AddressResolver.h>

#include "FilePacket_m.h"

#define WINDOW_SIZE 0.1

// file receiver class for UDP protocol
class FileReceiverUDP : public omnetpp::cSimpleModule
{
    inet::UdpSocket socket;

    unsigned long _rx_count = 0;
    unsigned long _rx_byte_sum_window = 0;

    unsigned long _tx_seq_id = 0;
    unsigned long _rx_seq_id = 0;
    unsigned long _loss_seq_id_cnt = 0;
    bool _first_rx_seen = false;
    int _last_payload_size = 0;  // stores last received payload size for throughput calculation

    omnetpp::SimTime _time_delay_sum = 0;
    omnetpp::SimTime _time_packet_last = 0;
    omnetpp::SimTime _time_last_inst_tp_cal = 0;  // last instant throughput calculation
    omnetpp::SimTime _time_window_size = WINDOW_SIZE;

    // 결과 파일
    unsigned int _temp_str_counter = 0;
    std::ostringstream _temp_str;
    std::ofstream _result_csv;
    
protected:
    virtual int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    void initialize(int stage) override;
    void handleMessage(omnetpp::cMessage *msg) override;
    void finish() override;
};

#endif
