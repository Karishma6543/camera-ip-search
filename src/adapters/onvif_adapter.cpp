#include "onvif_adapter.h"

#include <QUrl>

QString OnvifAdapter::id() const
{
    return QStringLiteral("onvif");
}

QString OnvifAdapter::displayName() const
{
    return QStringLiteral("ONVIF");
}

bool OnvifAdapter::canHandle(const DeviceRecord &device) const
{
    const QUrl endpoint(device.deviceServiceUrl);
    if (!endpoint.isValid() ||
        (endpoint.scheme() != QStringLiteral("http") &&
         endpoint.scheme() != QStringLiteral("https")) ||
        endpoint.host().isEmpty() || !endpoint.userInfo().isEmpty() || !endpoint.query().isEmpty() || !endpoint.fragment().isEmpty())
        return false;

    // The advertised service endpoint must belong to the discovered host.
    return endpoint.host().compare(device.ipAddress, Qt::CaseInsensitive) == 0 &&
           (device.discoverySources.contains(QStringLiteral("ONVIF")) ||
            device.discoverySources.contains(QStringLiteral("ONVIF WS-Discovery")) ||
            !device.onvifEndpoint.isEmpty() ||
            device.discoverySources.contains(QStringLiteral("ONVIF GetDeviceInformation")));
}

QStringList OnvifAdapter::verifiedReadOperations() const
{
    return {
        QStringLiteral("GetDeviceInformation"),
        QStringLiteral("GetCapabilities"),
        QStringLiteral("GetNetworkInterfaces")
    };
}


QStringList OnvifAdapter::implementedWriteOperations() const
{
    return {
        QStringLiteral("SetNetworkInterfaces"),
        QStringLiteral("SetNetworkDefaultGateway"),
        QStringLiteral("SetSystemFactoryDefault")
    };
}
