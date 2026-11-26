#include "discovery/onvif_wsse.h"

#include <QCryptographicHash>
#include <QXmlStreamReader>
#include <QtTest>

class OnvifWsseTest final : public QObject
{
    Q_OBJECT
private slots:
    void createsPasswordDigestUsernameToken();
    void rejectsInvalidEnvelopeAndMissingUsername();
};

void OnvifWsseTest::createsPasswordDigestUsernameToken()
{
    const QByteArray envelope = QByteArrayLiteral(
        "<s:Envelope xmlns:s='http://www.w3.org/2003/05/soap-envelope'><s:Body>"
        "<tds:GetDeviceInformation xmlns:tds='urn:test'/>"
        "</s:Body></s:Envelope>");
    const QByteArray nonce = QByteArray::fromHex("0102030405060708090a0b0c0d0e0f10");
    const QString created = QStringLiteral("2026-11-26T12:00:00.000Z");
    const QByteArray result = OnvifWsse::addUsernameToken(
        envelope, QStringLiteral("operator"), QStringLiteral("secret"), nonce, created);
    QVERIFY(!result.isEmpty());
    QVERIFY(result.indexOf("<s:Header>") < result.indexOf("<s:Body"));

    const QByteArray digest = QCryptographicHash::hash(
        nonce + created.toUtf8() + QByteArrayLiteral("secret"),
        QCryptographicHash::Sha1).toBase64();
    QXmlStreamReader xml(result);
    bool usernameFound = false;
    bool passwordFound = false;
    bool nonceFound = false;
    bool createdFound = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) continue;
        const QString name = xml.name().toString();
        if (name == QStringLiteral("Username")) {
            usernameFound = xml.readElementText() == QStringLiteral("operator");
        } else if (name == QStringLiteral("Password")) {
            passwordFound = xml.readElementText().toLatin1() == digest;
        } else if (name == QStringLiteral("Nonce")) {
            nonceFound = QByteArray::fromBase64(xml.readElementText().toLatin1()) == nonce;
        } else if (name == QStringLiteral("Created")) {
            createdFound = xml.readElementText() == created;
        }
    }
    QVERIFY2(!xml.hasError(), qPrintable(xml.errorString()));
    QVERIFY(usernameFound);
    QVERIFY(passwordFound);
    QVERIFY(nonceFound);
    QVERIFY(createdFound);
}

void OnvifWsseTest::rejectsInvalidEnvelopeAndMissingUsername()
{
    const QByteArray nonce = QByteArrayLiteral("nonce");
    const QString created = QStringLiteral("2026-11-26T12:00:00Z");
    QVERIFY(OnvifWsse::addUsernameToken(QByteArrayLiteral("<invalid/>"),
        QStringLiteral("user"), QStringLiteral("pass"), nonce, created).isEmpty());
    QVERIFY(OnvifWsse::addUsernameToken(
        QByteArrayLiteral("<s:Envelope><s:Body/></s:Envelope>"),
        QString(), QStringLiteral("pass"), nonce, created).isEmpty());
}

QTEST_GUILESS_MAIN(OnvifWsseTest)
#include "onvif_wsse_test.moc"
