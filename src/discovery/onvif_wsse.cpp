#include "onvif_wsse.h"

#include <QCryptographicHash>
#include <QXmlStreamWriter>

QByteArray OnvifWsse::addUsernameToken(const QByteArray &soapEnvelope,
                                       const QString &username,
                                       const QString &password,
                                       const QByteArray &nonce,
                                       const QString &createdUtc)
{
    if (username.isEmpty() || nonce.isEmpty() || createdUtc.isEmpty())
        return {};
    const qsizetype bodyPosition = soapEnvelope.indexOf("<s:Body");
    if (bodyPosition < 0 || !soapEnvelope.contains("<s:Envelope"))
        return {};

    const QByteArray digestInput = nonce + createdUtc.toUtf8() + password.toUtf8();
    const QByteArray digest = QCryptographicHash::hash(digestInput, QCryptographicHash::Sha1).toBase64();
    QByteArray header;
    QXmlStreamWriter xml(&header);
    xml.writeStartElement(QStringLiteral("s:Header"));
    xml.writeStartElement(QStringLiteral("wsse:Security"));
    xml.writeAttribute(QStringLiteral("xmlns:wsse"),
        QStringLiteral("http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd"));
    xml.writeAttribute(QStringLiteral("xmlns:wsu"),
        QStringLiteral("http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd"));
    xml.writeAttribute(QStringLiteral("s:mustUnderstand"), QStringLiteral("1"));
    xml.writeStartElement(QStringLiteral("wsse:UsernameToken"));
    xml.writeTextElement(QStringLiteral("wsse:Username"), username);
    xml.writeStartElement(QStringLiteral("wsse:Password"));
    xml.writeAttribute(QStringLiteral("Type"),
        QStringLiteral("http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest"));
    xml.writeCharacters(QString::fromLatin1(digest));
    xml.writeEndElement();
    xml.writeStartElement(QStringLiteral("wsse:Nonce"));
    xml.writeAttribute(QStringLiteral("EncodingType"),
        QStringLiteral("http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary"));
    xml.writeCharacters(QString::fromLatin1(nonce.toBase64()));
    xml.writeEndElement();
    xml.writeTextElement(QStringLiteral("wsu:Created"), createdUtc);
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndElement();
    if (xml.hasError())
        return {};
    QByteArray result = soapEnvelope;
    result.insert(bodyPosition, header);
    return result;
}
