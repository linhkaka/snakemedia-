#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

#include <corelib_global.h>

namespace BusinessLayer {

struct DossierParagraph {
    QString text;
    bool isHeading = false;
};

struct DossierSection {
    QString title;
    QVector<DossierParagraph> paragraphs;
};

/**
 * @brief Jenny Script fork: HTML of the Google Doc with the parts of a screenplay other than its
 *        text. Google Docs converts HTML to a Doc, the part titles appear in its outline.
 * @return empty array when no section has text, such a section is left out
 */
CORE_LIBRARY_EXPORT QByteArray screenplayDossierHtml(const QVector<DossierSection>& _sections);

} // namespace BusinessLayer
