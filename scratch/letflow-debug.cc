/*
 * Author: Enhuan Dong
 */
#include <ctime>
#include <sys/time.h>
#include <stdint.h>
#include <fstream>
#include <string>
#include <cassert>
#include <iostream>
#include <iomanip>
#include "ns3/log.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"
#include "ns3/callback.h"
#include "ns3/string.h"

using namespace std;
using namespace ns3;
NS_LOG_COMPONENT_DEFINE("letflow-debug");

typedef enum
{
  Spine, Leaf, Host, Spine_Leaf, Spine_Leaf_Host
} Layers_t;

typedef enum
{
  NONE, NEW_INCAST, TYPICAL_INCAST, NEW_INCAST_BK, ONE_MAP_ONE
} TrafficMatrix_t;


std::map<string, TrafficMatrix_t> stringToTrafficMatrix;
vector<ApplicationContainer> sourceShortFlowApps;
vector<ApplicationContainer> sourceLargeFlowApps;
vector<ApplicationContainer> sinkApps;

#define PORT 5000

// Setup general parameters
string g_topology = "Inter-Rack";
string g_simName  = "Incast";


// Setup topology parameters
string g_downLinkCapacity = "10Gbps";  
string g_upLinkCapacity = "40Gbps";  //Normal
string g_linkDelay = "25us";       

uint32_t g_numSpine = 4;
uint32_t g_numLeaf = 8;
uint32_t g_numHost = 32;

uint32_t g_flowSize = 128000;   // 128KB
uint32_t g_simTime = 30;             // 20 Seconds (default)
uint32_t g_seed = 0;                // RNG seed
TrafficMatrix_t g_trafficMatrix = NEW_INCAST;

string g_simInstance = "0";
string g_simStartTime = "NULL";
uint32_t g_minRTO = 200;
double g_flowStartTime = 0.0;

uint32_t g_rGap = 50;
uint32_t g_subflows = 8;
uint32_t g_initialCWND = 10;

uint32_t g_segmentSize = 1400;
uint32_t g_REDMeanPktSize = g_segmentSize;

uint32_t g_downLinkQueueLimit = 350;   
uint32_t g_upLinkQueueLimit = 1400;  

// [Second][Metrics][Node][Dev]
/*
double spine_data[g_simTime + 5][2][g_numLeaf * g_numHost][64];
double leaf_data [g_simTime + 5][2][g_numLeaf * g_numHost][64];
double host_data[g_simTime + 5][2][g_numLeaf * g_numHost][2];
*/
// These arrays should have larger space than the previous annotated ones.
double spine_data[60][2][256][64];
double leaf_data[60][2][256][64];
double host_data[60][2][256][2];

double totalSpineUtil, totalSpineLoss, totalLeafUtil, totalLeafLoss, totalHostUtil, totalHostLoss = 0.0;
double meanSpineUtil, meanSpineLoss, meanLeafUtil, meanLeafLoss, meanHostUtil, meanHostLoss = 0.0;

// NodeContainer to use for link utilization and loss rate
NodeContainer spine_c;
NodeContainer leaf_c;
NodeContainer host_c;

//Incast
uint32_t g_numFlow = 10;

void SimHeaderWritter(Ptr<OutputStreamWrapper> stream);
void SimFooterWritter(Ptr<OutputStreamWrapper> stream);

uint64_t
GetLinkRate(string linkRate)
{
  DataRate tmp(linkRate);
  return tmp.GetBitRate();
}

void
SetupStringToTM()
{
  stringToTrafficMatrix["TYPICAL_INCAST"] = TYPICAL_INCAST;
  stringToTrafficMatrix["NEW_INCAST"] = NEW_INCAST;
  stringToTrafficMatrix["NEW_INCAST_BK"] = NEW_INCAST_BK;
  stringToTrafficMatrix["ONE_MAP_ONE"] = ONE_MAP_ONE;
  stringToTrafficMatrix["NONE"] = NONE;
}

string
GetKeyFromValueTM(TrafficMatrix_t tm)
{
  map<string, TrafficMatrix_t>::const_iterator it = stringToTrafficMatrix.begin();
  for (; it != stringToTrafficMatrix.end(); it++)
    {
      if (it->second == tm)
        return it->first;
    }
  return "";
}

string
SetupSimFileName(string input)
{
  ostringstream oss;
  oss.str("");
  oss << g_simName << "_" << g_topology  <<  "_MPTCP_" << GetKeyFromValueTM(g_trafficMatrix) << "_" << input << "_" << g_simInstance
      << ".data";
  string tmp = oss.str();
  oss.str("");
  return tmp;
}

void
OutPutSpine()
{
  Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper>(SetupSimFileName("Spine"), std::ios::out);
  ostream *os = stream->GetStream();
  for (uint32_t s = 0; s <= g_simTime; s++)                          // [Seconds]
    {
      *os << s;
      for (uint32_t m = 0; m < 2; m++)                               // [Metrics]
        {
          for (uint32_t n = 0; n < spine_c.GetN(); n++)               // [Node]
            {
              for (uint32_t d = 1; d <= g_numLeaf; d++)                    // [Dev]
                {

                  if (s == 0)
                    *os << " " << "Node" << n <<"Link" << d << "Metric"<<m;
                  else
                    {
                      *os << "    " << spine_data[s][m][n][d];
                      if (m == 0)            // Utilization
                        totalSpineUtil += spine_data[s][m][n][d];
                      else if (m == 1)       // Loss" " << "Node" << n <<"Link" << d << "Metric"<<m;
                        totalSpineLoss += spine_data[s][m][n][d];
                    }
                }
            }
        }
      *os << endl;
    }
}


void
OutPutLeaf()
{
  Ptr<OutputStreamWrapper> stream_leaf = Create<OutputStreamWrapper>(SetupSimFileName("LEAF"), std::ios::out);
  ostream *os_leaf = stream_leaf->GetStream();
  for (uint32_t s = 0; s <= g_simTime; s++)                           // [Seconds]
    {
      *os_leaf << s;
      for (uint32_t m = 0; m < 2; m++)                                // [Metrics]
        {
          for (uint32_t n = 0; n < leaf_c.GetN(); n++)                 // [Node]
            {
              for (uint32_t d = 1; d <= g_numSpine + g_numHost; d++)                     // [Dev]
                {
                  if (s == 0)
                    *os_leaf << " " << "Node" << n <<"Link" << d << "Metric"<<m;
                  else
                    {
                      *os_leaf << "    " << leaf_data[s][m][n][d];
                      if (m == 0)            // leaf Utilization
                        totalLeafUtil += leaf_data[s][m][n][d];
                      else if (m == 1)       // Leaf Loss
                        totalLeafLoss += leaf_data[s][m][n][d];
                    }
                }
            }
        }
      *os_leaf << endl;
    }
}

void
OutPutHost(){
  Ptr<OutputStreamWrapper> stream_host = Create<OutputStreamWrapper>(SetupSimFileName("HOST"), std::ios::out);
    ostream *os_host = stream_host->GetStream();
    for (uint32_t s = 0; s <= g_simTime; s++)                   // [Seconds]
      {
        *os_host << s;
        for (uint32_t m = 0; m < 2; m++)                        // [Metrics]
          {
            for (uint32_t n = 0; n < host_c.GetN(); n++)        // [Node]
              {
                for (uint32_t d = 1; d <= 1; d++)               // [Dev]
                  {
                    if (s == 0)
                      *os_host << " " << "Node" << n <<"Link" << d << "Metric"<<m;
                    else
                      {
                        *os_host << "    " << host_data[s][m][n][d];
                        if (m == 0)       // host Utilization
                          totalHostUtil += host_data[s][m][n][d];
                        else if (m == 1)  // host Loss
                          totalHostLoss += host_data[s][m][n][d];
                      }
                  }
              }
          }
        *os_host << endl;
      }
}

void
PrintSimParams()
{
  cout << endl;
  cout << "Traffic Matrix   : " << GetKeyFromValueTM(g_trafficMatrix).c_str() << endl;
  cout << "Seed             : " << g_seed << endl;
  cout << "Instance         : " << g_simInstance << endl;
  //cout << "Incast Number    : " << g_incastNumber << endl;
}

void
SimTimeMonitor()
{
  NS_LOG_UNCOND("ClockTime: " << Simulator::Now().GetSeconds());
  double now = Simulator::Now().GetSeconds();
  cout << "[" << g_simName << "](" << g_topology << ")[]{"
      << GetKeyFromValueTM(g_trafficMatrix)
      << "} -> SimClock: " << now << endl;
  if (now < g_simTime)
    Simulator::Schedule(Seconds(0.1), &SimTimeMonitor);
}


Ptr<Queue>
FindQueue(Ptr<NetDevice> dev)
{
  PointerValue ptr;
  dev->GetAttribute("TxQueue", ptr);
  return ptr.Get<Queue>();
}


// [Second][Node][Dev][Metrics]
void
SetupTraces(const Layers_t &layer)
{
  uint32_t T = (uint32_t) Simulator::Now().GetSeconds();
  if (layer == Spine || layer >= Spine_Leaf)
    {
      for (uint32_t i = 0; i < spine_c.GetN(); i++)
        { // j should 1 as lookback interface should not be counted
          for (uint32_t j = 1; j < spine_c.Get(i)->GetNDevices(); j++)
            {
              Ptr<Queue> txQueue = FindQueue(spine_c.Get(i)->GetDevice(j));
              uint32_t totalDropBytes = txQueue->GetTotalDroppedBytes();
              uint32_t totalRxBytes = txQueue->GetTotalReceivedBytes();
              uint32_t totalBytes = totalRxBytes + totalDropBytes;
              spine_data[T][0][i][j] = (((double) totalRxBytes * 8 * 100) / GetLinkRate(g_upLinkCapacity)); // Link Utilization
              spine_data[T][1][i][j] = (((double) totalDropBytes * 1.0 / totalBytes) * 100); // LossRate
              
              if (totalDropBytes)
                NS_LOG_INFO ("Loss!!!");
              NS_LOG_INFO ("spine packet loss:" << T << " spine:" << i << " NIC:" << j << " " << totalDropBytes * 1.0 / 1400 << " " << totalBytes * 1.0 / 1400);

              // Make sure util is not nan
              if (isNaN(spine_data[T][0][i][j]))
                spine_data[T][0][i][j] = 0;
              // Make sure loss is not nan
              if (isNaN(spine_data[T][1][i][j]))
                spine_data[T][1][i][j] = 0;
              // Reset txQueue
              txQueue->ResetStatistics();
            }
        }
    }

  if (layer == Leaf || layer >= Spine_Leaf)
    {
      for (uint32_t i = 0; i < leaf_c.GetN(); i++)
        { // dev = 1 as loop back interface should not be counted here.
          for (uint32_t j = 1; j < leaf_c.Get(i)->GetNDevices(); j++)
            {
              Ptr<Queue> txQueue = FindQueue(leaf_c.Get(i)->GetDevice(j));
              uint32_t totalDropBytes = txQueue->GetTotalDroppedBytes();
              uint32_t totalRxBytes = txQueue->GetTotalReceivedBytes();
              uint32_t totalBytes = totalRxBytes + totalDropBytes;
              if (j <= g_numHost)
                leaf_data[T][0][i][j] = (((double) totalRxBytes * 8 * 100) / GetLinkRate(g_downLinkCapacity));
              else
                leaf_data[T][0][i][j] = (((double) totalRxBytes * 8 * 100) / GetLinkRate(g_upLinkCapacity));

              leaf_data[T][1][i][j] = (((double) totalDropBytes * 1.0 / totalBytes) * 100);
              if (totalDropBytes)
                NS_LOG_INFO ("Loss!!!");
              NS_LOG_INFO ("leaf packet loss:" << T << " leaf:" << i << " NIC:" << j << " " << totalDropBytes * 1.0 / 1400 << " " << totalBytes * 1.0 / 1400);

              // Make sure leaf util is not nan
              if (isNaN(leaf_data[T][0][i][j]))
                leaf_data[T][0][i][j] = 0;
              // Make sure leaf loss is not nan
              if (isNaN(leaf_data[T][1][i][j]))
                leaf_data[T][1][i][j] = 0;
              txQueue->ResetStatistics();
            }
        }
    }
  if (layer == Host || layer >= Spine_Leaf_Host)
    {
      for (uint32_t i = 0; i < host_c.GetN(); i++)
        { // dev = 1 as loop back interface should not be counted here.
          for (uint32_t j = 1; j < host_c.Get(i)->GetNDevices(); j++)
            {
              Ptr<Queue> txQueue = FindQueue(host_c.Get(i)->GetDevice(j));
              uint32_t totalDropBytes = txQueue->GetTotalDroppedBytes();
              uint32_t totalRxBytes = txQueue->GetTotalReceivedBytes();
              uint32_t totalBytes = totalRxBytes + totalDropBytes;
              double util = (((double) totalRxBytes * 8 * 100) / GetLinkRate(g_downLinkCapacity));
              double loss = (((double) totalDropBytes * 1.0 / totalBytes) * 100);
              if (totalDropBytes)
                NS_LOG_INFO ("Loss!!!");
              NS_LOG_INFO ("host packet loss:" << T << " host:" << i << " NIC:" << j << " " << totalDropBytes * 1.0 / 1400 << " " << totalBytes * 1.0 / 1400);
              
              if (isNaN(util))
                util = 0;
              if (isNaN(loss))
                loss = 0;
              host_data[T][0][i][j] = util;
              host_data[T][1][i][j] = loss;

              txQueue->ResetStatistics();
            }
        }
    }
  if (T < g_simTime)
    Simulator::Schedule(Seconds(1), &SetupTraces, layer);
}

string
GetDateTimeNow()
{
  time_t T = time(0);
  struct tm* now = localtime(&T);
  string simStartDate = asctime(now);
  return simStartDate.substr(0, 24);
}

void
SetSimStartTime()
{
  g_simStartTime = GetDateTimeNow();
}

string
GetSimStartTime()
{
  return g_simStartTime;
}

void
SimHeaderWritter(Ptr<OutputStreamWrapper> stream)
{
  ostream *os = stream->GetStream ();
  *os << "SimStart [" << g_simStartTime << "] simName[" << g_simName
      << "] Topology[" << g_topology 
      << "] TrafficMatrix[" << GetKeyFromValueTM (g_trafficMatrix)
      << "] Seed[" << g_seed << "] simInstance["
      << g_simInstance << "] SimPeriod[" << g_simTime
      << "s] HostPerLeaf[" << g_numHost 
      << "] ShortFlowSize[" << g_flowSize
      << "] upLinkRate["<< g_upLinkCapacity << "] downLinkRate["<< g_downLinkCapacity << "] LinkDelay[" << g_linkDelay
      << "] RTOmin[ " << g_minRTO << "]" << endl;
}

void
SimFooterWritter(Ptr<OutputStreamWrapper> stream)
{
  ostream *os = stream->GetStream();
  *os << "SimEnd [" << GetDateTimeNow() << "] AllFlows[" << sourceLargeFlowApps.size() + sourceShortFlowApps.size()
      << "] LargeFlow[" << sourceLargeFlowApps.size() << "] ShortFlows[" << sourceShortFlowApps.size() << "]  SpineUtil["
      << meanSpineUtil << "] SpineLoss[" << meanSpineLoss << "]"
      << endl;
}

void
SimOverallResultWritter()
{
  Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper>(SetupSimFileName("OVERALL"), std::ios::out | std::ios::app);
  SimHeaderWritter(stream);
  ostream *os = stream->GetStream();
  meanSpineUtil = totalSpineUtil / (g_simTime * g_numSpine);
  meanSpineLoss = totalSpineLoss / (g_simTime * g_numSpine);
  if (isNaN(meanSpineUtil))
    meanSpineUtil = 0;
  if (isNaN(meanSpineLoss))
    meanSpineLoss = 0;
  meanLeafUtil = totalLeafUtil / (g_simTime * g_numLeaf);
  meanLeafLoss = totalLeafLoss / (g_simTime * g_numLeaf);
  if (isNaN(meanLeafUtil))
    meanLeafUtil = 0;
  if (isNaN(meanLeafLoss))
    meanLeafLoss = 0;
  meanHostUtil = totalHostUtil / (g_simTime * g_numHost);
  meanHostLoss = totalHostLoss / (g_simTime * g_numHost);
  if (isNaN(meanHostUtil))
    meanHostUtil = 0;
  if (isNaN(meanHostLoss))
    meanHostLoss = 0;
  *os << "SpineUtil [!" << meanSpineUtil << "!]\nSpineLoss [@" << meanSpineLoss 
      << "$]\nLeafUtil [%" << meanLeafUtil << "%]\nLeafLoss [^" << meanLeafLoss
      << "^]\nHostUtil [&" << meanHostUtil << "&]\nHostLoss [*" << meanHostLoss << "*]" << endl;
  SimFooterWritter(stream);
}

void
SimResultHeaderGenerator()
{
  Ptr<OutputStreamWrapper> streamSimParam = Create<OutputStreamWrapper>(SetupSimFileName("RESULT"),
      std::ios::out | std::ios::app);
  SimHeaderWritter(streamSimParam);
}

void
SimResultFooterGenerator()
{
  Ptr<OutputStreamWrapper> streamSimParam = Create<OutputStreamWrapper>(SetupSimFileName("RESULT"),
      std::ios::out | std::ios::app);
  SimFooterWritter(streamSimParam);
}


bool
SetShortFlowSize(std::string input)
{
  g_flowSize = atoi(input.c_str()) * 1000;
  cout << "FlowSize    : " << g_flowSize << endl;
  return true;
}

bool
SetSimTime(std::string input)
{
  cout << "SimDuration      : " << g_simTime << " -> " << input << endl;
  g_simTime = atoi(input.c_str());
  return true;
}

bool
SetTrafficMatrix(std::string input)
{
  cout << "TrafficMatrix    : " << GetKeyFromValueTM(g_trafficMatrix) << " -> " << input << endl;
  if (stringToTrafficMatrix.count(input) != 0)
    {
      g_trafficMatrix = stringToTrafficMatrix[input];
    }
  else
    NS_FATAL_ERROR("Input for setting up traffic matrix has spelling issue - try again!");
  return true;
}


bool
SetSimInstance(std::string input)
{
  cout << "SimInstance      : " << g_simInstance << " -> " << input << endl;
  g_simInstance = input;
  return true;
}

bool
SetRTO(std::string input)
{
  cout << "RTO              : " << g_minRTO << " -> " << input << endl;
  g_minRTO = atoi(input.c_str());
  return true;
}

bool
SetSimName(std::string input)
{
  cout << "SimName          : " << g_simName << " -> " << input << endl;
  g_simName = input;
  return true;
}

bool
SetIW(std::string input)
{
  cout << "InitalCWND       : " << g_initialCWND << " -> " << input << endl;
  g_initialCWND = atoi(input.c_str());
  return true;
}



bool
SetFST(std::string input)
{
  cout << "SetFST          : " << g_flowStartTime << " -> " << input << endl;
  g_flowStartTime = atof(input.c_str());
  return true;
}

bool
SetHostNumber(std::string input){
  cout << "Host Number    : " << g_numHost << " -> " << input << endl;
  g_numHost = atoi(input.c_str());
  return true;
}

bool
SetFlowNumber(std::string input){
  cout << "Flow Number    : " << g_numFlow << " -> " << input << endl;
  g_numFlow = atoi(input.c_str());
  return true;
}

void printNodeAddr(Ptr<Node> tmp)
{
  Ptr<Ipv4> ipv4Src = tmp->GetObject<Ipv4>();
  Ipv4InterfaceAddress ipv4InterfaceAddressSrc = ipv4Src->GetAddress(1, 0);
  Ipv4Address ipv4AddressSrc = ipv4InterfaceAddressSrc.GetLocal();
  ipv4AddressSrc.Print(cout);
  cout << endl;
}



// All to one
void
FlowConfigForOneMapOne(const NodeContainer *host)
{    
  for (uint32_t j = 0; j < g_numFlow; j++)
  {  // dst setup
    Ptr<Node> dstNode = host[1].Get(j);
    Ptr<Ipv4> ipv4Dst = dstNode->GetObject<Ipv4>();
    Ipv4InterfaceAddress ipv4InterfaceAddressDst = ipv4Dst->GetAddress(1, 0);
    Ipv4Address ipv4AddressDst = ipv4InterfaceAddressDst.GetLocal();
    cout << "DST:";
    ipv4AddressDst.Print(cout);
    cout << endl;

    Ptr<Node> srcNode = host[0].Get(j);
    Ptr<Ipv4> ipv4Src = srcNode->GetObject<Ipv4>();
    Ipv4InterfaceAddress ipv4InterfaceAddressSrc = ipv4Src->GetAddress(1, 0);
    Ipv4Address ipv4AddressSrc = ipv4InterfaceAddressSrc.GetLocal();

    cout << "SRC:";
    ipv4AddressSrc.Print(cout);
    cout << endl;

    // Assign flowId
    int flowId = srcNode->GetNApplications();

    // Source
    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address(ipv4AddressDst), PORT));
    source.SetAttribute("MaxBytes", UintegerValue(g_flowSize));
    source.SetAttribute("FlowId", UintegerValue(flowId));
    string tempString = SetupSimFileName("RESULT");
    source.SetAttribute("OutputFileName", StringValue(tempString));

    ApplicationContainer sourceApps = source.Install(srcNode);
    sourceApps.Start (Seconds(0.0));
    sourceApps.Stop (Seconds(g_simTime));

    sourceShortFlowApps.push_back(sourceApps);
    cout << "{" << GetKeyFromValueTM(g_trafficMatrix) << "} StartNow: "
                  << Simulator::Now().GetSeconds() << " No." << j << endl;
    }
}

// Main
int
main(int argc, char *argv[])
{
  SetupStringToTM(); // Should be done before cmd parsing
  SetSimStartTime();

  // Enable log components
  LogComponentEnable("letflow-debug", LOG_ALL);
  LogComponentEnable("LetFlow", LOG_ALL);
  LogComponentEnable("Ipv4GlobalRouting", LOG_LEVEL_INFO);
  //LogComponentEnable("TcpNewReno", LOG_LEVEL_INFO);
  /*
  LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_INFO);
*/

  // Set up command line parameters
  CommandLine cmd;
  cmd.AddValue("sfs", "Short Flow Size", MakeCallback(SetShortFlowSize));
  cmd.AddValue("st", "Simulation Time", MakeCallback(SetSimTime));
  cmd.AddValue("tm", "Set traffic matrix", MakeCallback(SetTrafficMatrix));
  cmd.AddValue("i", "Set simulation instance number as a string", MakeCallback(SetSimInstance));
  cmd.AddValue("rto", "Set minRTO", MakeCallback(SetRTO));
  cmd.AddValue("sim", "Set sim name", MakeCallback(SetSimName));
  cmd.AddValue("iw", "Set initial cwnd of initial subflow of MPTCP", MakeCallback(SetIW));
  cmd.AddValue("hn", "Number of hosts in Incast scenrio", MakeCallback(SetHostNumber));
  cmd.AddValue("fn", "Number of flows in Incast scenrio", MakeCallback(SetFlowNumber));
  

  cmd.Parse(argc, argv);

  // Set up default simulation parameters
  Config::SetDefault("ns3::Ipv4GlobalRouting::LetFlowRouting", BooleanValue(true));
  Config::SetDefault("ns3::LetFlow::TimeInterval", TimeValue (MicroSeconds (150)));
  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(g_segmentSize));
  Config::SetDefault("ns3::DropTailQueue::Mode", StringValue("QUEUE_MODE_PACKETS"));
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
  Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(g_initialCWND));
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (0)); // MPTCP hasn't implemented delayed ack now.
 
  g_seed = static_cast<uint32_t>(atoi(g_simInstance.c_str()));
  cout << "Seed             : " << g_seed << endl;
  srand(g_seed);

  SimResultHeaderGenerator();

  cout << endl;
  cout << "Node num:" 
  << " g_numSpine:" << g_numSpine 
  << " g_numHost:" << g_numHost 
  << " g_numLeaf:" << g_numLeaf << endl;


  PrintSimParams();

  InternetStackHelper internet;

// ------------------ Topology Construction ------------------
  NodeContainer allHosts;

// Host Layer Nodes
  NodeContainer host[g_numLeaf]; // NodeContainer for hosts

  for (uint32_t j = 0; j < g_numLeaf; j++)
    { // host[g_numLeaf]
      host[j].Create(g_numHost); 
      internet.Install(host[j]);
      allHosts.Add(host[j]);     // Add all server to GlobalHostContainer
      host_c.Add(host[j]);       // Add all server to Host_c for link utilization
    }

// Access layer Nodes
  NodeContainer leaf;          // NodeContainer for leaf switches
  leaf.Create(g_numLeaf);
  internet.Install(leaf);
  leaf_c.Add(leaf);

// Core Layer Nodes
  NodeContainer spine;       // NodeContainer for core switches
  spine.Create(g_numSpine);
  internet.Install(spine);
  spine_c.Add(spine);
// -----------------------------------------------------------
  PointToPointHelper p2p;

  p2p.SetQueue ("ns3::DropTailQueue",
                "MaxPackets", UintegerValue (g_downLinkQueueLimit));
  p2p.SetDeviceAttribute ("DataRate", StringValue (g_downLinkCapacity));
  p2p.SetChannelAttribute ("Delay", StringValue (g_linkDelay));
  

  Ipv4AddressHelper ipv4Address;
  LetFlowHelper letflowhelper;

//=========== Connect hosts to leafs ===========//
  NetDeviceContainer hostToLeafNetDevice[g_numLeaf][g_numHost];
  stringstream ss;

  for (uint32_t t = 0; t < g_numLeaf; t++)
    {
      ss.str("");
      ss << "10." << t << ".0." << "0";
      string tmp = ss.str();
      const char* address = tmp.c_str();
      ipv4Address.SetBase(address, "255.255.255.0");
      
      letflowhelper.Install (leaf.Get(t));
      for (uint32_t h = 0; h < g_numHost; h++)
        {
          hostToLeafNetDevice[t][h] = p2p.Install(NodeContainer(host[t].Get(h), leaf.Get(t)));
          ipv4Address.Assign(hostToLeafNetDevice[t][h]);
          Ipv4Address newNet = ipv4Address.NewNetwork();
          NS_LOG_UNCOND("Leaf:" << t << " IP:" << newNet);
        }
    }
  NS_LOG_INFO("Finished connecting leaf switches and hosts");

  p2p.SetQueue ("ns3::DropTailQueue",
                  "MaxPackets", UintegerValue (g_upLinkQueueLimit));
   p2p.SetDeviceAttribute ("DataRate", StringValue (g_upLinkCapacity));
//=========== Connect core switches to aggregate switches ===========//
  NetDeviceContainer ct[g_numSpine][g_numLeaf];
  ipv4Address.SetBase("30.30.0.0", "255.255.255.0");

  for (uint32_t c = 0; c < g_numSpine; c++)
    {
      for (uint32_t p = 0; p < g_numLeaf; p++)
        {
          ct[c][p] = p2p.Install(spine.Get(c), leaf.Get(p));
          ipv4Address.Assign(ct[c][p]);
          Ipv4Address newNet = ipv4Address.NewNetwork();
          NS_LOG_UNCOND(newNet);
        }
    }
  NS_LOG_INFO("Finished connecting core and aggregation");
  //p2p.EnablePcapAll(SetupSimFileName("PCAP"));

  // Populate Global Routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  SetupTraces(Spine_Leaf_Host);

  //=========== Initialize settings for On/Off Application ===========//
  // SINK application - It would be closed doubled the time of source closure!
  //NS_LOG_INFO("\nSink App Install on following nodes: ");
  for (uint32_t i = 0; i < allHosts.GetN(); i++)
    {
      PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), PORT));
      ApplicationContainer tmp = sink.Install(allHosts.Get(i));
      tmp.Start(Seconds(0.0));
      tmp.Stop(Seconds(g_simTime));
      sinkApps.push_back(tmp);
    }


  if (g_trafficMatrix == ONE_MAP_ONE)
      Simulator::Schedule(Seconds(g_flowStartTime), &FlowConfigForOneMapOne, host);


  SimTimeMonitor();

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop(Seconds(g_simTime));
  Simulator::Run();

  cout << Simulator::Now().GetSeconds() << " -> Generate Out puts"<< endl;

  OutPutSpine();
  OutPutLeaf();
  OutPutHost();
 
  SimOverallResultWritter();
  SimResultFooterGenerator(); // OveralResultWritter should be called first!
  //-----------------------------------------------------------------------//
  Simulator::Destroy();
  NS_LOG_INFO ("Done.");
  cout << Simulator::Now().GetSeconds() << " END "<< endl;
  return 0;
}



