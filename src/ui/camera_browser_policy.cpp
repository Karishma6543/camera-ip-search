#include "camera_browser_policy.h"

namespace {
bool sameCameraHost(const QUrl &url, const QString &cameraHost)
{
    return url.isValid() && !cameraHost.trimmed().isEmpty() && url.userInfo().isEmpty() &&
           url.host().compare(cameraHost.trimmed(), Qt::CaseInsensitive) == 0;
}
}

namespace CameraBrowserPolicy {
bool allowsMainFrameNavigation(const QUrl &url, const QString &cameraHost)
{
    const QString scheme = url.scheme().toLower();
    const bool web = scheme == QStringLiteral("http") || scheme == QStringLiteral("https");
    return web && sameCameraHost(url, cameraHost);
}

bool allowsResourceRequest(const QUrl &url, const QString &cameraHost)
{
    const QString scheme = url.scheme().toLower();
    if (scheme == QStringLiteral("data") || scheme == QStringLiteral("blob") ||
        scheme == QStringLiteral("about"))
        return true;
    const bool web = scheme == QStringLiteral("http") || scheme == QStringLiteral("https") ||
                     scheme == QStringLiteral("ws") || scheme == QStringLiteral("wss");
    return web && sameCameraHost(url, cameraHost);
}
}
