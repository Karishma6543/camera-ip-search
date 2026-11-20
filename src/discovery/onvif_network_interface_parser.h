#pragma once
#include <QByteArray>
#include <QString>
#include <QVector>
struct OnvifNetworkInterface {
    QString token, name, macAddress, ipv4Address, prefixLength, dhcp, gateway;
};
class OnvifNetworkInterfaceParser final {
public:
    static bool parse(const QByteArray &payload, QVector<OnvifNetworkInterface> *interfaces,
                      QString *error = nullptr);
};
