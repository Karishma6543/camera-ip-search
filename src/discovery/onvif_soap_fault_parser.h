#pragma once

#include <QByteArray>
#include <QString>

namespace OnvifSoapFaultParser
{
// Returns true only for a well-formed SOAP response containing a Fault element.
// The message is extracted from SOAP 1.1 faultstring or SOAP 1.2 Reason/Text.
bool parse(const QByteArray &payload, QString *message, QString *code = nullptr);
}
