#pragma once

#include <QDialog>
#include <QUrl>

class QLabel;
class QLineEdit;
class QWebEngineProfile;
class QWebEngineView;

class CameraBrowserDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit CameraBrowserDialog(const QUrl &cameraUrl,
                                  const QString &deviceLabel,
                                  QWidget *parent = nullptr);

    QUrl currentUrl() const;

private:
    QWebEngineProfile *profile_{nullptr};
    QWebEngineView *view_{nullptr};
    QLineEdit *addressBar_{nullptr};
    QLabel *statusLabel_{nullptr};
    QString allowedHost_;
    void updateAddress(const QUrl &url);
    void showAuthenticationPrompt(const QUrl &url, class QAuthenticator *authenticator);
};
