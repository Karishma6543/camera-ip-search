#pragma once

#include "adapters/device_adapter.h"

class OnvifAdapter final : public DeviceAdapter
{
public:
    QString id() const override;
    QString displayName() const override;
    bool canHandle(const DeviceRecord &device) const override;
    QStringList verifiedReadOperations() const override;
};
