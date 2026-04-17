#include "missionpage.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QFont>

namespace {

QString mission1StoryLogText()
{
    return QStringLiteral(
        "[21:04:12] SECURITY SYSTEM v2.1\n"
        "[21:04:13] ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n"
        "[21:04:14] [1단계 인증]\n"
        "[21:04:16] 비콘 장애 원인을 조사하던 중\n"
        "[21:04:18] 손상된 장치에서 마지막 통신 로그가 복구되었습니다.\n"
        "[21:04:21] 장애 직전 비콘이 수신한 신호 데이터가 남아있습니다.\n"
        "[21:04:24] LED 점멸 신호를 해독하여\n"
        "[21:04:26] 1단계 인증 코드를 입력하십시오.\n"
        "[21:04:27] ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
}

void setupMission1StoryUi(QVBoxLayout *contentLayout, QWidget *parent)
{
    auto *storyFrame = new QFrame(parent);
    storyFrame->setObjectName(QStringLiteral("missionStoryFrame"));
    storyFrame->setFrameShape(QFrame::StyledPanel);

    auto *frameLayout = new QVBoxLayout(storyFrame);
    frameLayout->setContentsMargins(20, 16, 20, 16);
    frameLayout->setSpacing(10);

    auto *storyLabel = new QLabel(storyFrame);
    storyLabel->setObjectName(QStringLiteral("eventDescriptionLabel"));
    storyLabel->setTextFormat(Qt::PlainText);
    storyLabel->setWordWrap(true);
    storyLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    storyLabel->setText(mission1StoryLogText());

    QFont terminalFont(QStringLiteral("Consolas"));
    terminalFont.setStyleHint(QFont::Monospace);
    terminalFont.setPointSize(11);
    storyLabel->setFont(terminalFont);

    frameLayout->addWidget(storyLabel, 1);

    contentLayout->addWidget(storyFrame, 1);
}

} // namespace

MissionPage::MissionPage(int missionNumber, QWidget *parent)
    : QWidget(parent)
    , m_missionNumber(missionNumber)
    , m_contentLayout(nullptr)
{
    setObjectName(QStringLiteral("missionPage_%1").arg(missionNumber));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_contentLayout = rootLayout;

    setupMission();
}

void MissionPage::setupMission()
{
    if (m_missionNumber == 1) {
        setupMission1StoryUi(m_contentLayout, this);
        return;
    }

    // Default placeholder – subclasses override this
    auto *placeholder = new QLabel(
        QStringLiteral("MISSION %1 — 준비 중").arg(m_missionNumber), this);
    placeholder->setObjectName(QStringLiteral("eventDescriptionLabel"));
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setWordWrap(true);
    m_contentLayout->addWidget(placeholder, 1);
}
