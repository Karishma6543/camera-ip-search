#include "core/operation_compatibility.h"

#include "adapters/device_adapter.h"
#include "adapters/onvif_adapter.h"

#include <array>

namespace {
struct OperationDefinition {
    const char *label;
    DeviceOperation operation;
    const char *service;
    const char *responseSource;
};

constexpr std::array definitions{
    OperationDefinition{"Read device information", DeviceOperation::ReadDeviceInformation, "Device", "ONVIF GetDeviceInformation"},
    OperationDefinition{"Read capabilities", DeviceOperation::ReadCapabilities, "Device", "ONVIF GetCapabilities"},
    OperationDefinition{"Read ONVIF service inventory", DeviceOperation::ReadServices, "Device", "ONVIF GetServices"},
    OperationDefinition{"Read network interfaces", DeviceOperation::ReadNetworkInterfaces, "Device", "ONVIF GetNetworkInterfaces"},
    OperationDefinition{"Configure static IPv4", DeviceOperation::ConfigureStaticIpv4, "Device", ""},
    OperationDefinition{"Configure DHCP", DeviceOperation::ConfigureDhcp, "Device", ""},
    OperationDefinition{"Configure DNS", DeviceOperation::ConfigureDns, "Device", ""},
    OperationDefinition{"Reboot camera", DeviceOperation::Reboot, "Device", ""},
    OperationDefinition{"Factory reset", DeviceOperation::FactoryReset, "Device", ""},
    OperationDefinition{"Preview stream / RTSP URI", DeviceOperation::PreviewStream, "Media", ""},
    OperationDefinition{"Capture snapshot", DeviceOperation::CaptureSnapshot, "Media", ""},
    OperationDefinition{"Change camera password", DeviceOperation::ChangePassword, "Device", ""},
    OperationDefinition{"Firmware upgrade", DeviceOperation::UpdateFirmware, "Device", ""},
    OperationDefinition{"Image settings", DeviceOperation::ConfigureImageSettings, "Imaging", ""},
    OperationDefinition{"PTZ controls", DeviceOperation::ConfigureImageSettings, "PTZ", ""},
    OperationDefinition{"Events and alarms", DeviceOperation::ManageNvrChannels, "Events", ""},
    OperationDefinition{"NVR channel management", DeviceOperation::ManageNvrChannels, "Recording", ""}
};

bool hasService(const DeviceRecord &device, const QString &service)
{
    const QString expected = QStringLiteral("Service: ") + service;
    for (const QString &capability : device.capabilities)
        if (capability.compare(expected, Qt::CaseInsensitive) == 0)
            return true;
    return false;
}

bool hasSource(const DeviceRecord &device, const QString &source)
{
    return !source.isEmpty() && device.discoverySources.contains(source);
}
}

QVector<OperationCompatibility::Entry> OperationCompatibility::evaluate(const DeviceRecord &device)
{
    QVector<Entry> entries;
    entries.reserve(static_cast<qsizetype>(definitions.size()));
    const OnvifAdapter adapter;
    const bool adapterAvailable = adapter.canHandle(device);

    for (const OperationDefinition &definition : definitions) {
        const QString label = QString::fromLatin1(definition.label);
        const QString service = QString::fromLatin1(definition.service);
        const QString responseSource = QString::fromLatin1(definition.responseSource);
        const bool implemented = adapterAvailable && adapter.supports(definition.operation);
        const bool advertised = hasService(device, service);
        const bool responded = hasSource(device, responseSource);

        Entry entry;
        entry.operation = label;
        if (responded) {
            entry.status = Status::DeviceResponded;
            entry.evidence = QStringLiteral("A successful response was recorded during discovery or refresh.");
        } else if (!implemented) {
            entry.status = advertised ? Status::ServiceAdvertisedNotVerified : Status::NotImplemented;
            entry.evidence = advertised
                ? QStringLiteral("The service is advertised, but this operation is not implemented by the application.")
                : QStringLiteral("This operation is not implemented by the current application adapter.");
        } else if (!responseSource.isEmpty()) {
            entry.status = Status::ImplementedNotTested;
            entry.evidence = QStringLiteral("The application implements this read request, but no successful device response has been recorded.");
        } else if (advertised && responseSource.isEmpty()) {
            entry.status = Status::AuthenticationNotVerified;
            entry.evidence = definition.operation == DeviceOperation::FactoryReset
                ? QStringLiteral("The device advertises the service and the request exists in the adapter, but the UI deliberately keeps factory reset disabled until exact model and firmware support is validated. Authentication and authorization are also unverified.")
                : QStringLiteral("The device advertises the service and the application implements this request, but authentication, authorization and operation success have not been verified.");
        } else {
            entry.status = Status::ServiceNotReported;
            entry.evidence = QStringLiteral("The application has an implementation, but the device has not reported this service or operation.");
        }
        entries.append(entry);
    }
    return entries;
}

QString OperationCompatibility::statusLabel(Status status)
{
    switch (status) {
    case Status::DeviceResponded: return QStringLiteral("Device responded");
    case Status::ImplementedNotTested: return QStringLiteral("Implemented; not tested");
    case Status::ServiceAdvertisedNotVerified: return QStringLiteral("Advertised; not implemented");
    case Status::AuthenticationNotVerified: return QStringLiteral("Authentication / permission not verified");
    case Status::ServiceNotReported: return QStringLiteral("Not reported by device");
    case Status::NotImplemented: return QStringLiteral("Not implemented");
    }
    return QStringLiteral("Unknown");
}
