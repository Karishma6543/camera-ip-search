#pragma once

#include "core/device_record.h"

#include <QString>

class DeviceClassifier final
{
public:
    enum class Type {
        Camera,
        Nvr,
        Router,
        Switch,
        Printer,
        Computer,
        Unknown
    };

    enum class Confidence {
        Verified,
        High,
        Probable,
        Unverified
    };

    struct Result {
        Type type{Type::Unknown};
        Confidence confidence{Confidence::Unverified};
        QString reason;

        QString key() const;
        QString confidenceLabel() const;
        QString displayLabel() const;
    };

    // Classifies only from observed protocol identity, explicit model/vendor
    // descriptors, and open service ports. A port-only result is never marked
    // verified; ambiguous devices remain Unknown instead of being guessed.
    static Result classify(const DeviceRecord &device);
};
