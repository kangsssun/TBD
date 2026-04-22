#ifndef ENDINGSEQUENCE_H
#define ENDINGSEQUENCE_H

#include <QWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QTimer>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QString>
#include <QPoint>

// ════════════════════════════════════════════════════════════════════════════
// EndingSequence — System Override & Glitch ending screen
//
//  Stage 1: Glitch  — red "ACCESS DENIED" screen shakes violently
//  Stage 2: Flash   — white‑out overlay fades in (300 ms)
//  Stage 3: Reveal  — clean "ACCESS GRANTED" + master code fades in (800 ms)
// ════════════════════════════════════════════════════════════════════════════
class EndingSequence : public QWidget
{
    Q_OBJECT

public:
    explicit EndingSequence(QWidget *parent = nullptr);
    ~EndingSequence() override = default;

    /**
     * @brief Run the full ending sequence (blocking — calls exec() internally).
     * @param parent           Widget used for parenting / sizing
     * @param teamName         Team name for display
     * @param remainingSeconds Countdown seconds left
     * @param totalSeconds     Total game time (default 45 min)
     */
    static void show(QWidget *parent,
                     const QString &teamName,
                     int remainingSeconds,
                     int totalSeconds = 45 * 60);

    /** Show the 4-digit master code input dialog. Returns true if correct. */
    static bool showCodeInputDialog(QWidget *topLevel, const QString &correctCode);

    /** Kick off the 3‑stage animation. */
    void startEndingSequence();

signals:
    /** Emitted when the user clicks the "입력하기" button on the success page. */
    void enterRequested();

private:
    void buildErrorPage();
    void buildSuccessPage();
    void buildFlashOverlay();

    // ── Stage control ───────────────────────────────────────────────
    void startGlitch();
    void stopGlitch();
    void startWhiteout();
    void revealSuccess();

    // ── Widgets ─────────────────────────────────────────────────────
    QStackedWidget *m_stackedWidget  = nullptr;

    // Error page (index 0)
    QWidget *m_errorPage             = nullptr;
    QLabel  *m_errorTitle            = nullptr;
    QLabel  *m_errorSub              = nullptr;
    QLabel  *m_errorDetail           = nullptr;

    // Success page (index 1)
    QWidget *m_successPage           = nullptr;
    QLabel  *m_successTitle          = nullptr;
    QLabel  *m_masterCodeLabel       = nullptr;
    QLabel  *m_successSub            = nullptr;

    // Flash overlay
    QWidget              *m_flashOverlay  = nullptr;
    QGraphicsOpacityEffect *m_flashEffect = nullptr;

    // ── Timers & animation state ────────────────────────────────────
    QTimer  *m_glitchTimer    = nullptr;
    QPoint   m_errorOrigPos;             // original pos of error content
    int      m_glitchElapsed  = 0;       // ms elapsed during glitch
    static constexpr int GLITCH_DURATION_MS = 2500;
};

#endif // ENDINGSEQUENCE_H
