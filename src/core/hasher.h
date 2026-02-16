#ifndef REMUS_HASHER_H
#define REMUS_HASHER_H

#include <QString>
#include <QObject>

namespace Remus {

/**
 * @brief Hash calculation result
 */
struct HashResult {
    QString crc32;
    QString md5;
    QString sha1;
    bool success = false;
    QString error;
};

/**
 * @brief Calculates file hashes (CRC32, MD5, SHA1)
 * 
 * Supports header stripping for systems that require it (NES, Lynx).
 */
class Hasher : public QObject {
    Q_OBJECT

public:
    explicit Hasher(QObject *parent = nullptr);

    /**
     * @brief Calculate all hashes for a file
     * @param filePath Path to file
     * @param stripHeader Whether to strip header (for NES, Lynx)
     * @param headerSize Size of header to strip (bytes)
     * @return Hash results
     */
    HashResult calculateHashes(const QString &filePath, 
                               bool stripHeader = false,
                               int headerSize = 0);

    /**
     * @brief Calculate specific hash
     * @param filePath Path to file
     * @param algorithm "CRC32", "MD5", or "SHA1"
     * @param stripHeader Whether to strip header
     * @param headerSize Size of header to strip
     * @return Hash string in hex format
     */
    QString calculateHash(const QString &filePath,
                          const QString &algorithm,
                          bool stripHeader = false,
                          int headerSize = 0);

    /**
     * @brief Detect and calculate header size for systems that need it
     * @param filePath Path to file
     * @param extension File extension
     * @return Header size in bytes (0 if no header)
     */
    static int detectHeaderSize(const QString &filePath, const QString &extension);

signals:
    void hashProgress(const QString &filePath, int percentage);

private:
    QByteArray readFileData(const QString &filePath, bool stripHeader, int headerSize);
    QString calculateCRC32(const QByteArray &data);
    QString calculateMD5(const QByteArray &data);
    QString calculateSHA1(const QByteArray &data);
};

} // namespace Remus

#endif // REMUS_HASHER_H
