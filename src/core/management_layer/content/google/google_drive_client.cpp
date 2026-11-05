#include "google_drive_client.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QUuid>

#include <algorithm>
#include <utility>


namespace ManagementLayer {

namespace {

const QString kFilesUrl = QStringLiteral("https://www.googleapis.com/drive/v3/files");
const QString kUploadUrl = QStringLiteral("https://www.googleapis.com/upload/drive/v3/files");
const QString kFileFields
    = QStringLiteral("id,name,mimeType,modifiedTime,version,webViewLink,appProperties,trashed");
constexpr int kTransferTimeoutMs = 60 * 1000;

using Query = QVector<std::pair<QString, QString>>;

/**
 * @brief Build an url with a fully percent encoded query (QUrlQuery leaves "+" as is)
 */
QUrl makeUrl(const QString& _base, const Query& _query)
{
    QByteArray query;
    for (const auto& item : _query) {
        if (!query.isEmpty()) {
            query += '&';
        }
        query += QUrl::toPercentEncoding(item.first) + '=' + QUrl::toPercentEncoding(item.second);
    }
    return QUrl(_base + (query.isEmpty() ? QString() : "?" + QString::fromLatin1(query)));
}

QString fileUrl(const QString& _base, const QString& _fileId, const QString& _suffix = {})
{
    return _base + "/" + QString::fromLatin1(QUrl::toPercentEncoding(_fileId)) + _suffix;
}

QJsonObject toJson(const QHash<QString, QString>& _properties)
{
    QJsonObject json;
    for (auto it = _properties.cbegin(); it != _properties.cend(); ++it) {
        json.insert(it.key(), it.value());
    }
    return json;
}

bool isTransientNetworkError(QNetworkReply::NetworkError _error)
{
    switch (_error) {
    case QNetworkReply::TimeoutError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::RemoteHostClosedError:
    case QNetworkReply::UnknownNetworkError:
    case QNetworkReply::ProxyTimeoutError: {
        return true;
    }
    default: {
        return false;
    }
    }
}

GoogleDriveError parseError(int _httpStatus, QNetworkReply::NetworkError _networkError,
                            const QString& _networkErrorString, const QByteArray& _body)
{
    GoogleDriveError error;
    error.httpStatus = _httpStatus;
    if (_httpStatus == 0) {
        error.type = GoogleDriveError::Type::Network;
        error.reason = isTransientNetworkError(_networkError) ? QStringLiteral("transient")
                                                              : QStringLiteral("network");
        error.message = _networkErrorString;
        return error;
    }

    const auto json = QJsonDocument::fromJson(_body).object().value("error").toObject();
    error.message = json.value("message").toString();
    const auto errors = json.value("errors").toArray();
    if (!errors.isEmpty()) {
        error.reason = errors.first().toObject().value("reason").toString();
    }
    if (error.reason.isEmpty()) {
        error.reason = json.value("status").toString();
    }

    if (_httpStatus == 401) {
        error.type = GoogleDriveError::Type::Unauthorized;
    } else if (_httpStatus == 403) {
        const bool isRateLimit = error.reason == QLatin1String("rateLimitExceeded")
            || error.reason == QLatin1String("userRateLimitExceeded");
        error.type
            = isRateLimit ? GoogleDriveError::Type::RateLimited : GoogleDriveError::Type::Forbidden;
    } else if (_httpStatus == 404) {
        error.type = GoogleDriveError::Type::NotFound;
    } else if (_httpStatus == 429) {
        error.type = GoogleDriveError::Type::RateLimited;
    } else if (_httpStatus >= 500) {
        error.type = GoogleDriveError::Type::Server;
    } else {
        error.type = GoogleDriveError::Type::Other;
    }
    return error;
}

bool isRetryable(const GoogleDriveError& _error)
{
    switch (_error.type) {
    case GoogleDriveError::Type::RateLimited:
    case GoogleDriveError::Type::Server: {
        return true;
    }
    case GoogleDriveError::Type::Network: {
        return _error.reason == QLatin1String("transient");
    }
    default: {
        return false;
    }
    }
}

GoogleDriveError invalidResponse(const QByteArray& _body)
{
    GoogleDriveError error;
    error.type = GoogleDriveError::Type::InvalidResponse;
    error.httpStatus = 200;
    error.message = QString::fromUtf8(_body.left(200));
    return error;
}

} // namespace


bool GoogleDriveFile::isValid() const
{
    return !id.isEmpty();
}

GoogleDriveFile GoogleDriveFile::fromJson(const QJsonObject& _json)
{
    GoogleDriveFile file;
    file.id = _json.value("id").toString();
    file.name = _json.value("name").toString();
    file.mimeType = _json.value("mimeType").toString();
    file.webViewLink = _json.value("webViewLink").toString();
    file.modifiedTime
        = QDateTime::fromString(_json.value("modifiedTime").toString(), Qt::ISODateWithMs);
    //
    // Drive returns int64 values as strings
    //
    const auto version = _json.value("version");
    file.version = version.isString() ? version.toString().toLongLong()
                                      : static_cast<qint64>(version.toDouble());
    file.trashed = _json.value("trashed").toBool();
    const auto properties = _json.value("appProperties").toObject();
    for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
        file.appProperties.insert(it.key(), it.value().toString());
    }
    return file;
}

bool GoogleDriveError::isError() const
{
    return type != Type::None;
}


// ****


const QString GoogleDriveClient::kGoogleDocumentMimeType
    = QStringLiteral("application/vnd.google-apps.document");
const QString GoogleDriveClient::kFolderMimeType
    = QStringLiteral("application/vnd.google-apps.folder");


class GoogleDriveClient::Implementation
{
public:
    using RawCallback = std::function<void(const QByteArray&, const GoogleDriveError&)>;

    struct Request {
        QByteArray verb;
        QUrl url;
        QByteArray contentType;
        QByteArray body;
        RawCallback callback;
        int attempt = 0;
        bool isTokenRefreshed = false;
    };

    Implementation(GoogleDriveClient* _q, QNetworkAccessManager* _networkAccessManager);

    void send(Request _request);
    void sendForFile(const QByteArray& _verb, const QUrl& _url, const QByteArray& _contentType,
                     const QByteArray& _body, const FileCallback& _callback);

    GoogleDriveClient* q = nullptr;
    QPointer<QNetworkAccessManager> networkAccessManager;
    AccessTokenProvider accessTokenProvider;
    TokenRefresher tokenRefresher;
    int maxRetries = 5;
    int baseDelayMs = 1000;
};

GoogleDriveClient::Implementation::Implementation(GoogleDriveClient* _q,
                                                  QNetworkAccessManager* _networkAccessManager)
    : q(_q)
    , networkAccessManager(_networkAccessManager)
{
}

void GoogleDriveClient::Implementation::send(Request _request)
{
    if (networkAccessManager.isNull()) {
        GoogleDriveError error;
        error.type = GoogleDriveError::Type::Network;
        error.message = QStringLiteral("Network access manager is not available");
        _request.callback({}, error);
        return;
    }

    const QString token = accessTokenProvider ? accessTokenProvider() : QString();

    //
    // After a restart the user is signed in with the stored refresh token, but there is no access
    // token yet. Google answers a request without a token with 403, not 401, so get it first
    //
    if (token.isEmpty() && accessTokenProvider && tokenRefresher && !_request.isTokenRefreshed) {
        _request.isTokenRefreshed = true;
        QPointer<GoogleDriveClient> client(q);
        tokenRefresher([this, client, _request](bool _refreshed) {
            if (client.isNull()) {
                return;
            }
            if (_refreshed) {
                send(_request);
                return;
            }
            GoogleDriveError error;
            error.type = GoogleDriveError::Type::Unauthorized;
            _request.callback({}, error);
        });
        return;
    }

    QNetworkRequest networkRequest(_request.url);
    if (!token.isEmpty()) {
        networkRequest.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    }
    if (!_request.contentType.isEmpty()) {
        networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, _request.contentType);
    }
    networkRequest.setTransferTimeout(kTransferTimeoutMs);

    QNetworkReply* reply = nullptr;
    if (_request.verb == "GET") {
        reply = networkAccessManager->get(networkRequest);
    } else {
        reply = networkAccessManager->sendCustomRequest(networkRequest, _request.verb,
                                                        _request.body);
    }

    QObject::connect(reply, &QNetworkReply::finished, q, [this, reply, _request]() mutable {
        reply->deleteLater();
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        if (httpStatus >= 200 && httpStatus < 300) {
            _request.callback(body, {});
            return;
        }

        const auto error = parseError(httpStatus, reply->error(), reply->errorString(), body);

        //
        // Access token is expired or revoked: refresh it once and repeat the request
        //
        if (error.type == GoogleDriveError::Type::Unauthorized && tokenRefresher
            && !_request.isTokenRefreshed) {
            _request.isTokenRefreshed = true;
            QPointer<GoogleDriveClient> client(q);
            tokenRefresher([this, client, _request, error](bool _refreshed) {
                if (client.isNull()) {
                    return;
                }
                if (_refreshed) {
                    send(_request);
                } else {
                    _request.callback({}, error);
                }
            });
            return;
        }

        if (isRetryable(error) && _request.attempt < maxRetries) {
            const int delay = baseDelayMs * (1 << _request.attempt);
            ++_request.attempt;
            QTimer::singleShot(delay, q, [this, _request] { send(_request); });
            return;
        }

        _request.callback({}, error);
    });
}

void GoogleDriveClient::Implementation::sendForFile(const QByteArray& _verb, const QUrl& _url,
                                                    const QByteArray& _contentType,
                                                    const QByteArray& _body,
                                                    const FileCallback& _callback)
{
    Request request;
    request.verb = _verb;
    request.url = _url;
    request.contentType = _contentType;
    request.body = _body;
    request.callback = [_callback](const QByteArray& _response, const GoogleDriveError& _error) {
        if (_error.isError()) {
            _callback({}, _error);
            return;
        }
        const auto file = GoogleDriveFile::fromJson(QJsonDocument::fromJson(_response).object());
        if (!file.isValid()) {
            _callback({}, invalidResponse(_response));
            return;
        }
        _callback(file, {});
    };
    send(request);
}


// ****


GoogleDriveClient::GoogleDriveClient(QNetworkAccessManager* _networkAccessManager,
                                     QObject* _parent)
    : QObject(_parent)
    , d(new Implementation(this, _networkAccessManager))
{
}

GoogleDriveClient::~GoogleDriveClient() = default;

void GoogleDriveClient::setAccessTokenProvider(const AccessTokenProvider& _provider)
{
    d->accessTokenProvider = _provider;
}

void GoogleDriveClient::setTokenRefresher(const TokenRefresher& _refresher)
{
    d->tokenRefresher = _refresher;
}

void GoogleDriveClient::setRetryPolicy(int _maxRetries, int _baseDelayMs)
{
    d->maxRetries = std::max(0, _maxRetries);
    d->baseDelayMs = std::max(0, _baseDelayMs);
}

void GoogleDriveClient::findFiles(const QString& _query, const FilesCallback& _callback)
{
    Implementation::Request request;
    request.verb = "GET";
    request.url = makeUrl(kFilesUrl,
                          {
                              { "q", _query },
                              { "spaces", "drive" },
                              { "pageSize", "100" },
                              { "fields", QString("files(%1)").arg(kFileFields) },
                          });
    request.callback = [_callback](const QByteArray& _response, const GoogleDriveError& _error) {
        if (_error.isError()) {
            _callback({}, _error);
            return;
        }
        const auto json = QJsonDocument::fromJson(_response).object();
        if (!json.value("files").isArray()) {
            _callback({}, invalidResponse(_response));
            return;
        }
        QVector<GoogleDriveFile> files;
        for (const auto& file : json.value("files").toArray()) {
            files.append(GoogleDriveFile::fromJson(file.toObject()));
        }
        _callback(files, {});
    };
    d->send(request);
}

void GoogleDriveClient::findOrCreateFolder(const QString& _name, const QString& _parentId,
                                           const FileCallback& _callback)
{
    const QString parentId = _parentId.isEmpty() ? QStringLiteral("root") : _parentId;
    const QString query
        = QString("mimeType = '%1' and name = '%2' and '%3' in parents and trashed = false")
              .arg(kFolderMimeType, escapeQueryValue(_name), escapeQueryValue(parentId));
    QPointer<GoogleDriveClient> client(this);
    findFiles(query,
              [this, client, _name, parentId, _callback](const QVector<GoogleDriveFile>& _files,
                                                          const GoogleDriveError& _error) {
                  if (client.isNull()) {
                      return;
                  }
                  if (_error.isError()) {
                      _callback({}, _error);
                      return;
                  }
                  if (!_files.isEmpty()) {
                      _callback(_files.constFirst(), {});
                      return;
                  }

                  QJsonObject metadata;
                  metadata.insert("name", _name);
                  metadata.insert("mimeType", kFolderMimeType);
                  metadata.insert("parents", QJsonArray{ parentId });
                  d->sendForFile("POST", makeUrl(kFilesUrl, { { "fields", kFileFields } }),
                                 "application/json; charset=UTF-8",
                                 QJsonDocument(metadata).toJson(QJsonDocument::Compact),
                                 _callback);
              });
}

void GoogleDriveClient::createDocument(const QString& _name, const QString& _parentId,
                                       const QByteArray& _content,
                                       const QString& _contentMimeType,
                                       const QHash<QString, QString>& _appProperties,
                                       const FileCallback& _callback)
{
    QJsonObject metadata;
    metadata.insert("name", _name);
    metadata.insert("mimeType", kGoogleDocumentMimeType);
    if (!_parentId.isEmpty()) {
        metadata.insert("parents", QJsonArray{ _parentId });
    }
    if (!_appProperties.isEmpty()) {
        metadata.insert("appProperties", toJson(_appProperties));
    }

    //
    // Multipart upload: JSON metadata and the media in one multipart/related request
    //
    const QByteArray boundary
        = "jenny_script_" + QUuid::createUuid().toString(QUuid::Id128).toLatin1();
    QByteArray body;
    body += "--" + boundary + "\r\n";
    body += "Content-Type: application/json; charset=UTF-8\r\n\r\n";
    body += QJsonDocument(metadata).toJson(QJsonDocument::Compact) + "\r\n";
    body += "--" + boundary + "\r\n";
    body += "Content-Type: " + _contentMimeType.toUtf8() + "\r\n\r\n";
    body += _content + "\r\n";
    body += "--" + boundary + "--\r\n";

    d->sendForFile("POST",
                   makeUrl(kUploadUrl, { { "uploadType", "multipart" }, { "fields", kFileFields } }),
                   "multipart/related; boundary=" + boundary, body, _callback);
}

void GoogleDriveClient::updateDocumentContent(const QString& _fileId, const QByteArray& _content,
                                              const QString& _contentMimeType,
                                              const FileCallback& _callback)
{
    d->sendForFile("PATCH",
                   makeUrl(fileUrl(kUploadUrl, _fileId),
                           { { "uploadType", "media" }, { "fields", kFileFields } }),
                   _contentMimeType.toUtf8(), _content, _callback);
}

void GoogleDriveClient::updateAppProperties(const QString& _fileId,
                                            const QHash<QString, QString>& _appProperties,
                                            const FileCallback& _callback)
{
    QJsonObject metadata;
    metadata.insert("appProperties", toJson(_appProperties));
    d->sendForFile("PATCH",
                   makeUrl(fileUrl(kFilesUrl, _fileId),
                           { { "fields", kFileFields } }),
                   "application/json; charset=UTF-8",
                   QJsonDocument(metadata).toJson(QJsonDocument::Compact), _callback);
}

void GoogleDriveClient::fileMetadata(const QString& _fileId, const FileCallback& _callback)
{
    d->sendForFile("GET",
                   makeUrl(fileUrl(kFilesUrl, _fileId),
                           { { "fields", kFileFields } }),
                   {}, {}, _callback);
}

void GoogleDriveClient::exportFile(const QString& _fileId, const QString& _mimeType,
                                   const DataCallback& _callback)
{
    Implementation::Request request;
    request.verb = "GET";
    request.url = makeUrl(fileUrl(kFilesUrl, _fileId, "/export"),
                          { { "mimeType", _mimeType } });
    request.callback = _callback;
    d->send(request);
}

QString GoogleDriveClient::escapeQueryValue(const QString& _value)
{
    QString escaped = _value;
    escaped.replace('\\', QStringLiteral("\\\\"));
    escaped.replace('\'', QStringLiteral("\\'"));
    return escaped;
}

} // namespace ManagementLayer
