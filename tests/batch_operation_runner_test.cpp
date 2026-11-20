#include "app/batch_operation_runner.h"

#include "adapters/adapter_registry.h"

#include <QSignalSpy>
#include <QtTest>

class BatchOperationRunnerTest final : public QObject
{
    Q_OBJECT
private slots:
    void rejectsEmptyOrMissingHandler();
    void executesOnlyPreflightApprovedTargets();
    void cancellationSkipsRemainingTargets();
};

static DeviceRecord validOnvif(const QString &id, const QString &ip)
{
    DeviceRecord device;
    device.id = id;
    device.ipAddress = ip;
    device.deviceServiceUrl = QStringLiteral("http://%1/onvif/device_service").arg(ip);
    device.discoverySources << QStringLiteral("ONVIF");
    return device;
}

void BatchOperationRunnerTest::rejectsEmptyOrMissingHandler()
{
    AdapterRegistry registry;
    BatchOperationRunner runner(registry);
    QVERIFY(!runner.start({}, DeviceOperation::ReadDeviceInformation,
                          [](const DeviceRecord &, DeviceOperation, QString &) { return true; }));
    QVERIFY(!runner.start({validOnvif(QStringLiteral("cam"), QStringLiteral("192.168.1.2"))},
                          DeviceOperation::ReadDeviceInformation, {}));
}

void BatchOperationRunnerTest::executesOnlyPreflightApprovedTargets()
{
    AdapterRegistry registry;
    BatchOperationRunner runner(registry);
    QSignalSpy finished(&runner, &BatchOperationRunner::finished);
    int calls = 0;
    const auto supported = validOnvif(QStringLiteral("cam-1"), QStringLiteral("192.168.1.2"));
    const auto unsupported = validOnvif(QStringLiteral("cam-2"), QStringLiteral("192.168.1.3"));
    QVERIFY(runner.start({supported, unsupported}, DeviceOperation::Reboot,
        [&calls](const DeviceRecord &, DeviceOperation, QString &) {
            ++calls;
            return true;
        }));
    QTRY_COMPARE(finished.size(), 1);
    QCOMPARE(calls, 0);
    const auto results = qvariant_cast<QVector<BatchTargetResult>>(finished.takeFirst().at(0));
    QCOMPARE(results.size(), 2);
    QCOMPARE(results.at(0).state, BatchTargetState::UnsupportedOperation);
    QCOMPARE(results.at(1).state, BatchTargetState::UnsupportedOperation);
}

void BatchOperationRunnerTest::cancellationSkipsRemainingTargets()
{
    AdapterRegistry registry;
    BatchOperationRunner runner(registry);
    QSignalSpy finished(&runner, &BatchOperationRunner::finished);
    int calls = 0;
    QVERIFY(runner.start(
        {validOnvif(QStringLiteral("cam-1"), QStringLiteral("192.168.1.2")),
         validOnvif(QStringLiteral("cam-2"), QStringLiteral("192.168.1.3"))},
        DeviceOperation::ReadDeviceInformation,
        [&runner, &calls](const DeviceRecord &, DeviceOperation, QString &) {
            ++calls;
            runner.cancel();
            return true;
        }));
    QTRY_COMPARE(finished.size(), 1);
    QCOMPARE(calls, 1);
    const auto results = qvariant_cast<QVector<BatchTargetResult>>(finished.takeFirst().at(0));
    QCOMPARE(results.at(0).state, BatchTargetState::Succeeded);
    QCOMPARE(results.at(1).state, BatchTargetState::Cancelled);
}

QTEST_MAIN(BatchOperationRunnerTest)
#include "batch_operation_runner_test.moc"
