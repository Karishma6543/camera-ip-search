#pragma once

#include "core/device_record.h"

#include <QByteArrayView>
#include <QDeadlineTimer>
#include <QDeadlineTimer>
#include <QString>

#include <atomic>

// Vendor-neutral activation boundary. Implementations must be backed by an
// authorized protocol/SDK contract for the exact model and firmware.
enum class ActivationStatus {
    Unknown,
    AlreadyInitialized,
    ActivationRequired,
    AuthenticationRequired,
    Unsupported,
    Error
};

enum class ActivationOutcome {
    CompletedAndVerified,
    AcceptedButUnverified,
    Rejected,
    AuthenticationFailed,
    TimedOut,
    Cancelled,
    Unsupported,
    Failed
};

struct InitializationEvidence
{
    QString protocol;
    QString protocolVersion;
    QString vendor;
    QString model;
    QString hardwareRevision;
    QString firmwareVersion;
    QString reference;
    bool authoritative{false};
};

struct ActivationProbeResult
{
    ActivationStatus status{ActivationStatus::Unknown};
    QString diagnostic;
    InitializationEvidence evidence;
};

struct InitializationContext
{
    QDeadlineTimer deadline;
    const std::atomic_bool *cancelled{nullptr};

    bool isCancelled() const
    {
        return cancelled && cancelled->load(std::memory_order_relaxed);
    }

    bool isExpired() const
    {
        return deadline.hasExpired();
    }
};

struct InitializationCredentials
{
    QString username;
    QByteArrayView secret;

    // The caller owns secret storage. Adapters must not retain either field,
    // log them, or write them to inventory, diagnostics or operation history.
};

struct ActivationResult
{
    ActivationOutcome outcome{ActivationOutcome::Failed};
    QString diagnostic;
    InitializationEvidence evidence;
    bool activationVerified{false};
};

// This interface deliberately contains no guessed endpoint or generic HTTP
// activation implementation. A vendor adapter may be registered only after
// its protocol is documented, licensed and tested on the exact target family.
class CameraInitializationAdapter
{
public:
    virtual ~CameraInitializationAdapter() = default;

    virtual QString id() const = 0;
    virtual QString displayName() const = 0;
    virtual bool canHandle(const DeviceRecord &device) const = 0;
    virtual InitializationEvidence supportedTargetEvidence() const = 0;

    virtual ActivationProbeResult probeActivation(
        const DeviceRecord &device,
        const InitializationContext &context) const = 0;

    virtual ActivationResult initialize(
        const DeviceRecord &device,
        const InitializationCredentials &credentials,
        const InitializationContext &context) const = 0;
};
