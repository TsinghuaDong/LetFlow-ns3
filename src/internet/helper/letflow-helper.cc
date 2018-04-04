/*
 * LetFlow implementation.
 * Enhuan Dong
 * Email: deh13@mails.tsinghua.edu.cn
 */

#include "ns3/letflow-helper.h"
#include "ns3/log.h"
#include "ns3/ipv4-header.h"

NS_LOG_COMPONENT_DEFINE ("LetFlowHelper");
using namespace std;
namespace ns3
{


LetFlowHelper::LetFlowHelper()
{
  
}

void
LetFlowHelper::CreateAndAggregateObjectFromTypeId (Ptr<Node> node, const std::string typeId)
{
  ObjectFactory factory;
  factory.SetTypeId (typeId);
  Ptr<Object> obj = factory.Create <Object> ();
  node->AggregateObject (obj);
}

void
LetFlowHelper::Install(Ptr<Node> node)
{
  NS_LOG_FUNCTION (node->GetId());
  CreateAndAggregateObjectFromTypeId (node, "ns3::LetFlow");
  Ptr<LetFlow> letFlow = node->GetObject<LetFlow> ();
  letFlow->SetNode (node);
  letFlow->SetTimer ();
}


}//namespace ns3
