#pragma once

#include "core/device_record.h"

#include <QString>
#include <QStringList>

// Protocol adapters describe what can be verified for a device. They do not
// perform UI work or imply that unimplemented management commands are safe.
class DeviceAdapter
{
public:
    virtual ~DeviceAdapter() = default;
    virtual QString id() const = 0;
    virtual QString displayName() const = 0;
    virtual bool canHandle(const DeviceRecord &device) const = 0;
    virtual QStringList verifiedReadOperations() const = 0;
    virtual QStringList implementedWriteOperations() const = 0;
};
