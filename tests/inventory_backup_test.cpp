#include "core/inventory_backup.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>
#include <QSqlError>
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
    void sqliteFullFailureDoesNotCorruptExistingRows();
    void uncommittedChangesAreRolledBackAfterConnectionLoss();
    void rejectsBackupDestinationWhenParentIsAFile();
    void rejectsActiveDatabaseAsRestoreSourceWithoutChangingRows();
    void rejectsDirectoryAndEmptyFilesAsBackups();
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


void InventoryBackupTest::sqliteFullFailureDoesNotCorruptExistingRows()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSqlDatabase database;
    QVERIFY(createInventory(database, directory.filePath(QStringLiteral("inventory.sqlite")),
                            QStringLiteral("safe-record")));
    QSqlQuery limit(database);
    QVERIFY(limit.exec(QStringLiteral("PRAGMA max_page_count=2")));
    QSqlQuery insert(database);
    insert.prepare(QStringLiteral("INSERT INTO devices VALUES (:name)"));
    insert.bindValue(QStringLiteral(":name"), QString(256 * 1024, QLatin1Char('x')));
    QVERIFY2(!insert.exec(), "The constrained SQLite database should report SQLITE_FULL.");
    QVERIFY(insert.lastError().text().contains(QStringLiteral("full"), Qt::CaseInsensitive));
    QSqlQuery verify(database);
    QVERIFY(verify.exec(QStringLiteral("SELECT name FROM devices")));
    QVERIFY(verify.next());
    QCOMPARE(verify.value(0).toString(), QStringLiteral("safe-record"));
    QVERIFY(!verify.next());
    database.close();
}

void InventoryBackupTest::rejectsBackupDestinationWhenParentIsAFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSqlDatabase database;
    QVERIFY(createInventory(database, directory.filePath(QStringLiteral("inventory.sqlite")),
                            QStringLiteral("preserve-me")));

    const QString blocker = directory.filePath(QStringLiteral("not-a-directory"));
    QFile file(blocker);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArrayLiteral("keep-this-file"));
    file.close();

    QString error;
    const QString destination = blocker + QStringLiteral("/backup.sqlite");
    QVERIFY(!InventoryBackup::create(database, destination, &error));
    QVERIFY(!error.isEmpty());
    QFile verifyBlocker(blocker);
    QVERIFY(verifyBlocker.open(QIODevice::ReadOnly));
    QCOMPARE(verifyBlocker.readAll(), QByteArrayLiteral("keep-this-file"));
    QVERIFY(!QFileInfo::exists(destination));
    database.close();
}

void InventoryBackupTest::rejectsActiveDatabaseAsRestoreSourceWithoutChangingRows()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("inventory.sqlite"));
    QSqlDatabase database;
    QVERIFY(createInventory(database, path, QStringLiteral("active-inventory")));

    QString error;
    QVERIFY(!InventoryBackup::restore(database, path, &error));
    QVERIFY(!error.isEmpty());
    QSqlQuery verify(database);
    QVERIFY(verify.exec(QStringLiteral("SELECT name FROM devices")));
    QVERIFY(verify.next());
    QCOMPARE(verify.value(0).toString(), QStringLiteral("active-inventory"));
    QVERIFY(!verify.next());
    database.close();
}

void InventoryBackupTest::rejectsDirectoryAndEmptyFilesAsBackups()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString emptyFile = directory.filePath(QStringLiteral("empty.sqlite"));
    QFile file(emptyFile);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    QString error;
    QVERIFY(!InventoryBackup::validate(emptyFile, &error));
    QVERIFY(!error.isEmpty());
    error.clear();
    QVERIFY(!InventoryBackup::validate(directory.path(), &error));
    QVERIFY(!error.isEmpty());
}

void InventoryBackupTest::uncommittedChangesAreRolledBackAfterConnectionLoss()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("inventory.sqlite"));
    QSqlDatabase database;
    QVERIFY(createInventory(database, path, QStringLiteral("before-crash")));
    QVERIFY(database.transaction());
    {
        QSqlQuery update(database);
        QVERIFY(update.exec(QStringLiteral("UPDATE devices SET name='uncommitted'")));
    }
    database.close();
    QVERIFY(database.open());
    QSqlQuery verify(database);
    QVERIFY(verify.exec(QStringLiteral("SELECT name FROM devices")));
    QVERIFY(verify.next());
    QCOMPARE(verify.value(0).toString(), QStringLiteral("before-crash"));
    database.close();
}

QTEST_GUILESS_MAIN(InventoryBackupTest)
#include "inventory_backup_test.moc"
