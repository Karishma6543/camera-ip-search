#include "firmware/firmware_package_validator.h"

#include <QCryptographicHash>
#include <QtTest>

class FirmwarePackageValidatorTest final : public QObject
{
    Q_OBJECT
private:
    FirmwareDeviceIdentity target() const
    {
        return {QStringLiteral("Acme"), QStringLiteral("Pro Series"),
                QStringLiteral("AC-200"), QStringLiteral("Rev B"),
                QStringLiteral("IN")};
    }

    FirmwarePackageManifest manifestFor(const QByteArray &bytes) const
    {
        return {QStringLiteral("Acme"), QStringLiteral("Pro Series"),
                QStringLiteral("AC-200"), QStringLiteral("Rev B"),
                QStringLiteral("IN"), QStringLiteral("2.4.1"),
                QString::fromLatin1(QCryptographicHash::hash(
                    bytes, QCryptographicHash::Sha256).toHex())};
    }

private slots:
    void acceptsExactMetadataAndDigest();
    void rejectsDifferentHardwareRevision();
    void rejectsDigestMismatch();
    void rejectsMalformedDigest();
    void rejectsMissingTargetMetadata();
    void rejectsEmptyPackage();
};

void FirmwarePackageValidatorTest::acceptsExactMetadataAndDigest()
{
    const QByteArray bytes("camera-firmware-image");
    const auto result = FirmwarePackageValidator::validateCandidate(
        manifestFor(bytes), target(), bytes);
    QVERIFY(result.candidateValidated());
    QVERIFY(result.message.contains(QStringLiteral("flashing is not enabled")));
}

void FirmwarePackageValidatorTest::rejectsDifferentHardwareRevision()
{
    const QByteArray bytes("camera-firmware-image");
    auto manifest = manifestFor(bytes);
    manifest.hardwareRevision = QStringLiteral("Rev A");
    const auto result = FirmwarePackageValidator::validateCandidate(
        manifest, target(), bytes);
    QVERIFY(result.state == FirmwareValidationState::TargetMismatch);
}

void FirmwarePackageValidatorTest::rejectsDigestMismatch()
{
    const QByteArray bytes("camera-firmware-image");
    auto manifest = manifestFor(bytes);
    manifest.sha256 = QString(64, QLatin1Char('0'));
    const auto result = FirmwarePackageValidator::validateCandidate(
        manifest, target(), bytes);
    QVERIFY(result.state == FirmwareValidationState::PackageHashMismatch);
}

void FirmwarePackageValidatorTest::rejectsMalformedDigest()
{
    const QByteArray bytes("camera-firmware-image");
    auto manifest = manifestFor(bytes);
    manifest.sha256 = QStringLiteral("not-a-sha256");
    const auto result = FirmwarePackageValidator::validateCandidate(
        manifest, target(), bytes);
    QVERIFY(result.state == FirmwareValidationState::InvalidPackageHash);
}

void FirmwarePackageValidatorTest::rejectsMissingTargetMetadata()
{
    const QByteArray bytes("camera-firmware-image");
    auto incompleteTarget = target();
    incompleteTarget.region.clear();
    const auto result = FirmwarePackageValidator::validateCandidate(
        manifestFor(bytes), incompleteTarget, bytes);
    QVERIFY(result.state == FirmwareValidationState::MissingMetadata);
}

void FirmwarePackageValidatorTest::rejectsEmptyPackage()
{
    const QByteArray bytes;
    auto manifest = manifestFor(QByteArray("placeholder"));
    manifest.sha256 = QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    const auto result = FirmwarePackageValidator::validateCandidate(
        manifest, target(), bytes);
    QVERIFY(result.state == FirmwareValidationState::EmptyPackage);
}

QTEST_MAIN(FirmwarePackageValidatorTest)
#include "firmware_package_validator_test.moc"
