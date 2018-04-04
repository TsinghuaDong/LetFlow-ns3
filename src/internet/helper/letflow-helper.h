/*
 * LetFlow implementation.
 * Enhuan Dong
 * Email: deh13@mails.tsinghua.edu.cn
 */
#ifndef LETFLOW_HELPER_H
#define LETFLOW_HELPER_H

#include "ns3/letflow.h"
#include <stdint.h>
#include <string>
#include "ns3/object-factory.h"
#include "ns3/address.h"
#include "ns3/attribute.h"
#include "ns3/net-device.h"
#include "ns3/node-container.h"
#include "ns3/ipv4-header.h"

using namespace std;
namespace ns3
{

class LetFlowHelper
{
public:
  
  LetFlowHelper ();
  void CreateAndAggregateObjectFromTypeId (Ptr<Node> node, const std::string typeId);  
  void Install (Ptr<Node> node);

};


} // namespace ns3

#endif /* LETFLOW_HELPER_H */
