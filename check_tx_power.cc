#include "ns3/core-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;

int main() {
    TypeId tid = TypeId::LookupByName("ns3::WifiPhy");
    for (uint32_t i = 0; i < tid.GetAttributeN(); ++i) {
        struct TypeId::AttributeInformation info = tid.GetAttribute(i);
        if (info.name == "TxPowerStart" || info.name == "TxPowerEnd" || info.name == "TxGain" || info.name == "RxGain") {
            std::cout << info.name << std::endl;
        }
    }
    return 0;
}
