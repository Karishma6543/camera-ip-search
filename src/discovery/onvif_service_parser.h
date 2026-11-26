#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace OnvifServiceParser
{
// Parses ONVIF GetServices and returns only service categories whose advertised
// XAddr is a safe HTTP(S) endpoint on the discovered camera host.
bool parseServiceCategories(const QByteArray &payload, const QString &expectedHost,
                            QStringList *categories, QString *error = nullptr);
}
