#ifndef REMUS_VERIFICATION_ENGINE_H
#define REMUS_VERIFICATION_ENGINE_H

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include "dat_parser.h"
#include "database.h"

namespace Remus {

/**
 * @brief Verification status for a ROM
 */
enum class VerificationStatus {
    Unknown,        // Not yet verified
    Verified,       // Hash matches DAT entry
    Mismatch,       // Hash doesn't match any DAT entry
    NotInDat,       // ROM not found in DAT file
    HashMissing,    // ROM has no hash calculated yet
    Corrupt,        // File cannot be read or is damaged
    HeaderMismatch  // Header-stripped hash matches but raw doesn't (informational)
};

/**
 * @brief Single verification result
 */
struct VerificationResult {
    int fileId = 0;
    QString filePath;
    QString filename;
    QString system;
    VerificationStatus status = VerificationStatus::Unknown;
    
    // Matched DAT entry (if verified)
    QString datName;           // Name from DAT
    QString datRomName;        // Expected ROM filename
    QString datDescription;    // Description from DAT
    
    // Hash comparison details
    QString fileHash;          // Hash from file
    QString datHash;           // Hash from DAT
    QString hashType;          // "crc32", "md5", "sha1"
    
    bool headerStripped = false;  // Whether header was stripped for verification
    QString notes;             // Additional notes
};

/**
 * @brief Summary of verification run
 */
struct VerificationSummary {
    int totalFiles = 0;
    int verified = 0;
    int mismatched = 0;
    int notInDat = 0;
    int noHash = 0;
    int corrupt = 0;
    QString datName;
    QString datVersion;
    QString datSource;         // "no-intro", "redump", etc.
};

/**
 * @brief Verifies ROMs against No-Intro/Redump DAT files
 * 
 * Workflow:
 * 1. Import DAT file(s) via importDat()
 * 2. Run verification via verifyLibrary() or verifyFiles()
 * 3. Review results and export report
 */
class VerificationEngine : public QObject {
    Q_OBJECT

public:
    explicit VerificationEngine(Database *database, QObject *parent = nullptr);

    /**
     * @brief Import a DAT file into the database
     * @param datFilePath Path to .dat or .xml file
     * @param systemName System this DAT applies to
     * @return Number of entries imported, or -1 on error
     */
    int importDat(const QString &datFilePath, const QString &systemName);

    /**
     * @brief Get list of imported DAT files
     * @return Map of system name to DAT info
     */
    QMap<QString, DatHeader> getImportedDats();

    /**
     * @brief Remove an imported DAT file
     * @param systemName System to remove DAT for
     * @return True if successful
     */
    bool removeDat(const QString &systemName);

    /**
     * @brief Verify all files in library
     * @param systemFilter Optional: only verify specific system
     * @return List of verification results
     */
    QList<VerificationResult> verifyLibrary(const QString &systemFilter = QString());

    /**
     * @brief Verify specific files
     * @param fileIds List of file IDs to verify
     * @return List of verification results
     */
    QList<VerificationResult> verifyFiles(const QList<int> &fileIds);

    /**
     * @brief Verify a single file against loaded DATs
     * @param fileId File ID from database
     * @return Verification result
     */
    VerificationResult verifyFile(int fileId);

    /**
     * @brief Get verification summary for last run
     * @return Summary statistics
     */
    VerificationSummary getLastSummary() const { return m_lastSummary; }

    /**
     * @brief Get missing games (in DAT but not in library)
     * @param systemName System to check
     * @return List of missing DAT entries
     */
    QList<DatRomEntry> getMissingGames(const QString &systemName);

    /**
     * @brief Export verification results to file
     * @param results Verification results to export
     * @param outputPath Output file path
     * @param format "csv" or "json"
     * @return True if successful
     */
    bool exportReport(const QList<VerificationResult> &results,
                     const QString &outputPath,
                     const QString &format = "csv");

    /**
     * @brief Check if system has imported DAT
     * @param systemName System name
     * @return True if DAT exists for system
     */
    bool hasDat(const QString &systemName);

signals:
    void verificationProgress(int current, int total, const QString &currentFile);
    void datImportProgress(int current, int total);
    void verificationComplete(const VerificationSummary &summary);
    void error(const QString &message);

private:
    Database *m_database;
    VerificationSummary m_lastSummary;
    
    // In-memory cache of loaded DAT entries (indexed by hash)
    QMap<QString, QMap<QString, DatRomEntry>> m_datCache;  // system -> (hash -> entry)
    QMap<QString, QString> m_datHashTypes;                  // system -> preferred hash type
    
    bool createVerificationSchema();
    void loadDatCache(const QString &systemName);
    QString getPreferredHashType(const QString &systemName);
    VerificationStatus compareHashes(const FileRecord &file, DatRomEntry &matchedEntry,
                                     QString &matchedHash, const QString &hashType);
};

} // namespace Remus

#endif // REMUS_VERIFICATION_ENGINE_H
