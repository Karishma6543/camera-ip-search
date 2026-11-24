#include "firmware_package_validator.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QRegularExpression>

namespace {
bool sameIdentityField(const QString &packageValue, const QString &deviceValue)
{
    return !packageValue.trimmed().isEmpty() &&
           !deviceValue.trimmed().isEmpty() &&
           packageValue.trimmed().compare(deviceValue.trimmed(), Qt::CaseInsensitive) == 0;
}

bool hasRequiredMetadata(const FirmwarePackageManifest &manifest,
                         const FirmwareDeviceIdentity &target)
{
    return !manifest.vendor.trimmed().isEmpty() &&
           !manifest.productSeries.trimmed().isEmpty() &&
           !manifest.model.trimmed().isEmpty() &&
           !manifest.hardwareRevision.trimmed().isEmpty() &&
           !manifest.region.trimmed().isEmpty() &&
           !manifest.version.trimmed().isEmpty() &&
           !target.vendor.trimmed().isEmpty() &&
           !target.productSeries.trimmed().isEmpty() &&
           !target.model.trimmed().isEmpty() &&
           !target.hardwareRevision.trimmed().isEmpty() &&
           !target.region.trimmed().isEmpty();
}
}

FirmwareValidationResult FirmwarePackageValidator::validateCandidate(
    const FirmwarePackageManifest &manifest,
    const FirmwareDeviceIdentity &target,
    const QByteArray &packageBytes)
{
    if (!hasRequiredMetadata(manifest, target)) {
        return {FirmwareValidationState::MissingMetadata,
                QStringLiteral("Vendor, series, model, hardware revision, region and version are all required.")};
    }

    if (!sameIdentityField(manifest.vendor, target.vendor) ||
        !sameIdentityField(manifest.productSeries, target.productSeries) ||
        !sameIdentityField(manifest.model, target.model) ||
        !sameIdentityField(manifest.hardwareRevision, target.hardwareRevision) ||
        !sameIdentityField(manifest.region, target.region)) {
        return {FirmwareValidationState::TargetMismatch,
                QStringLiteral("Firmware package does not exactly match the target vendor, series, model, hardware revision and region.")};
    }

    static const QRegularExpression sha256Pattern(QStringLiteral("^[0-9a-fA-F]{64}$"));
    const QString expectedHash = manifest.sha256.trimmed();
    if (!sha256Pattern.match(expectedHash).hasMatch()) {
        return {FirmwareValidationState::InvalidPackageHash,
                QStringLiteral("Manifest SHA-256 must contain exactly 64 hexadecimal characters.")};
    }

    if (packageBytes.isEmpty()) {
        return {FirmwareValidationState::EmptyPackage,
                QStringLiteral("Firmware package is empty.")};
    }

    const QString actualHash = QString::fromLatin1(
        QCryptographicHash::hash(packageBytes, QCryptographicHash::Sha256).toHex());
    if (actualHash.compare(expectedHash, Qt::CaseInsensitive) != 0) {
        return {FirmwareValidationState::PackageHashMismatch,
                QStringLiteral("Firmware package SHA-256 does not match its manifest.")};
    }

    return {FirmwareValidationState::CandidateValidated,
            QStringLiteral("Metadata and package hash match. Signature verification and an authorized update transport are still required; flashing is not enabled.")};
}
