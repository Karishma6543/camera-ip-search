#include "onvif_service_parser.h"

#include <QSet>
#include <QUrl>
#include <QXmlStreamReader>

namespace {
QString categoryFromNamespace(const QString &namespaceUri)
{
    QString value = namespaceUri.trimmed();
    if (value.endsWith(QStringLiteral("/wsdl"), Qt::CaseInsensitive))
        value.chop(5);
    const int slash = value.lastIndexOf(QLatin1Char('/'));
    if (slash < 0 || slash == value.size() - 1)
        return {};
    value = value.mid(slash + 1).trimmed();
    if (value.isEmpty())
        return {};
    value[0] = value.at(0).toUpper();
    return value;
}

bool isSafeServiceEndpoint(const QString &address, const QString &expectedHost)
{
    const QUrl url(address.trimmed());
    return url.isValid() &&
           (url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https")) &&
           !url.host().isEmpty() &&
           url.host().compare(expectedHost.trimmed(), Qt::CaseInsensitive) == 0 &&
           url.userInfo().isEmpty() && url.query().isEmpty() && url.fragment().isEmpty();
}
}

bool OnvifServiceParser::parseServiceCategories(const QByteArray &payload,
                                                 const QString &expectedHost,
                                                 QStringList *categories,
                                                 QString *error)
{
    if (categories)
        categories->clear();
    if (error)
        error->clear();
    if (!categories || expectedHost.trimmed().isEmpty() || payload.isEmpty()) {
        if (error) *error = QStringLiteral("Missing ONVIF service response, host or output.");
        return false;
    }

    QXmlStreamReader xml(payload);
    QStringList parsed;
    QString namespaceUri;
    QString xaddr;
    int serviceDepth = -1;
    bool responseSeen = false;

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString name = xml.name().toString();
            if (name == QStringLiteral("GetServicesResponse"))
                responseSeen = true;
            else if (name == QStringLiteral("Service")) {
                serviceDepth = xml.depth();
                namespaceUri.clear();
                xaddr.clear();
            } else if (serviceDepth >= 0 && name == QStringLiteral("Namespace")) {
                namespaceUri = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
            } else if (serviceDepth >= 0 && name == QStringLiteral("XAddr")) {
                xaddr = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
            }
        } else if (xml.isEndElement() &&
                   xml.name() == QStringLiteral("Service") &&
                   serviceDepth == xml.depth()) {
            const QString category = categoryFromNamespace(namespaceUri);
            if (namespaceUri.startsWith(QStringLiteral("http://www.onvif.org/"), Qt::CaseInsensitive) &&
                !category.isEmpty() && isSafeServiceEndpoint(xaddr, expectedHost) &&
                !parsed.contains(category, Qt::CaseInsensitive))
                parsed.append(category);
            serviceDepth = -1;
        }
    }

    if (xml.hasError() || !responseSeen) {
        if (error)
            *error = xml.hasError() ? xml.errorString()
                                    : QStringLiteral("The response is not an ONVIF GetServicesResponse.");
        return false;
    }

    *categories = parsed;
    return true;
}
