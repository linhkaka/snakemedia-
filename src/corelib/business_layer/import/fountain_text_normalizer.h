#pragma once

#include <corelib_global.h>

#include <QString>

namespace BusinessLayer {

/**
 * @brief Clean up Fountain text copied or exported from other apps (Google Docs with Fountainize,
 *        word processors) before it is parsed
 *
 * - removes BOM, carriage returns, converts soft line breaks and non-breaking spaces;
 * - removes scene numbers written as plain text around a scene heading
 *   ("1 EXT. ROAD - NIGHT 1" -> "EXT. ROAD - NIGHT"), the app numbers scenes itself;
 * - forces Vietnamese scene headings ("NỘI.", "NGOẠI.", "NỘI/NGOẠI.") with the Fountain "."
 *   prefix, because the parser recognizes only the INT/EXT family.
 */
class CORE_LIBRARY_EXPORT FountainTextNormalizer
{
public:
    static QString normalize(const QString& _text);

    /**
     * @brief Prepare plain text for the Fountain parser, which needs blank lines between elements
     *
     * Text with hardly any blank lines (one paragraph per line, like a script pasted from a chat
     * or a word processor) gets them back: scene headings and action lines stand alone, a
     * character cue keeps its parentheticals and one dialogue line. Other text is only normalized.
     */
    static QString formatPlainText(const QString& _text);
};

} // namespace BusinessLayer
