#pragma once

#include <QByteArray>
#include <QString>

struct FirmwareDeviceIdentity
{
    QString vendor;
    QString productSeries;
    QString model;
    QString hardwareRevision;
    QString region;
};

struct FirmwarePackageManifest
{
    QString vendor;
    QString productSeries;
    QString model;
    QString hardwareRevision;
    QString region;
    QString version;
    QString sha256;
};

enum class FirmwareValidationState {
    CandidateValidated,
    MissingMetadata,
    TargetMismatch,
    InvalidPackageHash,
    PackageHashMismatch,
    EmptyPackage
};

struct FirmwareValidationResult
{
    FirmwareValidationState state{FirmwareValidationState::MissingMetadata};
    QString message;

    bool candidateValidated() const
    {
        return state == FirmwareValidationState::CandidateValidated;
    }
};

class FirmwarePackageValidator final
{
public:
    // Validates exact target metadata and the package's SHA-256 digest.
    // A validated candidate is NOT authorization to flash: signature
    // verification and a vendor-authorized update transport are separate gates.
    static FirmwareValidationResult validateCandidate(
        const FirmwarePackageManifest &manifest,
        const FirmwareDeviceIdentity &target,
        const QByteArray &packageBytes);
};
