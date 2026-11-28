#include "initialization_adapter_registry.h"

#include <utility>

bool InitializationAdapterRegistry::registerAdapter(
    std::unique_ptr<CameraInitializationAdapter> adapter)
{
    if (!adapter)
        return false;

    const QString id = adapter->id().trimmed();
    if (id.isEmpty())
        return false;

    for (const auto &registered : adapters_) {
        if (registered->id().trimmed().compare(id, Qt::CaseInsensitive) == 0)
            return false;
    }

    adapters_.push_back(std::move(adapter));
    return true;
}

std::vector<const CameraInitializationAdapter *>
InitializationAdapterRegistry::matchingAdapters(const DeviceRecord &device) const
{
    std::vector<const CameraInitializationAdapter *> matches;
    for (const auto &adapter : adapters_) {
        if (adapter->canHandle(device))
            matches.push_back(adapter.get());
    }
    return matches;
}

const CameraInitializationAdapter *
InitializationAdapterRegistry::adapterFor(const DeviceRecord &device) const
{
    const auto matches = matchingAdapters(device);
    return matches.size() == 1 ? matches.front() : nullptr;
}

QStringList InitializationAdapterRegistry::registeredAdapterIds() const
{
    QStringList ids;
    for (const auto &adapter : adapters_)
        ids.push_back(adapter->id());
    return ids;
}
