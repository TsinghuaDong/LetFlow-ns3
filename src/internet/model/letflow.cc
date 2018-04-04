/*
 * LetFlow implementation.
 * Enhuan Dong
 * Email: deh13@mails.tsinghua.edu.cn
 */

#include "ns3/letflow.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/log.h"
#include "ns3/callback.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-route.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/net-device.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/object-vector.h"
#include "ns3/ipv4-header.h"
#include "ns3/boolean.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/tcp-header.h"

#include "loopback-net-device.h"
#include "arp-l3-protocol.h"
#include "ipv4-l3-protocol.h"
#include "icmpv4-l4-protocol.h"
#include "ipv4-interface.h"
#include "ipv4-raw-socket-impl.h"
#include <algorithm>

NS_LOG_COMPONENT_DEFINE ("LetFlow");
using namespace std;
namespace ns3
{
NS_OBJECT_ENSURE_REGISTERED (LetFlow);

TypeId LetFlow::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::LetFlow")
                      .SetParent<Object> ()
                      .AddConstructor<LetFlow> ()
                      .AddAttribute ("TimeInterval", "Time interval to update the flowlet table.",
                                     TimeValue (MicroSeconds (500)),
                                     MakeTimeAccessor (&LetFlow::m_timeInterval),
                                     MakeTimeChecker ())
                      ;
  return tid;
}

LetFlow::LetFlow()
{
  
}
LetFlow::~LetFlow()
{
  
}

void
LetFlow::SetNode (Ptr<Node> node)
{
  NS_LOG_FUNCTION (this << "Node"<< node->GetId());
  m_node = node;
}

void
LetFlow::CheckFlowLetTable()
{
  NS_LOG_FUNCTION (m_node->GetId());
  list<FlowLetEntry>::iterator it;
  for (it = m_flowLetTable.begin(); it != m_flowLetTable.end(); it ++)
  {
    if (!it->m_age)
    {
      NS_LOG_INFO ("Hash:" << it->m_hashValue << " set age.");
      it->m_age = true;
    }
    else if (it->m_valid)
    {
      NS_LOG_INFO ("Hash:" << it->m_hashValue << " clear valid.");
      it->m_valid = false;
    }
  }
  
  Simulator::ScheduleWithContext(m_node->GetId(), m_timeInterval, &LetFlow::CheckFlowLetTable, this);
}

void
LetFlow::SetTimer ()
{
  NS_LOG_FUNCTION(m_node->GetId());
  Simulator::ScheduleWithContext(m_node->GetId(), Seconds(0.0), &LetFlow::CheckFlowLetTable, this);
}

uint32_t
LetFlow::ChooseRoute (uint32_t routeNum, const Ipv4Header &header, Ptr<const Packet> ipPayload)
{
  NS_LOG_FUNCTION (routeNum);
  
  m_hasher.clear();  
  TcpHeader tcpHeader;
  ipPayload->PeekHeader(tcpHeader);
  ostringstream oss;
  oss << tcpHeader.GetDestinationPort() << tcpHeader.GetSourcePort() << header.GetSource() << header.GetDestination() <<header.GetProtocol();
  std::string data = oss.str();
  uint32_t hash = m_hasher.GetHash32(data);
  
  NS_LOG_INFO (header.GetDestination() << " Hash:" << hash);
  
  FlowLetEntry tmpFle;
  tmpFle.m_hashValue = hash;
  list<FlowLetEntry>::iterator it = find(m_flowLetTable.begin(), m_flowLetTable.end(), tmpFle);
  
  if (it != m_flowLetTable.end())
  {
    if (it->m_valid)
    {
      it->m_age = false;
      NS_LOG_INFO ("Found hash:" << hash << " Valid to:" << it->m_selectIndex);
      return it->m_selectIndex;
    }
    else
    {
      uint32_t selected = rand () % routeNum;
      it->m_age = false;
      it->m_valid = true;
      it->m_selectIndex = selected;
      NS_LOG_INFO ("Found hash:" << hash << " Not Valid to:" << selected);
      return selected;
    }
  }
  else 
  {
    uint32_t selected = rand () % routeNum;
    tmpFle.m_age = false;
    tmpFle.m_valid = true;
    tmpFle.m_selectIndex = selected;
    m_flowLetTable.push_back(tmpFle);
    NS_LOG_INFO ("Not Found hash:" << hash << " New to:" << selected);
    return selected;
  }
  
  return 0;
}

FlowLetEntry::FlowLetEntry()
{
  m_hashValue = 0; // 
  m_selectIndex = 0; // 
  m_valid = false;
  m_age = true;
}

bool FlowLetEntry::operator==(const FlowLetEntry &fle) const
{
  return (m_hashValue == fle.m_hashValue);
}


}//namespace ns3
