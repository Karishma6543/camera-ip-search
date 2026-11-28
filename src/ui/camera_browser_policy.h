#pragma once

#include <QString>
#include <QUrl>

namespace CameraBrowserPolicy {
bool allowsMainFrameNavigation(const QUrl &url, const QString &cameraHost);
bool allowsResourceRequest(const QUrl &url, const QString &cameraHost);
}
