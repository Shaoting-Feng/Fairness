#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>
#include <fstream>
#include <cstdlib>  // 包含 system 函数的头文件
#include "dumbbell_long/my-source.h"
#include <iomanip>
#include <cstdio> // 包含这个头文件以使用 remove 函数
#include "ns3/traffic-control-helper.h"
#include "ns3/tcp-l4-protocol.h"

//8 servers: n1~8; 4 ToR switches: t1~4;
//2 aggregation switches: a1~2; 1 core switch: c1
//The network is partitioned/ into two clusters
//
//                                          c1
//                                       -     -  
//                                     -          -
//                 1.5Mbps           -              -       1.5Mbps
//              192.168.1.0/24     -                  -  192.168.2.0/24
//                               -                      -
//                             -                           -
//                           -                                -
//     Cluster1            -                       Clusters2     -
//                      a1                                         a2
//      1.0Mbps         |                          1.0Mbps          |
//   10.1.1.0/24  ==============                 10.2.1.0/24  ============== 
//                  |         |                                 |         |
//                  t1        t2                                t3        t4
//     1.0Mbps      |         |       1.0Mbps      1.0Mbps      |         |       1.0Mbps    
//   10.0.1.0/24 ========  ======== 10.0.2.0/24  10.0.3.0/24 ========  ======== 10.0.4.0/24  
//                |    |    |    |                            |    |    |    |
//                n1   n2   n3   n4                           n5   n6   n7   n8


using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DataCenter_Network");

std::string result_dir = "tmp_index/dcn1/";

static void
RxWithAddressesPacketSink (Ptr<const Packet> p, const Address& from, const Address& local) {
  MySourceIDTag tag;
  if (p->FindFirstMatchingByteTag(tag)) {
    // NS_LOG_DEBUG ("[" << Simulator::Now ().GetNanoSeconds() << "] SourceIDTag: " << tag.Get() << ", size: " << p->GetSize()
    //              << ", from: " << InetSocketAddress::ConvertFrom(from).GetIpv4()
    //              << ", local: " << InetSocketAddress::ConvertFrom(local).GetIpv4());
  }

  // Create the directory if it doesn't exist
  if (system(("mkdir -p " + result_dir).c_str()) != 0) {
  } else {
  }
  std::ofstream throput_ofs2 (result_dir + "received_ms.dat", std::ios::out | std::ios::app);
  throput_ofs2 << "[" << Simulator::Now ().GetMilliSeconds() << "] SourceIDTag: " << tag.Get() << ", size: " << p->GetSize()
              << std::endl;
}

void
ParseAppBw (std::string input, std::vector<std::string>* values)
{
  size_t startPos = 0;
  size_t commaPos;

  // Search for commas and split the string within the loop
  while ((commaPos = input.find(',', startPos)) != std::string::npos) {
    std::string item = input.substr(startPos, commaPos - startPos);
    (*values).push_back(item);

    startPos = commaPos + 1;
  }

  // Process the last substring
  std::string lastItem = input.substr(startPos);
  (*values).push_back(lastItem);
}

double app_seconds_start0 = 0.1;
double app_seconds_start1 = 0.1;
double app_seconds_start2 = 0.1;
double app_seconds_start3 = 0.1;
double app_seconds_start4 = 0.1;
double app_seconds_start5 = 0.1;
double app_seconds_start6 = 0.1;
double app_seconds_start7 = 0.1;
double app_seconds_start8 = 0.1;

void
ParseAppSecondsChange (std::string input, std::vector<double>* values, int cca_num)
{
  size_t startPos = 0;
  size_t commaPos;

  // added on 09.01 to specify different start time 
  int i = 0;

  // added on 09.02 to calculate delay in latter applications
  double first_time = 0;

  // Search for commas and split the string within the loop
  while ((commaPos = input.find(',', startPos)) != std::string::npos) {

    std::string item = input.substr(startPos, commaPos - startPos);
    double value = std::stod(item);
    if (i != 0) {
      (*values).push_back(value - first_time);
    }
    else {
      first_time = value;
      switch(cca_num) {
        case 8:
          app_seconds_start8 = value;
          break;   
        case 7:
          app_seconds_start7 = value;
          break;   
        case 6:
          app_seconds_start6 = value;
          break;   
        case 5:
          app_seconds_start5 = value;
          break;   
        case 4:
          app_seconds_start4 = value;
          break;      
        case 3:
          app_seconds_start3 = value;
          break;        
        case 2:
          app_seconds_start2 = value;
          break;         
        case 1:
          app_seconds_start1 = value;
          break;            
        case 0:
          app_seconds_start0 = value;
          break;      
      }
    }
    startPos = commaPos + 1;

    i++;
  }

  // Process the last substring
  std::string lastItem = input.substr(startPos);
  double lastValue = std::stod(lastItem);
  (*values).push_back(lastValue - first_time);
}

void
PrintProgress (Time interval)
{
  std::cout << "[PID:" << getpid() << "] Progress: " << std::fixed << std::setprecision (1) << Simulator::Now ().GetSeconds () << "[s]" << std::endl;
  Simulator::Schedule (interval, &PrintProgress, interval);
}

int main(int argc, char *argv[])
{
    // added some parameters
    std::string switch_total_bufsize = "100p";
    std::string max_switch_total_bufsize = "2000p"; // for "FqCoDelQueueDisc"
    std::string switch_netdev_size = "100p";
    uint32_t app_packet_size = 1446; // in bytes 
    bool sack = true; 
    std::string recovery = "ns3::TcpClassicRecovery";
    uint32_t delackcount = 1;
    std::string transport_prot0 = "TcpCubic";
    transport_prot0 = std::string("ns3::") + transport_prot0;

    bool verbose = true;
    std::string re;

    CommandLine cmd;
    //添加自己的变量 （属性名称，属性说明，变量）
    cmd.AddValue ("re", "Part of result directory", re);
    cmd.Parse (argc,argv);
    result_dir = result_dir + re + "/";
    std::cout << result_dir << std::endl;    

    // 使用 remove 函数删除文件
    if (std::remove((result_dir + "received_ms.dat").c_str()) != 0) {
    } else {
    }
    if (std::remove((result_dir + "sent_ms.dat").c_str()) != 0) {
    } else {
    }
    
    if (verbose){
        LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    /************网络拓扑部分*************/
    //创建2个aggregation:a1,a2   core-->a1  core-->a2   ptpNodes
    //a1 = aggregation1.Get(1)   a2 = aggregation2.Get(1)  core = aggregation1.Get(0)
    NodeContainer aggregation1;
    aggregation1.Create(2);   //c1,a1

    NodeContainer aggregation2;
    aggregation2.Add(aggregation1.Get(0));//C1
    aggregation2.Create(1);  //a2
    
    //中间网络部分 ：a1-->t1,t2  a2-->t3,t4
    //创建ToR
    NodeContainer toR1,toR2;
    toR1.Add(aggregation1.Get(1));//a1
    toR1.Create(2);  //t1,t2
    toR2.Add(aggregation2.Get(1));//a2
    toR2.Create(2);  //t3,t4

    //底层网络部分：t1-->n1,n2  t2-->n3,n4  t3-->n5,n6  t4-->n7,n8
    //创建节点Nodes   csmaNodes
    NodeContainer csmaNode1,csmaNode2,csmaNode3,csmaNode4;
    csmaNode1.Add(toR1.Get(1));//t1
    csmaNode1.Create(2);//n1,n2
    csmaNode2.Add(toR1.Get(2));//t2
    csmaNode2.Create(2);//n3,n4
    csmaNode3.Add(toR2.Get(1));//t3
    csmaNode3.Create(2);//n5,n6
    csmaNode4.Add(toR2.Get(2));//t4
    csmaNode4.Create(2);//n7,n8

    //设置传送速率和信道延迟
    PointToPointHelper pointToPoint1,pointToPoint2;
    pointToPoint1.SetDeviceAttribute ("DataRate", StringValue ("1.5Mbps"));
    pointToPoint1.SetChannelAttribute ("Delay"  , TimeValue(NanoSeconds(500)) );
    pointToPoint2.SetDeviceAttribute ("DataRate", StringValue ("1.5Mbps"));
    pointToPoint2.SetChannelAttribute ("Delay"  , TimeValue(NanoSeconds(500)) );

    //安装P2P网卡设备到P2P网络节点
    NetDeviceContainer p2pDevices1,p2pDevices2;
    p2pDevices1 = pointToPoint1.Install(aggregation1);
    p2pDevices2 = pointToPoint2.Install(aggregation2);

    //创建和连接CSMA设备及信道
    CsmaHelper csma2,csma3;
    csma2.SetChannelAttribute("DataRate", StringValue("1Mbps"));
    csma2.SetChannelAttribute("Delay"   , TimeValue(NanoSeconds(500)) );
    csma3.SetChannelAttribute("DataRate", StringValue("1Mbps"));
    csma3.SetChannelAttribute("Delay"   , TimeValue(NanoSeconds(500)) );

    //中间层csma
    NetDeviceContainer toR1Devices1,toR2Devices2;
    toR1Devices1 = csma2.Install(toR1);
    toR2Devices2 = csma2.Install(toR2);
    //底层 csma
    NetDeviceContainer csmaNode1Devices1,csmaNode1Devices2,csmaNode1Devices3,csmaNode1Devices4;
    csmaNode1Devices1 = csma3.Install(csmaNode1);
    csmaNode1Devices2 = csma3.Install(csmaNode2);
    csmaNode1Devices3 = csma3.Install(csmaNode3);
    csmaNode1Devices4 = csma3.Install(csmaNode4);

    //安装网络协议
    InternetStackHelper stack;
    stack.Install (aggregation1);
    stack.Install (aggregation2.Get(1));
    stack.Install (toR1.Get(1));
    stack.Install (toR1.Get(2));
    stack.Install (toR2.Get(1));
    stack.Install (toR2.Get(2));
    stack.Install (csmaNode1.Get(1));
    stack.Install (csmaNode1.Get(2));
    stack.Install (csmaNode2.Get(1));
    stack.Install (csmaNode2.Get(2));
    stack.Install (csmaNode3.Get(1));
    stack.Install (csmaNode3.Get(2));
    stack.Install (csmaNode4.Get(1));
    stack.Install (csmaNode4.Get(2));

    NS_LOG_DEBUG("================== Install TCP transport ==================");
    // large TCP buffers to prevent the applications from bottlenecking the exp
    Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (1 << 30)); // RcvBufSize: TcpSocket maximum receive buffer size (bytes)
    Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (1 << 30)); // SndBufSize: TcpSocket maximum transmit buffer size (bytes)
    // Reset the default MSS ~500
    // IP MTU = IP header (20B-60B) + TCP header (20B-60B) + TCP MSS
    // Ethernet frame = Ethernet header (14B) + IP MTU + FCS (4B)
    // The additional header overhead for this instance is 20+20+14=54B, hence app_packet_size <= 1500-54 = 1446 
    // 1500 (the standard Ethernet frame size) set in PointToPointHelper
    Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (app_packet_size));
    Config::SetDefault ("ns3::TcpSocketBase::Sack", BooleanValue (sack));
    Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (delackcount));
    Config::SetDefault ("ns3::TcpL4Protocol::RecoveryType", TypeIdValue (TypeId::LookupByName (recovery)));

    Ptr<TcpL4Protocol> protol;
    Ptr<TcpL4Protocol> protor;  
    protol = csmaNode1.Get(1)->GetObject<TcpL4Protocol> (); 
    protor = csmaNode3.Get(1)->GetObject<TcpL4Protocol> ();
    protol->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot0)));
    protor->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot0))); 
    
    /*
    protol = csmaNode3.Get(2)->GetObject<TcpL4Protocol> (); 
    protor = csmaNode1.Get(2)->GetObject<TcpL4Protocol> ();
    protol->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot0)));
    protor->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot0))); 
    protol = csmaNode2.Get(1)->GetObject<TcpL4Protocol> (); 
    protor = csmaNode4.Get(1)->GetObject<TcpL4Protocol> ();
    protol->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot0)));
    protor->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot0)));
    protol = csmaNode4.Get(2)->GetObject<TcpL4Protocol> (); 
    protor = csmaNode2.Get(2)->GetObject<TcpL4Protocol> ();
    protol->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot0)));
    protor->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot0)));
    */

    TrafficControlHelper tch_switch;
    if (result_dir == "tmp_index/dcn1/fifo/") {
      tch_switch.SetRootQueueDisc ("ns3::FifoQueueDisc", "MaxSize", StringValue (switch_total_bufsize));
      tch_switch.Install(p2pDevices1.Get(0));
      tch_switch.Install(p2pDevices1.Get(1));
      tch_switch.Install(p2pDevices2.Get(0));
      tch_switch.Install(p2pDevices2.Get(1));
    } 
    else if (result_dir == "tmp_index/dcn1/fq/") {
      tch_switch.SetRootQueueDisc ("ns3::FqCoDelQueueDisc", "MaxSize", StringValue (max_switch_total_bufsize),
                                                            "Flows", UintegerValue (4294967295)); // 2^32 - 1
      tch_switch.Install(p2pDevices1.Get(0));
      tch_switch.Install(p2pDevices1.Get(1));
      tch_switch.Install(p2pDevices2.Get(0));
      tch_switch.Install(p2pDevices2.Get(1));
    } 

    //两个网段的IP地址类对象
    Ipv4AddressHelper address;
    //安排P2P网段的地址
    address.SetBase("192.168.1.0","255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces1 = address.Assign(p2pDevices1);
    address.SetBase("192.168.2.0","255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces2 = address.Assign(p2pDevices2);

    //安排CSMA网段地址
    address.SetBase("10.1.1.0","255.255.255.0");
    Ipv4InterfaceContainer toRInterfaces1 = address.Assign(toR1Devices1);
    address.SetBase("10.1.2.0","255.255.255.0");
    Ipv4InterfaceContainer toRInterfaces2 = address.Assign(toR2Devices2);
    address.SetBase("10.0.1.0","255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces1 = address.Assign(csmaNode1Devices1);
    address.SetBase("10.0.2.0","255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces2 = address.Assign(csmaNode1Devices2);
    address.SetBase("10.0.3.0","255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces3 = address.Assign(csmaNode1Devices3);
    address.SetBase("10.0.4.0","255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces4 = address.Assign(csmaNode1Devices4);
    /************网络拓扑部分结束************/

    /**************应用程序部分**************/
    // adds some parameters for sending
    std::vector<std::string> app_bw0{};
    std::vector<std::string> app_bw1{};
    std::vector<std::string> app_bw2{};
    std::vector<std::string> app_bw3{};
    std::string app_bw0_str;
    std::string app_bw1_str;
    std::string app_bw2_str;
    std::string app_bw3_str;
    app_bw0_str = "1Mbps,0Mbps,1Mbps,0Mbps,1Mbps,0Mbps,1Mbps,0Mbps,1Mbps,0Mbps";
    app_bw1_str = "1Mbps,0Mbps,1Mbps,0Mbps,1Mbps,0Mbps,1Mbps,0Mbps,1Mbps,0Mbps";
    app_bw2_str = "1Mbps,0Mbps,1Mbps,0Mbps,1Mbps,0Mbps,1Mbps,0Mbps,1Mbps,0Mbps";
    app_bw3_str = "1Mbps,0Mbps,1Mbps,0Mbps,1Mbps,0Mbps,1Mbps,0Mbps,1Mbps,0Mbps";
    ParseAppBw(app_bw0_str, &app_bw0);
    ParseAppBw(app_bw1_str, &app_bw1);
    ParseAppBw(app_bw2_str, &app_bw2);
    ParseAppBw(app_bw3_str, &app_bw3);
    std::vector<double> app_seconds_change0{};
    std::vector<double> app_seconds_change1{};
    std::vector<double> app_seconds_change2{};
    std::vector<double> app_seconds_change3{};
    std::string app_sc0_str;
    std::string app_sc1_str;
    std::string app_sc2_str;
    std::string app_sc3_str;
    app_sc0_str = "0,1,2,3,4,5,6,7,8,9,40";
    app_sc1_str = "0,1,2,3,4,5,6,7,8,9,40";
    app_sc2_str = "0,1,2,3,4,5,6,7,8,9,40";
    app_sc3_str = "0,1,2,3,4,5,6,7,8,9,40";
    ParseAppSecondsChange(app_sc0_str, &app_seconds_change0, 0);
    ParseAppSecondsChange(app_sc1_str, &app_seconds_change1, 0);
    ParseAppSecondsChange(app_sc2_str, &app_seconds_change2, 0);
    ParseAppSecondsChange(app_sc3_str, &app_seconds_change3, 0);
    Time starttime = Seconds (1.0 );
    Time endtime = Seconds (41.0 );

    //Pattern 1 :  inter-cluster traffic
    //Each server communicates using TCP with another server that comes from different cluster
    //For example, 1-5, 6-2, 3-7, 8-4
    int16_t sinkPort = 8080;
    
    //建立 traffic : n1-n5
    PacketSinkHelper packetSinkHelper1("ns3::TcpSocketFactory",InetSocketAddress(csmaInterfaces3.GetAddress(1),sinkPort));//n5
    ApplicationContainer sinkApp1 = packetSinkHelper1.Install(csmaNode3.Get(1));
    // added to generate receving curve
    sinkApp1.Get(0)->TraceConnectWithoutContext("RxWithAddresses", MakeCallback(&RxWithAddressesPacketSink));
    sinkApp1.Start(Seconds(0.0));
    sinkApp1.Stop(Seconds(40.0));

    // used custom application to add ByteTag
    Address sinkAddress (InetSocketAddress (csmaInterfaces3.GetAddress(1), sinkPort)); // InetSocketAddress: this class holds an Ipv4Address and a port number to form an ipv4 transport endpoint
    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (csmaNode1.Get (1), TcpSocketFactory::GetTypeId ());
    Ptr<MySource> app = CreateObject<MySource> ();
    app->Setup (ns3TcpSocket, sinkAddress, app_packet_size, &app_bw0, 0, false, result_dir, &app_seconds_change0); // i provides the source id for the packets
    app->SetStartTime (starttime);                       
    csmaNode1.Get (1)->AddApplication (app);
    app->SetStopTime (endtime);

    //建立 traffic : n6-n2
    PacketSinkHelper packetSinkHelper2("ns3::TcpSocketFactory",InetSocketAddress(csmaInterfaces1.GetAddress(2),sinkPort));//n2
    ApplicationContainer sinkApp2 = packetSinkHelper2.Install(csmaNode1.Get(2));
    // added to generate receving curve
    sinkApp2.Get(0)->TraceConnectWithoutContext("RxWithAddresses", MakeCallback(&RxWithAddressesPacketSink));
    sinkApp2.Start(Seconds(0.0));
    sinkApp2.Stop(Seconds(40.0));

    // used custom application to add ByteTag
    Address sinkAddress1 (InetSocketAddress (csmaInterfaces1.GetAddress(2), sinkPort)); // InetSocketAddress: this class holds an Ipv4Address and a port number to form an ipv4 transport endpoint
    ns3TcpSocket = Socket::CreateSocket (csmaNode3.Get (2), TcpSocketFactory::GetTypeId ());
    app = CreateObject<MySource> ();
    app->Setup (ns3TcpSocket, sinkAddress1, app_packet_size, &app_bw1, 1, false, result_dir, &app_seconds_change1); // i provides the source id for the packets
    app->SetStartTime (starttime);                       
    csmaNode3.Get (2)->AddApplication (app);
    app->SetStopTime (endtime);


    //建立 traffic : n3-n7
    PacketSinkHelper packetSinkHelper3("ns3::TcpSocketFactory",InetSocketAddress(csmaInterfaces4.GetAddress(1),sinkPort));//n7
    ApplicationContainer sinkApp3 = packetSinkHelper3.Install(csmaNode4.Get(1));
    // added to generate receving curve
    sinkApp3.Get(0)->TraceConnectWithoutContext("RxWithAddresses", MakeCallback(&RxWithAddressesPacketSink));
    sinkApp3.Start(Seconds(0.0));
    sinkApp3.Stop(Seconds(40.0));

    // used custom application to add ByteTag
    Address sinkAddress2 (InetSocketAddress (csmaInterfaces4.GetAddress(1), sinkPort)); // InetSocketAddress: this class holds an Ipv4Address and a port number to form an ipv4 transport endpoint
    ns3TcpSocket = Socket::CreateSocket (csmaNode2.Get (1), TcpSocketFactory::GetTypeId ());
    app = CreateObject<MySource> ();
    app->Setup (ns3TcpSocket, sinkAddress2, app_packet_size, &app_bw2, 2, false, result_dir, &app_seconds_change2); // i provides the source id for the packets
    app->SetStartTime (starttime);                       
    csmaNode2.Get (1)->AddApplication (app);
    app->SetStopTime (endtime);


    //建立 traffic : n8-n4
    PacketSinkHelper packetSinkHelper4("ns3::TcpSocketFactory",InetSocketAddress(csmaInterfaces2.GetAddress(2),sinkPort));//n4
    ApplicationContainer sinkApp4 = packetSinkHelper4.Install(csmaNode2.Get(2));
    // added to generate receving curve
    sinkApp4.Get(0)->TraceConnectWithoutContext("RxWithAddresses", MakeCallback(&RxWithAddressesPacketSink));
    sinkApp4.Start(Seconds(0.0));
    sinkApp4.Stop(Seconds(40.0));

    // used custom application to add ByteTag
    Address sinkAddress3 (InetSocketAddress (csmaInterfaces2.GetAddress(2), sinkPort)); // InetSocketAddress: this class holds an Ipv4Address and a port number to form an ipv4 transport endpoint
    ns3TcpSocket = Socket::CreateSocket (csmaNode4.Get (2), TcpSocketFactory::GetTypeId ());
    app = CreateObject<MySource> ();
    app->Setup (ns3TcpSocket, sinkAddress3, app_packet_size, &app_bw3, 3, false, result_dir, &app_seconds_change3); // i provides the source id for the packets
    app->SetStartTime (starttime);                       
    csmaNode4.Get (2)->AddApplication (app);
    app->SetStopTime (endtime);

    
    /****调用全局路由,帮助建立网络路由****/
    //全局路由管理器根据节点产生的链路通告为每个节点建立路由表
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    Simulator::Schedule (MilliSeconds(1000), &PrintProgress, MilliSeconds(1000));
    Simulator::Stop (Seconds (41.0));
    
    Simulator::Run();
    Simulator::Destroy();
    return 0;
    
}




