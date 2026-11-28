#pragma once

#include "adapters/initialization_adapter.h"

#include <memory>
#include <vector>

// Keeps camera activation adapters separate from read/configuration adapters.
// An ambiguous match is deliberately not resolved by registration order.
class InitializationAdapterRegistry final
{
public:
    bool registerAdapter(std::unique_ptr<CameraInitializationAdapter> adapter);
    std::vector<const CameraInitializationAdapter *> matchingAdapters(
        const DeviceRecord &device) const;
    const CameraInitializationAdapter *adapterFor(const DeviceRecord &device) const;
    QStringList registeredAdapterIds() const;

private:
    std::vector<std::unique_ptr<CameraInitializationAdapter>> adapters_;
};
