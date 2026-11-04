#include "app_update_manager.h"

#include "update_info.h"

#include <ui/widgets/dialog/dialog.h>
#include <ui/widgets/dialog/standard_dialog.h>
#include <ui/widgets/task_bar/task_bar.h>
#include <utils/logging.h>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QTemporaryFile>
#include <QUrl>
#include <QWidget>

namespace ManagementLayer {

namespace {

const QUrl kLatestReleaseApiUrl(
    QStringLiteral("https://api.github.com/repos/vienglacial/starc/releases/latest"));
const QUrl kReleasesPageUrl(QStringLiteral("https://github.com/vienglacial/starc/releases/latest"));
const QString kDownloadTaskId = QStringLiteral("jenny-script-update-download");
const QString kExecutableName = QStringLiteral("starcapp.exe");
constexpr int kCheckTimeoutMs = 15000;
constexpr int kDownloadStallTimeoutMs = 60000;

/**
 * @brief Can the running app replace its own files: it is the packaged app in a writable folder
 */
bool canInstallInPlace()
{
    if (QFileInfo(QCoreApplication::applicationFilePath()).fileName() != kExecutableName) {
        return false;
    }
    QTemporaryFile probe(QCoreApplication::applicationDirPath() + "/update-probe-XXXXXX.tmp");
    return probe.open();
}

} // namespace


class AppUpdateManager::Implementation
{
public:
    Implementation(AppUpdateManager* _q, QWidget* _parentWidget);

    void offerUpdate();
    void offerManualDownload();
    void download();
    void finishDownload(QNetworkReply* _reply);
    void failDownload(const QString& _details);

    AppUpdateManager* q = nullptr;
    QWidget* parentWidget = nullptr;
    QNetworkAccessManager network;
    AppUpdate::ReleaseInfo release;
    QString workDirPath;
    QString packagePath;
    QFile packageFile;
    QCryptographicHash packageHash{ QCryptographicHash::Sha256 };
};

AppUpdateManager::Implementation::Implementation(AppUpdateManager* _q, QWidget* _parentWidget)
    : q(_q)
    , parentWidget(_parentWidget)
{
}

void AppUpdateManager::Implementation::offerUpdate()
{
    if (!canInstallInPlace()) {
        offerManualDownload();
        return;
    }

    const int kLaterButtonId = 0;
    const int kUpdateButtonId = 1;
    auto dialog = new Dialog(parentWidget);
    dialog->showDialog(
        {},
        AppUpdateManager::tr("Version %1 is available. Do you want to update now? The app will "
                             "restart, your projects stay as they are.")
            .arg(release.version),
        { { kLaterButtonId, AppUpdateManager::tr("Later"), Dialog::RejectButton },
          { kUpdateButtonId, AppUpdateManager::tr("Update"), Dialog::AcceptButton } });
    QObject::connect(dialog, &Dialog::finished, dialog,
                     [this, dialog, kUpdateButtonId](const Dialog::ButtonInfo& _buttonInfo) {
                         dialog->hideDialog();
                         if (_buttonInfo.id == kUpdateButtonId) {
                             download();
                         }
                     });
    QObject::connect(dialog, &Dialog::disappeared, dialog, &Dialog::deleteLater);
}

void AppUpdateManager::Implementation::offerManualDownload()
{
    const int kLaterButtonId = 0;
    const int kOpenButtonId = 1;
    auto dialog = new Dialog(parentWidget);
    dialog->showDialog(
        {},
        AppUpdateManager::tr("Version %1 is available, but the app can't update itself in this "
                             "folder. You can download the new version from the website.")
            .arg(release.version),
        { { kLaterButtonId, AppUpdateManager::tr("Later"), Dialog::RejectButton },
          { kOpenButtonId, AppUpdateManager::tr("Open download page"), Dialog::AcceptButton } });
    QObject::connect(dialog, &Dialog::finished, dialog,
                     [dialog, kOpenButtonId](const Dialog::ButtonInfo& _buttonInfo) {
                         dialog->hideDialog();
                         if (_buttonInfo.id == kOpenButtonId) {
                             QDesktopServices::openUrl(kReleasesPageUrl);
                         }
                     });
    QObject::connect(dialog, &Dialog::disappeared, dialog, &Dialog::deleteLater);
}

void AppUpdateManager::Implementation::download()
{
    //
    // The folder belongs to the updater only, leftovers of an interrupted update can go
    //
    workDirPath = QDir::temp().filePath(QStringLiteral("JennyScriptUpdate"));
    QDir(workDirPath).removeRecursively();
    QDir().mkpath(workDirPath);
    packagePath = QDir(workDirPath).filePath(release.downloadUrl.fileName());

    packageFile.setFileName(packagePath);
    if (!packageFile.open(QIODevice::WriteOnly)) {
        failDownload(packageFile.errorString());
        return;
    }
    packageHash.reset();

    TaskBar::addTask(kDownloadTaskId);
    TaskBar::setTaskTitle(kDownloadTaskId,
                          AppUpdateManager::tr("Downloading version %1").arg(release.version));

    QNetworkRequest request(release.downloadUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("JennyScript-Updater"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(kDownloadStallTimeoutMs);
    auto reply = network.get(request);
    QObject::connect(reply, &QNetworkReply::readyRead, q, [this, reply] {
        const auto data = reply->readAll();
        packageFile.write(data);
        packageHash.addData(data);
    });
    QObject::connect(reply, &QNetworkReply::downloadProgress, q,
                     [this](qint64 _received, qint64 /*_total*/) {
                         TaskBar::setTaskProgress(kDownloadTaskId,
                                                  _received * 100.0 / release.size);
                     });
    QObject::connect(reply, &QNetworkReply::finished, q,
                     [this, reply] { finishDownload(reply); });
}

void AppUpdateManager::Implementation::finishDownload(QNetworkReply* _reply)
{
    _reply->deleteLater();
    TaskBar::finishTask(kDownloadTaskId);

    const auto rest = _reply->readAll();
    packageFile.write(rest);
    packageHash.addData(rest);
    const qint64 writtenSize = packageFile.size();
    const bool isWritten = packageFile.error() == QFileDevice::NoError;
    packageFile.close();

    if (_reply->error() != QNetworkReply::NoError) {
        failDownload(_reply->errorString());
    } else if (!isWritten || writtenSize != release.size) {
        failDownload(QStringLiteral("Wrong package size: %1 of %2").arg(writtenSize).arg(release.size));
    } else if (packageHash.result().toHex() != release.sha256Hex) {
        failDownload(QStringLiteral("Wrong package checksum"));
    } else {
        Log::info("Update %1 is downloaded and checked", release.version);
        emit q->installReady();
    }
}

void AppUpdateManager::Implementation::failDownload(const QString& _details)
{
    Log::warning("Update download failed: %1", _details);
    packageFile.close();
    QFile::remove(packagePath);
    StandardDialog::information(
        parentWidget, {},
        AppUpdateManager::tr("Can't download the update. Check your connection and open the app "
                             "again, it will offer the update next time."));
}


// ****


AppUpdateManager::AppUpdateManager(QWidget* _parentWidget, QObject* _parent)
    : QObject(_parent)
    , d(new Implementation(this, _parentWidget))
{
}

AppUpdateManager::~AppUpdateManager() = default;

void AppUpdateManager::checkForUpdate()
{
    QNetworkRequest request(kLatestReleaseApiUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("JennyScript-Updater"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setTransferTimeout(kCheckTimeoutMs);
    auto reply = d->network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        //
        // No network or no answer from GitHub: nothing to tell the user, try on the next start
        //
        if (reply->error() != QNetworkReply::NoError) {
            Log::info("Update check skipped: %1", reply->errorString());
            return;
        }

        const auto release = AppUpdate::parseLatestRelease(reply->readAll());
        if (!release.has_value()
            || !AppUpdate::isNewerVersion(release->version, AppUpdate::currentVersion())) {
            return;
        }

        d->release = *release;
        d->offerUpdate();
    });
}

bool AppUpdateManager::startInstaller()
{
    const auto scriptPath = QDir(d->workDirPath).filePath(QStringLiteral("install_update.ps1"));
    QFile::remove(scriptPath);
    const bool isScriptReady
        = QFile::copy(QStringLiteral(":/app-update/install_update.ps1"), scriptPath);

    const auto powerShell = QDir(qEnvironmentVariable("SystemRoot"))
                                .filePath(QStringLiteral(
                                    "System32/WindowsPowerShell/v1.0/powershell.exe"));
    const QStringList arguments = {
        QStringLiteral("-NoProfile"),
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-WindowStyle"),
        QStringLiteral("Hidden"),
        QStringLiteral("-File"),
        QDir::toNativeSeparators(scriptPath),
        QStringLiteral("-ProcessId"),
        QString::number(QCoreApplication::applicationPid()),
        QStringLiteral("-ZipPath"),
        QDir::toNativeSeparators(d->packagePath),
        QStringLiteral("-InstallDir"),
        QDir::toNativeSeparators(QCoreApplication::applicationDirPath()),
        QStringLiteral("-ExePath"),
        QDir::toNativeSeparators(QCoreApplication::applicationFilePath()),
    };

    //
    // The working folder is the temporary one, otherwise the script would hold the app folder
    //
    if (!isScriptReady
        || !QProcess::startDetached(QDir::toNativeSeparators(powerShell), arguments,
                                    d->workDirPath)) {
        Log::warning("%1", QStringLiteral("Can't start the update script"));
        StandardDialog::information(d->parentWidget, {},
                                    tr("Can't start the update. You can download the new version "
                                       "from the website."));
        QDesktopServices::openUrl(kReleasesPageUrl);
        return false;
    }
    return true;
}

} // namespace ManagementLayer
