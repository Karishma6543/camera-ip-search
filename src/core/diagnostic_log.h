#pragma once

#include <QString>

namespace DiagnosticLog {
void install();
QString directoryPath();
QString redactSensitiveData(const QString &message);
}
