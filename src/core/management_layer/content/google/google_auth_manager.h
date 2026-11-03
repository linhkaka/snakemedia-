#pragma once

#include <QObject>

#include <functional>

class QNetworkAccessManager;

namespace ManagementLayer {

/**
 * @brief Google account of the application: OAuth 2.0 for desktop apps (authorization code flow
 *        with PKCE, loopback redirect, system browser)
 *
 * The refresh token is kept in the OS keychain, the account email in the application settings.
 * Requested scopes are openid, email and drive.file only.
 */
class GoogleAuthManager : public QObject
{
    Q_OBJECT

public:
    explicit GoogleAuthManager(QObject* _parent = nullptr);
    ~GoogleAuthManager() override;

    /**
     * @brief Is OAuth client of the application compiled in
     */
    static bool isConfigured();

    bool isSignedIn() const;
    QString email() const;

    /**
     * @brief Current access token, empty until it is obtained or refreshed
     */
    QString accessToken() const;

    /**
     * @brief Network access manager to be used for Google API requests
     */
    QNetworkAccessManager* networkAccessManager() const;

    /**
     * @brief Load the refresh token from the keychain (asynchronously)
     */
    void restoreSession();

    /**
     * @brief Open the Google sign in page in the system browser
     */
    void signIn();

    /**
     * @brief Revoke the tokens and remove them from the keychain
     */
    void signOut();

    /**
     * @brief Get a new access token with the refresh token
     */
    void refreshAccessToken(const std::function<void(bool _refreshed)>& _callback);

signals:
    void signedInChanged(bool _isSignedIn);
    void signInFailed(const QString& _error);

    /**
     * @brief The refresh token is expired or revoked, user should sign in again
     */
    void reauthenticationRequired();

private:
    class Implementation;
    QScopedPointer<Implementation> d;
};

} // namespace ManagementLayer
