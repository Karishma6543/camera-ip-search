#pragma once

#include <QByteArray>
#include <QString>

namespace OnvifWsse {
QByteArray addUsernameToken(const QByteArray &soapEnvelope,
                           const QString &username,
                           const QString &password,
                           const QByteArray &nonce,
                           const QString &createdUtc);
}
