#pragma once

#include <QString>
#include <QStringList>

// Capability is explicit evidence, not an inference from a manufacturer name.
struct DeviceCapability
{
    QString id;              // stable key, e.g. onvif.device_information
    QString displayName;
    QString protocol;
    QString endpoint;
    bool supported{false};
    bool verified{false};
    QString evidence;
    QString limitation;
};
