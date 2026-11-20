#include "ip_assignment_planner.h"

#include <QHostAddress>
#include <QtGlobal>

namespace {
void setError(QString *error, const QString &message)
{
    if (error) *error = message;
}

bool parseIpv4(const QString &text, quint32 *value)
{
    QHostAddress address;
    if (!address.setAddress(text.trimmed()) ||
        address.protocol() != QAbstractSocket::IPv4Protocol)
        return false;
    if (value) *value = address.toIPv4Address();
    return true;
}

bool contiguousMask(quint32 mask)
{
    // A valid IPv4 netmask has all one bits followed by all zero bits.
    const quint32 inverse = ~mask;
    return (inverse & (inverse + 1U)) == 0U && mask != 0U;
}
}

bool IpAssignmentPlanner::buildRangePlan(const QString &startAddress,
                                         const QString &endAddress,
                                         const QString &subnetMask,
                                         int deviceCount,
                                         const QSet<QString> &reservedAddresses,
                                         QStringList *assignments,
                                         QString *error)
{
    if (assignments) assignments->clear();
    if (!assignments || deviceCount < 1 || deviceCount > 256) {
        setError(error, QStringLiteral("Provide an output list and a device count between 1 and 256."));
        return false;
    }

    quint32 start = 0, end = 0, mask = 0;
    if (!parseIpv4(startAddress, &start) || !parseIpv4(endAddress, &end) ||
        !parseIpv4(subnetMask, &mask)) {
        setError(error, QStringLiteral("Start, end and subnet mask must be valid IPv4 values."));
        return false;
    }
    if (!contiguousMask(mask)) {
        setError(error, QStringLiteral("Subnet mask must be contiguous."));
        return false;
    }
    if (start > end) {
        setError(error, QStringLiteral("Range start must not be greater than range end."));
        return false;
    }

    const quint32 network = start & mask;
    const quint32 broadcast = network | ~mask;
    if ((end & mask) != network) {
        setError(error, QStringLiteral("The complete assignment range must stay within one subnet."));
        return false;
    }
    if (start <= network || end >= broadcast) {
        setError(error, QStringLiteral("Network and broadcast addresses cannot be assigned."));
        return false;
    }

    QSet<quint32> reserved;
    for (const QString &address : reservedAddresses) {
        quint32 value = 0;
        if (!parseIpv4(address, &value)) {
            setError(error, QStringLiteral("Reserved address list contains an invalid IPv4 address."));
            return false;
        }
        reserved.insert(value);
    }

    QStringList plan;
    for (quint32 current = start; current <= end && plan.size() < deviceCount; ++current) {
        if (!reserved.contains(current))
            plan.append(QHostAddress(current).toString());
        if (current == 0xFFFFFFFFU) break;
    }
    if (plan.size() != deviceCount) {
        setError(error, QStringLiteral("Range has only %1 available address(es) after reserved addresses; %2 required.")
                           .arg(plan.size()).arg(deviceCount));
        return false;
    }

    *assignments = plan;
    if (error) error->clear();
    return true;
}
