#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

// A single piece of evidence about a device. Observations are retained separately
// from the merged inventory record so discovery sources are not conflated.
struct DeviceObservation
{
    QString deviceKey;
    QString source;          // e.g. ONVIF, Project UDP, subnet TCP, manual
    QString protocol;        // e.g. SOAP/HTTP, UDP, TCP
    QString endpoint;
    QString observedAddress;
    QString summary;
    QDateTime observedAt;
    QStringList evidence;
};
