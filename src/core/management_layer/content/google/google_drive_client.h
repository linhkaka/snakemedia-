#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QVector>

#include <functional>

class QNetworkAccessManager;
class QJsonObject;

namespace ManagementLayer {

/**
 * @brief Metadata of a file stored in Google Drive
 */
struct GoogleDriveFile {
    QString id;
    QString name;
    QString mimeType;
    QString webViewLink;
    QDateTime modifiedTime;
    qint64 version = 0;
    bool trashed = false;
    QHash<QString, QString> appProperties;

    bool isValid() const;
    static GoogleDriveFile fromJson(const QJsonObject& _json);
};

/**
 * @brief Error of a Google Drive request
 */
struct GoogleDriveError {
    enum class Type {
        None,
        Network,
        Unauthorized,
        Forbidden,
        NotFound,
        RateLimited,
        Server,
        InvalidResponse,
        Other,
    };

    Type type = Type::None;
    int httpStatus = 0;
    QString reason;
    QString message;

    bool isError() const;
};

/**
 * @brief Thin client for the Google Drive REST API v3
 *
 * Every request is authorized with the token from the access token provider. A 401 response
 * triggers the token refresher once and the request is repeated. 429, 5xx, 403 rate limit errors
 * and transient network errors are retried with exponential backoff.
 */
class GoogleDriveClient : public QObject
{
    Q_OBJECT

public:
    static const QString kGoogleDocumentMimeType;
    static const QString kFolderMimeType;

    using AccessTokenProvider = std::function<QString()>;
    using TokenRefresher = std::function<void(std::function<void(bool _refreshed)>)>;
    using FileCallback = std::function<void(const GoogleDriveFile&, const GoogleDriveError&)>;
    using FilesCallback
        = std::function<void(const QVector<GoogleDriveFile>&, const GoogleDriveError&)>;
    using DataCallback = std::function<void(const QByteArray&, const GoogleDriveError&)>;

public:
    explicit GoogleDriveClient(QNetworkAccessManager* _networkAccessManager,
                               QObject* _parent = nullptr);
    ~GoogleDriveClient() override;

    void setAccessTokenProvider(const AccessTokenProvider& _provider);
    void setTokenRefresher(const TokenRefresher& _refresher);

    /**
     * @brief Retry policy for rate limit, server and transient network errors
     * @param _maxRetries - number of retries after the first attempt
     * @param _baseDelayMs - delay before the first retry, doubled for each next one
     */
    void setRetryPolicy(int _maxRetries, int _baseDelayMs);

    /**
     * @brief Search files with the Drive query language (q parameter of files.list)
     */
    void findFiles(const QString& _query, const FilesCallback& _callback);

    /**
     * @brief Find a folder with the given name inside the parent (or in My Drive root if the
     *        parent is empty) and create it when it does not exist
     */
    void findOrCreateFolder(const QString& _name, const QString& _parentId,
                            const FileCallback& _callback);

    /**
     * @brief Upload content and convert it to a Google Doc
     */
    void createDocument(const QString& _name, const QString& _parentId,
                        const QByteArray& _content, const QString& _contentMimeType,
                        const QHash<QString, QString>& _appProperties,
                        const FileCallback& _callback);

    /**
     * @brief Replace content of an existing Google Doc
     */
    void updateDocumentContent(const QString& _fileId, const QByteArray& _content,
                               const QString& _contentMimeType, const FileCallback& _callback);

    /**
     * @brief Replace app properties of a file (the given keys only)
     */
    void updateAppProperties(const QString& _fileId,
                             const QHash<QString, QString>& _appProperties,
                             const FileCallback& _callback);

    void fileMetadata(const QString& _fileId, const FileCallback& _callback);

    /**
     * @brief Export a Google Workspace file, e.g. a Google Doc as text/plain
     */
    void exportFile(const QString& _fileId, const QString& _mimeType,
                    const DataCallback& _callback);

    /**
     * @brief Escape a string value for the Drive query language
     */
    static QString escapeQueryValue(const QString& _value);

private:
    class Implementation;
    QScopedPointer<Implementation> d;
};

} // namespace ManagementLayer
