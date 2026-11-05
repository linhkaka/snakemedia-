#pragma once

#include "screenplay_docx_importer.h"

#include <QHash>
#include <QString>

namespace BusinessLayer {

class TextModel;

/**
 * @brief Jenny Script fork: reads back a screenplay that was pushed to Google Docs
 *
 * Paragraph types are not stored in a Google Doc. A paragraph that still has the text it has in the
 * app keeps its type, a new or edited one is detected from its indent and its text. Unlike the
 * generic DOCX importer it knows Vietnamese headings like "NGOẠI." and "CẢNH 1:".
 */
class CORE_LIBRARY_EXPORT ScreenplayGoogleDocImporter : public ScreenplayDocxImporter
{
public:
    /**
     * @brief Remember the types of the paragraphs of the screenplay in the app
     */
    void addKnownParagraphs(const TextModel* _model);

    /**
     * @brief Remember a paragraph type, a text used with two different types is not trusted
     */
    void addKnownParagraph(const QString& _text, TextParagraphType _type);

protected:
    TextParagraphType typeForTextCursor(const QTextCursor& _cursor,
                                        TextParagraphType _lastBlockType, int _prevEmptyLines,
                                        int _minLeftMargin) const override;

    /**
     * @brief Google exports text colors everywhere, they must not become review marks
     */
    void writeReviewMarks(QXmlStreamWriter& _writer, QTextCursor& _cursor) const override;

private:
    QHash<QString, TextParagraphType> m_knownParagraphs;
};

} // namespace BusinessLayer
