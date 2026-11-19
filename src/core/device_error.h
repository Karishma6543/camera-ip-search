#pragma once

#include <QString>

// Structured, user-safe error information. Never put credentials, tokens,
// or complete authenticated request bodies in message or diagnosticContext.
struct DeviceError
{
    enum class Category {
        Network,
        Timeout,
        Protocol,
        Authentication,
        Unsupported,
        Validation,
        Storage,
        Internal
    };

    Category category{Category::Internal};
    QString code;
    QString message;
    QString operation;
    QString endpoint;
    bool retryable{false};
    QString diagnosticContext;
};
