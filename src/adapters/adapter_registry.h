#pragma once

#include "adapters/device_adapter.h"

#include <memory>
#include <vector>

class AdapterRegistry final
{
public:
    AdapterRegistry();

    // Adapters are ordered by registration. IDs must be unique so that a
    // vendor integration cannot silently shadow another adapter.
    bool registerAdapter(std::unique_ptr<DeviceAdapter> adapter);
    const DeviceAdapter *adapterFor(const DeviceRecord &device) const;
    QStringList supportedReadOperations(const DeviceRecord &device) const;

private:
    std::vector<std::unique_ptr<DeviceAdapter>> adapters_;
};
