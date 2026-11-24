#pragma once

#include <QString>
#include <QSqlDatabase>

class InventoryBackup final
{
public:
    static bool create(const QSqlDatabase &database, const QString &destinationPath,
                       QString *error = nullptr);
    static bool restore(QSqlDatabase &database, const QString &backupPath,
                        QString *error = nullptr);
    static bool validate(const QString &databasePath, QString *error = nullptr);
};
