#include "filename_normalizer.h"
#include <QRegularExpression>

namespace Remus {
namespace Metadata {

QString FilenameNormalizer::normalize(const QString &filename)
{
    if (filename.isEmpty()) {
        return filename;
    }

    QString cleaned = filename;

    // Step 1: Remove file extension
    // Find the last dot and remove everything after it
    int dotPos = cleaned.lastIndexOf('.');
    if (dotPos != -1) {
        cleaned = cleaned.left(dotPos);
    }

    // Step 2: Remove tags in parentheses (regions, languages, etc.)
    // Examples: (USA), (Europe), (Japan), (En,Fr,De), (Rev 1), etc.
    QRegularExpression parenRegex("\\s*\\([^)]*\\)");
    cleaned.remove(parenRegex);

    // Step 3: Remove tags in square brackets (special versions, fixes, etc.)
    // Examples: [!], [b1], [Classics], [Automap], [h1], [t1], etc.
    QRegularExpression bracketRegex("\\s*\\[[^\\]]*\\]");
    cleaned.remove(bracketRegex);

    // Step 4: Replace underscores and dots with spaces
    // Some ROM naming conventions use underscores instead of spaces
    cleaned.replace('_', ' ');
    cleaned.replace('.', ' ');

    // Step 5: Remove extra whitespace and trim
    // Collapses multiple spaces into one and removes leading/trailing spaces
    cleaned = cleaned.simplified();

    return cleaned;
}

} // namespace Metadata
} // namespace Remus
