#include <chrono>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/my-source-id-tag.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include "my-source.h"

// added on 08.08 for data rate change
#include <vector>

using namespace ns3;

// Long-lived flows, single-bottleneck
NS_LOG_COMPONENT_DEFINE ("DumbbellLong");

#define MAX_CCA 9

// int num_tracing_periods = 0;
double sim_seconds = 1;
double app_seconds_start = 0.1;
double app_seconds_end = 10;
// "sim_seconds" is the total simulation time; "app_seconds_end" is the time when app ends. 
// added on 09.01 to specify different start time
double app_seconds_start0 = 0.1;
double app_seconds_start1 = 0.1;
double app_seconds_start2 = 0.1;
double app_seconds_start3 = 0.1;
double app_seconds_start4 = 0.1;
double app_seconds_start5 = 0.1;
double app_seconds_start6 = 0.1;
double app_seconds_start7 = 0.1;
double app_seconds_start8 = 0.1;
std::string result_dir;
bool printprogress = true;

static void
RxWithAddressesPacketSink (Ptr<const Packet> p, const Address& from, const Address& local) {
  MySourceIDTag tag;
  if (p->FindFirstMatchingByteTag(tag)) {
    // NS_LOG_DEBUG ("[" << Simulator::Now ().GetNanoSeconds() << "] SourceIDTag: " << tag.Get() << ", size: " << p->GetSize()
    //              << ", from: " << InetSocketAddress::ConvertFrom(from).GetIpv4()
    //              << ", local: " << InetSocketAddress::ConvertFrom(local).GetIpv4());
  }

  // added on 07.27
  std::ofstream throput_ofs2 (result_dir + "received_ms.dat", std::ios::out | std::ios::app);
  throput_ofs2 << "[" << Simulator::Now ().GetMilliSeconds() << "] SourceIDTag: " << tag.Get() << ", size: " << p->GetSize()
              << std::endl;
}

void
PrintProgress (Time interval)
{
  std::cout << "[PID:" << getpid() << "] Progress: " << std::fixed << std::setprecision (1) << Simulator::Now ().GetSeconds () << "[s]" << std::endl;
  Simulator::Schedule (interval, &PrintProgress, interval);
}

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

int 
main (int argc, char *argv[])
{
  // 'cmd' object used for adding, parsing, and retrieving command-line parameters
  CommandLine cmd (__FILE__); 

  // Non-configurable or derived params
  uint32_t num_leaf = 2;
  /* Selective Acknowledgment: Enabling SACK allows the receiving party to explicitly 
  notify the sender about which data packets have been successfully received, thereby 
  enhancing the recovery efficiency in the case of lost packets. */ 
  bool sack = true; 
  std::string recovery = "ns3::TcpClassicRecovery";
  // Naming the output directory using local system time
  time_t rawtime;
  struct tm * timeinfo;
  char buffer [80];
  time (&rawtime);
  timeinfo = localtime (&rawtime);
  // https://zetcode.com/articles/cdatetime/
  strftime (buffer, sizeof (buffer), "%Y-%m-%d-%H-%M-%S-%Z", timeinfo);
  std::string current_time (buffer);
  result_dir = "tmp_index/" + current_time + "/"; // we actually specify this in the config
  std::string max_switch_total_bufsize = "2000p"; // for "FqCoDelQueueDisc"

  // CMD configurable params
  std::string config_path = "";  
  uint32_t progress_interval_ms = 1000;
  bool enable_debug = 0;  
  bool enable_stdout = 1; 
  uint32_t seed = 1;  // Fixed
  uint32_t run = 1;  // Vary across replications
  sim_seconds = 10;
  /* In TCP, delayed acknowledgment is a mechanism where the receiver intentionally delays
  sending acknowledgments for received data packets. This delay is done to wait and see if
  additional packets will arrive soon, so that the acknowledgment can cover multiple 
  packets with a single acknowledgment frame, reducing overhead. */
  uint32_t delackcount = 1;
  /* In the context of NS-3's PointToPointHelper, the MaxSize attribute for the queue 
  represents the maximum size of the buffer or queue that holds packets before they are 
  transmitted over the point-to-point link. */
  std::string switch_netdev_size = "100p";
  std::string server_netdev_size = "100p";  
  uint32_t app_packet_size = 1440; // in bytes 
  // Configure 0 will give 1000
  std::string switch_total_bufsize = "100p";
  /* What is the difference between "switch_netdev_size" and "switch_total_bufsize"?
  "NetDeviceContainer" = "PointToPointHelper (limited by "switch_netdev_size")" installed 
  in "NodeContainer". "TrafficControlHelper (limited by "switch_total_bufsize")" is 
  installed on "NetDeviceContainer", which stores the mapping between a device and the 
  associated queue disc into the traffic control layer of the corresponding node. They 
  control different layers. */
  std::string queuedisc_type = "FifoQueueDisc";
  std::string bottleneck_bw = "5Mbps";
  std::string bottleneck_delay = "2ms";
  std::string leaf_bw0 = "0Mbps";
  std::string leaf_bw1 = "0Mbps";
  std::string leaf_bw2 = "0Mbps";
  std::string leaf_bw3 = "0Mbps";
  std::string leaf_bw4 = "0Mbps";
  std::string leaf_bw5 = "0Mbps";
  std::string leaf_bw6 = "0Mbps";
  std::string leaf_bw7 = "0Mbps";
  std::string leaf_bw8 = "0Mbps";
  // edited on 08.07 for data rate change
  std::vector<std::string> app_bw0{};
  std::vector<std::string> app_bw1{};
  std::vector<std::string> app_bw2{};
  std::vector<std::string> app_bw3{};
  std::vector<std::string> app_bw4{};
  std::vector<std::string> app_bw5{};
  std::vector<std::string> app_bw6{};
  std::vector<std::string> app_bw7{};
  std::vector<std::string> app_bw8{};
  // added on 08.07 for data rate change
  std::vector<double> app_seconds_change0{};
  std::vector<double> app_seconds_change1{};
  std::vector<double> app_seconds_change2{};
  std::vector<double> app_seconds_change3{};
  std::vector<double> app_seconds_change4{};
  std::vector<double> app_seconds_change5{};
  std::vector<double> app_seconds_change6{};
  std::vector<double> app_seconds_change7{};
  std::vector<double> app_seconds_change8{};
  uint32_t num_cca = 0;
  std::string transport_prot0 = "TcpCubic";
  std::string transport_prot1 = "TcpCubic";
  std::string transport_prot2 = "TcpCubic";
  std::string transport_prot3 = "TcpCubic";
  std::string transport_prot4 = "TcpCubic";
  std::string transport_prot5 = "TcpCubic";
  std::string transport_prot6 = "TcpCubic";
  std::string transport_prot7 = "TcpCubic";
  std::string transport_prot8 = "TcpCubic";
  std::string leaf_delay0 = "1ms";
  std::string leaf_delay1 = "1ms";
  std::string leaf_delay2 = "1ms";
  std::string leaf_delay3 = "1ms";
  std::string leaf_delay4 = "1ms";
  std::string leaf_delay5 = "1ms";
  std::string leaf_delay6 = "1ms";
  std::string leaf_delay7 = "1ms";
  std::string leaf_delay8 = "1ms";              
  uint32_t num_cca0 = 0;
  uint32_t num_cca1 = 0;
  uint32_t num_cca2 = 0;
  uint32_t num_cca3 = 0;
  uint32_t num_cca4 = 0;
  uint32_t num_cca5 = 0;
  uint32_t num_cca6 = 0;
  uint32_t num_cca7 = 0;
  uint32_t num_cca8 = 0;

  cmd.AddValue("config_path", "Path to the json configuration file", config_path);
  cmd.AddValue("result_dir", "Optional path to the output dir", result_dir);  
  cmd.AddValue("seed", "Seed", seed);
  cmd.AddValue("run", "Run", run);
  cmd.AddValue ("enable_debug", "Enable logging", enable_debug); 
  cmd.AddValue ("enable_stdout", "Enable verbose rmterminal print", enable_stdout);  
  cmd.AddValue ("printprogress", "Enable verbose rmterminal print", printprogress);    
  cmd.AddValue ("sim_seconds", "Simulation time [s]", sim_seconds);
  cmd.AddValue ("app_seconds_start", "Application start time [s]", app_seconds_start);  
  cmd.AddValue ("app_seconds_end", "Application stop time [s]", app_seconds_end);
  cmd.AddValue ("progress_interval_ms", "Prograss interval [ms]", progress_interval_ms);    
  cmd.AddValue ("delackcount", "TcpSocket::DelAckCount", delackcount);  
  cmd.AddValue ("app_packet_size", "App payload size", app_packet_size);    
  cmd.AddValue ("num_cca", "Number of CCA groups", num_cca);
  cmd.AddValue ("transport_prot0", "Transport protocol to use: TcpNewReno, TcpLinuxReno, "
                "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno, "
                "TcpBic, TcpYeah, TcpIllinois, TcpWestwood, TcpWestwoodPlus, TcpLedbat, "
		            "TcpLp, TcpDctcp, TcpCubic, TcpBbr", transport_prot0);
  cmd.AddValue ("transport_prot1", "", transport_prot1);
  cmd.AddValue ("transport_prot2", "", transport_prot2);
  cmd.AddValue ("transport_prot3", "", transport_prot3);
  cmd.AddValue ("transport_prot4", "", transport_prot4);
  cmd.AddValue ("transport_prot5", "", transport_prot5);
  cmd.AddValue ("transport_prot6", "", transport_prot6);
  cmd.AddValue ("transport_prot7", "", transport_prot7);
  cmd.AddValue ("transport_prot8", "", transport_prot8);          
  cmd.AddValue ("bottleneck_bw", "BW of the bottleneck link", bottleneck_bw);
  cmd.AddValue ("bottleneck_delay", "Delay of the bottleneck link", bottleneck_delay);
  cmd.AddValue ("leaf_bw0", "BW of the leaf link grp 0", leaf_bw0);
  cmd.AddValue ("leaf_bw1", "", leaf_bw1);
  cmd.AddValue ("leaf_bw2", "", leaf_bw2);
  cmd.AddValue ("leaf_bw3", "", leaf_bw3);
  cmd.AddValue ("leaf_bw4", "", leaf_bw4);
  cmd.AddValue ("leaf_bw5", "", leaf_bw5);
  cmd.AddValue ("leaf_bw6", "", leaf_bw6);
  cmd.AddValue ("leaf_bw7", "", leaf_bw7);
  cmd.AddValue ("leaf_bw8", "", leaf_bw8);                
  cmd.AddValue ("leaf_delay0", "Delay of the leaf links for grp0", leaf_delay0);
  cmd.AddValue ("leaf_delay1", "", leaf_delay1);  
  cmd.AddValue ("leaf_delay2", "", leaf_delay2);  
  cmd.AddValue ("leaf_delay3", "", leaf_delay3);  
  cmd.AddValue ("leaf_delay4", "", leaf_delay4);  
  cmd.AddValue ("leaf_delay5", "", leaf_delay5);  
  cmd.AddValue ("leaf_delay6", "", leaf_delay6);  
  cmd.AddValue ("leaf_delay7", "", leaf_delay7);              
  cmd.AddValue ("leaf_delay8", "", leaf_delay8);   
  // added on 08.11 for data rate change
  std::string app_bw0_str, app_bw1_str, app_bw2_str, app_bw3_str, app_bw4_str, app_bw5_str, app_bw6_str, app_bw7_str, app_bw8_str;
  // added on 08.08 for data rate change
  cmd.AddValue ("app_bw0", "BW of each application", app_bw0_str);
  cmd.AddValue ("app_bw1", "", app_bw1_str);  
  cmd.AddValue ("app_bw2", "", app_bw2_str);  
  cmd.AddValue ("app_bw3", "", app_bw3_str);  
  cmd.AddValue ("app_bw4", "", app_bw4_str);  
  cmd.AddValue ("app_bw5", "", app_bw5_str);  
  cmd.AddValue ("app_bw6", "", app_bw6_str);  
  cmd.AddValue ("app_bw7", "", app_bw7_str);  
  cmd.AddValue ("app_bw8", "", app_bw8_str);
  // added on 08.11 for data rate change
  std::string app_sc0_str, app_sc1_str, app_sc2_str, app_sc3_str, app_sc4_str, app_sc5_str, app_sc6_str, app_sc7_str, app_sc8_str;
  cmd.AddValue ("app_seconds_change0", "Timestamp of changes of each application", app_sc0_str);
  cmd.AddValue ("app_seconds_change1", "", app_sc1_str);  
  cmd.AddValue ("app_seconds_change2", "", app_sc2_str);  
  cmd.AddValue ("app_seconds_change3", "", app_sc3_str);  
  cmd.AddValue ("app_seconds_change4", "", app_sc4_str);  
  cmd.AddValue ("app_seconds_change5", "", app_sc5_str);  
  cmd.AddValue ("app_seconds_change6", "", app_sc6_str);  
  cmd.AddValue ("app_seconds_change7", "", app_sc7_str);  
  cmd.AddValue ("app_seconds_change8", "", app_sc8_str);
  cmd.AddValue ("switch_netdev_size", "Netdevice queue size (switch)", switch_netdev_size);  
  cmd.AddValue ("server_netdev_size", "Netdevice queue size (server)", server_netdev_size);    
  cmd.AddValue ("switch_total_bufsize", "Switch buffer size", switch_total_bufsize);
  cmd.AddValue ("queuedisc_type", "Queue Disc type", queuedisc_type);
  cmd.AddValue ("num_cca0", "Number of flows/mysource for cca0", num_cca0);
  cmd.AddValue ("num_cca1", "Number of flows/mysource for cca1", num_cca1);
  cmd.AddValue ("num_cca2", "Number of flows/mysource for cca2", num_cca2);
  cmd.AddValue ("num_cca3", "Number of flows/mysource for cca3", num_cca3);
  cmd.AddValue ("num_cca4", "Number of flows/mysource for cca4", num_cca4);
  cmd.AddValue ("num_cca5", "Number of flows/mysource for cca5", num_cca5);
  cmd.AddValue ("num_cca6", "Number of flows/mysource for cca6", num_cca6);
  cmd.AddValue ("num_cca7", "Number of flows/mysource for cca7", num_cca7);
  cmd.AddValue ("num_cca8", "Number of flows/mysource for cca8", num_cca8);
  cmd.Parse (argc, argv);

  // added on 08.11 for data rate change
  switch(num_cca) {
    case 9:
      ParseAppBw(app_bw8_str, &app_bw8);
      ParseAppSecondsChange(app_sc8_str, &app_seconds_change8, 8);
    case 8:
      ParseAppBw(app_bw7_str, &app_bw7);
      ParseAppSecondsChange(app_sc7_str, &app_seconds_change7, 7);
    case 7:
      ParseAppBw(app_bw6_str, &app_bw6);
      ParseAppSecondsChange(app_sc6_str, &app_seconds_change6, 6);
    case 6:
      ParseAppBw(app_bw5_str, &app_bw5);  
      ParseAppSecondsChange(app_sc5_str, &app_seconds_change5, 5);
    case 5:
      ParseAppBw(app_bw4_str, &app_bw4);  
      ParseAppSecondsChange(app_sc4_str, &app_seconds_change4, 4);      
    case 4:
      ParseAppBw(app_bw3_str, &app_bw3);
      ParseAppSecondsChange(app_sc3_str, &app_seconds_change3, 3);       
    case 3:
      ParseAppBw(app_bw2_str, &app_bw2);
      ParseAppSecondsChange(app_sc2_str, &app_seconds_change2, 2);        
    case 2:
      ParseAppBw(app_bw1_str, &app_bw1);
      ParseAppSecondsChange(app_sc1_str, &app_seconds_change1, 1);         
    case 1:
      ParseAppBw(app_bw0_str, &app_bw0);
      ParseAppSecondsChange(app_sc0_str, &app_seconds_change0, 0);
      break;      
  }

  if (enable_debug) {
    LogComponentEnable ("CebinaeQueueDisc", LOG_LEVEL_DEBUG);
    LogComponentEnable ("DumbbellLong", LOG_LEVEL_DEBUG);
  }

  /* This code prepares the working environment by removing the existing result directory,
  creating a new one, and mirroring the content of the configuration file into a new file 
  within the result directory. */
  std::string rm_dir_cmd = "rm -rf " + result_dir;
  if (system (rm_dir_cmd.c_str ()) == -1) {
    std::cout << "ERR: " << rm_dir_cmd << " failed, proceed anyway." << std::endl;
  };
  std::string create_dir_cmd = "mkdir -p " + result_dir;
  if (system (create_dir_cmd.c_str ()) == -1) {
    std::cout << "ERR: " << create_dir_cmd << " failed, proceed anyway." << std::endl;
  }
  std::ifstream in_file {config_path};
  std::ofstream out_file {result_dir+"/config.json"};
  std::string line;
  if(in_file && out_file){
    while(getline(in_file, line)) { out_file << line << "\n"; }
  } else {
    printf("ERR mirroring config file");
    return 1;
  }
  in_file.close();
  out_file.close();

  transport_prot0 = std::string("ns3::") + transport_prot0;
  transport_prot1 = std::string("ns3::") + transport_prot1;
  transport_prot2 = std::string("ns3::") + transport_prot2;
  transport_prot3 = std::string("ns3::") + transport_prot3;
  transport_prot4 = std::string("ns3::") + transport_prot4;
  transport_prot5 = std::string("ns3::") + transport_prot5;
  transport_prot6 = std::string("ns3::") + transport_prot6;
  transport_prot7 = std::string("ns3::") + transport_prot7;            
  transport_prot8 = std::string("ns3::") + transport_prot8;

  if (num_cca > MAX_CCA) {
    std::cout << "ERR: " << num_cca << " > " << MAX_CCA << std::endl;
    return 1;
  }

  num_leaf = 0;
  switch(num_cca) {
    case 1:
      num_leaf = num_cca0;
      break;
    case 2:
      num_leaf = num_cca0+num_cca1;
      break;
    case 3:
      num_leaf = num_cca0+num_cca1+num_cca2;  
      break;
    case 4:
      num_leaf = num_cca0+num_cca1+num_cca2+num_cca3;  
      break;
    case 5:
      num_leaf = num_cca0+num_cca1+num_cca2+num_cca3+num_cca4;  
      break;      
    case 6:
      num_leaf = num_cca0+num_cca1+num_cca2+num_cca3+num_cca4+num_cca5;
      break;       
    case 7:
      num_leaf = num_cca0+num_cca1+num_cca2+num_cca3+num_cca4+num_cca5+num_cca6;
      break;         
    case 8:
      num_leaf = num_cca0+num_cca1+num_cca2+num_cca3+num_cca4+num_cca5+num_cca6+num_cca7;
      break;         
    case 9:
      num_leaf = num_cca0+num_cca1+num_cca2+num_cca3+num_cca4+num_cca5+num_cca6+num_cca7+num_cca8;
      break;      
  }

  std::ostringstream oss;
  oss       << "=== CMD varas ===\n"
            << "enable_debug: " << std::boolalpha << enable_debug << "\n" 
            << "enable_stdout: " << std::boolalpha << enable_stdout << "\n"      
            << "printprogress: " << std::boolalpha << printprogress << "\n"            
            << "config_path: " << config_path << "\n"
            << "result_dir: " << result_dir << "\n"
            << "sack: " << sack << "\n"
            << "recovery: " << recovery << "\n"
            << "app_packet_size: " << app_packet_size << "\n"            
            << "delackcount: " << delackcount << "\n"
            << "seed: " << seed << "\n"
            << "run: " << run << "\n"  
            << "progress_interval_ms: " << progress_interval_ms << "\n"         
            << "sim_seconds: " << sim_seconds << "\n"
            << "app_seconds_start: " << app_seconds_start << "\n"
            << "app_seconds_end: " << app_seconds_end << "\n"
            << "transport_prot0: " << transport_prot0 << "\n"
            << "transport_prot1: " << transport_prot1 << "\n"
            << "transport_prot2: " << transport_prot2 << "\n"
            << "transport_prot3: " << transport_prot3 << "\n"
            << "transport_prot4: " << transport_prot4 << "\n"
            << "transport_prot5: " << transport_prot5 << "\n"
            << "transport_prot6: " << transport_prot6 << "\n"
            << "transport_prot7: " << transport_prot7 << "\n"
            << "transport_prot8: " << transport_prot8 << "\n"
            << "bottleneck_bw: " << bottleneck_bw << "\n"
            << "bottleneck_delay: " << bottleneck_delay << "\n"
            << "leaf_bw0: " << leaf_bw0 << "\n"
            << "leaf_bw1: " << leaf_bw1 << "\n"
            << "leaf_bw2: " << leaf_bw2 << "\n"
            << "leaf_bw3: " << leaf_bw3 << "\n"
            << "leaf_bw4: " << leaf_bw4 << "\n"
            << "leaf_bw5: " << leaf_bw5 << "\n"
            << "leaf_bw6: " << leaf_bw6 << "\n"
            << "leaf_bw7: " << leaf_bw7 << "\n"
            << "leaf_bw8: " << leaf_bw8 << "\n"                                                                                                
            << "leaf_delay0: " << leaf_delay0 << "\n"
            << "leaf_delay1: " << leaf_delay1 << "\n"
            << "leaf_delay2: " << leaf_delay2 << "\n"
            << "leaf_delay3: " << leaf_delay3 << "\n"
            << "leaf_delay4: " << leaf_delay4 << "\n"
            << "leaf_delay5: " << leaf_delay5 << "\n"
            << "leaf_delay6: " << leaf_delay6 << "\n"
            << "leaf_delay7: " << leaf_delay7 << "\n"
            << "leaf_delay8: " << leaf_delay8 << "\n"
            << "switch_total_bufsize: " << switch_total_bufsize << "\n"
            << "switch_netdev_size: " << switch_netdev_size << "\n"
            << "server_netdev_size: " << server_netdev_size << "\n"            
            << "queuedisc_type: " << queuedisc_type << "\n"
            << "num_cca0: " << num_cca0 << "\n"
            << "num_cca1: " << num_cca1 << "\n"
            << "num_cca2: " << num_cca2 << "\n"
            << "num_cca3: " << num_cca3 << "\n"
            << "num_cca4: " << num_cca4 << "\n"
            << "num_cca5: " << num_cca5 << "\n"
            << "num_cca6: " << num_cca6 << "\n"
            << "num_cca7: " << num_cca7 << "\n"
            << "num_cca8: " << num_cca8 << "\n"
            << "num_leaf: " << num_leaf << "\n";
  // added on 08.07 for data rate change
  for (uint32_t i = 0; i < app_bw0.size(); ++i) {
    oss << "app_bw0[" << i << "] = " << app_bw0[i] << "\n";
  }
  for (uint32_t i = 0; i < app_bw1.size(); ++i) {
    oss << "app_bw1[" << i << "] = " << app_bw1[i] << "\n";
  }
  for (uint32_t i = 0; i < app_bw2.size(); ++i) {
    oss << "app_bw2[" << i << "] = " << app_bw2[i] << "\n";
  }
  for (uint32_t i = 0; i < app_bw3.size(); ++i) {
    oss << "app_bw3[" << i << "] = " << app_bw3[i] << "\n";
  }
  for (uint32_t i = 0; i < app_bw4.size(); ++i) {
    oss << "app_bw4[" << i << "] = " << app_bw4[i] << "\n";
  }
  for (uint32_t i = 0; i < app_bw5.size(); ++i) {
    oss << "app_bw5[" << i << "] = " << app_bw5[i] << "\n";
  }
  for (uint32_t i = 0; i < app_bw6.size(); ++i) {
    oss << "app_bw6[" << i << "] = " << app_bw6[i] << "\n";
  }
  for (uint32_t i = 0; i < app_bw7.size(); ++i) {
    oss << "app_bw7[" << i << "] = " << app_bw7[i] << "\n";
  }
  for (uint32_t i = 0; i < app_bw8.size(); ++i) {
    oss << "app_bw8[" << i << "] = " << app_bw8[i] << "\n";
  }
  for (uint32_t i = 0; i < app_seconds_change0.size(); ++i) {
    oss << "app_seconds_change0[" << i << "] = " << app_seconds_change0[i] << "\n";
  }
  for (uint32_t i = 0; i < app_seconds_change1.size(); ++i) {
    oss << "app_seconds_change1[" << i << "] = " << app_seconds_change1[i] << "\n";
  }
  for (uint32_t i = 0; i < app_seconds_change2.size(); ++i) {
    oss << "app_seconds_change2[" << i << "] = " << app_seconds_change2[i] << "\n";
  }
  for (uint32_t i = 0; i < app_seconds_change3.size(); ++i) {
    oss << "app_seconds_change3[" << i << "] = " << app_seconds_change3[i] << "\n";
  }
  for (uint32_t i = 0; i < app_seconds_change4.size(); ++i) {
    oss << "app_seconds_change4[" << i << "] = " << app_seconds_change4[i] << "\n";
  }
  for (uint32_t i = 0; i < app_seconds_change5.size(); ++i) {
    oss << "app_seconds_change5[" << i << "] = " << app_seconds_change5[i] << "\n";
  }
  for (uint32_t i = 0; i < app_seconds_change6.size(); ++i) {
    oss << "app_seconds_change6[" << i << "] = " << app_seconds_change6[i] << "\n";
  }
  for (uint32_t i = 0; i < app_seconds_change7.size(); ++i) {
    oss << "app_seconds_change7[" << i << "] = " << app_seconds_change7[i] << "\n";
  }
  for (uint32_t i = 0; i < app_seconds_change8.size(); ++i) {
    oss << "app_seconds_change8[" << i << "] = " << app_seconds_change8[i] << "\n";
  }
  oss << "======\n";

  RngSeedManager::SetSeed (seed);
  RngSeedManager::SetRun (run);

  NS_LOG_DEBUG("================== Topology: dumbell (leaf=2) ==================");

  NodeContainer router;
  NodeContainer leftleaf;
  NodeContainer rightleaf;
  router.Create (2);
  leftleaf.Create (num_leaf);
  rightleaf.Create (num_leaf);

  PointToPointHelper p2p_bottleneck;
  p2p_bottleneck.SetDeviceAttribute ("DataRate", StringValue (bottleneck_bw));
  p2p_bottleneck.SetDeviceAttribute ("Mtu", UintegerValue(1500));   
  p2p_bottleneck.SetChannelAttribute ("Delay", StringValue (bottleneck_delay)); // Propagation delay // point-to-point-channel.h
  PointToPointHelper p2p_leaf0;
  p2p_leaf0.SetDeviceAttribute ("DataRate", StringValue (leaf_bw0));
  p2p_leaf0.SetDeviceAttribute ("Mtu", UintegerValue(1500));  
  p2p_leaf0.SetChannelAttribute ("Delay", StringValue (leaf_delay0));
  PointToPointHelper p2p_leaf1;
  p2p_leaf1.SetDeviceAttribute ("DataRate", StringValue (leaf_bw1));
  p2p_leaf1.SetDeviceAttribute ("Mtu", UintegerValue(1500));    
  p2p_leaf1.SetChannelAttribute ("Delay", StringValue (leaf_delay1));  
  PointToPointHelper p2p_leaf2;
  p2p_leaf2.SetDeviceAttribute ("DataRate", StringValue (leaf_bw2));
  p2p_leaf2.SetDeviceAttribute ("Mtu", UintegerValue(1500));   
  p2p_leaf2.SetChannelAttribute ("Delay", StringValue (leaf_delay2));  
  PointToPointHelper p2p_leaf3;
  p2p_leaf3.SetDeviceAttribute ("DataRate", StringValue (leaf_bw3));
  p2p_leaf3.SetDeviceAttribute ("Mtu", UintegerValue(1500));   
  p2p_leaf3.SetChannelAttribute ("Delay", StringValue (leaf_delay3));  
  PointToPointHelper p2p_leaf4;
  p2p_leaf4.SetDeviceAttribute ("DataRate", StringValue (leaf_bw4));
  p2p_leaf4.SetDeviceAttribute ("Mtu", UintegerValue(1500));   
  p2p_leaf4.SetChannelAttribute ("Delay", StringValue (leaf_delay4));  
  PointToPointHelper p2p_leaf5;
  p2p_leaf5.SetDeviceAttribute ("DataRate", StringValue (leaf_bw5));
  p2p_leaf5.SetDeviceAttribute ("Mtu", UintegerValue(1500));   
  p2p_leaf5.SetChannelAttribute ("Delay", StringValue (leaf_delay5));  
  PointToPointHelper p2p_leaf6;
  p2p_leaf6.SetDeviceAttribute ("DataRate", StringValue (leaf_bw6));
  p2p_leaf6.SetDeviceAttribute ("Mtu", UintegerValue(1500));   
  p2p_leaf6.SetChannelAttribute ("Delay", StringValue (leaf_delay6)); 
  PointToPointHelper p2p_leaf7;
  p2p_leaf7.SetDeviceAttribute ("DataRate", StringValue (leaf_bw7));
  p2p_leaf7.SetDeviceAttribute ("Mtu", UintegerValue(1500));   
  p2p_leaf7.SetChannelAttribute ("Delay", StringValue (leaf_delay7)); 
  PointToPointHelper p2p_leaf8;
  p2p_leaf8.SetDeviceAttribute ("DataRate", StringValue (leaf_bw8));
  p2p_leaf8.SetDeviceAttribute ("Mtu", UintegerValue(1500));   
  p2p_leaf8.SetChannelAttribute ("Delay", StringValue (leaf_delay8));        

  // Default NS-3 DropTailQueue size for the NetDevice/NIC is 100p, make them configurable anyway (e.g., 1p where FQ has more predictable perf)
  p2p_bottleneck.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue (switch_netdev_size));
  p2p_leaf0.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue (server_netdev_size));
  p2p_leaf1.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue (server_netdev_size));
  p2p_leaf2.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue (server_netdev_size));
  p2p_leaf3.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue (server_netdev_size));
  p2p_leaf4.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue (server_netdev_size));
  p2p_leaf5.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue (server_netdev_size));
  p2p_leaf6.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue (server_netdev_size));
  p2p_leaf7.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue (server_netdev_size));
  p2p_leaf8.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue (server_netdev_size));                

  NetDeviceContainer leftleaf_devices;
  NetDeviceContainer rightleaf_devices;
  NetDeviceContainer router_devices; // contains the bottleneck link part of the router
  NetDeviceContainer leftrouter_devices; // contains the leaf link part of the router  
  NetDeviceContainer rightrouter_devices;

  router_devices = p2p_bottleneck.Install(router);

  for (uint32_t i = 0; i < num_leaf; ++i) {
    NetDeviceContainer cl;
    NetDeviceContainer cr;    
    if (i < num_cca0) {
      cl = p2p_leaf0.Install(router.Get (0),
                            leftleaf.Get (i));
      cr = p2p_leaf0.Install(router.Get (1),
                            rightleaf.Get (i));                            
    } else if (i < (num_cca0+num_cca1)) {
      cl = p2p_leaf1.Install(router.Get (0),
                      leftleaf.Get (i));
      cr = p2p_leaf1.Install(router.Get (1),
                            rightleaf.Get (i));                        
    } else if (i < (num_cca0+num_cca1+num_cca2)) {
      cl = p2p_leaf2.Install(router.Get (0),
                      leftleaf.Get (i));  
      cr = p2p_leaf2.Install(router.Get (1),
                            rightleaf.Get (i));                            
    } else if (i < (num_cca0+num_cca1+num_cca2+num_cca3)) {
      cl = p2p_leaf3.Install(router.Get (0),
                      leftleaf.Get (i));    
      cr = p2p_leaf3.Install(router.Get (1),
                            rightleaf.Get (i));                          
    } else if (i < (num_cca0+num_cca1+num_cca2+num_cca3+num_cca4)) {
      cl = p2p_leaf4.Install(router.Get (0),
                      leftleaf.Get (i)); 
      cr = p2p_leaf4.Install(router.Get (1),
                            rightleaf.Get (i));                             
    } else if (i < (num_cca0+num_cca1+num_cca2+num_cca3+num_cca4+num_cca5)) {
      cl = p2p_leaf5.Install(router.Get (0),
                      leftleaf.Get (i));
      cr = p2p_leaf5.Install(router.Get (1),
                            rightleaf.Get (i));                          
    } else if (i < (num_cca0+num_cca1+num_cca2+num_cca3+num_cca4+num_cca5+num_cca6)) {
      cl = p2p_leaf6.Install(router.Get (0),
                      leftleaf.Get (i)); 
      cr = p2p_leaf6.Install(router.Get (1),
                            rightleaf.Get (i));                         
    } else if (i < (num_cca0+num_cca1+num_cca2+num_cca3+num_cca4+num_cca5+num_cca6+num_cca7)) {
      cl = p2p_leaf7.Install(router.Get (0),
                      leftleaf.Get (i));  
      cr = p2p_leaf7.Install(router.Get (1),
                            rightleaf.Get (i));                        
    } else if (i < (num_cca0+num_cca1+num_cca2+num_cca3+num_cca4+num_cca5+num_cca6+num_cca7+num_cca8)) {
      cl = p2p_leaf8.Install(router.Get (0),
                      leftleaf.Get (i)); 
      cr = p2p_leaf8.Install(router.Get (1),
                            rightleaf.Get (i));                         
    }                                    
    leftrouter_devices.Add (cl.Get (0));
    leftleaf_devices.Add (cl.Get (1));
    rightrouter_devices.Add (cr.Get (0));
    rightleaf_devices.Add (cr.Get (1));    
  }

  NS_LOG_DEBUG("================== InternetStackHelper ==================");

  InternetStackHelper stack;
  for (uint32_t i = 0; i < num_leaf; ++i) {
    stack.Install (leftleaf.Get(i));
  }
  for (uint32_t i = 0; i < num_leaf; ++i) {
    stack.Install (rightleaf.Get(i));
  }
  stack.Install(router.Get(0));
  stack.Install(router.Get(1));

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

  TypeId tcpTid;
  bool ok = TypeId::LookupByNameFailSafe (transport_prot0, &tcpTid);
  if (!ok) {
    std::cout << "TypeId " << transport_prot0 << " not found" << std::endl;
    exit(1);
  }  
  ok = TypeId::LookupByNameFailSafe (transport_prot1, &tcpTid);
  if (!ok) {
    std::cout << "TypeId " << transport_prot1 << " not found" << std::endl;
    exit(1);
  }   
  ok = TypeId::LookupByNameFailSafe (transport_prot2, &tcpTid);
  if (!ok) {
    std::cout << "TypeId " << transport_prot2 << " not found" << std::endl;
    exit(1);
  }  
  ok = TypeId::LookupByNameFailSafe (transport_prot3, &tcpTid);
  if (!ok) {
    std::cout << "TypeId " << transport_prot3 << " not found" << std::endl;
    exit(3);
  }  
  ok = TypeId::LookupByNameFailSafe (transport_prot4, &tcpTid);
  if (!ok) {
    std::cout << "TypeId " << transport_prot4 << " not found" << std::endl;
    exit(1);
  }  
  ok = TypeId::LookupByNameFailSafe (transport_prot5, &tcpTid);
  if (!ok) {
    std::cout << "TypeId " << transport_prot5 << " not found" << std::endl;
    exit(1);
  }  
  ok = TypeId::LookupByNameFailSafe (transport_prot6, &tcpTid);
  if (!ok) {
    std::cout << "TypeId " << transport_prot6 << " not found" << std::endl;
    exit(1);
  }  
  ok = TypeId::LookupByNameFailSafe (transport_prot7, &tcpTid);
  if (!ok) {
    std::cout << "TypeId " << transport_prot7 << " not found" << std::endl;
    exit(1);
  }          
  ok = TypeId::LookupByNameFailSafe (transport_prot8, &tcpTid);
  if (!ok) {
    std::cout << "TypeId " << transport_prot8 << " not found" << std::endl;
    exit(1);
  }          
  Ptr<TcpL4Protocol> protol;
  Ptr<TcpL4Protocol> protor;  
  for (uint32_t i = 0; i < num_leaf; ++i) {
    protol = leftleaf.Get(i)->GetObject<TcpL4Protocol> ();    
    protor = rightleaf.Get(i)->GetObject<TcpL4Protocol> ();
    if (i < num_cca0) {
      protol->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot0)));
      protor->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot0)));      
    } else if (i < (num_cca0+num_cca1)) {
      protol->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot1)));
      protor->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot1)));        
    } else if (i < (num_cca0+num_cca1+num_cca2)) {
      protol->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot2)));
      protor->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot2)));        
    } else if (i < (num_cca0+num_cca1+num_cca2+num_cca3)) {
      protol->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot3)));
      protor->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot3)));        
    } else if (i < (num_cca0+num_cca1+num_cca2+num_cca3+num_cca4)) {
      protol->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot4)));
      protor->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot4)));        
    } else if (i < (num_cca0+num_cca1+num_cca2+num_cca3+num_cca4+num_cca5)) {
      protol->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot5)));
      protor->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot5)));        
    } else if (i < (num_cca0+num_cca1+num_cca2+num_cca3+num_cca4+num_cca5+num_cca6)) {
      protol->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot6)));
      protor->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot6)));        
    } else if (i < (num_cca0+num_cca1+num_cca2+num_cca3+num_cca4+num_cca5+num_cca6+num_cca7)) {
      protol->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot7)));
      protor->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot7)));        
    } else if (i < (num_cca0+num_cca1+num_cca2+num_cca3+num_cca4+num_cca5+num_cca6+num_cca7+num_cca8)) {
      protol->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot8)));
      protor->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName(transport_prot8)));        
    }                                    
  }  

  // added on 11.14 to change loss rate
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  em->SetAttribute ("ErrorRate", DoubleValue (0)); // 0.00001
  router_devices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));

  NS_LOG_DEBUG("================== Configure TrafficControlLayer ==================");
  // Manual: To install a queue disc other than the default one, it is necessary to install such queue disc before an IP address is
  // assigned to the device, but after InternetStackHelper::Install(). Alternatively, the default queue disc can be removed from the device after assigning an IP
  // address, by using the Uninstall method of the TrafficControlHelper C++ class, and then installing a different queue
  // disc on the device. By uninstalling without adding a new queue disc, it is also possible to have no queue disc installed
  // on a device.

  // We keep the tch default ns3::FqCoDelQueueDisc (for point-to-point) untouched for sources and sinks.
  // Such DRR fair queueing (https://www.nsnam.org/docs/models/html/fq-codel.html) is useful upon multiple apps on a single sender node.

  // NetDevice switch0 [only tch to change] ---> switch1
  TrafficControlHelper tch_switch;
  QueueDiscContainer qdiscs;
  if (queuedisc_type.compare("FifoQueueDisc") == 0) {
    tch_switch.SetRootQueueDisc ("ns3::FifoQueueDisc", "MaxSize", StringValue (switch_total_bufsize));
    qdiscs = tch_switch.Install(router_devices.Get(0));
    oss << "Configured FifoQueueDisc\n";
  } 
  else if (queuedisc_type.compare("FqCoDelQueueDisc") == 0) {
    tch_switch.SetRootQueueDisc ("ns3::FqCoDelQueueDisc", "MaxSize", StringValue (max_switch_total_bufsize),
                                                          "Flows", UintegerValue (4294967295)); // 2^32 - 1
    qdiscs = tch_switch.Install(router_devices.Get(0));    
    oss << "Configured FqCoDelQueueDisc\n";
  } 
  // added on 08.22 to add more queue disciplines
  else if (queuedisc_type.compare("TbfQueueDisc") == 0) {
    tch_switch.SetRootQueueDisc ("ns3::TbfQueueDisc", "MaxSize", StringValue (switch_total_bufsize));
    qdiscs = tch_switch.Install(router_devices.Get(0));    
    oss << "Configured TbfQueueDisc\n";
  }
  else if (queuedisc_type.compare("RedQueueDisc") == 0) {
    tch_switch.SetRootQueueDisc ("ns3::RedQueueDisc", "MaxSize", StringValue (switch_total_bufsize),
                                                      "MeanPktSize", UintegerValue (app_packet_size)); 
    qdiscs = tch_switch.Install(router_devices.Get(0));    
    oss << "Configured RedQueueDisc\n";
  }
  else if (queuedisc_type.compare("PieQueueDisc") == 0) {
    tch_switch.SetRootQueueDisc ("ns3::PieQueueDisc", "MaxSize", StringValue (switch_total_bufsize),
                                                      "MeanPktSize", UintegerValue (app_packet_size));
    qdiscs = tch_switch.Install(router_devices.Get(0));    
    oss << "Configured PieQueueDisc\n";
  }
  else {
    oss << "Configured NULL QueueDisc (which is the default FqCoDelQueueDisc and buffer size)\n";
  }

  NS_LOG_DEBUG("================== Configure Ipv4AddressHelper ==================");

  Ipv4AddressHelper ipv4_left;
  ipv4_left.SetBase ("1.1.1.0", "255.255.255.0");  
  Ipv4AddressHelper ipv4_right("100.1.1.0", "255.255.255.0");
  Ipv4AddressHelper ipv4_router("200.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer leftleaf_ifc;
  Ipv4InterfaceContainer leftrouter_ifc;
  Ipv4InterfaceContainer rightleaf_ifc;
  Ipv4InterfaceContainer rightrouter_ifc;
  Ipv4InterfaceContainer router_ifc;

  router_ifc = ipv4_router.Assign (router_devices); // implements tch_switch.Install (twice: FqCoDelQueueDisc)

  for (uint32_t i = 0; i < num_leaf; ++i) {
    NetDeviceContainer ndc;
    ndc.Add (leftleaf_devices.Get (i));
    ndc.Add (leftrouter_devices.Get (i));
    Ipv4InterfaceContainer ifc = ipv4_left.Assign (ndc); // implements tch_switch.Install twice
    leftleaf_ifc.Add (ifc.Get (0));
    leftrouter_ifc.Add (ifc.Get (1));
    ipv4_left.NewNetwork ();
  } 

  for (uint32_t i = 0; i < num_leaf; ++i) {
    NetDeviceContainer ndc;
    ndc.Add (rightleaf_devices.Get (i));
    ndc.Add (rightrouter_devices.Get (i));
    Ipv4InterfaceContainer ifc = ipv4_right.Assign (ndc); // implements tch_switch.Install twice
    rightleaf_ifc.Add (ifc.Get (0));
    rightrouter_ifc.Add (ifc.Get (1));   
    ipv4_right.NewNetwork ();
  }  

  NS_LOG_DEBUG("================== Generate application ==================");
  // leftleaf uses app; rightleaf uses sink
  // app and sink are both application layer

  // We want to look at changes in the ns-3 TCP congestion window:
  // 1. Create a socket
  // 2. Hook the CongestionWindow attribute and do the trace connect
  // 3. Pass the socket into the constructor of our simple application which we then install in the source node

  Ptr<MySource>* sources;
  sources = new Ptr<MySource>[num_leaf];

  for (uint32_t i = 0; i < num_leaf; ++i) {
    uint16_t sinkPort = 8080;
    Address sinkAddress (InetSocketAddress (rightleaf_ifc.GetAddress(i), sinkPort)); // InetSocketAddress: this class holds an Ipv4Address and a port number to form an ipv4 transport endpoint

    /////////////////////////////////////////////////////////////////////////////   This is the receive application (rightleaf part)   /////////////////////////////////////////////////////////////////////////////////////////////
    Ptr<PacketSink> sink = CreateObject<PacketSink> ();
    // PacketSink: Receive and consume traffic generated to an IP address and port. This application was written to complement OnOffApplication.
    // PacketSink: The constructor specifies the Address (IP address and port) and the transport protocol to use.
    // PacketSink: A virtual Receive () method is installed as a callback on the receiving socket. By default, when logging is enabled, it prints out the size of packets and their address. A tracing source to Receive() is also available.
    sink->SetAttribute ("Protocol", StringValue ("ns3::TcpSocketFactory"));
    sink->SetAttribute ("Local", AddressValue (InetSocketAddress (Ipv4Address::GetAny (), sinkPort)));
    rightleaf.Get(i)->AddApplication(sink);
    sink->SetStartTime (Seconds (0.));
    sink->SetStopTime (Seconds (app_seconds_end));  

    sink->TraceConnectWithoutContext("RxWithAddresses", MakeCallback(&RxWithAddressesPacketSink));
    //////////////////////////////////////////////////////////////////////////////////   receive application ends here   ///////////////////////////////////////////////////////////////////////////////////////////////////////////

    /////////////////////////////////////////////////////////////////////////////   This is the send application (leftleaf part)   /////////////////////////////////////////////////////////////////////////////////////////////////
    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (leftleaf.Get (i), TcpSocketFactory::GetTypeId ());

    Ptr<MySource> app = CreateObject<MySource> ();
    if (i < num_cca0) {
      app->Setup (ns3TcpSocket, sinkAddress, app_packet_size, &app_bw0, i, false, result_dir, &app_seconds_change0); // i provides the source id for the packets
      app->SetStartTime (Seconds (app_seconds_start0));
    } else if (i < (num_cca0+num_cca1)) {
      app->Setup (ns3TcpSocket, sinkAddress, app_packet_size, &app_bw1, i, false, result_dir, &app_seconds_change1);  
      app->SetStartTime (Seconds (app_seconds_start1));     
    } else if (i < (num_cca0+num_cca1+num_cca2)) {
      app->Setup (ns3TcpSocket, sinkAddress, app_packet_size, &app_bw2, i, false, result_dir, &app_seconds_change2); 
      app->SetStartTime (Seconds (app_seconds_start2));   
    } else if (i < (num_cca0+num_cca1+num_cca2+num_cca3)) {
      app->Setup (ns3TcpSocket, sinkAddress, app_packet_size, &app_bw3, i, false, result_dir, &app_seconds_change3);    
      app->SetStartTime (Seconds (app_seconds_start3));  
    } else if (i < (num_cca0+num_cca1+num_cca2+num_cca3+num_cca4)) {
      app->Setup (ns3TcpSocket, sinkAddress, app_packet_size, &app_bw4, i, false, result_dir, &app_seconds_change4); 
      app->SetStartTime (Seconds (app_seconds_start4));    
    } else if (i < (num_cca0+num_cca1+num_cca2+num_cca3+num_cca4+num_cca5)) {
      app->Setup (ns3TcpSocket, sinkAddress, app_packet_size, &app_bw5, i, false, result_dir, &app_seconds_change5);   
      app->SetStartTime (Seconds (app_seconds_start5));    
    } else if (i < (num_cca0+num_cca1+num_cca2+num_cca3+num_cca4+num_cca5+num_cca6)) {
      app->Setup (ns3TcpSocket, sinkAddress, app_packet_size, &app_bw6, i, false, result_dir, &app_seconds_change6);   
      app->SetStartTime (Seconds (app_seconds_start6));  
    } else if (i < (num_cca0+num_cca1+num_cca2+num_cca3+num_cca4+num_cca5+num_cca6+num_cca7)) {
      app->Setup (ns3TcpSocket, sinkAddress, app_packet_size, &app_bw7, i, false, result_dir, &app_seconds_change7);  
      app->SetStartTime (Seconds (app_seconds_start7));     
    } else if (i < (num_cca0+num_cca1+num_cca2+num_cca3+num_cca4+num_cca5+num_cca6+num_cca7+num_cca8)) {
      app->Setup (ns3TcpSocket, sinkAddress, app_packet_size, &app_bw8, i, false, result_dir, &app_seconds_change8);  
      app->SetStartTime (Seconds (app_seconds_start8));    
    }                                    
    leftleaf.Get (i)->AddApplication (app);
    app->SetStopTime (Seconds (app_seconds_end));
    sources[i] = app;
    //////////////////////////////////////////////////////////////////////////////////   send application ends here   ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
  }

  NS_LOG_DEBUG("================== Tracing ==================");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  if (printprogress) {
    Simulator::Schedule (MilliSeconds(progress_interval_ms), &PrintProgress, MilliSeconds(progress_interval_ms));
  }

  NS_LOG_DEBUG("================== Run ==================");

  Simulator::Stop (Seconds (sim_seconds));
  auto start = std::chrono::high_resolution_clock::now();
  Simulator::Run ();
  auto stop = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed_seconds = stop - start;

  NS_LOG_DEBUG("================== Export digest ==================");

  std::cout << elapsed_seconds.count() << "s" << std::endl;

  oss << "=== Ipv4 addresses ===\n";
  oss << "--- leftleaf_ifc ---\n";
  for (uint32_t i = 0; i < num_leaf; i++) {
    oss << i << " " << leftleaf_ifc.GetAddress(i) << "\n";
  }
  oss << "--- leftrouter_ifc ---\n";
  for (uint32_t i = 0; i < num_leaf; i++) {
    oss << i << " " << leftrouter_ifc.GetAddress(i) << "\n";
  }
  oss << "--- rightleaf_ifc ---\n";
  for (uint32_t i = 0; i < num_leaf; i++) {
    oss << i << " " << rightleaf_ifc.GetAddress(i) << "\n";
  }  
  oss << "--- rightrouter_ifc ---\n";
  for (uint32_t i = 0; i < num_leaf; i++) {
    oss << i << " " << rightrouter_ifc.GetAddress(i) << "\n";
  }
  oss << "--- router_ifc ---\n";
  for (uint32_t i = 0; i < 2; i++) {
    oss << i << " " << router_ifc.GetAddress(i) << "\n";
  }  

  oss << "\n=== Completion time [s]: " << elapsed_seconds.count() << "===\n";
  std::ofstream summary_ofs (result_dir + "/digest", std::ios::out | std::ios::app);  
  summary_ofs << oss.str();

  if (enable_stdout) {
    std::cout << oss.str() << std::endl;
  }
  std::cout << "Result_dir: " << result_dir << std::endl;

  Simulator::Destroy ();

  return 0;
}
