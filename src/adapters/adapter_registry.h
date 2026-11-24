#pragma once

#include "adapters/device_adapter.h"

#include <memory>
#include <vector>

class AdapterRegistry final
{
public:
    AdapterRegistry();
    const DeviceAdapter *adapterFor(const DeviceRecord &device) const;
    QStringList supportedReadOperations(const DeviceRecord &device) const;
    QStringList implementedWriteOperations(const DeviceRecord &device) const;

private:
    std::vector<std::unique_ptr<DeviceAdapter>> adapters_;
};
