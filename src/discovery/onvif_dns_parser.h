#pragma once
#include <QByteArray>
#include <QString>
namespace OnvifDnsParser {
bool parseIpv4DnsAddress(const QByteArray &payload, QString *address, QString *error = nullptr);
}
