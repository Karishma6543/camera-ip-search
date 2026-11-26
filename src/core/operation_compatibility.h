#pragma once

#include "core/device_record.h"

#include <QString>
#include <QVector>

namespace OperationCompatibility
{
enum class Status {
    DeviceResponded,
    ImplementedNotTested,
    ServiceAdvertisedNotVerified,
    AuthenticationNotVerified,
    ServiceNotReported,
    NotImplemented
};

struct Entry {
    QString operation;
    Status status{Status::NotImplemented};
    QString evidence;
};

QVector<Entry> evaluate(const DeviceRecord &device);
QString statusLabel(Status status);
}
