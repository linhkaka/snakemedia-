#include "screenplay_dossier.h"

#include <algorithm>


namespace BusinessLayer {

QByteArray screenplayDossierHtml(const QVector<DossierSection>& _sections)
{
    const auto kParagraphStyle
        = QStringLiteral("font-family:'Courier New';font-size:12pt;margin:0");

    QString body;
    for (const auto& section : _sections) {
        const bool hasText = std::any_of(section.paragraphs.cbegin(), section.paragraphs.cend(),
                                         [](const DossierParagraph& _paragraph) {
                                             return !_paragraph.text.trimmed().isEmpty();
                                         });
        if (!hasText) {
            continue;
        }

        body += QStringLiteral("<h1 style=\"font-family:'Courier New';font-size:14pt%1\">%2</h1>\n")
                    .arg(body.isEmpty() ? QString() : QStringLiteral(";page-break-before:always"),
                         section.title.toHtmlEscaped());
        for (const auto& paragraph : section.paragraphs) {
            const auto text = paragraph.text.trimmed().isEmpty() ? QStringLiteral("&nbsp;")
                                                                 : paragraph.text.toHtmlEscaped();
            body += QStringLiteral("<p style=\"%1\">%2</p>\n")
                        .arg(kParagraphStyle,
                             paragraph.isHeading ? QStringLiteral("<b>%1</b>").arg(text) : text);
        }
    }
    if (body.isEmpty()) {
        return {};
    }

    return QStringLiteral("<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\"></head><body>\n%1"
                          "</body></html>\n")
        .arg(body)
        .toUtf8();
}

} // namespace BusinessLayer
