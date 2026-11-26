#pragma once

#include "core/device_record.h"

#include <QString>
#include <QStringList>

enum class DeviceOperation {
    ReadDeviceInformation,
    ReadCapabilities,
    ReadServices,
    ReadNetworkInterfaces,
    ConfigureStaticIpv4,
    ConfigureDhcp,
    ConfigureDns,
    PreviewStream,
    CaptureSnapshot,
    Reboot,
    ChangePassword,
    UpdateFirmware,
    ConfigureImageSettings,
    ConfigurePtz,
    ManageEvents,
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
        case DeviceOperation::ReadServices:
            return verifiedReadOperations().contains(QStringLiteral("GetServices"));
        case DeviceOperation::ReadNetworkInterfaces:
            return verifiedReadOperations().contains(QStringLiteral("GetNetworkInterfaces"));
        case DeviceOperation::ConfigureStaticIpv4:
            return implementedWriteOperations().contains(QStringLiteral("SetNetworkInterfaces"));
        case DeviceOperation::ConfigureDhcp:
            return implementedWriteOperations().contains(QStringLiteral("SetNetworkInterfaces"));
        case DeviceOperation::ConfigureDns:
            return implementedWriteOperations().contains(QStringLiteral("SetDNS"));
        case DeviceOperation::Reboot:
            return implementedWriteOperations().contains(QStringLiteral("SystemReboot"));
        case DeviceOperation::PreviewStream:
        case DeviceOperation::CaptureSnapshot:
            return false;
        case DeviceOperation::ChangePassword:
        case DeviceOperation::UpdateFirmware:
        case DeviceOperation::ConfigureImageSettings:
        case DeviceOperation::ConfigurePtz:
        case DeviceOperation::ManageEvents:
        case DeviceOperation::ManageNvrChannels:
            return false;
        case DeviceOperation::FactoryReset:
            return implementedWriteOperations().contains(QStringLiteral("SetSystemFactoryDefault"));
        }
        return false;
    }
};
