#include "ssdp_parser.h"

#include <QHash>
#include <QRegularExpression>

bool SsdpParser::parseResponse(const QByteArray &payload,
                               const QHostAddress &sender,
                               const QString &interfaceName,
                               DeviceRecord *device)
{
    if (!device || payload.isEmpty() || payload.size() > 16 * 1024 ||
        sender.protocol() != QAbstractSocket::IPv4Protocol ||
        sender.isNull() || sender.isLoopback() || sender.isMulticast())
        return false;

    const QString text = QString::fromLatin1(payload);
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("\\r?\\n")),
                                         Qt::SkipEmptyParts);
    if (lines.isEmpty()) return false;
    const QString status = lines.first().trimmed();
    if (!status.startsWith(QStringLiteral("HTTP/1.1 200"), Qt::CaseInsensitive) &&
        !status.startsWith(QStringLiteral("HTTP/1.0 200"), Qt::CaseInsensitive))
        return false;

    QHash<QString, QString> headers;
    for (int i = 1; i < lines.size() && i <= 128; ++i) {
        const QString line = lines.at(i).trimmed();
        const qsizetype separator = line.indexOf(QLatin1Char(':'));
        if (separator <= 0) continue;
        const QString key = line.left(separator).trimmed().toLower();
        const QString value = line.mid(separator + 1).trimmed();
        if (!key.isEmpty() && !headers.contains(key))
            headers.insert(key, value.left(2048));
    }

    const QString st = headers.value(QStringLiteral("st"));
    const QString usn = headers.value(QStringLiteral("usn"));
    const QString server = headers.value(QStringLiteral("server"));
    if (st.isEmpty() && usn.isEmpty() && server.isEmpty())
        return false;

    DeviceRecord parsed;
    parsed.ipAddress = sender.toString();
    parsed.interfaceName = interfaceName;
    parsed.manufacturer = QStringLiteral("Unknown");
    parsed.model = !st.isEmpty() ? st : server;
    parsed.hardwareId = usn.left(256);
    parsed.userLabel = QStringLiteral("UPnP network device");
    parsed.activationState = QStringLiteral("Unknown");
    parsed.discoverySources << QStringLiteral("SSDP/UPnP");
    parsed.isCandidate = true;
    *device = parsed;
    return true;
}
