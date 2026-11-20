#pragma once

#include <QSet>
#include <QString>
#include <QStringList>

namespace IpAssignmentPlanner {

// Produces a dry-run sequential IPv4 plan. It never sends network requests.
// Existing/reserved addresses are skipped; network and broadcast addresses are
// never assigned. The caller must still confirm device-specific write support.
bool buildRangePlan(const QString &startAddress, const QString &endAddress,
                    const QString &subnetMask, int deviceCount,
                    const QSet<QString> &reservedAddresses,
                    QStringList *assignments, QString *error = nullptr);

} // namespace IpAssignmentPlanner


// Associates a dry-run address plan with stable inventory IDs. All inventory
// addresses are reserved, including addresses belonging to selected devices:
// this avoids assigning an address that is still occupied during a staged
// migration. Live ARP/ping conflict detection remains the caller's job.
struct IpAssignmentTarget
{
    QString deviceId;
    QString currentAddress;
    QString proposedAddress;
};

namespace IpAssignmentPlanner {
bool buildDevicePlan(const QString &startAddress, const QString &endAddress,
                     const QString &subnetMask,
                     const QList<IpAssignmentTarget> &selectedDevices,
                     const QSet<QString> &additionalReservedAddresses,
                     QList<IpAssignmentTarget> *plan,
                     QString *error = nullptr);
}
