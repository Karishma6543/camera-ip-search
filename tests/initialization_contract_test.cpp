#include "adapters/initialization_adapter.h"

#include <QtTest>

class InitializationContractTest final : public QObject
{
    Q_OBJECT
private slots:
    void defaultsAreUnknownAndUnverified();
    void cancellationAndDeadlineAreExplicit();
    void credentialViewIsNonOwning();
};

void InitializationContractTest::defaultsAreUnknownAndUnverified()
{
    const ActivationProbeResult probe;
    QCOMPARE(probe.status, ActivationStatus::Unknown);
    QVERIFY(!probe.evidence.authoritative);

    const ActivationResult result;
    QCOMPARE(result.outcome, ActivationOutcome::Failed);
    QVERIFY(!result.activationVerified);
}

void InitializationContractTest::cancellationAndDeadlineAreExplicit()
{
    std::atomic_bool cancelled{false};
    InitializationContext context;
    context.cancelled = &cancelled;
    QVERIFY(!context.isCancelled());
    cancelled.store(true, std::memory_order_relaxed);
    QVERIFY(context.isCancelled());

    InitializationContext expired;
    expired.deadline = QDeadlineTimer(0);
    QVERIFY(expired.isExpired());
}

void InitializationContractTest::credentialViewIsNonOwning()
{
    const QByteArray secret("temporary-secret");
    InitializationCredentials credentials;
    credentials.username = QStringLiteral("operator");
    credentials.secret = QByteArrayView(secret);
    QCOMPARE(credentials.secret, QByteArrayView(secret));
    // The contract carries a view only; persistence and ownership stay with
    // the caller and adapters are forbidden from retaining it.
}

QTEST_APPLESS_MAIN(InitializationContractTest)
#include "initialization_contract_test.moc"
