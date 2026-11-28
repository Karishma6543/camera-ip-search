#include "adapters/initialization_adapter_registry.h"

#include <QtTest>

#include <memory>

namespace {
class FakeInitializationAdapter final : public CameraInitializationAdapter
{
public:
    FakeInitializationAdapter(QString adapterId, bool matches)
        : adapterId_(std::move(adapterId)), matches_(matches) {}

    QString id() const override { return adapterId_; }
    QString displayName() const override { return adapterId_; }
    bool canHandle(const DeviceRecord &) const override { return matches_; }
    InitializationEvidence supportedTargetEvidence() const override
    {
        InitializationEvidence evidence;
        evidence.protocol = QStringLiteral("test-only");
        evidence.authoritative = true;
        return evidence;
    }
    ActivationProbeResult probeActivation(
        const DeviceRecord &, const InitializationContext &) const override
    {
        return {};
    }
    ActivationResult initialize(
        const DeviceRecord &, const InitializationCredentials &,
        const InitializationContext &) const override
    {
        return {};
    }

private:
    QString adapterId_;
    bool matches_{false};
};
}

class InitializationAdapterRegistryTest final : public QObject
{
    Q_OBJECT
private slots:
    void rejectsNullAndEmptyIds();
    void rejectsDuplicateIdsCaseInsensitively();
    void returnsOnlyMatchingAdapter();
    void doesNotSilentlyResolveAmbiguousMatches();
    void exposesRegisteredIds();
};

void InitializationAdapterRegistryTest::rejectsNullAndEmptyIds()
{
    InitializationAdapterRegistry registry;
    QVERIFY(!registry.registerAdapter(nullptr));
    QVERIFY(!registry.registerAdapter(std::make_unique<FakeInitializationAdapter>(QString(), true)));
    QVERIFY(registry.registeredAdapterIds().isEmpty());
}

void InitializationAdapterRegistryTest::rejectsDuplicateIdsCaseInsensitively()
{
    InitializationAdapterRegistry registry;
    QVERIFY(registry.registerAdapter(std::make_unique<FakeInitializationAdapter>(
        QStringLiteral("vendor-a"), true)));
    QVERIFY(!registry.registerAdapter(std::make_unique<FakeInitializationAdapter>(
        QStringLiteral("VENDOR-A"), true)));
    QCOMPARE(registry.registeredAdapterIds().size(), 1);
}

void InitializationAdapterRegistryTest::returnsOnlyMatchingAdapter()
{
    InitializationAdapterRegistry registry;
    QVERIFY(registry.registerAdapter(std::make_unique<FakeInitializationAdapter>(
        QStringLiteral("not-this-camera"), false)));
    QVERIFY(registry.registerAdapter(std::make_unique<FakeInitializationAdapter>(
        QStringLiteral("supported-camera"), true)));

    const DeviceRecord device;
    const auto matches = registry.matchingAdapters(device);
    QCOMPARE(matches.size(), size_t(1));
    QVERIFY(registry.adapterFor(device));
    QCOMPARE(registry.adapterFor(device)->id(), QStringLiteral("supported-camera"));
}

void InitializationAdapterRegistryTest::doesNotSilentlyResolveAmbiguousMatches()
{
    InitializationAdapterRegistry registry;
    QVERIFY(registry.registerAdapter(std::make_unique<FakeInitializationAdapter>(
        QStringLiteral("adapter-one"), true)));
    QVERIFY(registry.registerAdapter(std::make_unique<FakeInitializationAdapter>(
        QStringLiteral("adapter-two"), true)));

    const DeviceRecord device;
    QCOMPARE(registry.matchingAdapters(device).size(), size_t(2));
    QVERIFY(!registry.adapterFor(device));
}

void InitializationAdapterRegistryTest::exposesRegisteredIds()
{
    InitializationAdapterRegistry registry;
    QVERIFY(registry.registerAdapter(std::make_unique<FakeInitializationAdapter>(
        QStringLiteral("adapter-one"), false)));
    QVERIFY(registry.registerAdapter(std::make_unique<FakeInitializationAdapter>(
        QStringLiteral("adapter-two"), false)));
    QCOMPARE(registry.registeredAdapterIds(),
             QStringList({QStringLiteral("adapter-one"), QStringLiteral("adapter-two")}));
}

QTEST_MAIN(InitializationAdapterRegistryTest)
#include "initialization_adapter_registry_test.moc"
