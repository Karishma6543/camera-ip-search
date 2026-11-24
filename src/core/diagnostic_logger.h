#pragma once

#include <QString>

class DiagnosticLogger final
{
public:
    // Initializes an append-only diagnostic log under the supplied directory
    // and installs a Qt message handler. No credentials should ever be logged.
    static bool initialize(const QString &directoryPath);
    static QString logFilePath();
    static QString redact(const QString &text);
    static bool exportRedacted(const QString &destinationPath, QString *error = nullptr);
};
