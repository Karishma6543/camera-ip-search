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
