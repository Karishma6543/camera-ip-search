#include "camera_browser_dialog.h"

#include <QAuthenticator>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>

namespace {
class CameraWebPage final : public QWebEnginePage
{
    Q_OBJECT
public:
    CameraWebPage(QWebEngineProfile *profile, const QString &allowedHost, QObject *parent)
        : QWebEnginePage(profile, parent), allowedHost_(allowedHost) {}

signals:
    void externalNavigationBlocked(const QUrl &url);

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override
    {
        Q_UNUSED(type);
        if (!isMainFrame) return true;
        const bool safeScheme = url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0 ||
                                url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0;
        const bool sameHost = url.host().compare(allowedHost_, Qt::CaseInsensitive) == 0;
        if (safeScheme && sameHost) return true;
        emit externalNavigationBlocked(url);
        return false;
    }

    QWebEnginePage *createWindow(WebWindowType type) override
    {
        Q_UNUSED(type);
        return nullptr;
    }

private:
    QString allowedHost_;
};
}

CameraBrowserDialog::CameraBrowserDialog(const QUrl &cameraUrl,
                                         const QString &deviceLabel,
                                         QWidget *parent)
    : QDialog(parent), allowedHost_(cameraUrl.host())
{
    setWindowTitle(tr("Camera setup — %1").arg(deviceLabel.isEmpty() ? cameraUrl.host() : deviceLabel));
    resize(1180, 780);
    setMinimumSize(760, 520);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    auto *toolbar = new QHBoxLayout();
    auto *backButton = new QPushButton(tr("Back"), this);
    backButton->setObjectName(QStringLiteral("cameraBrowserBack"));
    auto *forwardButton = new QPushButton(tr("Forward"), this);
    forwardButton->setObjectName(QStringLiteral("cameraBrowserForward"));
    auto *reloadButton = new QPushButton(tr("Reload"), this);
    reloadButton->setObjectName(QStringLiteral("cameraBrowserReload"));
    addressBar_ = new QLineEdit(this);
    addressBar_->setObjectName(QStringLiteral("cameraBrowserAddress"));
    addressBar_->setReadOnly(true);
    addressBar_->setClearButtonEnabled(false);
    toolbar->addWidget(backButton);
    toolbar->addWidget(forwardButton);
    toolbar->addWidget(reloadButton);
    toolbar->addWidget(addressBar_, 1);
    root->addLayout(toolbar);

    statusLabel_ = new QLabel(tr("Connecting to the camera web interface…"), this);
    statusLabel_->setObjectName(QStringLiteral("cameraBrowserStatus"));
    statusLabel_->setWordWrap(true);
    root->addWidget(statusLabel_);

    profile_ = new QWebEngineProfile(this);
    profile_->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
    profile_->setHttpCacheType(QWebEngineProfile::NoCache);

    view_ = new QWebEngineView(this);
    view_->setObjectName(QStringLiteral("cameraBrowserView"));
    auto *page = new CameraWebPage(profile_, allowedHost_, view_);
    view_->setPage(page);
    page->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    page->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, false);
    page->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
    root->addWidget(view_, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close | QDialogButtonBox::Ok, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Done — verify camera"));
    buttons->button(QDialogButtonBox::Ok)->setObjectName(QStringLiteral("cameraSetupDone"));
    buttons->button(QDialogButtonBox::Close)->setObjectName(QStringLiteral("cameraSetupClose"));
    root->addWidget(buttons);

    connect(backButton, &QPushButton::clicked, view_, &QWebEngineView::back);
    connect(forwardButton, &QPushButton::clicked, view_, &QWebEngineView::forward);
    connect(reloadButton, &QPushButton::clicked, view_, &QWebEngineView::reload);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(view_, &QWebEngineView::urlChanged, this, &CameraBrowserDialog::updateAddress);
    connect(view_, &QWebEngineView::loadProgress, this, [this](int progress) {
        statusLabel_->setText(tr("Loading camera page… %1%").arg(progress));
    });
    connect(view_, &QWebEngineView::loadFinished, this, [this](bool ok) {
        statusLabel_->setText(ok
            ? tr("Camera page loaded. Complete the camera's own setup, then choose “Done — verify camera”.")
            : tr("The camera page could not be loaded. Check the cable, power, address, authentication and browser compatibility."));
    });
    connect(page, &CameraWebPage::externalNavigationBlocked, this, [this](const QUrl &url) {
        statusLabel_->setText(tr("Blocked navigation away from the selected camera: %1").arg(url.toString()));
    });
    connect(page, &QWebEnginePage::authenticationRequired,
            this, &CameraBrowserDialog::showAuthenticationPrompt);
    connect(page, &QWebEnginePage::renderProcessTerminated, this,
            [this](QWebEnginePage::RenderProcessTerminationStatus, int) {
        statusLabel_->setText(tr("The camera page renderer stopped. Reload the page or close this window."));
    });

    view_->setUrl(cameraUrl);
}

QUrl CameraBrowserDialog::currentUrl() const
{
    return view_ ? view_->url() : QUrl();
}

void CameraBrowserDialog::updateAddress(const QUrl &url)
{
    if (addressBar_) addressBar_->setText(url.toString(QUrl::FullyEncoded));
}

void CameraBrowserDialog::showAuthenticationPrompt(const QUrl &url, QAuthenticator *authenticator)
{
    if (!authenticator) return;
    bool accepted = false;
    const QString username = QInputDialog::getText(
        this, tr("Camera authentication"),
        tr("Username for %1:").arg(url.host()), QLineEdit::Normal, QString(), &accepted);
    if (!accepted) {
        statusLabel_->setText(tr("Authentication cancelled. No credentials were saved."));
        return;
    }
    const QString password = QInputDialog::getText(
        this, tr("Camera authentication"), tr("Password:"),
        QLineEdit::Password, QString(), &accepted);
    if (!accepted) {
        statusLabel_->setText(tr("Authentication cancelled. No credentials were saved."));
        return;
    }
    authenticator->setUser(username);
    authenticator->setPassword(password);
}
#include "camera_browser_dialog.moc"
