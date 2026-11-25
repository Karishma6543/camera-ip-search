#include "device_classifier.h"

#include <QSet>

namespace {
QString normalized(const QString &value)
{
    return value.trimmed().toLower();
}

bool hasAny(const QString &value, const QStringList &needles)
{
    const QString text = normalized(value);
    for (const QString &needle : needles)
        if (text.contains(needle)) return true;
    return false;
}

bool hasPort(const QSet<QString> &ports, std::initializer_list<const char *> values)
{
    for (const char *value : values)
        if (ports.contains(QString::fromLatin1(value))) return true;
    return false;
}
}

QString DeviceClassifier::Result::key() const
{
    switch (type) {
    case Type::Camera: return QStringLiteral("camera");
    case Type::Nvr: return QStringLiteral("nvr");
    case Type::Router: return QStringLiteral("router");
    case Type::Switch: return QStringLiteral("switch");
    case Type::Printer: return QStringLiteral("printer");
    case Type::Computer: return QStringLiteral("computer");
    case Type::Unknown: return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

QString DeviceClassifier::Result::confidenceLabel() const
{
    switch (confidence) {
    case Confidence::Verified: return QStringLiteral("Verified");
    case Confidence::High: return QStringLiteral("High");
    case Confidence::Probable: return QStringLiteral("Probable");
    case Confidence::Unverified: return QStringLiteral("Unverified");
    }
    return QStringLiteral("Unverified");
}

QString DeviceClassifier::Result::displayLabel() const
{
    switch (type) {
    case Type::Camera: return QStringLiteral("Camera");
    case Type::Nvr: return QStringLiteral("NVR");
    case Type::Router: return QStringLiteral("Router");
    case Type::Switch: return QStringLiteral("Switch");
    case Type::Printer: return QStringLiteral("Printer");
    case Type::Computer: return QStringLiteral("Computer");
    case Type::Unknown: return QStringLiteral("Unknown");
    }
    return QStringLiteral("Unknown");
}

DeviceClassifier::Result DeviceClassifier::classify(const DeviceRecord &device)
{
    const QString identity = normalized(device.manufacturer + QLatin1Char(' ') + device.model +
                                        QLatin1Char(' ') + device.hardwareId);
    QSet<QString> ports;
    for (const QString &port : device.openPorts)
        ports.insert(port.trimmed());

    const bool onvif = device.discoverySources.contains(QStringLiteral("ONVIF")) ||
        device.discoverySources.contains(QStringLiteral("ONVIF WS-Discovery")) ||
        device.discoverySources.contains(QStringLiteral("ONVIF GetDeviceInformation"));
    const bool oxita = normalized(device.manufacturer) == QStringLiteral("oxita") ||
        device.discoverySources.contains(QStringLiteral("Oxita")) ||
        device.discoverySources.contains(QStringLiteral("Oxita UDP discovery")) ||
        device.discoverySources.contains(QStringLiteral("Project UDP 9988"));

    const bool ssdp = device.discoverySources.contains(QStringLiteral("SSDP/UPnP"));
    if (ssdp && hasAny(identity, {QStringLiteral("internetgatewaydevice"),
                                  QStringLiteral("wanconnectiondevice"),
                                  QStringLiteral("wandevice")})) {
        return {Type::Router, Confidence::High,
                QStringLiteral("SSDP identifies an Internet Gateway Device")};
    }
    if (ssdp && hasAny(identity, {QStringLiteral(":printer"),
                                  QStringLiteral("urn:schemas-upnp-org:device:printer")})) {
        return {Type::Printer, Confidence::High,
                QStringLiteral("SSDP identifies a UPnP printer")};
    }

    if (hasAny(identity, {QStringLiteral("nvr"), QStringLiteral("dvr"),
                          QStringLiteral("xvr"), QStringLiteral("video recorder"),
                          QStringLiteral("network video recorder")})) {
        return {Type::Nvr, Confidence::High,
                QStringLiteral("Manufacturer or model identifies a video recorder")};
    }
    if (onvif || oxita) {
        return {Type::Camera, Confidence::Verified,
                onvif ? QStringLiteral("Identified through ONVIF discovery")
                      : QStringLiteral("Identified through Oxita device discovery")};
    }
    if (hasAny(identity, {QStringLiteral("printer"), QStringLiteral("laserjet"),
                          QStringLiteral("deskjet"), QStringLiteral("officejet"),
                          QStringLiteral("imageclass"), QStringLiteral("workcentre")})) {
        return {Type::Printer, Confidence::High,
                QStringLiteral("Manufacturer or model identifies a printer")};
    }
    if (hasAny(identity, {QStringLiteral("wireless router"), QStringLiteral("router"),
                          QStringLiteral("gateway"), QStringLiteral("firewall")})) {
        return {Type::Router, Confidence::High,
                QStringLiteral("Manufacturer or model identifies a router or gateway")};
    }
    if (hasAny(identity, {QStringLiteral("network switch"), QStringLiteral("ethernet switch"),
                          QStringLiteral("poe switch"), QStringLiteral("switch")})) {
        return {Type::Switch, Confidence::High,
                QStringLiteral("Manufacturer or model identifies a network switch")};
    }
    if (hasAny(identity, {QStringLiteral("workstation"), QStringLiteral("desktop pc"),
                          QStringLiteral("laptop"), QStringLiteral("windows pc")})) {
        return {Type::Computer, Confidence::High,
                QStringLiteral("Manufacturer or model identifies a computer")};
    }

    // Port fingerprints are useful for narrowing the list, not for asserting
    // device identity. Multiple printer-specific ports are stronger evidence.
    const int printerSignals = int(ports.contains(QStringLiteral("9100"))) +
        int(ports.contains(QStringLiteral("631"))) +
        int(ports.contains(QStringLiteral("515")));
    if (printerSignals >= 2)
        return {Type::Printer, Confidence::Probable,
                QStringLiteral("Multiple printer service ports responded")};
    if (hasPort(ports, {"9100"}) && hasPort(ports, {"631", "515"}))
        return {Type::Printer, Confidence::Probable,
                QStringLiteral("Raw printing and print service ports responded")};

    // DNS plus a router-management service is a useful gateway fingerprint,
    // but still not sufficient to call an unknown host a confirmed router.
    if (hasPort(ports, {"53"}) && hasPort(ports, {"80", "443", "7547"}))
        return {Type::Router, Confidence::Probable,
                QStringLiteral("DNS and gateway-management service ports responded")};

    if (hasPort(ports, {"445", "3389"}) ||
        (hasPort(ports, {"135"}) && hasPort(ports, {"139"})))
        return {Type::Computer, Confidence::Probable,
                QStringLiteral("File-sharing or remote-desktop service ports responded")};

    if (hasPort(ports, {"554"}) && hasPort(ports, {"80", "443", "8000", "8080"}))
        return {Type::Camera, Confidence::Probable,
                QStringLiteral("RTSP and web-management ports responded; camera identity is unverified")};

    return {Type::Unknown, Confidence::Unverified,
            device.isCandidate
                ? QStringLiteral("Only generic network reachability has been established")
                : QStringLiteral("Not enough reliable identity evidence is available")};
}
