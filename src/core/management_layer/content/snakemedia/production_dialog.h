#pragma once

#include "production_brief.h"

#include <QDialog>
#include <QByteArray>

class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

namespace SnakeMedia {
class ProductionDialog : public QDialog
{
public:
    explicit ProductionDialog(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;
    void reject() override;

private:
    ProductionBrief readForm() const;
    void fillForm(const ProductionBrief& brief);
    bool confirmUnsaved();
    bool saveBrief(bool choosePath = false);
    void openBrief();
    void importScript();
    void exportPack();
    void sendToStudio();
    void error(const QString& message);

    ProductionBrief m_brief;
    QString m_path;
    QByteArray m_saved;
    QLineEdit *m_title, *m_channel, *m_slot;
    QComboBox *m_market, *m_genre, *m_status;
    QPlainTextEdit *m_logline, *m_script, *m_characters, *m_locations, *m_notes;
    QLabel* m_state;
    QPushButton* m_ai;
};
} // namespace SnakeMedia
