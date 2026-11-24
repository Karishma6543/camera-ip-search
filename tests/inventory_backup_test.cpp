#include "core/inventory_backup.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

class InventoryBackupTest final : public QObject
{
    Q_OBJECT
private slots:
    void createsAndValidatesConsistentBackup();
    void rejectsCorruptBackupWithoutChangingInventory();
    void restoresBackupAndKeepsRecoverySnapshot();
};

static bool createInventory(QSqlDatabase &database, const QString &path, const QString &value)
{
    database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
        QStringLiteral("inventory_backup_test_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    database.setDatabaseName(path);
    if (!database.open()) return false;
    QSqlQuery query(database);
    return query.exec(QStringLiteral("CREATE TABLE devices (name TEXT NOT NULL)")) &&
           query.exec(QStringLiteral("INSERT INTO devices VALUES ('%1')").arg(value));
}

void InventoryBackupTest::createsAndValidatesConsistentBackup()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSqlDatabase database;
    QVERIFY(createInventory(database, directory.filePath(QStringLiteral("inventory.sqlite")), QStringLiteral("camera-a")));
    QString error;
    const QString backup = directory.filePath(QStringLiteral("backup.sqlite"));
    QVERIFY2(InventoryBackup::create(database, backup, &error), qPrintable(error));
    QVERIFY2(InventoryBackup::validate(backup, &error), qPrintable(error));
    QVERIFY(QFileInfo(backup).size() > 0);
    QVERIFY(!InventoryBackup::create(database, backup, &error));
    database.close();
}

void InventoryBackupTest::rejectsCorruptBackupWithoutChangingInventory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSqlDatabase database;
    QVERIFY(createInventory(database, directory.filePath(QStringLiteral("inventory.sqlite")), QStringLiteral("camera-safe")));
    const QString corrupt = directory.filePath(QStringLiteral("corrupt.sqlite"));
    QFile file(corrupt);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArrayLiteral("not sqlite"));
    file.close();

    QString error;
    QVERIFY(!InventoryBackup::restore(database, corrupt, &error));
    QSqlQuery query(database);
    QVERIFY(query.exec(QStringLiteral("SELECT name FROM devices")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("camera-safe"));
    database.close();
}

void InventoryBackupTest::restoresBackupAndKeepsRecoverySnapshot()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSqlDatabase database;
    const QString target = directory.filePath(QStringLiteral("inventory.sqlite"));
    QVERIFY(createInventory(database, target, QStringLiteral("current-camera")));

    const QString backup = directory.filePath(QStringLiteral("backup.sqlite"));
    QString error;
    QVERIFY2(InventoryBackup::create(database, backup, &error), qPrintable(error));

    QSqlQuery update(database);
    QVERIFY(update.exec(QStringLiteral("UPDATE devices SET name='new-camera'")));
    QVERIFY2(InventoryBackup::restore(database, backup, &error), qPrintable(error));
    QSqlQuery restored(database);
    QVERIFY(restored.exec(QStringLiteral("SELECT name FROM devices")));
    QVERIFY(restored.next());
    QCOMPARE(restored.value(0).toString(), QStringLiteral("current-camera"));

    const QStringList recoveryFiles = QDir(directory.path()).entryList(
        {QStringLiteral("inventory.sqlite.pre-restore-*.sqlite")}, QDir::Files);
    QCOMPARE(recoveryFiles.size(), 1);
    QVERIFY(InventoryBackup::validate(directory.filePath(recoveryFiles.first()), &error));
    database.close();
}

QTEST_GUILESS_MAIN(InventoryBackupTest)
#include "inventory_backup_test.moc"
