#include "production_brief.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStringList>
#include <QUuid>

namespace SnakeMedia {
namespace {
constexpr qint64 kMaxBytes = 2 * 1024 * 1024;
const QStringList kFields{ "id", "title", "channel", "slot", "market", "genre", "status", "logline",
                           "script", "characters", "locations", "notes" };
} // namespace

ProductionBrief ProductionBrief::create()
{
    ProductionBrief brief;
    brief.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return brief;
}

QJsonObject ProductionBrief::toJson() const
{
    return { { "schema", 1 }, { "id", id }, { "title", title }, { "channel", channel },
             { "slot", slot }, { "market", market }, { "genre", genre }, { "status", status },
             { "logline", logline }, { "script", script }, { "characters", characters },
             { "locations", locations }, { "notes", notes } };
}

bool ProductionBrief::validate(QString& error) const
{
    if (QUuid(id).isNull() || title.trimmed().isEmpty() || title.size() > 200) {
        error = QStringLiteral("Tên tập không được trống, tối đa 200 ký tự; mã hồ sơ phải hợp lệ.");
        return false;
    }
    if (!QStringList{ "drama", "lesson", "info", "documentary", "auto" }.contains(genre)
        || !QStringList{ "draft", "review", "ready", "published" }.contains(status)) {
        error = QStringLiteral("Thể loại hoặc trạng thái sản xuất không hợp lệ.");
        return false;
    }
    if (!QStringList{ "VN", "KR", "JP", "ZH", "PH", "US", "DE", "FR", "ES", "IT", "NL", "PL", "PT", "PT-PT", "TR" }.contains(market)
        || channel.size() > 200 || slot.size() > 100 || logline.size() > 20000
        || script.size() > 500000 || characters.size() > 500000 || locations.size() > 500000 || notes.size() > 500000) {
        error = QStringLiteral("Thị trường không hợp lệ hoặc trường nội dung vượt giới hạn bàn giao.");
        return false;
    }
    if (QJsonDocument(toJson()).toJson(QJsonDocument::Compact).size() > kMaxBytes) {
        error = QStringLiteral("Hồ sơ vượt 2 MB. Hãy chia thành nhiều tập.");
        return false;
    }
    error.clear();
    return true;
}

bool ProductionBrief::fromJson(const QJsonObject& object, ProductionBrief& brief, QString& error)
{
    if (object.value("schema").toInt() != 1) {
        error = QStringLiteral("Chỉ đọc hồ sơ SNAKEMEDIA schema 1 (*.snake.json).");
        return false;
    }
    for (const auto& field : kFields) {
        if (!object.value(field).isString()) {
            error = QStringLiteral("Trường dữ liệu không hợp lệ: %1").arg(field);
            return false;
        }
    }
    ProductionBrief value;
    value.id = object.value("id").toString();
    value.title = object.value("title").toString();
    value.channel = object.value("channel").toString();
    value.slot = object.value("slot").toString();
    value.market = object.value("market").toString();
    value.genre = object.value("genre").toString();
    value.status = object.value("status").toString();
    value.logline = object.value("logline").toString();
    value.script = object.value("script").toString();
    value.characters = object.value("characters").toString();
    value.locations = object.value("locations").toString();
    value.notes = object.value("notes").toString();
    if (!value.validate(error)) {
        return false;
    }
    brief = value;
    return true;
}

bool ProductionBrief::save(const QString& path, QString& error) const
{
    if (!validate(error)) {
        return false;
    }
    QSaveFile file(path);
    const auto bytes = QJsonDocument(toJson()).toJson(QJsonDocument::Compact);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        error = QStringLiteral("Không lưu được hồ sơ: %1").arg(file.errorString());
        return false;
    }
    return true;
}

bool ProductionBrief::load(const QString& path, ProductionBrief& brief, QString& error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > kMaxBytes) {
        error = QStringLiteral("Không đọc được file hoặc file vượt 2 MB.");
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("File hồ sơ không phải JSON hợp lệ.");
        return false;
    }
    return fromJson(document.object(), brief, error);
}

QJsonObject ProductionBrief::toStudioProject() const
{
    const auto note = [](const QString& title, const QString& text) {
        return QJsonObject{ { "id", QUuid::createUuid().toString(QUuid::Id128) },
                            { "title", title }, { "text", text }, { "url", "" } };
    };
    return { { "title", title }, { "channel", channel }, { "market", market }, { "genre", genre },
             { "status", status == "published" ? "ready" : status }, { "synopsis", logline },
             { "goal", slot },
             { "scenes", QJsonArray{ QJsonObject{
                 { "id", QUuid::createUuid().toString(QUuid::Id128) }, { "title", "Bản thảo desktop" },
                 { "text", script }, { "status", "draft" } } } },
             { "notes", QJsonArray{ note(QStringLiteral("Khóa nhân vật"), characters),
                                    note(QStringLiteral("Khóa bối cảnh"), locations),
                                    note(QStringLiteral("Ghi chú sản xuất"), notes) } } };
}

QString ProductionBrief::handoffText() const
{
    return QStringLiteral("SNAKEMEDIA — HỒ SƠ BÀN GIAO\nTập: %1\nKênh: %2\nSlot: %3\n"
                          "Thị trường: %4\nThể loại: %5\nTrạng thái: %6\n\nTÓM TẮT\n%7\n\n"
                          "KHÓA NHÂN VẬT\n%8\n\nKHÓA BỐI CẢNH\n%9\n\nGHI CHÚ\n%10\n")
        .arg(title, channel, slot, market, genre, status, logline, characters, locations, notes);
}
} // namespace SnakeMedia
