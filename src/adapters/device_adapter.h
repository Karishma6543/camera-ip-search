#pragma once

#include "core/device_record.h"

#include <QString>
#include <QStringList>

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
    ManageNvrChannels,
    FactoryReset
};

class DeviceAdapter
{
public:
    virtual ~DeviceAdapter() = default;
    virtual QString id() const = 0;
    virtual QString displayName() const = 0;
    virtual bool canHandle(const DeviceRecord &device) const = 0;
    virtual QStringList verifiedReadOperations() const = 0;

    // Empty by default: write operations must be explicitly implemented by an adapter.
    virtual QStringList implementedWriteOperations() const { return {}; }

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
            return implementedWriteOperations().contains(QStringLiteral("SetNetworkInterfaces"));
        case DeviceOperation::PreviewStream:
        case DeviceOperation::CaptureSnapshot:
        case DeviceOperation::Reboot:
        case DeviceOperation::ChangePassword:
        case DeviceOperation::UpdateFirmware:
        case DeviceOperation::ConfigureImageSettings:
        case DeviceOperation::ManageNvrChannels:
            return false;
        case DeviceOperation::FactoryReset:
            return implementedWriteOperations().contains(QStringLiteral("SetSystemFactoryDefault"));
        }
        return false;
    }
};
