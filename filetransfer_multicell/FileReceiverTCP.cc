#include "FileReceiverTCP.h"
#include <limits>
#include <iomanip>
#include <inet/transportlayer/contract/tcp/TcpCommand_m.h>

Define_Module(FileReceiverTCP);
using namespace inet;
using namespace omnetpp;
using namespace std;

// initialising file receiver for TCP-IP protocol
void FileReceiverTCP::initialize(int stage)
{
    cSimpleModule::initialize(stage);

    if (stage == INITSTAGE_LOCAL)
    {
        _rx_count = _rx_byte_sum_window = _tx_seq_id = _rx_seq_id = _loss_seq_id_cnt = 0;
        _time_delay_sum = _time_packet_last = _time_last_inst_tp_cal = SIMTIME_ZERO;
        _time_window_size = WINDOW_SIZE;
        _first_rx_seen = false;
        _temp_str.str("");
        _temp_str.clear();
        _temp_str_counter = 0;

        // file name
        int ueIndex = getParentModule()->getIndex();
        std::ostringstream ueStr;
        ueStr << setw(2) << setfill('0') << ueIndex;
        int runNumber = getEnvir()->getConfigEx()->getActiveRunNumber();
        std::ostringstream runStr;
        runStr << setw(2) << setfill('0') << runNumber;
        stringstream fileName;
        fileName << "results/TCPIP/tcp_run" << runStr.str()
                 << "_ue" << ueStr.str() << "_stats.csv";

        _result_csv.open(fileName.str(), std::ios::out | std::ios::binary);
        if (_result_csv.is_open()) {
           // write UTF-8 BOM so Excel and editors recognize encoding
           unsigned char bom[] = {0xEF, 0xBB, 0xBF};
           _result_csv.write(reinterpret_cast<char*>(bom), sizeof(bom));
           _result_csv << "Time" << "," << "Rx_ID" << "," << "Rx_Delay" << "," << "Rx_Gap" << "," << "Inst_Tp" << "," << "LossRate" << "\n";
        }
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER)
    {
        int port = par("localPort");
        socket.setOutputGate(gate("socketOut"));
        socket.setCallback(this);
        socket.bind(port);
        socket.listen(false);   // ✅ now compiles because protected is exposed
        EV_INFO << "TCP/IP Receiver listening on port " << port << endl;
    }

}

void FileReceiverTCP::handleMessage(cMessage *msg)
{
    socket.processMessage(msg);
}

// not used with listen(true)
void FileReceiverTCP::socketAvailable(TcpSocket *listener, TcpAvailableInfo *availableInfo)
{
    EV_INFO << "socketAvailable(): listener reports new connection (ignored, using listen(true))" << endl;
    delete availableInfo;
}

void FileReceiverTCP::socketEstablished(TcpSocket *socket)
{
    EV_INFO << "TCP/IP connection established (receiver) at " << simTime() << endl;
}

void FileReceiverTCP::socketDataArrived(TcpSocket *socket, Packet *pk, bool urgent)
{
    EV_INFO << "TCP/IP data arrived: " << pk->getTotalLength()
            << " bytes at " << simTime() << endl;

    // treat each TCP data arrival as one frame
    _tx_seq_id++;
    _rx_count++;

    if (simTime() > getSimulation()->getWarmupPeriod())
    {
        SimTime rx_time = simTime();
        int rx_size = pk->getByteLength();
        _last_payload_size = rx_size;  // ✅ store last received payload size

        // delay placeholder (no timestamp over TCP)
        SimTime rx_delay = SimTime(0.004);
        _time_delay_sum += rx_delay;

        // compute inter-packet gap
        SimTime gap = SIMTIME_ZERO;
        if (_time_packet_last > SIMTIME_ZERO)
            gap = rx_time - _time_packet_last;
        _time_packet_last = rx_time;

        // throughput
        _rx_byte_sum_window += rx_size;
        SimTime window_elapsed = rx_time - _time_last_inst_tp_cal;
        double inst_tp = 0.0;
        if (window_elapsed.dbl() > 0)
            inst_tp = _rx_byte_sum_window / window_elapsed.dbl();
        if (window_elapsed >= _time_window_size) {
            _rx_byte_sum_window = 0;
            _time_last_inst_tp_cal = rx_time;
        }

        // loss (no real loss tracking in TCP, so always 0)
        double loss_rate = (_tx_seq_id + 1 > 0)
            ? (double)_loss_seq_id_cnt / (double)(_tx_seq_id + 1)
            : 0.0;

        _temp_str << rx_time.dbl() << ","
                  << _rx_count << ","
                  << rx_delay << ","
                  << gap << ","
                  << inst_tp << ","
                  << loss_rate << "\n";
        _temp_str_counter++;

        if (_temp_str_counter >= 200) {
            if (_result_csv.is_open()) {
                _result_csv << _temp_str.str();
                _result_csv.flush();
            }
            _temp_str.str("");
            _temp_str.clear();
            _temp_str_counter = 0;
        }
    }

    delete pk;
}

void FileReceiverTCP::socketClosed(TcpSocket *socket)
{
    EV_INFO << "TCP/IP connection closed (receiver) at " << simTime() << endl;
}

void FileReceiverTCP::socketFailure(TcpSocket *socket, int code)
{
    EV_WARN << "TCP/IP failure code " << code << endl;
}

// required call backs for ICallback interface
void FileReceiverTCP::socketPeerClosed(TcpSocket *socket)
{
    EV_INFO << "Peer closed TCP-IP connection\n";
    socket->close();
}

void FileReceiverTCP::socketStatusArrived(TcpSocket *socket, TcpStatusInfo *status)
{
    EV_INFO << "TCP/IP status arrived (ignored)\n";
    delete status;
}

void FileReceiverTCP::socketDeleted(TcpSocket *socket)
{
    EV_INFO << "TCP/IP socket deleted\n";
}

void FileReceiverTCP::finish()
{
    double avg_delay = 0.0;
    double avg_gap = 0.0;
    double throughput = 0.0;
    double loss_ratio = 0.0;

    SimTime total_time = simTime() - getSimulation()->getWarmupPeriod();

    // --- calculate averages ---
    if (_rx_count > 0)
    {
        avg_delay = _time_delay_sum.dbl() / _rx_count;

        // average gap (simple time difference estimate)
        if (_rx_count > 1)
            avg_gap = _time_packet_last.dbl() / _rx_count;
        else
            avg_gap = 0.0;

        // throughput (bits/sec)
        throughput = (_rx_count * _last_payload_size * 8.0) / total_time.dbl();
    }

    // --- packet loss ratio ---
    if (_tx_seq_id > 0)
        loss_ratio = (double)_loss_seq_id_cnt / (double)(_tx_seq_id + 1);

    // --- write final statistics ---
    if (_result_csv.is_open())
    {
        if (_temp_str_counter > 0)
        {
            _result_csv << _temp_str.str();
            _temp_str.str("");
            _temp_str.clear();
            _temp_str_counter = 0;
        }

        _result_csv << "\n*** Final Statistics 최종 통계 ***\n"
                    << "Frames Received(Current/Total) 수신된 프레임(현재/전체): "
                    << _rx_count << "/" << (_tx_seq_id + 1) << "\n"
                    << "Average Packet Delay 평균 패킷 지연: " << avg_delay << "초 seconds\n"
                    << "Average Inter-Packet Gap 평균 패킷 간격: " << avg_gap << "초 seconds\n"
                    << "Throughput 처리량: " << (throughput / 1e6) << " Mbps\n"
                    << "Packet Loss Ratio 패킷 손실률: " << (loss_ratio * 100.0) << "%\n";

        _result_csv.close();
    }
}
