#include "FileReceiverUDP.h"

#include <limits>
#include <iomanip>

Define_Module(FileReceiverUDP);
using namespace inet;
using namespace omnetpp;
using namespace std;

// initialise file receiver for UDP protocol
void FileReceiverUDP::initialize(int stage)
{
    cSimpleModule::initialize(stage);

    if (stage == INITSTAGE_LOCAL) // initialise
    {
        // variables initialize
        _rx_count = 0;                  // number of successfully received packets
        _rx_byte_sum_window = 0;        // received bytes in the current window

        _tx_seq_id = 0;
        _rx_seq_id = 0;
        _loss_seq_id_cnt = 0;

        _time_delay_sum = SIMTIME_ZERO;             // total delay time (for averaging)
        _time_packet_last = SIMTIME_ZERO;           // time stamp of the last received packet
        _time_last_inst_tp_cal = SIMTIME_ZERO;      // last throughput calculation time
        _time_window_size = WINDOW_SIZE;            // throughput calculation window size (100ms)

        _temp_str.str("");
        _temp_str.clear();
        _temp_str_counter = 0;

        // create CSV file
        stringstream fileName;

        int ueIndex = getParentModule()->getIndex();
        std::ostringstream ueStr;
        ueStr << std::setw(2) << std::setfill('0') << ueIndex;
        int runNumber = getEnvir()->getConfigEx()->getActiveRunNumber();
        std::ostringstream runStr;
        runStr << std::setw(2) << std::setfill('0') << runNumber;

        fileName << "results/UDP/udp_run" << runStr.str() << "_ue" << ueStr.str() << "_stats.csv";
        
        _result_csv.open(fileName.str(), std::ios::out | std::ios::binary);
        if (_result_csv.is_open())
        {
            // write UTF-8 BOM so Excel and editors recognize encoding
            unsigned char bom[] = {0xEF, 0xBB, 0xBF};
            _result_csv.write(reinterpret_cast<char*>(bom), sizeof(bom));
            _result_csv << "Time" << "," << "Rx_ID" << "," << "Rx_Delay" << "," << "Rx_Gap" << "," << "Inst_Tp" << "," << "LossRate" << "\n";
        }
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) // socket initialisation
    {
        int port = par("localPort");

        if (port != -1)
        {
            socket.setOutputGate(gate("socketOut"));
            socket.bind(port);
        }
    }
}

void FileReceiverUDP::handleMessage(cMessage *msg)
{
    // ignore self-messages
    // handle only incoming external messages
    if (msg->isSelfMessage())
    {
        return;
    }

    Packet* pPacket = check_and_cast<Packet*>(msg);
    auto fileHeader = pPacket->popAtFront<FilePacket>();

    // tx sequence id get
    _tx_seq_id = fileHeader->getIDframe();

    // --- loss packet check (based on increasing IDframe; tolerate reordering/duplicates) ---
    if (!_first_rx_seen) {
        _rx_seq_id = _tx_seq_id;
        _first_rx_seen = true;
    }
    else {
        if (_tx_seq_id > _rx_seq_id + 1) {
            _loss_seq_id_cnt += (_tx_seq_id - _rx_seq_id - 1); // gaps imply losses
            _rx_seq_id = _tx_seq_id;
        }
        else if (_tx_seq_id >= _rx_seq_id) {
            _rx_seq_id = _tx_seq_id;
        }
        // if _tx_seq_id < _rx_seq_id, then we treat it as out-of-order/duplicate and don't count it as loss
    }

    // increase rx frame count
    _rx_count++;

    // we do not need to check initialize time data.
    if (simTime() > getSimulation()->getWarmupPeriod())
    {
        // packet receive time set - handleMessage run == packet rx
        SimTime rx_time = simTime(); // packet rx time

        // rx payload size set
        int rx_data_size = fileHeader->getPayloadSize();
        _last_payload_size = rx_data_size;  // ✅ track last payload size for throughput calculation

        // --- calculate packet delay - Rx Delay: rx time - payload timestamp ---
        SimTime rx_delay = rx_time - fileHeader->getPayloadTimestamp();
        if (rx_delay < SIMTIME_ZERO) rx_delay = SIMTIME_ZERO; // guard

        // accumulate for avg in finish()
        _time_delay_sum += rx_delay;

        // --- calculate time since last packet - Rx Gap: before_rx to rx ---
        SimTime time_since_last_packet = SIMTIME_ZERO;
        if (_time_packet_last > SIMTIME_ZERO) {
            time_since_last_packet = rx_time - _time_packet_last;
        }
        _time_packet_last = rx_time;

        // --- instant throughput - bytes/sec over sliding window ---
        _rx_byte_sum_window += rx_data_size;
        SimTime window_elapsed = rx_time - _time_last_inst_tp_cal;
        double inst_tp = 0.0; // bytes per second (consistent with CSV samples)
        if (window_elapsed.dbl() > 0.0) {
            inst_tp = _rx_byte_sum_window / window_elapsed.dbl();
        }
        // roll the window if elapsed >= window size
        if (window_elapsed >= _time_window_size) {
            _rx_byte_sum_window = 0;
            _time_last_inst_tp_cal = rx_time;
        }

        // --- loss rate ---
        double loss_rate = 0.0;
        if (_tx_seq_id + 1 > 0) {
            loss_rate = static_cast<double>(_loss_seq_id_cnt) / static_cast<double>(_tx_seq_id + 1);
        }

        // write results to CSV
        _temp_str << rx_time.dbl() << ","
                  << _rx_count << ","
                  << rx_delay << ","
                  << time_since_last_packet << ","
                  << inst_tp << ","
                  << loss_rate << "\n";
        _temp_str_counter++;


        if (_temp_str_counter >= 200)
        {
            if (_result_csv.is_open())
            {
                _result_csv << _temp_str.str();
                _result_csv.flush();
            }
            _temp_str.str("");
            _temp_str.clear();
            _temp_str_counter = 0;
        }
    }

    delete msg;
}

void FileReceiverUDP::finish()
{
    // important metrics for final statistics
    double avg_delay = 0.0;
    double avg_gap = 0.0;
    double throughput = 0.0;
    double loss_ratio = 0.0;

    SimTime cum_time = simTime() - getSimulation()->getWarmupPeriod();

    // --- Averages ---
    if (_rx_count > 0) {
        avg_delay = _time_delay_sum.dbl() / _rx_count;

        // average inter-packet gap (simple estimate)
        if (_rx_count > 1)
            avg_gap = (_time_packet_last.dbl() / _rx_count);
        else
            avg_gap = 0.0;

        // throughput = (received_bytes * 8 bits) / total_time
        throughput = (_rx_count * _last_payload_size * 8.0) / cum_time.dbl();  // bits/sec
    }

    // --- Loss ratio ---
    if (_tx_seq_id > 0)
        loss_ratio = (double)_loss_seq_id_cnt / (double)(_tx_seq_id + 1);

    // --- Write results ---
    if (_result_csv.is_open()) {

        // flush any remaining buffer
        if (_temp_str_counter > 0) {
            _result_csv << _temp_str.str();
            _temp_str.str("");
            _temp_str.clear();
            _temp_str_counter = 0;
        }

        // write UTF-8 summary (with Korean and English labels)
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
