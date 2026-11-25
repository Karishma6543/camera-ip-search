#pragma once

#include "core/device_record.h"

#include <QByteArray>
#include <QHostAddress>

class SsdpParser final
{
public:
    // Parses an SSDP discovery response without dereferencing its LOCATION URL.
    // Only the response sender is used as the network address.
    static bool parseResponse(const QByteArray &payload,
                              const QHostAddress &sender,
                              const QString &interfaceName,
                              DeviceRecord *device);
};
