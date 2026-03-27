#pragma once

#include <QString>

namespace Remus {

struct ConversionResult {
    bool success = false;
    QString inputPath;
    QString outputPath;
    qint64 inputSize = 0;
    qint64 outputSize = 0;
    double compressionRatio = 0.0;
    QString error;
    int exitCode = 0;
    QString stdOutput;
    QString stdError;
};

struct VerifyResult {
    bool valid = false;
    QString path;
    QString error;
    QString details;
};

} // namespace Remus
