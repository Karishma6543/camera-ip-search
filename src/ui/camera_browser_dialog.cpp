#include "camera_browser_dialog.h"
#include "camera_browser_policy.h"

#include <QAuthenticator>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWebEngineCertificateError>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineUrlRequestInfo>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineView>
#include <utility>

namespace CameraBrowserInternal {
class CameraRequestInterceptor final : public QWebEngineUrlRequestInterceptor
{
public:
    CameraRequestInterceptor(QString allowedHost, QObject *parent)
        : QWebEngineUrlRequestInterceptor(parent), allowedHost_(std::move(allowedHost)) {}

    void interceptRequest(QWebEngineUrlRequestInfo &info) override
    {
        const QUrl url = info.requestUrl();
        if (!CameraBrowserPolicy::allowsResourceRequest(url, allowedHost_))
            info.block(true);
    }

private:
    QString allowedHost_;
};

class CameraWebPage final : public QWebEnginePage
{
    Q_OBJECT
public:
    CameraWebPage(QWebEngineProfile *profile, const QString &allowedHost, QObject *parent)
        : QWebEnginePage(profile, parent), allowedHost_(allowedHost) {}

signals:
    void externalNavigationBlocked(const QUrl &url);
    void certificateRejected(const QString &description);

protected:
#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
    bool certificateError(const QWebEngineCertificateError &error) override
    {
        emit certificateRejected(error.description());
        return false;
    }
#endif

    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override
    {
        Q_UNUSED(type);
        if (!isMainFrame) return true;
        if (CameraBrowserPolicy::allowsMainFrameNavigation(url, allowedHost_))
            return true;
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
    auto *schemeButton = new QPushButton(tr("Try HTTPS"), this);
    schemeButton->setObjectName(QStringLiteral("cameraBrowserSchemeToggle"));
    reloadButton->setObjectName(QStringLiteral("cameraBrowserReload"));
    addressBar_ = new QLineEdit(this);
    addressBar_->setObjectName(QStringLiteral("cameraBrowserAddress"));
    addressBar_->setReadOnly(false);
    addressBar_->setClearButtonEnabled(false);
    addressBar_->setPlaceholderText(tr("Camera address and port (same camera only)"));
    toolbar->addWidget(backButton);
    toolbar->addWidget(forwardButton);
    toolbar->addWidget(reloadButton);
    toolbar->addWidget(schemeButton);
    toolbar->addWidget(addressBar_, 1);
    root->addLayout(toolbar);

    statusLabel_ = new QLabel(tr("Connecting to the camera web interface…"), this);
    statusLabel_->setObjectName(QStringLiteral("cameraBrowserStatus"));
    statusLabel_->setWordWrap(true);
    root->addWidget(statusLabel_);

    profile_ = new QWebEngineProfile(this);
    profile_->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
    profile_->setHttpCacheType(QWebEngineProfile::NoCache);
    profile_->setUrlRequestInterceptor(new CameraBrowserInternal::CameraRequestInterceptor(allowedHost_, profile_));
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    profile_->setPersistentPermissionsPolicy(QWebEngineProfile::PersistentPermissionsPolicy::AskEveryTime);
#endif

    view_ = new QWebEngineView(this);
    view_->setObjectName(QStringLiteral("cameraBrowserView"));
    auto *page = new CameraBrowserInternal::CameraWebPage(profile_, allowedHost_, view_);
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
    connect(addressBar_, &QLineEdit::returnPressed, this, [this] {
        const QUrl requested = QUrl::fromUserInput(addressBar_->text().trimmed());
        if (!CameraBrowserPolicy::allowsMainFrameNavigation(requested, allowedHost_)) {
            statusLabel_->setText(tr("Only HTTP/HTTPS addresses on the selected camera host are allowed. Credentials and external hosts are blocked."));
            updateAddress(view_->url());
            return;
        }
        view_->setUrl(requested);
    });
    connect(schemeButton, &QPushButton::clicked, this, [this, schemeButton] {
        QUrl next = view_->url();
        if (next.scheme() == QStringLiteral("https")) {
            next.setScheme(QStringLiteral("http"));
            if (next.port() == 443) next.setPort(-1);
            else if (next.port() == 8443) next.setPort(8080);
            schemeButton->setText(tr("Try HTTPS"));
        } else {
            next.setScheme(QStringLiteral("https"));
            if (next.port() == -1) next.setPort(443);
            else if (next.port() == 8080) next.setPort(8443);
            schemeButton->setText(tr("Try HTTP"));
        }
        view_->setUrl(next);
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(view_, &QWebEngineView::urlChanged, this, [this, schemeButton](const QUrl &url) {
        updateAddress(url);
        schemeButton->setText(url.scheme() == QStringLiteral("https") ? tr("Try HTTP") : tr("Try HTTPS"));
    });
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
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    connect(page, &QWebEnginePage::certificateError, this,
            [this](const QWebEngineCertificateError &error) {
        QWebEngineCertificateError rejected = error;
        rejected.rejectCertificate();
        statusLabel_->setText(tr("The camera HTTPS certificate was rejected: %1. Oxita will not bypass certificate validation. You can try HTTP on a trusted local network.")
                                  .arg(error.description()));
    });
#else
    connect(page, &CameraBrowserInternal::CameraWebPage::certificateRejected, this,
            [this](const QString &description) {
        statusLabel_->setText(tr("The camera HTTPS certificate was rejected: %1. Oxita will not bypass certificate validation. You can try HTTP on a trusted local network.")
                                  .arg(description));
    });
#endif
    connect(page, &QWebEnginePage::renderProcessTerminated, this,
            [this](QWebEnginePage::RenderProcessTerminationStatus, int) {
        statusLabel_->setText(tr("The camera page renderer stopped. Reload the page or close this window."));
    });

    view_->setUrl(cameraUrl);
}

CameraBrowserDialog::~CameraBrowserDialog()
{
    // The page references its profile. Destroy the view/page first, then the
    // profile, rather than relying on QObject child destruction order.
    delete view_;
    view_ = nullptr;
    delete profile_;
    profile_ = nullptr;
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
        *authenticator = QAuthenticator();
        statusLabel_->setText(tr("Authentication cancelled. No credentials were saved."));
        return;
    }
    const QString password = QInputDialog::getText(
        this, tr("Camera authentication"), tr("Password:"),
        QLineEdit::Password, QString(), &accepted);
    if (!accepted) {
        *authenticator = QAuthenticator();
        statusLabel_->setText(tr("Authentication cancelled. No credentials were saved."));
        return;
    }
    authenticator->setUser(username);
    authenticator->setPassword(password);
}
#include "camera_browser_dialog.moc"
