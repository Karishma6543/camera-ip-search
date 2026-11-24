#pragma once

#include "core/device_record.h"

#include <QString>
#include <QStringList>

// Operations are deliberately explicit: discovering a device does not imply
// that management operations are supported for its model or firmware.
enum class DeviceOperation {
    ReadDeviceInformation,
    ReadCapabilities,
    ReadNetworkInterfaces,
    ConfigureStaticIpv4,
    PreviewStream,
    CaptureSnapshot,
    Reboot,
    ChangePassword,
    UpdateFirmware,
    ConfigureImageSettings,
    ManageNvrChannels
};

class DeviceAdapter
{
public:
    virtual ~DeviceAdapter() = default;
    virtual QString id() const = 0;
    virtual QString displayName() const = 0;
    virtual bool canHandle(const DeviceRecord &device) const = 0;
    virtual QStringList verifiedReadOperations() const = 0;

    // Conservative default: only operations represented by the adapter's
    // verified read-operation list are reported as supported.
    virtual bool supports(DeviceOperation operation) const
    {
        switch (operation) {
        case DeviceOperation::ReadDeviceInformation:
            return verifiedReadOperations().contains(QStringLiteral("GetDeviceInformation"));
        case DeviceOperation::ReadCapabilities:
            return verifiedReadOperations().contains(QStringLiteral("GetCapabilities"));
        case DeviceOperation::ReadNetworkInterfaces:
            return verifiedReadOperations().contains(QStringLiteral("GetNetworkInterfaces"));
        case DeviceOperation::ConfigureStaticIpv4:
        case DeviceOperation::PreviewStream:
        case DeviceOperation::CaptureSnapshot:
        case DeviceOperation::Reboot:
        case DeviceOperation::ChangePassword:
        case DeviceOperation::UpdateFirmware:
        case DeviceOperation::ConfigureImageSettings:
        case DeviceOperation::ManageNvrChannels:
            return false;
        }
        return false;
    }
};
