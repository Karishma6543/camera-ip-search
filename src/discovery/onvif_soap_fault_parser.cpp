#include "onvif_soap_fault_parser.h"

#include <QXmlStreamReader>

bool OnvifSoapFaultParser::parse(const QByteArray &payload, QString *message, QString *code)
{
    if (message) message->clear();
    if (code) code->clear();
    if (!message || payload.isEmpty())
        return false;

    QXmlStreamReader xml(payload);
    bool inFault = false;
    bool faultSeen = false;
    QString parsedMessage;
    QString parsedCode;

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString name = xml.name().toString();
            if (name.compare(QStringLiteral("Fault"), Qt::CaseInsensitive) == 0) {
                inFault = true;
                faultSeen = true;
            } else if (inFault &&
                       (name.compare(QStringLiteral("faultstring"), Qt::CaseInsensitive) == 0 ||
                        name.compare(QStringLiteral("Text"), Qt::CaseInsensitive) == 0)) {
                const QString value = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
                if (!value.isEmpty())
                    parsedMessage = value;
            } else if (inFault && (name.compare(QStringLiteral("Value"), Qt::CaseInsensitive) == 0 ||
                                  name.compare(QStringLiteral("faultcode"), Qt::CaseInsensitive) == 0)) {
                const QString value = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
                if (!value.isEmpty())
                    parsedCode = value;
            }
        } else if (xml.isEndElement() &&
                   xml.name().compare(QStringLiteral("Fault"), Qt::CaseInsensitive) == 0) {
            inFault = false;
        }
    }

    if (xml.hasError() || !faultSeen)
        return false;

    *message = parsedMessage.isEmpty()
        ? QStringLiteral("The camera returned a SOAP fault.")
        : parsedMessage.left(512);
    if (code)
        *code = parsedCode.left(128);
    return true;
}
