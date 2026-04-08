#include "updatechecker.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QSysInfo>
#include <QProcess>
#include <QTimer>

UpdateChecker::UpdateChecker(QObject* parent) : QObject(parent) {}

void UpdateChecker::checkForUpdates() {
    if (m_checking) return;
    m_checking = true;
    emit checkingChanged();

    QString url = QString("https://api.github.com/repos/%1/releases/latest")
                      .arg(Protocol::GITHUB_REPO);

    QUrl qurl(url);
    QNetworkRequest req{qurl};
    req.setHeader(QNetworkRequest::UserAgentHeader, "Jacal-Game-Client");
    req.setRawHeader("Accept", "application/vnd.github.v3+json");

    QNetworkReply* reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onCheckFinished(reply);
    });

    // Timeout: 10 seconds
    QTimer::singleShot(10000, reply, [reply]() {
        if (reply->isRunning()) reply->abort();
    });
}

void UpdateChecker::onCheckFinished(QNetworkReply* reply) {
    m_checking = false;
    emit checkingChanged();
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        // Silently fail — no internet or no repo
        m_updateAvailable = false;
        emit updateChecked();
        return;
    }

    QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
    QString tagName = release["tag_name"].toString();
    if (tagName.startsWith('v')) tagName = tagName.mid(1);

    m_latestVersion = tagName;
    m_updateAvailable = isNewerVersion(tagName, currentVersion());

    // Find download URL for current platform
    QString platform;
#ifdef Q_OS_WIN
    platform = "win64";
#elif defined(Q_OS_LINUX)
    platform = "linux";
#elif defined(Q_OS_ANDROID)
    platform = "android";
#elif defined(Q_OS_MAC)
    platform = "macos";
#endif

    m_downloadUrl = "";
    QJsonArray assets = release["assets"].toArray();
    for (auto a : assets) {
        QJsonObject asset = a.toObject();
        QString name = asset["name"].toString().toLower();
        if (name.contains(platform) && (name.endsWith(".zip") || name.endsWith(".tar.gz"))) {
            m_downloadUrl = asset["browser_download_url"].toString();
            break;
        }
    }

    m_releaseNotes = release["body"].toString();
    emit updateChecked();
}

void UpdateChecker::downloadUpdate() {
    if (m_downloadUrl.isEmpty() || m_downloading) return;
    m_downloading = true;
    m_downloadProgress = 0;
    emit downloadingChanged();
    emit downloadProgressChanged();

    QUrl dlUrl(m_downloadUrl);
    QNetworkRequest req{dlUrl};
    req.setHeader(QNetworkRequest::UserAgentHeader, "Jacal-Game-Client");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = m_nam.get(req);
    connect(reply, &QNetworkReply::downloadProgress, this, &UpdateChecker::onDownloadProgress);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onDownloadFinished(reply);
    });
}

void UpdateChecker::onDownloadProgress(qint64 received, qint64 total) {
    m_downloadProgress = total > 0 ? static_cast<int>(received * 100 / total) : 0;
    emit downloadProgressChanged();
}

void UpdateChecker::onDownloadFinished(QNetworkReply* reply) {
    m_downloading = false;
    emit downloadingChanged();
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred("Ошибка загрузки: " + reply->errorString());
        return;
    }

    // Save to temp file
    QString appDir = QCoreApplication::applicationDirPath();
    QString fileName = QUrl(m_downloadUrl).fileName();
    QString filePath = appDir + "/" + fileName;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        emit errorOccurred("Не удалось сохранить файл: " + filePath);
        return;
    }
    file.write(reply->readAll());
    file.close();

    m_downloadProgress = 100;
    emit downloadProgressChanged();

    // Extract and restart
#ifdef Q_OS_LINUX
    // Extract zip/tar.gz
    if (filePath.endsWith(".tar.gz")) {
        QProcess::execute("tar", {"xzf", filePath, "-C", appDir});
    } else if (filePath.endsWith(".zip")) {
        QProcess::execute("unzip", {"-o", filePath, "-d", appDir});
    }
    QFile::remove(filePath);

    // Make executable
    QFile::setPermissions(appDir + "/jackal",
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
        QFileDevice::ReadGroup | QFileDevice::ExeGroup);
#endif

    emit updateReady(filePath);

    // Restart application
    QProcess::startDetached(QCoreApplication::applicationFilePath(),
                            QCoreApplication::arguments());
    QCoreApplication::quit();
}

bool UpdateChecker::isNewerVersion(const QString& latest, const QString& current) const {
    QStringList lParts = latest.split('.');
    QStringList cParts = current.split('.');
    for (int i = 0; i < qMax(lParts.size(), cParts.size()); i++) {
        int l = i < lParts.size() ? lParts[i].toInt() : 0;
        int c = i < cParts.size() ? cParts[i].toInt() : 0;
        if (l > c) return true;
        if (l < c) return false;
    }
    return false;
}
