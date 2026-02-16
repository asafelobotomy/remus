// Test for Levenshtein distance - standalone test
#include <QCoreApplication>
#include <QDebug>
#include <QVector>

int levenshteinDistance(const QString &s1, const QString &s2) {
    const int len1 = s1.length();
    const int len2 = s2.length();
    QVector<QVector<int>> d(len1 + 1, QVector<int>(len2 + 1));
    for (int i = 0; i <= len1; ++i) d[i][0] = i;
    for (int j = 0; j <= len2; ++j) d[0][j] = j;
    for (int i = 1; i <= len1; ++i) {
        for (int j = 1; j <= len2; ++j) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            d[i][j] = qMin(d[i-1][j] + 1, qMin(d[i][j-1] + 1, d[i-1][j-1] + cost));
        }
    }
    return d[len1][len2];
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    qDebug() << "\n=== Testing Levenshtein Distance ===\n";
    
    bool ok = (levenshteinDistance("mario", "mario") == 0 &&
               levenshteinDistance("mario", "maria") == 1 &&
               levenshteinDistance("super mario bros", "super mario") == 5);
    
    if (ok) {
        qDebug() << "✅ SUCCESS: Levenshtein tests passed!";
        return 0;
    }
    qCritical() << "❌ FAILED";
    return 1;
}
