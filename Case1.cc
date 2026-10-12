#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/error-model.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-header.h"

#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <iostream>
#include <sys/stat.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CaseStudy1VideoStreaming");

static std::ofstream csv;
static Ptr<FlowMonitor> monitor;
static FlowMonitorHelper flowHelper;
static Ptr<Ipv4FlowClassifier> classifier;

static std::map<FlowId, uint64_t> lastRxBytes;
static std::set<uint32_t> tcpSequences;
static uint32_t tcpRetransmissions = 0;

void TrackTcpTx(Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t interface)
{
    Ptr<Packet> copy = packet->Copy();
    Ipv4Header ipHeader;
    if (copy->PeekHeader(ipHeader) == 0) return;
    if (ipHeader.GetProtocol() != 6) return;
    copy->RemoveHeader(ipHeader);
    TcpHeader tcpHeader;
    if (copy->PeekHeader(tcpHeader) == 0) return;
    uint32_t headerSize = tcpHeader.GetSerializedSize();
    if (copy->GetSize() <= headerSize) return;
    uint32_t seq = tcpHeader.GetSequenceNumber().GetValue();
    if (!tcpSequences.insert(seq).second)
    {
        tcpRetransmissions++;
    }
}

void GetUdpStats(uint64_t &udpTx, uint64_t &udpRx)
{
    udpTx = 0;
    udpRx = 0;
    auto stats = monitor->GetFlowStats();
    for (const auto &item : stats)
    {
        auto tuple = classifier->FindFlow(item.first);
        if (tuple.protocol == 17 && tuple.destinationPort == 9000)
        {
            udpTx += item.second.txPackets;
            udpRx += item.second.rxPackets;
        }
    }
}

void SaveResults()
{
    monitor->CheckForLostPackets();
    auto stats = monitor->GetFlowStats();

    uint64_t udpTransmitted = 0, udpReceived = 0;
    GetUdpStats(udpTransmitted, udpReceived);
    uint64_t udpNotReceived = udpTransmitted > udpReceived ? udpTransmitted - udpReceived : 0;
    double udpLossPercent = udpTransmitted > 0 ? 100.0 * udpNotReceived / static_cast<double>(udpTransmitted) : 0.0;

    for (const auto &item : stats)
    {
        FlowId id = item.first;
        const auto &s = item.second;
        auto tuple = classifier->FindFlow(id);

        if (tuple.destinationPort != 9000 && tuple.destinationPort != 9001) continue;

        std::string protocol = (tuple.destinationPort == 9000) ? "UDP (Video Conference)" : "TCP (Netflix VoD)";

        uint64_t previous = lastRxBytes[id];
        uint64_t current = s.rxBytes;
        double throughput = (current >= previous) ? (current - previous) * 8.0 / 0.5 / 1000000.0 : 0.0;
        lastRxBytes[id] = current;

        double latency = (s.rxPackets > 0) ? s.delaySum.GetSeconds() * 1000.0 / s.rxPackets : 0.0;
        double jitter = (s.rxPackets > 1) ? s.jitterSum.GetSeconds() * 1000.0 / (s.rxPackets - 1) : 0.0;

        csv << std::fixed << std::setprecision(3)
            << Simulator::Now().GetSeconds() << ","
            << protocol << ","
            << s.txPackets << ","
            << s.rxPackets << ","
            << s.lostPackets << ","
            << latency << ","
            << jitter << ","
            << throughput << ","
            << udpLossPercent << ","
            << tcpRetransmissions << "\n";
    }

    if (Simulator::Now().GetSeconds() < 20.0)
    {
        Simulator::Schedule(Seconds(0.5), &SaveResults);
    }
}

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    struct stat st = {0};
    if (stat("results", &st) == -1) {
        mkdir("results", 0777);
    }

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("25ms"));
    NetDeviceContainer devices = p2p.Install(nodes);

    // Cấu hình Packet Loss 2%
    Ptr<RateErrorModel> errorModel = CreateObject<RateErrorModel>();
    errorModel->SetAttribute("ErrorRate", DoubleValue(0.02));
    errorModel->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));
    devices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(errorModel));

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 1. UDP Server & Client (Mô phỏng Video Conference - Zoom)
    uint16_t udpPort = 9000;
    UdpServerHelper udpServer(udpPort);
    ApplicationContainer udpServerApps = udpServer.Install(nodes.Get(1));
    udpServerApps.Start(Seconds(0.5));
    udpServerApps.Stop(Seconds(20.0));

    UdpClientHelper udpClient(interfaces.GetAddress(1), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(100000));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1400));
    ApplicationContainer udpApps = udpClient.Install(nodes.Get(0));
    udpApps.Start(Seconds(1.0));
    udpApps.Stop(Seconds(19.5));

    // 2. TCP Server & Client (Mô phỏng Netflix VoD)
    uint16_t tcpPort = 9001;
    PacketSinkHelper tcpSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    ApplicationContainer sinkApps = tcpSink.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.5));
    sinkApps.Stop(Seconds(20.0));

    OnOffHelper tcpClient("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), tcpPort));
    tcpClient.SetAttribute("DataRate", StringValue("2Mbps"));
    tcpClient.SetAttribute("PacketSize", UintegerValue(1400));
    tcpClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    tcpClient.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer tcpApps = tcpClient.Install(nodes.Get(0));
    tcpApps.Start(Seconds(1.0));
    tcpApps.Stop(Seconds(19.5));

    Config::ConnectWithoutContext("/NodeList/0/$ns3::Ipv4L3Protocol/Tx", MakeCallback(&TrackTcpTx));

    monitor = flowHelper.InstallAll();
    classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());

    csv.open("results/case1.csv");
    if (!csv.is_open())
    {
        std::cerr << "Khong mo duoc results/case1.csv" << std::endl;
        return 1;
    }

    csv << "time,protocol,tx_packets,rx_packets,lost_packets,latency_ms,jitter_ms,throughput_mbps,udp_loss_percent,tcp_retransmissions\n";

    Simulator::Schedule(Seconds(1.5), &SaveResults);

    Simulator::Stop(Seconds(20.5));
    Simulator::Run();

    monitor->CheckForLostPackets();

    uint64_t totalUdpTx = 0, totalUdpRx = 0;
    GetUdpStats(totalUdpTx, totalUdpRx);
    uint64_t totalUdpNotReceived = totalUdpTx > totalUdpRx ? totalUdpTx - totalUdpRx : 0;
    double finalUdpLoss = totalUdpTx > 0 ? 100.0 * totalUdpNotReceived / static_cast<double>(totalUdpTx) : 0.0;

    std::cout << "\n===== THONG KE CASE 1 (VIDEO STREAMING) =====\n";
    std::cout << "UDP Sent (Video Conference)  : " << totalUdpTx << "\n";
    std::cout << "UDP Received                 : " << totalUdpRx << "\n";
    std::cout << "UDP Loss Estimate            : " << std::fixed << std::setprecision(2) << finalUdpLoss << "%\n";
    std::cout << "TCP Retransmissions (Netflix): " << tcpRetransmissions << "\n";
    std::cout << "=============================================\n";

    monitor->SerializeToXmlFile("results/case1_flowmon.xml", true, true);
    csv.close();
    Simulator::Destroy();

    return 0;
}
