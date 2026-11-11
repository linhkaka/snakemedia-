#include "production_dialog.h"

#include <include/snakemedia_brand.h>

#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

namespace SnakeMedia {
namespace {
QString productionFolder()
{
    const auto path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        + QStringLiteral("/SNAKEMEDIA/production");
    QDir().mkpath(path);
    return path;
}

QPushButton* button(const QString& text, QHBoxLayout* layout)
{
    auto result = new QPushButton(text);
    layout->addWidget(result);
    return result;
}
} // namespace

ProductionDialog::ProductionDialog(QWidget* parent)
    : QDialog(parent)
    , m_brief(ProductionBrief::create())
    , m_title(new QLineEdit(this))
    , m_channel(new QLineEdit(this))
    , m_slot(new QLineEdit(this))
    , m_market(new QComboBox(this))
    , m_genre(new QComboBox(this))
    , m_status(new QComboBox(this))
    , m_logline(new QPlainTextEdit(this))
    , m_script(new QPlainTextEdit(this))
    , m_characters(new QPlainTextEdit(this))
    , m_locations(new QPlainTextEdit(this))
    , m_notes(new QPlainTextEdit(this))
    , m_state(new QLabel(this))
    , m_ai(nullptr)
{
    setWindowTitle(QStringLiteral("Bàn sản xuất · SNAKEMEDIA Studio"));
    resize(980, 700);
    setMinimumSize(720, 520);
    auto layout = new QVBoxLayout(this);
    auto heading = new QLabel(QStringLiteral("SNAKEMEDIA  /  HỒ SƠ SẢN XUẤT"), this);
    heading->setStyleSheet("font-size:20px;font-weight:600;color:#397d57;padding:10px 0;");
    layout->addWidget(heading);
    auto intro = new QLabel(QStringLiteral("Hồ sơ *.snake.json lưu riêng; không thay cấu trúc dự án .starc. "
                                           "Xuất TXT/Fountain từ trình viết rồi nhập vào tab Bản thảo."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto tools = new QHBoxLayout;
    layout->addLayout(tools);
    auto fresh = button(QStringLiteral("Hồ sơ mới"), tools);
    auto open = button(QStringLiteral("Mở hồ sơ"), tools);
    auto save = button(QStringLiteral("Lưu"), tools);
    auto saveAs = button(QStringLiteral("Lưu thành…"), tools);
    auto exportButton = button(QStringLiteral("Xuất gói bàn giao"), tools);

    auto tabs = new QTabWidget(this);
    layout->addWidget(tabs, 1);
    auto formPage = new QWidget(tabs);
    auto form = new QFormLayout(formPage);
    m_title->setMaxLength(200);
    m_channel->setMaxLength(200);
    m_slot->setMaxLength(100);
    form->addRow(QStringLiteral("Tên tập / dự án"), m_title);
    form->addRow(QStringLiteral("Kênh"), m_channel);
    form->addRow(QStringLiteral("Mã slot"), m_slot);
    m_market->addItems({ "VN", "KR", "JP", "ZH", "PH", "US", "DE", "FR", "ES", "IT", "NL", "PL", "PT", "PT-PT", "TR" });
    form->addRow(QStringLiteral("Thị trường"), m_market);
    for (const auto& entry : QList<QPair<QString, QString>>{
             { QStringLiteral("Drama hư cấu"), "drama" }, { QStringLiteral("Bài học cuộc sống"), "lesson" },
             { QStringLiteral("Kiến thức / sức khỏe"), "info" }, { QStringLiteral("Tài liệu có thật"), "documentary" },
             { QStringLiteral("Tự nhận diện"), "auto" } }) {
        m_genre->addItem(entry.first, entry.second);
    }
    for (const auto& entry : QList<QPair<QString, QString>>{
             { QStringLiteral("Đang viết"), "draft" }, { QStringLiteral("Chờ duyệt"), "review" },
             { QStringLiteral("Sẵn sàng sản xuất"), "ready" }, { QStringLiteral("Đã bàn giao"), "published" } }) {
        m_status->addItem(entry.first, entry.second);
    }
    form->addRow(QStringLiteral("Thể loại"), m_genre);
    form->addRow(QStringLiteral("Trạng thái"), m_status);
    form->addRow(QStringLiteral("Logline / tóm tắt"), m_logline);
    tabs->addTab(formPage, QStringLiteral("Thông tin tập"));
    tabs->addTab(m_script, QStringLiteral("Bản thảo TXT / Fountain"));
    tabs->addTab(m_characters, QStringLiteral("Khóa nhân vật"));
    tabs->addTab(m_locations, QStringLiteral("Khóa bối cảnh"));
    tabs->addTab(m_notes, QStringLiteral("Nguồn / ghi chú duyệt"));
    m_script->setPlaceholderText(QStringLiteral("Nhập bản thảo đã xuất từ trình soạn thảo hoặc dán văn bản vào đây."));
    m_characters->setPlaceholderText(QStringLiteral("Tên cố định, tuổi, vai trò, quan hệ, động cơ, cách xưng hô…"));
    m_locations->setPlaceholderText(QStringLiteral("Quốc gia, địa điểm, thời gian, văn hóa và các chi tiết phải giữ…"));
    m_notes->setPlaceholderText(QStringLiteral("Nguồn tham khảo, chi tiết chưa kiểm chứng, yêu cầu của biên tập…"));
    auto bottom = new QHBoxLayout;
    layout->addLayout(bottom);
    auto import = button(QStringLiteral("Nhập bản thảo…"), bottom);
    m_ai = button(QStringLiteral("Đưa bản sao sang Bàn viết AI"), bottom);
    auto web = button(QStringLiteral("Mở hệ thống AI"), bottom);
    auto close = button(QStringLiteral("Đóng"), bottom);
    layout->addWidget(m_state);
    m_state->setWordWrap(true);

    connect(fresh, &QPushButton::clicked, this, [this] {
        if (confirmUnsaved()) { m_path.clear(); fillForm(ProductionBrief::create()); }
    });
    connect(open, &QPushButton::clicked, this, [this] { openBrief(); });
    connect(save, &QPushButton::clicked, this, [this] { saveBrief(); });
    connect(saveAs, &QPushButton::clicked, this, [this] { saveBrief(true); });
    connect(import, &QPushButton::clicked, this, [this] { importScript(); });
    connect(exportButton, &QPushButton::clicked, this, [this] { exportPack(); });
    connect(m_ai, &QPushButton::clicked, this, [this] { sendToStudio(); });
    connect(web, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl(QString::fromLatin1(kAiUrl))); });
    connect(close, &QPushButton::clicked, this, &ProductionDialog::reject);

    const auto last = QSettings().value("snakemedia/lastProductionFile").toString();
    ProductionBrief initial;
    QString loadError;
    if (!last.isEmpty() && ProductionBrief::load(last, initial, loadError)) {
        m_path = last;
        fillForm(initial);
    } else {
        fillForm(m_brief);
    }
}

ProductionBrief ProductionDialog::readForm() const
{
    auto brief = m_brief;
    brief.title = m_title->text().trimmed();
    brief.channel = m_channel->text().trimmed();
    brief.slot = m_slot->text().trimmed();
    brief.market = m_market->currentText();
    brief.genre = m_genre->currentData().toString();
    brief.status = m_status->currentData().toString();
    brief.logline = m_logline->toPlainText();
    brief.script = m_script->toPlainText();
    brief.characters = m_characters->toPlainText();
    brief.locations = m_locations->toPlainText();
    brief.notes = m_notes->toPlainText();
    return brief;
}

void ProductionDialog::fillForm(const ProductionBrief& brief)
{
    m_brief = brief;
    m_title->setText(brief.title); m_channel->setText(brief.channel); m_slot->setText(brief.slot);
    m_market->setCurrentText(brief.market);
    m_genre->setCurrentIndex(m_genre->findData(brief.genre));
    m_status->setCurrentIndex(m_status->findData(brief.status));
    m_logline->setPlainText(brief.logline); m_script->setPlainText(brief.script);
    m_characters->setPlainText(brief.characters); m_locations->setPlainText(brief.locations);
    m_notes->setPlainText(brief.notes);
    m_saved = QJsonDocument(readForm().toJson()).toJson(QJsonDocument::Compact);
    m_state->setText(m_path.isEmpty() ? QStringLiteral("Hồ sơ mới — bấm Lưu để chọn nơi lưu.") : m_path);
}

bool ProductionDialog::confirmUnsaved()
{
    if (QJsonDocument(readForm().toJson()).toJson(QJsonDocument::Compact) == m_saved) { return true; }
    const auto answer = QMessageBox::question(this, QStringLiteral("Bản đang sửa chưa lưu"),
        QStringLiteral("Lưu thay đổi của hồ sơ trước khi tiếp tục?"), QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    return answer == QMessageBox::Discard || (answer == QMessageBox::Save && saveBrief());
}

void ProductionDialog::error(const QString& message)
{
    m_state->setText(message);
    QMessageBox::warning(this, QStringLiteral("SNAKEMEDIA Studio"), message);
}

bool ProductionDialog::saveBrief(bool choosePath)
{
    auto brief = readForm();
    QString details;
    if (!brief.validate(details)) { error(details); return false; }
    QString path = m_path;
    if (choosePath || path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, QStringLiteral("Lưu hồ sơ sản xuất"),
            path.isEmpty() ? productionFolder() + "/" + brief.id + ".snake.json" : path,
            QStringLiteral("Hồ sơ SNAKEMEDIA (*.snake.json)"));
        if (path.isEmpty()) { return false; }
        if (!path.endsWith(".snake.json", Qt::CaseInsensitive)) { path += ".snake.json"; }
    }
    if (!brief.save(path, details)) { error(details); return false; }
    m_path = path;
    QSettings().setValue("snakemedia/lastProductionFile", path);
    fillForm(brief);
    m_state->setText(QStringLiteral("Đã lưu: %1").arg(path));
    return true;
}

void ProductionDialog::openBrief()
{
    if (!confirmUnsaved()) { return; }
    const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("Mở hồ sơ SNAKEMEDIA"), productionFolder(), "*.snake.json");
    if (path.isEmpty()) { return; }
    ProductionBrief brief;
    QString details;
    if (!ProductionBrief::load(path, brief, details)) { error(details); return; }
    m_path = path;
    QSettings().setValue("snakemedia/lastProductionFile", path);
    fillForm(brief);
}

void ProductionDialog::importScript()
{
    const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("Nhập bản thảo đã xuất"),
        productionFolder(), QStringLiteral("Văn bản (*.txt *.fountain)"));
    if (path.isEmpty()) { return; }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1500000) { error(QStringLiteral("Không đọc được file hoặc bản thảo vượt 1,5 MB.")); return; }
    if (!m_script->toPlainText().trimmed().isEmpty()
        && QMessageBox::question(this, QStringLiteral("Thay bản thảo"), QStringLiteral("Thay nội dung tab Bản thảo bằng file vừa chọn?")) != QMessageBox::Yes) { return; }
    auto text = QString::fromUtf8(file.readAll());
    if (text.startsWith(QChar(0xfeff))) { text.remove(0, 1); }
    m_script->setPlainText(text);
    m_state->setText(QStringLiteral("Đã nhập %1 — bấm Lưu để giữ bản mới.").arg(path));
}

void ProductionDialog::exportPack()
{
    if (!saveBrief()) { return; }
    const auto root = QFileDialog::getExistingDirectory(this, QStringLiteral("Chọn thư mục bàn giao"), productionFolder());
    if (root.isEmpty()) { return; }
    const auto folder = root + "/SNAKEMEDIA-" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss") + "-" + QUuid::createUuid().toString(QUuid::Id128).left(6);
    if (!QDir().mkdir(folder)) { error(QStringLiteral("Không tạo được thư mục bàn giao.")); return; }
    const auto brief = readForm();
    QString details;
    if (!brief.save(folder + "/ho_so.snake.json", details)) { error(details); return; }
    for (const auto& pair : QList<QPair<QString, QByteArray>>{
             { "kich_ban.txt", brief.script.toUtf8() }, { "brief_san_xuat.txt", brief.handoffText().toUtf8() },
             { "du_an_ban_viet.json", QJsonDocument(brief.toStudioProject()).toJson(QJsonDocument::Indented) } }) {
        QSaveFile output(folder + "/" + pair.first);
        if (!output.open(QIODevice::WriteOnly) || output.write(pair.second) != pair.second.size() || !output.commit()) {
            error(QStringLiteral("Gói chưa hoàn tất; kiểm tra quyền ghi và dung lượng ổ đĩa: %1").arg(folder)); return;
        }
    }
    m_state->setText(QStringLiteral("Đã xuất gói bàn giao: %1").arg(folder));
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

void ProductionDialog::sendToStudio()
{
    if (!saveBrief()) { return; }
    const auto brief = readForm();
    if (brief.script.trimmed().isEmpty()) { error(QStringLiteral("Chưa có bản thảo để đưa sang AI.")); return; }
    auto manager = new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl("http://127.0.0.1:8890/api/studio/import"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(15000);
    const QJsonObject payload{ { "format", "json" },
        { "text", QString::fromUtf8(QJsonDocument(brief.toStudioProject()).toJson(QJsonDocument::Compact)) } };
    auto reply = manager->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    m_ai->setEnabled(false);
    m_state->setText(QStringLiteral("Đang tạo bản sao trong Bàn viết trên máy này…"));
    connect(reply, &QNetworkReply::finished, this, [this, reply, manager] {
        m_ai->setEnabled(true);
        const auto data = QJsonDocument::fromJson(reply->readAll()).object();
        const auto id = data.value("project").toObject().value("id").toString();
        if (reply->error() != QNetworkReply::NoError
            || !QRegularExpression("^[0-9a-f]{32}$").match(id).hasMatch()) {
            error(QStringLiteral("Chưa chuyển được bản sao. Hãy mở CHAY TOOL.bat của SNAKEMEDIA VOICE "
                "ở cổng 8890 rồi thử lại. Hồ sơ desktop đã được lưu. Chi tiết: %1")
                .arg(data.value("error").toString(reply->errorString())));
        } else {
            m_state->setText(QStringLiteral("Đã tạo bản sao. Mọi lần bấm tạo một dự án Bàn viết mới."));
            QDesktopServices::openUrl(QUrl(QString::fromLatin1(kAiUrl) + "?project=" + id));
        }
        reply->deleteLater();
        manager->deleteLater();
    });
}

void ProductionDialog::closeEvent(QCloseEvent* event)
{
    if (confirmUnsaved()) { event->accept(); } else { event->ignore(); }
}

void ProductionDialog::reject()
{
    if (confirmUnsaved()) { QDialog::reject(); }
}
} // namespace SnakeMedia
