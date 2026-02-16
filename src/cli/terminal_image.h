#ifndef REMUS_TERMINAL_IMAGE_H
#define REMUS_TERMINAL_IMAGE_H

#include <QImage>
#include <QProcess>
#include <QTextStream>
#include <QFileInfo>

#ifdef Q_OS_UNIX
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace Remus {

/**
 * @brief Display images in terminal using Unicode half-blocks or chafa
 * 
 * Two-tier approach:
 * 1. If chafa is available in PATH, uses it for best quality (Sixel/Kitty/etc.)
 * 2. Falls back to Unicode half-block rendering (works in any truecolor terminal)
 */
class TerminalImage {
public:
    /**
     * @brief Get terminal width in columns
     * @return Terminal width, or 80 as default
     */
    static int getTerminalWidth()
    {
#ifdef Q_OS_UNIX
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
            return w.ws_col;
        }
#endif
        return 80;
    }

    /**
     * @brief Check if chafa is available in PATH
     */
    static bool isChafaAvailable()
    {
        QProcess proc;
        proc.start("which", {"chafa"});
        proc.waitForFinished(2000);
        return proc.exitCode() == 0;
    }

    /**
     * @brief Display image in terminal
     * @param imagePath Path to image file (PNG, JPEG, etc.)
     * @param maxCols Maximum width in columns (default: auto from terminal width)
     * @return True if image was displayed
     */
    static bool display(const QString &imagePath, int maxCols = 0)
    {
        QFileInfo fi(imagePath);
        if (!fi.exists() || !fi.isFile()) {
            return false;
        }

        if (maxCols <= 0) {
            maxCols = qMin(getTerminalWidth() - 2, 60);
        }

        // Try chafa first for best quality (supports Sixel, Kitty, iTerm2, etc.)
        if (isChafaAvailable()) {
            return displayWithChafa(imagePath, maxCols);
        }

        // Fallback: Unicode half-block rendering (works in any truecolor terminal)
        return displayWithHalfBlocks(imagePath, maxCols);
    }

private:
    /**
     * @brief Display using chafa (external tool, best quality)
     */
    static bool displayWithChafa(const QString &imagePath, int maxCols)
    {
        int maxRows = maxCols / 2;  // Approximate aspect ratio

        QProcess proc;
        proc.setProcessChannelMode(QProcess::ForwardedChannels);
        proc.start("chafa", {
            "--size", QString("%1x%2").arg(maxCols).arg(maxRows),
            "--animate", "off",
            imagePath
        });
        proc.waitForFinished(10000);
        return proc.exitCode() == 0;
    }

    /**
     * @brief Display using Unicode half-block characters with 24-bit ANSI colors
     * 
     * Each character cell represents 2 vertical pixels using ▀ (upper half block)
     * with foreground color = top pixel and background color = bottom pixel.
     * Works in virtually every modern terminal with truecolor support.
     */
    static bool displayWithHalfBlocks(const QString &imagePath, int maxCols)
    {
        QImage img(imagePath);
        if (img.isNull()) {
            return false;
        }

        // Scale image to fit terminal width while maintaining aspect ratio
        // Each character cell is roughly 1:2 (width:height), so we double vertical resolution
        int targetWidth = maxCols;
        int targetHeight = static_cast<int>(
            static_cast<double>(img.height()) / img.width() * targetWidth
        );

        // Ensure even height for half-block pairing
        if (targetHeight % 2 != 0) {
            targetHeight++;
        }

        img = img.scaled(targetWidth, targetHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        img = img.convertToFormat(QImage::Format_RGBA8888);

        QTextStream out(stdout);

        for (int y = 0; y < img.height() - 1; y += 2) {
            for (int x = 0; x < img.width(); x++) {
                QRgb top = img.pixel(x, y);
                QRgb bot = img.pixel(x, y + 1);

                int tR = qRed(top), tG = qGreen(top), tB = qBlue(top);
                int bR = qRed(bot), bG = qGreen(bot), bB = qBlue(bot);

                // ▀ with foreground=top pixel, background=bottom pixel
                out << QStringLiteral("\033[38;2;%1;%2;%3m\033[48;2;%4;%5;%6m▀")
                       .arg(tR).arg(tG).arg(tB)
                       .arg(bR).arg(bG).arg(bB);
            }
            out << QStringLiteral("\033[0m\n");
        }

        // Handle odd last row
        if (img.height() % 2 != 0) {
            int y = img.height() - 1;
            for (int x = 0; x < img.width(); x++) {
                QRgb px = img.pixel(x, y);
                out << QStringLiteral("\033[38;2;%1;%2;%3m▀")
                       .arg(qRed(px)).arg(qGreen(px)).arg(qBlue(px));
            }
            out << QStringLiteral("\033[0m\n");
        }

        out.flush();
        return true;
    }
};

} // namespace Remus

#endif // REMUS_TERMINAL_IMAGE_H
