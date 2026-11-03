#include "google_auth_manager.h"

#include <data_layer/storage/storage_facade.h>
#include <qtkeychain/keychain.h>

#include <QDesktopServices>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QOAuth2AuthorizationCodeFlow>
#include <QOAuthHttpServerReplyHandler>
#include <QUrlQuery>

#include <utility>

//
// OAuth client of the application is provided at build time (src/google_oauth.pri or CI secrets)
//
#ifndef GOOGLE_OAUTH_CLIENT_ID
#define GOOGLE_OAUTH_CLIENT_ID ""
#endif
#ifndef GOOGLE_OAUTH_CLIENT_SECRET
#define GOOGLE_OAUTH_CLIENT_SECRET ""
#endif


namespace ManagementLayer {

namespace {

const QString kKeychainService = QStringLiteral("Jenny Script");
const QString kKeychainRefreshTokenKey = QStringLiteral("google-refresh-token");
//
// Keys starting with "account" survive the settings reset
//
const QString kEmailSettingsKey = QStringLiteral("account/google/email");

const QUrl kAuthorizationUrl(QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth"));
const QUrl kTokenUrl(QStringLiteral("https://oauth2.googleapis.com/token"));
const QUrl kRevokeUrl(QStringLiteral("https://oauth2.googleapis.com/revoke"));
const QByteArray kDriveFileScope = QByteArrayLiteral("https://www.googleapis.com/auth/drive.file");

/**
 * @brief Read the email claim from the payload of an OpenID Connect id token
 */
QString emailFromIdToken(const QString& _idToken)
{
    const auto parts = _idToken.split('.');
    if (parts.size() < 2) {
        return {};
    }
    const auto payload = QByteArray::fromBase64(
        parts.at(1).toLatin1(), QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return QJsonDocument::fromJson(payload).object().value("email").toString();
}

} // namespace


class GoogleAuthManager::Implementation
{
public:
    explicit Implementation(GoogleAuthManager* _q);

    void storeRefreshToken(const QString& _refreshToken);
    void clearSession();
    void finishRefresh(bool _isRefreshed);
    void finishSignIn(const QString& _error);

    GoogleAuthManager* q = nullptr;
    QNetworkAccessManager networkAccessManager;
    QOAuth2AuthorizationCodeFlow flow;
    QOAuthHttpServerReplyHandler* replyHandler = nullptr;
    QString email;
    QString storedRefreshToken;
    bool isSignInInProgress = false;
    QVector<std::function<void(bool)>> refreshCallbacks;
};

GoogleAuthManager::Implementation::Implementation(GoogleAuthManager* _q)
    : q(_q)
{
    flow.setNetworkAccessManager(&networkAccessManager);
    flow.setAuthorizationUrl(kAuthorizationUrl);
    flow.setTokenUrl(kTokenUrl);
    flow.setClientIdentifier(QString::fromUtf8(GOOGLE_OAUTH_CLIENT_ID));
    flow.setClientIdentifierSharedKey(QString::fromUtf8(GOOGLE_OAUTH_CLIENT_SECRET));
    flow.setRequestedScopeTokens({
        QByteArrayLiteral("openid"),
        QByteArrayLiteral("email"),
        kDriveFileScope,
    });
    flow.setPkceMethod(QOAuth2AuthorizationCodeFlow::PkceMethod::S256);
    flow.setAutoRefresh(true);
    //
    // Ask for a refresh token: offline access and the consent screen every time the user signs in
    //
    flow.setModifyParametersFunction(
        [](QAbstractOAuth::Stage _stage, QMultiMap<QString, QVariant>* _parameters) {
            if (_stage == QAbstractOAuth::Stage::RequestingAuthorization) {
                _parameters->insert(QStringLiteral("access_type"), QStringLiteral("offline"));
                _parameters->insert(QStringLiteral("prompt"), QStringLiteral("consent"));
            }
        });

    //
    // The handler also parses token responses, so it lives all the time, but it listens on the
    // loopback interface only while the user signs in
    //
    replyHandler = new QOAuthHttpServerReplyHandler(QHostAddress::LocalHost, 0, q);
    replyHandler->close();
    flow.setReplyHandler(replyHandler);
}

void GoogleAuthManager::Implementation::storeRefreshToken(const QString& _refreshToken)
{
    if (_refreshToken.isEmpty() || _refreshToken == storedRefreshToken) {
        return;
    }

    storedRefreshToken = _refreshToken;
    auto job = new QKeychain::WritePasswordJob(kKeychainService, q);
    job->setKey(kKeychainRefreshTokenKey);
    job->setTextData(_refreshToken);
    job->start();
}

void GoogleAuthManager::Implementation::clearSession()
{
    const bool wasSignedIn = !flow.refreshToken().isEmpty();
    flow.setRefreshToken({});
    flow.setToken({});
    storedRefreshToken.clear();
    email.clear();
    setSettingsValue(kEmailSettingsKey, {});

    auto job = new QKeychain::DeletePasswordJob(kKeychainService, q);
    job->setKey(kKeychainRefreshTokenKey);
    job->start();

    if (wasSignedIn) {
        emit q->signedInChanged(false);
    }
}

void GoogleAuthManager::Implementation::finishRefresh(bool _isRefreshed)
{
    const auto callbacks = std::exchange(refreshCallbacks, {});
    for (const auto& callback : callbacks) {
        callback(_isRefreshed);
    }
}

void GoogleAuthManager::Implementation::finishSignIn(const QString& _error)
{
    if (!isSignInInProgress) {
        return;
    }

    isSignInInProgress = false;
    replyHandler->close();
    if (_error.isEmpty()) {
        emit q->signedInChanged(true);
    } else {
        emit q->signInFailed(_error);
    }
}


// ****


GoogleAuthManager::GoogleAuthManager(QObject* _parent)
    : QObject(_parent)
    , d(new Implementation(this))
{
    connect(&d->flow, &QAbstractOAuth::authorizeWithBrowser, this, &QDesktopServices::openUrl);
    connect(&d->flow, &QAbstractOAuth2::refreshTokenChanged, this,
            [this](const QString& _refreshToken) { d->storeRefreshToken(_refreshToken); });
    connect(&d->flow, &QAbstractOAuth::granted, this, [this] {
        //
        // Google consent screen lets the user untick single permissions, without Drive access the
        // integration can't work, so drop such a sign in
        //
        if (const auto grantedScopes = d->flow.grantedScopeTokens();
            !grantedScopes.isEmpty() && !grantedScopes.contains(kDriveFileScope)) {
            d->finishRefresh(false);
            signOut();
            d->finishSignIn(tr("Google Drive access was not granted. Sign in again and tick the "
                               "permission to see and edit the Google Drive files used with "
                               "Jenny Script."));
            return;
        }

        if (const auto email = emailFromIdToken(d->flow.idToken()); !email.isEmpty()) {
            d->email = email;
            setSettingsValue(kEmailSettingsKey, email);
        }
        d->storeRefreshToken(d->flow.refreshToken());
        d->finishSignIn({});
        d->finishRefresh(true);
    });
    connect(&d->flow, &QAbstractOAuth2::serverReportedErrorOccurred, this,
            [this](const QString& _error, const QString& _errorDescription, const QUrl&) {
                //
                // Refresh token is expired (7 days for apps in testing mode) or revoked
                //
                if (_error == QLatin1String("invalid_grant") && !d->isSignInInProgress) {
                    d->clearSession();
                    emit reauthenticationRequired();
                }
                d->finishSignIn(_errorDescription.isEmpty() ? _error : _errorDescription);
                d->finishRefresh(false);
            });
    connect(&d->flow, &QAbstractOAuth::requestFailed, this, [this](QAbstractOAuth::Error _error) {
        d->finishSignIn(tr("Google sign in failed (error %1)").arg(static_cast<int>(_error)));
        d->finishRefresh(false);
    });
}

GoogleAuthManager::~GoogleAuthManager() = default;

bool GoogleAuthManager::isConfigured()
{
    return !QString::fromUtf8(GOOGLE_OAUTH_CLIENT_ID).isEmpty();
}

bool GoogleAuthManager::isSignedIn() const
{
    return !d->flow.refreshToken().isEmpty();
}

QString GoogleAuthManager::email() const
{
    return d->email;
}

QString GoogleAuthManager::accessToken() const
{
    return d->flow.token();
}

QNetworkAccessManager* GoogleAuthManager::networkAccessManager() const
{
    return &d->networkAccessManager;
}

void GoogleAuthManager::restoreSession()
{
    d->email = settingsValue(kEmailSettingsKey).toString();

    auto job = new QKeychain::ReadPasswordJob(kKeychainService, this);
    job->setKey(kKeychainRefreshTokenKey);
    connect(job, &QKeychain::Job::finished, this, [this, job] {
        const auto refreshToken = job->error() == QKeychain::NoError ? job->textData() : QString();
        if (refreshToken.isEmpty()) {
            d->email.clear();
            return;
        }

        d->storedRefreshToken = refreshToken;
        d->flow.setRefreshToken(refreshToken);
        emit signedInChanged(true);
    });
    job->start();
}

void GoogleAuthManager::signIn()
{
    if (!isConfigured()) {
        emit signInFailed(tr("Google sign in is not available in this build"));
        return;
    }

    if (!d->replyHandler->isListening()
        && !d->replyHandler->listen(QHostAddress::LocalHost, 0)) {
        emit signInFailed(tr("Can't start the local server for Google sign in"));
        return;
    }
    d->replyHandler->setCallbackText(
        tr("Signed in to Google. You can close this page and return to Jenny Script."));

    d->isSignInInProgress = true;
    d->flow.grant();
}

void GoogleAuthManager::signOut()
{
    //
    // Revoking the refresh token revokes the access tokens issued with it too
    //
    const auto token = !d->flow.refreshToken().isEmpty() ? d->flow.refreshToken() : d->flow.token();
    if (!token.isEmpty()) {
        QNetworkRequest request(kRevokeUrl);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
        QUrlQuery body;
        body.addQueryItem(QStringLiteral("token"), token);
        auto reply = d->networkAccessManager.post(request,
                                                  body.toString(QUrl::FullyEncoded).toUtf8());
        connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
    }

    d->clearSession();
}

void GoogleAuthManager::refreshAccessToken(const std::function<void(bool)>& _callback)
{
    if (!isSignedIn()) {
        _callback(false);
        return;
    }

    d->refreshCallbacks.append(_callback);
    if (d->refreshCallbacks.size() == 1) {
        d->flow.refreshTokens();
    }
}

} // namespace ManagementLayer
