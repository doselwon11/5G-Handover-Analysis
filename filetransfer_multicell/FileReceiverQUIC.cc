#include "FileReceiverQUIC.h"
#include <iomanip>

Define_Module(FileReceiverQUIC);
using namespace inet;
using namespace omnetpp;
using namespace std;

void FileReceiverQUIC::initialize(int stage)
{
    cSimpleModule::initialize(stage);

    if (stage == INITSTAGE_LOCAL)
    {
        rxCount = rxByteSumWindow = txSeqId = rxSeqId = lossSeqCnt = 0;
        firstRxSeen = false;
        timeDelaySum = timePacketLast = timeLastInstTpCal = SIMTIME_ZERO;
        timeWindowSize = WINDOW_SIZE;
        tempStr.str("");
        tempStr.clear();
        tempStrCounter = 0;

        // Create CSV file (UTF-8 with BOM for Korean text support)
        stringstream fileName;
        int ueIndex = getParentModule()->getIndex();
        std::ostringstream ueStr;
        ueStr << setw(2) << setfill('0') << ueIndex;
        int runNumber = getEnvir()->getConfigEx()->getActiveRunNumber();
        std::ostringstream runStr;
        runStr << setw(2) << setfill('0') << runNumber;
        fileName << "results/QUIC/quic_run" << runStr.str() << "_ue" << ueStr.str() << "_stats.csv";

        resultCsv.open(fileName.str(), ios::out | ios::binary);
        if (resultCsv.is_open())
        {
            unsigned char bom[] = {0xEF, 0xBB, 0xBF};
            resultCsv.write(reinterpret_cast<char*>(bom), sizeof(bom));
            resultCsv << "Time,Rx_ID,Rx_Delay,Rx_Gap,Inst_Tp,LossRate\n";
        }
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER)
    {
        int port = par("localPort");
        if (port != -1)
        {
            socket.setOutputGate(gate("socketOut"));
            socket.bind(port);
        }
    }
}

void FileReceiverQUIC::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage())
        return;

    Packet *pPacket = check_and_cast<Packet*>(msg);
    auto fileHeader = pPacket->popAtFront<FilePacket>();
    txSeqId = fileHeader->getIDframe();

    // loss detection
    if (!firstRxSeen)
    {
        rxSeqId = txSeqId;
        firstRxSeen = true;
    }
    else if (txSeqId > rxSeqId + 1)
    {
        lossSeqCnt += (txSeqId - rxSeqId - 1);
        rxSeqId = txSeqId;
    }
    else if (txSeqId >= rxSeqId)
    {
        rxSeqId = txSeqId;
    }

    rxCount++;

    if (simTime() > getSimulation()->getWarmupPeriod())
    {
        SimTime rx_time = simTime();
        int rx_data_size = fileHeader->getPayloadSize();
        lastPayloadSize = rx_data_size;   // track last payload size

        SimTime rx_delay = rx_time - fileHeader->getPayloadTimestamp();
        if (rx_delay < SIMTIME_ZERO)
            rx_delay = SIMTIME_ZERO;
        timeDelaySum += rx_delay;

        SimTime gap = SIMTIME_ZERO;
        if (timePacketLast > SIMTIME_ZERO)
            gap = rx_time - timePacketLast;
        timePacketLast = rx_time;

        rxByteSumWindow += rx_data_size;
        SimTime window_elapsed = rx_time - timeLastInstTpCal;
        double inst_tp = 0.0;
        if (window_elapsed.dbl() > 0)
            inst_tp = rxByteSumWindow / window_elapsed.dbl();
        if (window_elapsed >= timeWindowSize)
        {
            rxByteSumWindow = 0;
            timeLastInstTpCal = rx_time;
        }

        double loss_rate = (txSeqId + 1 > 0)
            ? (double)lossSeqCnt / (double)(txSeqId + 1)
            : 0.0;

        tempStr << rx_time.dbl() << ","
                << rxCount << ","
                << rx_delay << ","
                << gap << ","
                << inst_tp << ","
                << loss_rate << "\n";
        tempStrCounter++;

        if (tempStrCounter >= 200)
        {
            if (resultCsv.is_open())
            {
                resultCsv << tempStr.str();
                resultCsv.flush();
            }
            tempStr.str("");
            tempStr.clear();
            tempStrCounter = 0;
        }
    }

    delete msg;
}

void FileReceiverQUIC::finish()
{
    double avg_delay = 0.0;
    double avg_gap = 0.0;
    double throughput = 0.0;
    double loss_ratio = 0.0;

    SimTime total_time = simTime() - getSimulation()->getWarmupPeriod();

    // --- averages ---
    if (rxCount > 0)
    {
        avg_delay = timeDelaySum.dbl() / rxCount;

        // average gap
        if (rxCount > 1)
            avg_gap = timePacketLast.dbl() / rxCount;

        // throughput (bits/sec)
        throughput = (rxCount * lastPayloadSize * 8.0) / total_time.dbl();
    }

    // --- loss ratio ---
    if (txSeqId > 0)
        loss_ratio = (double)lossSeqCnt / (double)(txSeqId + 1);

    // --- write results ---
    if (resultCsv.is_open())
    {
        if (tempStrCounter > 0)
        {
            resultCsv << tempStr.str();
            tempStr.str("");
            tempStr.clear();
            tempStrCounter = 0;
        }

        resultCsv << "\n*** Final Statistics 최종 통계 ***\n"
                  << "Frames Received(Current/Total) 수신된 프레임(현재/전체): "
                  << rxCount << "/" << (txSeqId + 1) << "\n"
                  << "Average Packet Delay 평균 패킷 지연: " << avg_delay << "초 seconds\n"
                  << "Average Inter-Packet Gap 평균 패킷 간격: " << avg_gap << "초 seconds\n"
                  << "Throughput 처리량: " << (throughput / 1e6) << " Mbps\n"
                  << "Packet Loss Ratio 패킷 손실률: " << (loss_ratio * 100.0) << "%\n";

        resultCsv.close();
    }
}

