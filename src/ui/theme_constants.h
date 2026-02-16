#pragma once

#include <QObject>
#include <QString>

namespace Remus {

class ThemeConstants : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool isDarkMode READ isDarkMode WRITE setDarkMode NOTIFY themeModeChanged)
    
    Q_PROPERTY(QString sidebarBg READ sidebarBg NOTIFY themeModeChanged)
    Q_PROPERTY(QString mainBg READ mainBg NOTIFY themeModeChanged)
    Q_PROPERTY(QString cardBg READ cardBg NOTIFY themeModeChanged)
    Q_PROPERTY(QString contentBg READ contentBg NOTIFY themeModeChanged)

    Q_PROPERTY(QString primary READ primary NOTIFY themeModeChanged)
    Q_PROPERTY(QString primaryHover READ primaryHover NOTIFY themeModeChanged)
    Q_PROPERTY(QString primaryPressed READ primaryPressed NOTIFY themeModeChanged)
    Q_PROPERTY(QString primaryText READ primaryText NOTIFY themeModeChanged)

    Q_PROPERTY(QString success READ success NOTIFY themeModeChanged)
    Q_PROPERTY(QString warning READ warning NOTIFY themeModeChanged)
    Q_PROPERTY(QString danger READ danger NOTIFY themeModeChanged)
    Q_PROPERTY(QString info READ info NOTIFY themeModeChanged)

    Q_PROPERTY(QString textPrimary READ textPrimary NOTIFY themeModeChanged)
    Q_PROPERTY(QString textSecondary READ textSecondary NOTIFY themeModeChanged)
    Q_PROPERTY(QString textMuted READ textMuted NOTIFY themeModeChanged)
    Q_PROPERTY(QString textLight READ textLight NOTIFY themeModeChanged)
    Q_PROPERTY(QString textPlaceholder READ textPlaceholder NOTIFY themeModeChanged)

    Q_PROPERTY(QString border READ border NOTIFY themeModeChanged)
    Q_PROPERTY(QString borderLight READ borderLight NOTIFY themeModeChanged)
    Q_PROPERTY(QString divider READ divider NOTIFY themeModeChanged)

    Q_PROPERTY(QString navText READ navText NOTIFY themeModeChanged)
    Q_PROPERTY(QString navHover READ navHover NOTIFY themeModeChanged)
    Q_PROPERTY(QString navActive READ navActive NOTIFY themeModeChanged)
    Q_PROPERTY(QString navActiveBg READ navActiveBg NOTIFY themeModeChanged)

    Q_PROPERTY(QString buttonDisabled READ buttonDisabled NOTIFY themeModeChanged)
    Q_PROPERTY(QString buttonDisabledText READ buttonDisabledText NOTIFY themeModeChanged)

public:
    explicit ThemeConstants(QObject *parent = nullptr);

    bool isDarkMode() const { return m_darkMode; }
    Q_INVOKABLE void setDarkMode(bool dark);
    
    Q_INVOKABLE void toggleTheme() { setDarkMode(!m_darkMode); }

    QString sidebarBg() const;
    QString mainBg() const;
    QString cardBg() const;
    QString contentBg() const;

    QString primary() const;
    QString primaryHover() const;
    QString primaryPressed() const;
    QString primaryText() const;

    QString success() const;
    QString warning() const;
    QString danger() const;
    QString info() const;

    QString textPrimary() const;
    QString textSecondary() const;
    QString textMuted() const;
    QString textLight() const;
    QString textPlaceholder() const;

    QString border() const;
    QString borderLight() const;
    QString divider() const;

    QString navText() const;
    QString navHover() const;
    QString navActive() const;
    QString navActiveBg() const;

    QString buttonDisabled() const;
    QString buttonDisabledText() const;

signals:
    void themeModeChanged();

private:
    bool m_darkMode;  // Loaded from QSettings in constructor
};

} // namespace Remus
