#include "google_drive_client.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimeZone>
#include <QTimer>
#include <QUrlQuery>
#include <QtTest>

#include <cstring>

using ManagementLayer::GoogleDriveClient;
using ManagementLayer::GoogleDriveError;
using ManagementLayer::GoogleDriveFile;

namespace {

/**
 * @brief Reply with a canned HTTP status and body, finished asynchronously like a real one
 */
class FakeReply : public QNetworkReply
{
public:
    FakeReply(const QNetworkRequest& _request, QNetworkAccessManager::Operation _operation,
              int _status, const QByteArray& _body, NetworkError _error, QObject* _parent)
        : QNetworkReply(_parent)
        , m_data(_body)
    {
        setRequest(_request);
        setOperation(_operation);
        setUrl(_request.url());
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        if (_status > 0) {
            setAttribute(QNetworkRequest::HttpStatusCodeAttribute, _status);
        }
        if (_error != NoError) {
            setError(_error, QStringLiteral("fake network error"));
        }
        QTimer::singleShot(0, this, [this, _error] {
            if (_error != NoError) {
                emit errorOccurred(_error);
            }
            setFinished(true);
            emit finished();
        });
    }

    void abort() override
    {
    }
    bool isSequential() const override
    {
        return true;
    }
    qint64 bytesAvailable() const override
    {
        return m_data.size() - m_position + QIODevice::bytesAvailable();
    }

protected:
    qint64 readData(char* _data, qint64 _maxSize) override
    {
        const qint64 size = qMin(_maxSize, qint64(m_data.size()) - m_position);
        if (size <= 0) {
            return 0;
        }
        std::memcpy(_data, m_data.constData() + m_position, size);
        m_position += size;
        return size;
    }

private:
    QByteArray m_data;
    qint64 m_position = 0;
};

struct CannedResponse {
    int status = 200;
    QByteArray body;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
};

struct RecordedRequest {
    QByteArray verb;
    QUrl url;
    QNetworkRequest request;
    QByteArray body;

    QString query(const QString& _key) const
    {
        return QUrlQuery(url).queryItemValue(_key, QUrl::FullyDecoded);
    }
};

/**
 * @brief Network access manager that records requests and answers with canned responses
 */
class FakeNetworkAccessManager : public QNetworkAccessManager
{
public:
    QVector<RecordedRequest> requests;
    QList<CannedResponse> responses;

protected:
    QNetworkReply* createRequest(Operation _operation, const QNetworkRequest& _request,
                                 QIODevice* _data) override
    {
        RecordedRequest recorded;
        recorded.url = _request.url();
        recorded.request = _request;
        switch (_operation) {
        case GetOperation:
            recorded.verb = "GET";
            break;
        case PostOperation:
            recorded.verb = "POST";
            break;
        case PutOperation:
            recorded.verb = "PUT";
            break;
        case DeleteOperation:
            recorded.verb = "DELETE";
            break;
        case CustomOperation:
            recorded.verb = _request.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray();
            break;
        default:
            recorded.verb = "?";
            break;
        }
        if (_data != nullptr) {
            recorded.body = _data->readAll();
        }
        requests.append(recorded);

        const auto response = responses.isEmpty()
            ? CannedResponse{ 500, R"({"error":{"code":500,"message":"no canned response"}})" }
            : responses.takeFirst();
        auto error = response.error;
        if (error == QNetworkReply::NoError && response.status >= 400) {
            error = QNetworkReply::UnknownContentError;
        }
        return new FakeReply(_request, _operation, response.status, response.body, error, this);
    }
};

QByteArray fileJson(const QString& _id, const QString& _name = QStringLiteral("Doc"))
{
    QJsonObject json{
        { "id", _id },
        { "name", _name },
        { "mimeType", GoogleDriveClient::kGoogleDocumentMimeType },
        { "modifiedTime", "2026-11-03T10:15:30.123Z" },
        { "version", "42" },
        { "webViewLink", "https://docs.google.com/document/d/" + _id + "/edit" },
        { "appProperties", QJsonObject{ { "documentUuid", "abc" } } },
    };
    return QJsonDocument(json).toJson(QJsonDocument::Compact);
}

QByteArray errorJson(int _code, const QString& _reason)
{
    QJsonObject error{
        { "code", _code },
        { "message", "error message" },
        { "errors", QJsonArray{ QJsonObject{ { "reason", _reason } } } },
    };
    return QJsonDocument(QJsonObject{ { "error", error } }).toJson(QJsonDocument::Compact);
}

} // namespace


class GoogleDriveClientTest : public QObject
{
    Q_OBJECT

private:
    struct FileResult {
        bool done = false;
        GoogleDriveFile file;
        GoogleDriveError error;
    };

    void prepareClient(FakeNetworkAccessManager& _nam, GoogleDriveClient& _client)
    {
        Q_UNUSED(_nam)
        _client.setAccessTokenProvider([] { return QStringLiteral("token-1"); });
        _client.setRetryPolicy(3, 1);
    }

    GoogleDriveClient::FileCallback store(FileResult& _result)
    {
        return [&_result](const GoogleDriveFile& _file, const GoogleDriveError& _error) {
            _result.file = _file;
            _result.error = _error;
            _result.done = true;
        };
    }

private slots:
    void createDocumentSendsMultipartUpload()
    {
        FakeNetworkAccessManager nam;
        nam.responses = { { 200, fileJson("doc-1", "Cảnh 1") } };
        GoogleDriveClient client(&nam);
        prepareClient(nam, client);

        FileResult result;
        const QByteArray content = "EXT. ĐƯỜNG ĐÈO - ĐÊM\n\nRừng thông đen kịt.";
        client.createDocument("Kịch bản", "folder-1", content, "text/plain; charset=UTF-8",
                              { { "documentUuid", "abc" } }, store(result));
        QTRY_VERIFY(result.done);

        QVERIFY(!result.error.isError());
        QCOMPARE(result.file.id, QStringLiteral("doc-1"));
        QCOMPARE(result.file.name, QStringLiteral("Cảnh 1"));
        QCOMPARE(result.file.version, qint64(42));
        QCOMPARE(result.file.appProperties.value("documentUuid"), QStringLiteral("abc"));
        QCOMPARE(result.file.modifiedTime,
                 QDateTime(QDate(2026, 11, 3), QTime(10, 15, 30, 123), QTimeZone::UTC));

        QCOMPARE(nam.requests.size(), 1);
        const auto& request = nam.requests.constFirst();
        QCOMPARE(request.verb, QByteArray("POST"));
        QCOMPARE(request.url.host(), QStringLiteral("www.googleapis.com"));
        QCOMPARE(request.url.path(), QStringLiteral("/upload/drive/v3/files"));
        QCOMPARE(request.query("uploadType"), QStringLiteral("multipart"));
        QCOMPARE(request.request.rawHeader("Authorization"), QByteArray("Bearer token-1"));

        const auto contentType
            = request.request.header(QNetworkRequest::ContentTypeHeader).toByteArray();
        QVERIFY(contentType.startsWith("multipart/related; boundary="));
        const QByteArray boundary = contentType.mid(contentType.indexOf('=') + 1);
        QVERIFY(request.body.startsWith("--" + boundary + "\r\n"));
        QVERIFY(request.body.endsWith("--" + boundary + "--\r\n"));
        QVERIFY(request.body.contains(content));

        const int jsonStart = request.body.indexOf('{');
        const int jsonEnd = request.body.indexOf("\r\n", jsonStart);
        const auto metadata
            = QJsonDocument::fromJson(request.body.mid(jsonStart, jsonEnd - jsonStart)).object();
        QCOMPARE(metadata.value("name").toString(), QStringLiteral("Kịch bản"));
        QCOMPARE(metadata.value("mimeType").toString(), GoogleDriveClient::kGoogleDocumentMimeType);
        QCOMPARE(metadata.value("parents").toArray(), QJsonArray{ "folder-1" });
        QCOMPARE(metadata.value("appProperties").toObject().value("documentUuid").toString(),
                 QStringLiteral("abc"));
    }

    void updateDocumentContentUsesPatchMedia()
    {
        FakeNetworkAccessManager nam;
        nam.responses = { { 200, fileJson("doc/1") } };
        GoogleDriveClient client(&nam);
        prepareClient(nam, client);

        FileResult result;
        client.updateDocumentContent("doc/1", "new text", "text/plain; charset=UTF-8",
                                     store(result));
        QTRY_VERIFY(result.done);

        QVERIFY(!result.error.isError());
        const auto& request = nam.requests.constFirst();
        QCOMPARE(request.verb, QByteArray("PATCH"));
        QCOMPARE(request.url.path(QUrl::FullyEncoded),
                 QStringLiteral("/upload/drive/v3/files/doc%2F1"));
        QCOMPARE(request.query("uploadType"), QStringLiteral("media"));
        QCOMPARE(request.body, QByteArray("new text"));
        QCOMPARE(request.request.header(QNetworkRequest::ContentTypeHeader).toByteArray(),
                 QByteArray("text/plain; charset=UTF-8"));
    }

    void updateAppPropertiesSendsJson()
    {
        FakeNetworkAccessManager nam;
        nam.responses = { { 200, fileJson("doc-1") } };
        GoogleDriveClient client(&nam);
        prepareClient(nam, client);

        FileResult result;
        client.updateAppProperties("doc-1", { { "lockedBy", "a@b.c" } }, store(result));
        QTRY_VERIFY(result.done);

        const auto& request = nam.requests.constFirst();
        QCOMPARE(request.verb, QByteArray("PATCH"));
        QCOMPARE(request.url.path(), QStringLiteral("/drive/v3/files/doc-1"));
        const auto json = QJsonDocument::fromJson(request.body).object();
        QCOMPARE(json.value("appProperties").toObject().value("lockedBy").toString(),
                 QStringLiteral("a@b.c"));
    }

    void exportFileReturnsData()
    {
        FakeNetworkAccessManager nam;
        const QByteArray text = "\xEF\xBB\xBFINT. XE QUANG - ĐÊM\r\n";
        nam.responses = { { 200, text } };
        GoogleDriveClient client(&nam);
        prepareClient(nam, client);

        bool done = false;
        QByteArray data;
        client.exportFile("doc-1", "text/plain",
                          [&](const QByteArray& _data, const GoogleDriveError& _error) {
                              QVERIFY(!_error.isError());
                              data = _data;
                              done = true;
                          });
        QTRY_VERIFY(done);

        QCOMPARE(data, text);
        const auto& request = nam.requests.constFirst();
        QCOMPARE(request.verb, QByteArray("GET"));
        QCOMPARE(request.url.path(), QStringLiteral("/drive/v3/files/doc-1/export"));
        QCOMPARE(request.query("mimeType"), QStringLiteral("text/plain"));
    }

    void fileMetadataRequestsFields()
    {
        FakeNetworkAccessManager nam;
        nam.responses = { { 200, fileJson("doc-1") } };
        GoogleDriveClient client(&nam);
        prepareClient(nam, client);

        FileResult result;
        client.fileMetadata("doc-1", store(result));
        QTRY_VERIFY(result.done);

        QCOMPARE(result.file.webViewLink,
                 QStringLiteral("https://docs.google.com/document/d/doc-1/edit"));
        const auto fields = nam.requests.constFirst().query("fields");
        QVERIFY(fields.contains("modifiedTime"));
        QVERIFY(fields.contains("appProperties"));
    }

    void unauthorizedRefreshesTokenOnce()
    {
        FakeNetworkAccessManager nam;
        nam.responses = { { 401, errorJson(401, "authError") }, { 200, fileJson("doc-1") } };
        GoogleDriveClient client(&nam);
        prepareClient(nam, client);

        QString token = QStringLiteral("old-token");
        int refreshCount = 0;
        client.setAccessTokenProvider([&token] { return token; });
        client.setTokenRefresher([&](std::function<void(bool)> _done) {
            ++refreshCount;
            token = QStringLiteral("new-token");
            _done(true);
        });

        FileResult result;
        client.fileMetadata("doc-1", store(result));
        QTRY_VERIFY(result.done);

        QVERIFY(!result.error.isError());
        QCOMPARE(refreshCount, 1);
        QCOMPARE(nam.requests.size(), 2);
        QCOMPARE(nam.requests.at(0).request.rawHeader("Authorization"),
                 QByteArray("Bearer old-token"));
        QCOMPARE(nam.requests.at(1).request.rawHeader("Authorization"),
                 QByteArray("Bearer new-token"));
    }

    void missingTokenIsRefreshedBeforeTheRequest()
    {
        FakeNetworkAccessManager nam;
        nam.responses = { { 200, fileJson("doc-1") } };
        GoogleDriveClient client(&nam);
        prepareClient(nam, client);

        QString token;
        int refreshCount = 0;
        client.setAccessTokenProvider([&token] { return token; });
        client.setTokenRefresher([&](std::function<void(bool)> _done) {
            ++refreshCount;
            token = QStringLiteral("new-token");
            _done(true);
        });

        FileResult result;
        client.fileMetadata("doc-1", store(result));
        QTRY_VERIFY(result.done);

        QVERIFY(!result.error.isError());
        QCOMPARE(refreshCount, 1);
        QCOMPARE(nam.requests.size(), 1);
        QCOMPARE(nam.requests.at(0).request.rawHeader("Authorization"),
                 QByteArray("Bearer new-token"));
    }

    void missingTokenWithFailedRefreshSendsNothing()
    {
        FakeNetworkAccessManager nam;
        GoogleDriveClient client(&nam);
        prepareClient(nam, client);
        client.setAccessTokenProvider([] { return QString(); });
        client.setTokenRefresher([](std::function<void(bool)> _done) { _done(false); });

        FileResult result;
        client.fileMetadata("doc-1", store(result));
        QTRY_VERIFY(result.done);

        QCOMPARE(result.error.type, GoogleDriveError::Type::Unauthorized);
        QCOMPARE(nam.requests.size(), 0);
    }

    void unauthorizedAfterRefreshIsReported()
    {
        FakeNetworkAccessManager nam;
        nam.responses = { { 401, errorJson(401, "authError") },
                          { 401, errorJson(401, "authError") } };
        GoogleDriveClient client(&nam);
        prepareClient(nam, client);
        int refreshCount = 0;
        client.setTokenRefresher([&](std::function<void(bool)> _done) {
            ++refreshCount;
            _done(true);
        });

        FileResult result;
        client.fileMetadata("doc-1", store(result));
        QTRY_VERIFY(result.done);

        QCOMPARE(result.error.type, GoogleDriveError::Type::Unauthorized);
        QCOMPARE(result.error.httpStatus, 401);
        QCOMPARE(refreshCount, 1);
        QCOMPARE(nam.requests.size(), 2);
    }

    void failedRefreshIsReported()
    {
        FakeNetworkAccessManager nam;
        nam.responses = { { 401, errorJson(401, "authError") } };
        GoogleDriveClient client(&nam);
        prepareClient(nam, client);
        client.setTokenRefresher([](std::function<void(bool)> _done) { _done(false); });

        FileResult result;
        client.fileMetadata("doc-1", store(result));
        QTRY_VERIFY(result.done);

        QCOMPARE(result.error.type, GoogleDriveError::Type::Unauthorized);
        QCOMPARE(nam.requests.size(), 1);
    }

    void retryableErrorsAreRetried_data()
    {
        QTest::addColumn<int>("status");
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<int>("networkError");

        QTest::newRow("429") << 429 << errorJson(429, "rateLimitExceeded")
                             << int(QNetworkReply::NoError);
        QTest::newRow("403-rate-limit") << 403 << errorJson(403, "userRateLimitExceeded")
                                        << int(QNetworkReply::NoError);
        QTest::newRow("500") << 500 << errorJson(500, "backendError")
                             << int(QNetworkReply::NoError);
        QTest::newRow("503") << 503 << QByteArray() << int(QNetworkReply::NoError);
        QTest::newRow("timeout") << 0 << QByteArray() << int(QNetworkReply::TimeoutError);
    }
    void retryableErrorsAreRetried()
    {
        QFETCH(int, status);
        QFETCH(QByteArray, body);
        QFETCH(int, networkError);

        FakeNetworkAccessManager nam;
        nam.responses = { { status, body, QNetworkReply::NetworkError(networkError) },
                          { 200, fileJson("doc-1") } };
        GoogleDriveClient client(&nam);
        prepareClient(nam, client);

        FileResult result;
        client.fileMetadata("doc-1", store(result));
        QTRY_VERIFY(result.done);

        QVERIFY(!result.error.isError());
        QCOMPARE(nam.requests.size(), 2);
    }

    void serverErrorGivesUpAfterRetries()
    {
        FakeNetworkAccessManager nam;
        nam.responses = { { 503, {} }, { 503, {} }, { 503, {} } };
        GoogleDriveClient client(&nam);
        prepareClient(nam, client);
        client.setRetryPolicy(2, 1);

        FileResult result;
        client.fileMetadata("doc-1", store(result));
        QTRY_VERIFY(result.done);

        QCOMPARE(result.error.type, GoogleDriveError::Type::Server);
        QCOMPARE(result.error.httpStatus, 503);
        QCOMPARE(nam.requests.size(), 3);
    }

    void permanentErrorsAreNotRetried_data()
    {
        QTest::addColumn<int>("status");
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<int>("networkError");
        QTest::addColumn<int>("type");
        QTest::addColumn<QString>("reason");

        QTest::newRow("403") << 403 << errorJson(403, "insufficientPermissions")
                             << int(QNetworkReply::NoError)
                             << int(GoogleDriveError::Type::Forbidden)
                             << QStringLiteral("insufficientPermissions");
        QTest::newRow("404") << 404 << errorJson(404, "notFound") << int(QNetworkReply::NoError)
                             << int(GoogleDriveError::Type::NotFound) << QStringLiteral("notFound");
        QTest::newRow("400") << 400 << errorJson(400, "badRequest") << int(QNetworkReply::NoError)
                             << int(GoogleDriveError::Type::Other) << QStringLiteral("badRequest");
        QTest::newRow("offline") << 0 << QByteArray() << int(QNetworkReply::HostNotFoundError)
                                 << int(GoogleDriveError::Type::Network)
                                 << QStringLiteral("network");
    }
    void permanentErrorsAreNotRetried()
    {
        QFETCH(int, status);
        QFETCH(QByteArray, body);
        QFETCH(int, networkError);
        QFETCH(int, type);
        QFETCH(QString, reason);

        FakeNetworkAccessManager nam;
        nam.responses = { { status, body, QNetworkReply::NetworkError(networkError) },
                          { 200, fileJson("doc-1") } };
        GoogleDriveClient client(&nam);
        prepareClient(nam, client);

        FileResult result;
        client.fileMetadata("doc-1", store(result));
        QTRY_VERIFY(result.done);

        QCOMPARE(int(result.error.type), type);
        QCOMPARE(result.error.reason, reason);
        QCOMPARE(nam.requests.size(), 1);
    }

    void invalidJsonIsReported()
    {
        FakeNetworkAccessManager nam;
        nam.responses = { { 200, "<html>not json</html>" } };
        GoogleDriveClient client(&nam);
        prepareClient(nam, client);

        FileResult result;
        client.fileMetadata("doc-1", store(result));
        QTRY_VERIFY(result.done);

        QCOMPARE(result.error.type, GoogleDriveError::Type::InvalidResponse);
    }

    void findOrCreateFolderReusesExisting()
    {
        FakeNetworkAccessManager nam;
        nam.responses = { { 200, R"({"files":[{"id":"folder-1","name":"Jenny Script"}]})" } };
        GoogleDriveClient client(&nam);
        prepareClient(nam, client);

        FileResult result;
        client.findOrCreateFolder("Jenny Script", {}, store(result));
        QTRY_VERIFY(result.done);

        QCOMPARE(result.file.id, QStringLiteral("folder-1"));
        QCOMPARE(nam.requests.size(), 1);
        const auto query = nam.requests.constFirst().query("q");
        QVERIFY(query.contains("name = 'Jenny Script'"));
        QVERIFY(query.contains("'root' in parents"));
        QVERIFY(query.contains("trashed = false"));
    }

    void findOrCreateFolderCreatesMissing()
    {
        FakeNetworkAccessManager nam;
        nam.responses = { { 200, R"({"files":[]})" },
                          { 200, R"({"id":"folder-2","name":"Phim của Kha"})" } };
        GoogleDriveClient client(&nam);
        prepareClient(nam, client);

        FileResult result;
        client.findOrCreateFolder("Phim của Kha's", "parent-1", store(result));
        QTRY_VERIFY(result.done);

        QCOMPARE(result.file.id, QStringLiteral("folder-2"));
        QCOMPARE(nam.requests.size(), 2);
        QVERIFY(nam.requests.at(0).query("q").contains(R"(name = 'Phim của Kha\'s')"));
        const auto& create = nam.requests.at(1);
        QCOMPARE(create.verb, QByteArray("POST"));
        QCOMPARE(create.url.path(), QStringLiteral("/drive/v3/files"));
        const auto json = QJsonDocument::fromJson(create.body).object();
        QCOMPARE(json.value("mimeType").toString(), GoogleDriveClient::kFolderMimeType);
        QCOMPARE(json.value("parents").toArray(), QJsonArray{ "parent-1" });
    }

    void queryValueIsEscaped()
    {
        QCOMPARE(GoogleDriveClient::escapeQueryValue(R"(a'b\c)"), QStringLiteral(R"(a\'b\\c)"));
    }

    void plusInQueryIsEncoded()
    {
        FakeNetworkAccessManager nam;
        nam.responses = { { 200, R"({"files":[]})" } };
        GoogleDriveClient client(&nam);
        prepareClient(nam, client);

        bool done = false;
        client.findFiles("name = 'A+B'",
                         [&](const QVector<GoogleDriveFile>&, const GoogleDriveError&) {
                             done = true;
                         });
        QTRY_VERIFY(done);
        QVERIFY(nam.requests.constFirst().url.toString(QUrl::FullyEncoded).contains("A%2BB"));
    }
};

QTEST_GUILESS_MAIN(GoogleDriveClientTest)

#include "tst_google_drive_client.moc"
