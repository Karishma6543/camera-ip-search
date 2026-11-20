#include "adapter_registry.h"

#include "adapters/onvif_adapter.h"

AdapterRegistry::AdapterRegistry()
{
    adapters_.push_back(std::make_unique<OnvifAdapter>());
}

const DeviceAdapter *AdapterRegistry::adapterFor(const DeviceRecord &device) const
{
    for (const auto &adapter : adapters_)
        if (adapter->canHandle(device)) return adapter.get();
    return nullptr;
}

QStringList AdapterRegistry::supportedReadOperations(const DeviceRecord &device) const
{
    const DeviceAdapter *adapter = adapterFor(device);
    return adapter ? adapter->verifiedReadOperations() : QStringList{};
}
