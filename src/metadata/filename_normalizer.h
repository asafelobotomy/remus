#ifndef FILENAME_NORMALIZER_H
#define FILENAME_NORMALIZER_H

#include <QString>

namespace Remus {
namespace Metadata {

/**
 * @brief Utility class for normalizing ROM filenames for metadata matching
 * 
 * Removes file extensions, region tags, version tags, and other metadata
 * to produce clean game titles suitable for provider searches.
 */
class FilenameNormalizer
{
public:
    /**
     * @brief Normalize a ROM filename to a clean game title
     * 
     * Performs the following transformations:
     * - Removes file extension (.md, .smc, .cue, etc.)
     * - Removes region tags in parentheses: (USA), (Europe), (Japan), etc.
     * - Removes version/revision tags in brackets: [!], [b1], [Classics], etc.
     * - Replaces underscores and dots with spaces
     * - Trims and removes extra whitespace
     * 
     * Example: "Sonic The Hedgehog (USA, Europe).md" -> "Sonic The Hedgehog"
     * 
     * @param filename The original ROM filename
     * @return Normalized game title suitable for metadata search
     */
    static QString normalize(const QString &filename);

private:
    FilenameNormalizer() = delete; // Static utility class
};

} // namespace Metadata
} // namespace Remus

#endif // FILENAME_NORMALIZER_H
