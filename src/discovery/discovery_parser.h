#pragma once

#include "core/device_record.h"

#include <QHostAddress>
#include <QByteArray>
#include <QString>

namespace DiscoveryParser {

bool parseProjectResponse(const QByteArray &payload, quint32 expectedSequence,
                          const QHostAddress &sender, const QString &interfaceName,
                          DeviceRecord *device);

bool parseOnvifProbeMatch(const QByteArray &payload, const QHostAddress &sender,
                          const QString &interfaceName, DeviceRecord *device);

}
