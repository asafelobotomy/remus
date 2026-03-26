#include "theme_constants.h"
#include "../core/constants/constants.h"
#include <QSettings>
#include <QDebug>

namespace Remus {

namespace {

QString lightVariantFor(const char *darkColor)
{
    if (darkColor == Constants::UI::Colors::SUCCESS) return QString::fromLatin1(Constants::UI::Colors::SUCCESS_LIGHT);
    if (darkColor == Constants::UI::Colors::WARNING) return QString::fromLatin1(Constants::UI::Colors::WARNING_LIGHT);
    if (darkColor == Constants::UI::Colors::DANGER) return QString::fromLatin1(Constants::UI::Colors::DANGER_LIGHT);
    if (darkColor == Constants::UI::Colors::INFO) return QString::fromLatin1(Constants::UI::Colors::INFO_LIGHT);
    return QString::fromLatin1(darkColor);
}

}

ThemeConstants::ThemeConstants(QObject *parent)
    : QObject(parent)
{
    // Load persisted theme preference from QSettings (single source of truth)
    QSettings settings;
    m_darkMode = settings.value("theme/darkMode", true).toBool();
    qDebug() << "Theme initialized:" << (m_darkMode ? "Dark" : "Light") << "mode";
}

void ThemeConstants::setDarkMode(bool dark)
{
    if (m_darkMode != dark) {
        m_darkMode = dark;
        qDebug() << "Theme switched to:" << (dark ? "Dark" : "Light");
        emit themeModeChanged();
    }
}

QString ThemeConstants::sidebarBg() const {
    return m_darkMode 
        ? QString::fromLatin1(Constants::UI::Colors::SIDEBAR_BG)
        : QString::fromLatin1(Constants::UI::Colors::SIDEBAR_BG_LIGHT);
}

QString ThemeConstants::mainBg() const {
    return m_darkMode
        ? QString::fromLatin1(Constants::UI::Colors::MAIN_BG)
        : QString::fromLatin1(Constants::UI::Colors::MAIN_BG_LIGHT);
}

QString ThemeConstants::cardBg() const {
    return m_darkMode
        ? QString::fromLatin1(Constants::UI::Colors::CARD_BG)
        : QString::fromLatin1(Constants::UI::Colors::CARD_BG_LIGHT);
}

QString ThemeConstants::contentBg() const {
    return m_darkMode
        ? QString::fromLatin1(Constants::UI::Colors::CONTENT_BG)
        : QString::fromLatin1(Constants::UI::Colors::CONTENT_BG_LIGHT);
}

QString ThemeConstants::primary() const {
    return m_darkMode
        ? QString::fromLatin1(Constants::UI::Colors::PRIMARY)
        : QString::fromLatin1(Constants::UI::Colors::PRIMARY_LIGHT);
}

QString ThemeConstants::primaryHover() const {
    return m_darkMode
        ? QString::fromLatin1(Constants::UI::Colors::PRIMARY_HOVER)
        : QString::fromLatin1(Constants::UI::Colors::PRIMARY_HOVER_LIGHT);
}

QString ThemeConstants::primaryPressed() const {
    return m_darkMode
        ? QString::fromLatin1(Constants::UI::Colors::PRIMARY_PRESSED)
        : QString::fromLatin1(Constants::UI::Colors::PRIMARY_PRESSED_LIGHT);
}

QString ThemeConstants::primaryText() const {
    return m_darkMode
        ? QString::fromLatin1(Constants::UI::Colors::PRIMARY_TEXT)
        : QString::fromLatin1(Constants::UI::Colors::PRIMARY_TEXT_LIGHT);
}

QString ThemeConstants::success() const {
    return m_darkMode
        ? QString::fromLatin1(Constants::UI::Colors::SUCCESS)
        : QString::fromLatin1(Constants::UI::Colors::SUCCESS_LIGHT);
}

QString ThemeConstants::warning() const {
    return m_darkMode
        ? QString::fromLatin1(Constants::UI::Colors::WARNING)
        : QString::fromLatin1(Constants::UI::Colors::WARNING_LIGHT);
}

QString ThemeConstants::danger() const {
    return m_darkMode
        ? QString::fromLatin1(Constants::UI::Colors::DANGER)
        : QString::fromLatin1(Constants::UI::Colors::DANGER_LIGHT);
}

QString ThemeConstants::info() const {
    return m_darkMode
        ? QString::fromLatin1(Constants::UI::Colors::INFO)
        : QString::fromLatin1(Constants::UI::Colors::INFO_LIGHT);
}

int ThemeConstants::confidenceHigh() const {
    return static_cast<int>(Constants::Confidence::Thresholds::HIGH);
}

int ThemeConstants::confidenceMedium() const {
    return static_cast<int>(Constants::Confidence::Thresholds::MEDIUM);
}

QString ThemeConstants::confidenceColor(int confidence) const {
    const char *darkColor = Constants::UI::getConfidenceColor(static_cast<float>(confidence));
    return m_darkMode ? QString::fromLatin1(darkColor)
                      : lightVariantFor(darkColor);
}

QString ThemeConstants::textPrimary() const {
    return m_darkMode
        ? QString::fromLatin1(Constants::UI::Colors::TEXT_PRIMARY)
        : QString::fromLatin1(Constants::UI::Colors::TEXT_PRIMARY_LIGHT);
}

QString ThemeConstants::textSecondary() const {
    return m_darkMode
        ? QString::fromLatin1(Constants::UI::Colors::TEXT_SECONDARY)
        : QString::fromLatin1(Constants::UI::Colors::TEXT_SECONDARY_LIGHT);
}

QString ThemeConstants::textMuted() const {
    return m_darkMode
        ? QString::fromLatin1(Constants::UI::Colors::TEXT_MUTED)
        : QString::fromLatin1(Constants::UI::Colors::TEXT_MUTED_LIGHT);
}

QString ThemeConstants::textLight() const {
    return m_darkMode
        ? QString::fromLatin1(Constants::UI::Colors::TEXT_LIGHT)
        : QString::fromLatin1(Constants::UI::Colors::TEXT_LIGHT_LIGHT);
}

QString ThemeConstants::textPlaceholder() const {
    return m_darkMode
        ? QString::fromLatin1(Constants::UI::Colors::TEXT_PLACEHOLDER)
        : QString::fromLatin1(Constants::UI::Colors::TEXT_PLACEHOLDER_LIGHT);
}

QString ThemeConstants::border() const {
    return m_darkMode
        ? QString::fromLatin1(Constants::UI::Colors::BORDER)
        : QString::fromLatin1(Constants::UI::Colors::BORDER_LIGHT_COLOR);
}

QString ThemeConstants::borderLight() const {
    return m_darkMode
        ? QString::fromLatin1(Constants::UI::Colors::BORDER_LIGHT)
        : QString::fromLatin1(Constants::UI::Colors::BORDER_LIGHT_LIGHT);
}

QString ThemeConstants::divider() const {
    return m_darkMode
        ? QString::fromLatin1(Constants::UI::Colors::DIVIDER)
        : QString::fromLatin1(Constants::UI::Colors::DIVIDER_LIGHT);
}

QString ThemeConstants::navText() const {
    return m_darkMode
        ? QString::fromLatin1(Constants::UI::Colors::NAV_TEXT)
        : QString::fromLatin1(Constants::UI::Colors::NAV_TEXT_LIGHT);
}

QString ThemeConstants::navHover() const {
    return m_darkMode
        ? QString::fromLatin1(Constants::UI::Colors::NAV_HOVER)
        : QString::fromLatin1(Constants::UI::Colors::NAV_HOVER_LIGHT);
}

QString ThemeConstants::navActive() const {
    return m_darkMode
        ? QString::fromLatin1(Constants::UI::Colors::NAV_ACTIVE)
        : QString::fromLatin1(Constants::UI::Colors::NAV_ACTIVE_LIGHT);
}

QString ThemeConstants::navActiveBg() const {
    return m_darkMode
        ? QString::fromLatin1(Constants::UI::Colors::NAV_ACTIVE_BG)
        : QString::fromLatin1(Constants::UI::Colors::NAV_ACTIVE_BG_LIGHT);
}

QString ThemeConstants::buttonDisabled() const {
    return m_darkMode
        ? QString::fromLatin1(Constants::UI::Colors::BUTTON_DISABLED)
        : QString::fromLatin1(Constants::UI::Colors::BUTTON_DISABLED_LIGHT);
}

QString ThemeConstants::buttonDisabledText() const {
    return m_darkMode
        ? QString::fromLatin1(Constants::UI::Colors::BUTTON_DISABLED_TEXT)
        : QString::fromLatin1(Constants::UI::Colors::BUTTON_DISABLED_TEXT_LIGHT);
}

} // namespace Remus
