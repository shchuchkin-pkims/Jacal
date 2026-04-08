#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include "protocol.h"

class UpdateChecker : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY updateChecked)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateChecked)
    Q_PROPERTY(QString downloadUrl READ downloadUrl NOTIFY updateChecked)
    Q_PROPERTY(bool checking READ isChecking NOTIFY checkingChanged)
    Q_PROPERTY(int downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(bool downloading READ isDownloading NOTIFY downloadingChanged)

public:
    explicit UpdateChecker(QObject* parent = nullptr);

    QString currentVersion() const { return Protocol::APP_VERSION; }
    QString latestVersion() const { return m_latestVersion; }
    bool updateAvailable() const { return m_updateAvailable; }
    QString downloadUrl() const { return m_downloadUrl; }
    bool isChecking() const { return m_checking; }
    int downloadProgress() const { return m_downloadProgress; }
    bool isDownloading() const { return m_downloading; }

    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void downloadUpdate();

signals:
    void updateChecked();
    void checkingChanged();
    void downloadProgressChanged();
    void downloadingChanged();
    void updateReady(QString filePath);
    void errorOccurred(QString error);

private slots:
    void onCheckFinished(QNetworkReply* reply);
    void onDownloadFinished(QNetworkReply* reply);
    void onDownloadProgress(qint64 received, qint64 total);

private:
    bool isNewerVersion(const QString& latest, const QString& current) const;

    QNetworkAccessManager m_nam;
    QString m_latestVersion;
    QString m_downloadUrl;
    QString m_releaseNotes;
    bool m_updateAvailable = false;
    bool m_checking = false;
    bool m_downloading = false;
    int m_downloadProgress = 0;
};
