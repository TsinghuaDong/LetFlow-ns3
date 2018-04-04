/*
 * LetFlow implementation.
 * Enhuan Dong
 * Email: deh13@mails.tsinghua.edu.cn
 */
#ifndef LET_FLOW_H
#define LET_FLOW_H

#include "ns3/object.h"
#include <stdint.h>
#include <string>
#include <list>
#include "ns3/object-factory.h"
#include "ns3/address.h"
#include "ns3/attribute.h"
#include "ns3/net-device.h"
#include "ns3/node-container.h"
#include "ns3/ipv4-header.h"
#include "ns3/nstime.h"

using namespace std;
namespace ns3
{

class FlowLetEntry
{
public:
  FlowLetEntry(void); 
  bool operator==(const FlowLetEntry &fle) const;

public:
  uint32_t m_hashValue; // 
  uint32_t m_selectIndex; // 
  bool m_valid;
  bool m_age;
  
};

class LetFlow : public Object
{
public:
  static TypeId GetTypeId(void);
  
  LetFlow();
  virtual ~LetFlow();
  void SetNode (Ptr<Node> node);
  uint32_t ChooseRoute (uint32_t routeNum, const Ipv4Header &header, Ptr<const Packet> ipPayload);
  void SetTimer ();
  void CheckFlowLetTable();
  
private:
  Ptr<Node> m_node;
  list<FlowLetEntry> m_flowLetTable;
  Hasher m_hasher;
  
  Time m_timeInterval;
};

}   //namespace ns3

#endif /* LET_FLOW_H */
