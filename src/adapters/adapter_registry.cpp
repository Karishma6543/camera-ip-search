#include "adapter_registry.h"

#include "adapters/onvif_adapter.h"

AdapterRegistry::AdapterRegistry()
{
    registerAdapter(std::make_unique<OnvifAdapter>());
}

bool AdapterRegistry::registerAdapter(std::unique_ptr<DeviceAdapter> adapter)
{
    if (!adapter || adapter->id().trimmed().isEmpty())
        return false;

    for (const auto &registered : adapters_) {
        if (registered->id().compare(adapter->id(), Qt::CaseInsensitive) == 0)
            return false;
    }

    adapters_.push_back(std::move(adapter));
    return true;
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
