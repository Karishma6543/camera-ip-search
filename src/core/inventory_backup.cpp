#include "inventory_backup.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace {
void setError(QString *error, const QString &message)
{
    if (error) *error = message;
}

QString quoteSqlString(QString value)
{
    value.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QStringLiteral("'") + value + QStringLiteral("'");
}

bool runQuickCheck(QSqlDatabase &database, QString *error)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA quick_check")) || !query.next()) {
        setError(error, query.lastError().text().isEmpty()
            ? QStringLiteral("SQLite integrity check did not return a result.")
            : query.lastError().text());
        return false;
    }
    if (query.value(0).toString().compare(QStringLiteral("ok"), Qt::CaseInsensitive) != 0) {
        setError(error, QStringLiteral("SQLite integrity check failed: %1").arg(query.value(0).toString()));
        return false;
    }
    return true;
}
}

bool InventoryBackup::validate(const QString &databasePath, QString *error)
{
    const QFileInfo info(databasePath);
    if (!info.exists() || !info.isFile() || info.size() <= 0) {
        setError(error, QStringLiteral("The selected backup does not exist or is empty."));
        return false;
    }

    const QString connectionName = QStringLiteral("inventory_backup_check_%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool valid = false;
    {
        QSqlDatabase check = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        check.setDatabaseName(info.absoluteFilePath());
        check.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (!check.open()) {
            setError(error, QStringLiteral("Could not open the backup read-only: %1").arg(check.lastError().text()));
        } else {
            valid = runQuickCheck(check, error);
            check.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return valid;
}

bool InventoryBackup::create(const QSqlDatabase &database, const QString &destinationPath,
                             QString *error)
{
    if (!database.isValid() || !database.isOpen()) {
        setError(error, QStringLiteral("The inventory database is not open."));
        return false;
    }
    const QFileInfo destination(destinationPath);
    if (destinationPath.trimmed().isEmpty() || destination.absoluteFilePath() ==
        QFileInfo(database.databaseName()).absoluteFilePath()) {
        setError(error, QStringLiteral("Choose a separate backup file path."));
        return false;
    }
    if (!QDir().mkpath(destination.absolutePath())) {
        setError(error, QStringLiteral("Could not create the backup directory."));
        return false;
    }
    if (QFileInfo::exists(destination.absoluteFilePath())) {
        setError(error, QStringLiteral("The backup destination already exists; choose a new filename."));
        return false;
    }

    QSqlQuery query(database);
    const QString sql = QStringLiteral("VACUUM INTO %1").arg(quoteSqlString(destination.absoluteFilePath()));
    if (!query.exec(sql)) {
        setError(error, QStringLiteral("SQLite backup failed: %1").arg(query.lastError().text()));
        return false;
    }
    if (!validate(destination.absoluteFilePath(), error)) {
        QFile::remove(destination.absoluteFilePath());
        return false;
    }
    return true;
}

bool InventoryBackup::restore(QSqlDatabase &database, const QString &backupPath, QString *error)
{
    if (!database.isValid() || !database.isOpen()) {
        setError(error, QStringLiteral("The inventory database is not open."));
        return false;
    }
    const QString targetPath = QFileInfo(database.databaseName()).absoluteFilePath();
    const QString sourcePath = QFileInfo(backupPath).absoluteFilePath();
    if (targetPath == sourcePath) {
        setError(error, QStringLiteral("The backup cannot be the active inventory database."));
        return false;
    }
    if (!validate(sourcePath, error)) return false;

    const QString timestamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmsszzz"));
    const QString safetyPath = targetPath + QStringLiteral(".pre-restore-") + timestamp + QStringLiteral(".sqlite");
    if (!create(database, safetyPath, error)) return false;

    const QString stagedPath = targetPath + QStringLiteral(".restore-tmp");
    const QString replacedPath = targetPath + QStringLiteral(".replaced-tmp");
    QFile::remove(stagedPath);
    QFile::remove(replacedPath);
    if (!QFile::copy(sourcePath, stagedPath)) {
        setError(error, QStringLiteral("Could not stage the selected backup beside the inventory database."));
        return false;
    }

    QSqlQuery checkpoint(database);
    checkpoint.exec(QStringLiteral("PRAGMA wal_checkpoint(FULL)"));
    QSqlQuery journal(database);
    journal.exec(QStringLiteral("PRAGMA journal_mode=DELETE"));
    database.close();

    QFile::remove(targetPath + QStringLiteral("-wal"));
    QFile::remove(targetPath + QStringLiteral("-shm"));
    if (!QFile::rename(targetPath, replacedPath)) {
        QFile::remove(stagedPath);
        database.open();
        setError(error, QStringLiteral("Could not move the current inventory aside before restore."));
        return false;
    }
    if (!QFile::rename(stagedPath, targetPath)) {
        QFile::rename(replacedPath, targetPath);
        database.open();
        setError(error, QStringLiteral("Could not install the validated backup; the previous inventory was restored."));
        return false;
    }

    if (database.open() && runQuickCheck(database, error)) {
        QFile::remove(replacedPath);
        return true;
    }

    database.close();
    QFile::remove(targetPath);
    QFile::rename(replacedPath, targetPath);
    const bool reopened = database.open();
    if (error && error->isEmpty())
        *error = QStringLiteral("Restored database failed validation; the previous inventory was reinstated%1.")
            .arg(reopened ? QString() : QStringLiteral(" but could not be reopened"));
    return false;
}
