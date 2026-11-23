#pragma once

#include "device_record.h"

#include <QHash>
#include <QString>

namespace DeviceIdentity {
// Returns the existing inventory key when two observations can be tied to the
// same physical device. An IP match is deliberately weak and is rejected when
// both observations already carry different MAC addresses.
QString findMatchingKey(const QHash<QString, DeviceRecord> &devices,
                        const DeviceRecord &observation);

// Merge only additive or missing information. Verified observations may
// refresh an address; candidate scans may only contribute candidate ports.
DeviceRecord mergeObservations(const DeviceRecord &existing,
                               const DeviceRecord &observation);
}
