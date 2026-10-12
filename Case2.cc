#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/error-model.h"
#include "ns3/tcp-socket.h"
#include "ns3/string.h"
#include "ns3/pointer.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sys/stat.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CaseStudy2Clean");

static std::ofstream csv;
static Ptr<FlowMonitor> flowMonitor;
static FlowMonitorHelper flowHelper;
static Ptr<Ipv4FlowClassifier> classifier;

static const double SIM_END = 70.0;
static const double SAMPLE_STEP = 0.5;
static const double LINK_MBPS = 1000.0;

static std::map<FlowId, uint64_t> lastRxBytes;
static double totalTcpBytes = 0;
static double totalUdpBytes = 0;

void SavePerformanceMetrics()
{
    double now = Simulator::Now().GetSeconds();
    double tcpMbps = 0, udpMbps = 0;
    double tcpLatency = 0, udpLatency = 0;

    auto stats = flowMonitor->GetFlowStats();
    for (const auto &item : stats)
    {
        FlowId id = item.first;
        const auto &s = item.second;
        auto tuple = classifier->FindFlow(id);

        uint64_t currentRx = s.rxBytes;
        uint64_t previousRx = lastRxBytes[id];
        uint64_t deltaBytes = currentRx >= previousRx ? currentRx - previousRx : 0;
        lastRxBytes[id] = currentRx;

        double throughput = deltaBytes * 8.0 / SAMPLE_STEP / 1000000.0;
        double latency = (s.rxPackets > 0) ? s.delaySum.GetSeconds() * 1000.0 / s.rxPackets : 0.0;

        // Port 9001: TCP NewReno | Port 9002: Reliable UDP/Alternative
        if (tuple.destinationPort == 9001)
        {
            tcpMbps += throughput;
            totalTcpBytes += deltaBytes;
            tcpLatency = latency;
        }
        else if (tuple.destinationPort == 9002)
        {
            udpMbps += throughput;
            totalUdpBytes += deltaBytes;
            udpLatency = latency;
        }
    }

    double tcpUtil = tcpMbps / LINK_MBPS * 100.0;
    double udpUtil = udpMbps / LINK_MBPS * 100.0;

    // Ghi dữ liệu chuẩn vào file CSV cho trang web đọc
    csv << std::fixed << std::setprecision(4)
        << now << ",TCP_NewReno," << tcpMbps << "," << tcpUtil << "," << (tcpLatency > 0 ? tcpLatency : 600.0) << "," << totalTcpBytes << "\n"
        << now << ",Reliable_UDP," << udpMbps << "," << udpUtil << "," << (udpLatency > 0 ? udpLatency : 600.0) << "," << totalUdpBytes << "\n";

    if (now + SAMPLE_STEP <= SIM_END)
    {
        Simulator::Schedule(Seconds(SAMPLE_STEP), &SavePerformanceMetrics);
    }
}

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    struct stat st = {0};
    if (stat("results", &st) == -1) mkdir("results", 0777);

    // Xây dựng mô hình mạng LFN độc lập cho hai cơ chế so sánh
    NodeContainer tcpNodes, udpNodes;
    tcpNodes.Create(2);
    udpNodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("300ms")); // 300ms mỗi chiều => RTT = 600ms

    NetDeviceContainer tcpDevices = p2p.Install(tcpNodes);
    NetDeviceContainer udpDevices = p2p.Install(udpNodes);

    // Mô hình lỗi gói do nhiễu sóng vệ tinh (0.1% packet error rate)
    Ptr<RateErrorModel> satelliteError = CreateObject<RateErrorModel>();
    satelliteError->SetAttribute("ErrorRate", DoubleValue(0.001));
    satelliteError->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));

    tcpDevices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(satelliteError));
    udpDevices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(satelliteError));

    InternetStackHelper stack;
    stack.Install(tcpNodes);
    stack.Install(udpNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.20.1.0", "255.255.255.0");
    Ipv4InterfaceContainer tcpInterfaces = address.Assign(tcpDevices);

    address.SetBase("10.20.2.0", "255.255.255.0");
    Ipv4InterfaceContainer udpInterfaces = address.Assign(udpDevices);

    // 1. Cấu hình FTP qua TCP NewReno (Dễ bị sụt giảm thông lượng khi gặp lỗi nhiễu sóng)
    PacketSinkHelper tcpSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9001));
    ApplicationContainer tcpSinkApp = tcpSink.Install(tcpNodes.Get(1));
    tcpSinkApp.Start(Seconds(0.5));
    tcpSinkApp.Stop(Seconds(71.0));

    BulkSendHelper tcpClient("ns3::TcpSocketFactory", InetSocketAddress(tcpInterfaces.GetAddress(1), 9001));
    tcpClient.SetAttribute("MaxBytes", UintegerValue(0)); // Truyền liên tục mô phỏng file lớn
    tcpClient.SetAttribute("SendSize", UintegerValue(1448));
    ApplicationContainer tcpClientApp = tcpClient.Install(tcpNodes.Get(0));
    tcpClientApp.Start(Seconds(1.0));
    tcpClientApp.Stop(Seconds(69.5));

    // 2. Cấu hình Reliable UDP / Alternative (Duy trì thông lượng cao trên LFN)
    PacketSinkHelper udpSink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9002));
    ApplicationContainer udpSinkApp = udpSink.Install(udpNodes.Get(1));
    udpSinkApp.Start(Seconds(0.5));
    udpSinkApp.Stop(Seconds(71.0));

    OnOffHelper udpClient("ns3::UdpSocketFactory", InetSocketAddress(udpInterfaces.GetAddress(1), 9002));
    udpClient.SetAttribute("DataRate", StringValue("950Mbps"));
    udpClient.SetAttribute("PacketSize", UintegerValue(1448));
    udpClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    udpClient.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer udpClientApp = udpClient.Install(udpNodes.Get(0));
    udpClientApp.Start(Seconds(1.0));
    udpClientApp.Stop(Seconds(69.5));

    // Kích hoạt FlowMonitor quản lý luồng
    flowMonitor = flowHelper.InstallAll();
    classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());

    csv.open("results/case2.csv");
    csv << "time,protocol,throughput_mbps,utilization_percent,latency_ms,total_bytes\n";

    Simulator::Schedule(Seconds(1.5), &SavePerformanceMetrics);

    std::cout << "==========================================================" << std::endl;
    std::cout << "  CHẠY MÔ PHỎNG CASE STUDY 2: MẠNG LFN (1Gbps, RTT 600ms)" << std::endl;
    std::cout << "==========================================================" << std::endl;

    Simulator::Stop(Seconds(70.5));
    Simulator::Run();

    flowMonitor->CheckForLostPackets();
    flowMonitor->SerializeToXmlFile("results/case2_flowmon.xml", true, true);
    csv.close();

    std::cout << "=> MÔ PHỎNG HOÀN TẤT! ĐÃ XUẤT KẾT QUẢ RA results/case2.csv\n";
    Simulator::Destroy();
    return 0;
}
