#include "reviewwidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

ReviewWidget::ReviewWidget(QWidget *parent)
    : QWidget(parent),
      m_currentIndex(0),
      m_titleLabel(new QLabel("Anki 复习", this)),
      m_statusLabel(new QLabel(this)),
      m_tagLabel(new QLabel(this)),
      m_frontEdit(new QTextEdit(this)),
      m_backEdit(new QTextEdit(this)),
      m_showAnswerButton(new QPushButton("显示答案", this)),
      m_backToPlannerButton(new QPushButton("返回计划", this)),
      m_forgetButton(new QPushButton("忘了", this)),
      m_blurryButton(new QPushButton("模糊", this)),
      m_rememberButton(new QPushButton("记住了", this)),
      m_easyButton(new QPushButton("很熟", this))
{
    setupUi();
    setupStyleSheet();
    connectSignals();
    loadFakeCards();
}

void ReviewWidget::setupUi()
{
    /*
     * 页面结构：
     * 顶部是标题、状态和返回按钮，中间是卡片正反面，底部是显示答案和四个反馈按钮。
     */
    m_titleLabel->setObjectName("reviewTitle");
    m_statusLabel->setObjectName("reviewStatus");
    m_tagLabel->setObjectName("reviewTag");

    m_frontEdit->setObjectName("cardText");
    m_frontEdit->setReadOnly(true);
    m_frontEdit->setMinimumHeight(150);

    m_backEdit->setObjectName("answerText");
    m_backEdit->setReadOnly(true);
    m_backEdit->setMinimumHeight(150);

    m_showAnswerButton->setObjectName("showAnswerButton");
    m_backToPlannerButton->setProperty("buttonRole", "ghost");

    const QList<QPushButton *> buttons = {
        m_showAnswerButton,
        m_backToPlannerButton,
        m_forgetButton,
        m_blurryButton,
        m_rememberButton,
        m_easyButton
    };

    for (QPushButton *button : buttons) {
        button->setCursor(Qt::PointingHandCursor);
        button->setMinimumHeight(38);
    }

    m_forgetButton->setProperty("ratingRole", "forget");
    m_blurryButton->setProperty("ratingRole", "blurry");
    m_rememberButton->setProperty("ratingRole", "remember");
    m_easyButton->setProperty("ratingRole", "easy");

    auto *titleLayout = new QVBoxLayout;
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(4);
    titleLayout->addWidget(m_titleLabel);
    titleLayout->addWidget(m_statusLabel);
    titleLayout->addWidget(m_tagLabel);

    auto *headerLayout = new QHBoxLayout;
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->addLayout(titleLayout, 1);
    headerLayout->addWidget(m_backToPlannerButton);

    auto *answerButtonLayout = new QHBoxLayout;
    answerButtonLayout->setContentsMargins(0, 0, 0, 0);
    answerButtonLayout->addStretch();
    answerButtonLayout->addWidget(m_showAnswerButton);
    answerButtonLayout->addStretch();

    auto *ratingLayout = new QHBoxLayout;
    ratingLayout->setContentsMargins(0, 0, 0, 0);
    ratingLayout->setSpacing(10);
    ratingLayout->addWidget(m_forgetButton);
    ratingLayout->addWidget(m_blurryButton);
    ratingLayout->addWidget(m_rememberButton);
    ratingLayout->addWidget(m_easyButton);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(22, 20, 22, 20);
    mainLayout->setSpacing(12);
    mainLayout->addLayout(headerLayout);
    mainLayout->addWidget(m_frontEdit);
    mainLayout->addWidget(m_backEdit);
    mainLayout->addLayout(answerButtonLayout);
    mainLayout->addLayout(ratingLayout);
}

void ReviewWidget::setupStyleSheet()
{
    setStyleSheet(R"(
        QWidget {
            background: #f5f7fb;
            color: #263244;
            font-size: 14px;
        }

        QLabel#reviewTitle {
            color: #1f2d3d;
            font-size: 22px;
            font-weight: 700;
        }

        QLabel#reviewStatus {
            color: #64748b;
            font-size: 13px;
        }

        QLabel#reviewTag {
            color: #2563eb;
            font-size: 13px;
            font-weight: 600;
        }

        QTextEdit#cardText,
        QTextEdit#answerText {
            background: #ffffff;
            border: 1px solid #dfe6f1;
            border-radius: 10px;
            padding: 12px;
            selection-background-color: #dce8ff;
        }

        QTextEdit#answerText {
            background: #fbfdff;
        }

        QPushButton {
            border-radius: 8px;
            border: 1px solid #cfd8e6;
            background: #ffffff;
            color: #263244;
            padding: 8px 16px;
            font-weight: 600;
        }

        QPushButton:hover {
            background: #f0f5ff;
            border-color: #adc3ea;
        }

        QPushButton#showAnswerButton {
            background: #2f6fed;
            border-color: #2f6fed;
            color: #ffffff;
            min-width: 130px;
        }

        QPushButton#showAnswerButton:hover {
            background: #255fd0;
            border-color: #255fd0;
        }

        QPushButton[buttonRole="ghost"] {
            background: #f8fafd;
            color: #475569;
        }

        QPushButton[ratingRole="forget"] {
            background: #fff1f1;
            border-color: #f1b8b8;
            color: #c73d3d;
        }

        QPushButton[ratingRole="blurry"] {
            background: #fff7ed;
            border-color: #f2c792;
            color: #a15c00;
        }

        QPushButton[ratingRole="remember"] {
            background: #eef8f4;
            border-color: #a9d8c4;
            color: #1f7a55;
        }

        QPushButton[ratingRole="easy"] {
            background: #eaf2ff;
            border-color: #adc3ea;
            color: #2563eb;
        }
    )");
}

void ReviewWidget::connectSignals()
{
    connect(m_backToPlannerButton, &QPushButton::clicked, this, &ReviewWidget::backRequested);
    connect(m_showAnswerButton, &QPushButton::clicked, this, &ReviewWidget::showAnswer);
    connect(m_forgetButton, &QPushButton::clicked, this, [this]() { submitRating(0); });
    connect(m_blurryButton, &QPushButton::clicked, this, [this]() { submitRating(1); });
    connect(m_rememberButton, &QPushButton::clicked, this, [this]() { submitRating(2); });
    connect(m_easyButton, &QPushButton::clicked, this, [this]() { submitRating(3); });
}

void ReviewWidget::loadFakeCards()
{
    /*
     * 第一阶段先用假数据验证页面流程。
     * 下一阶段再把这里替换成数据库查询。
     */
    m_cards = {
        {"Qt 中信号和槽的作用是什么？",
         "信号和槽用于对象之间通信。一个对象发出 signal，另一个对象的 slot 可以响应这个事件。",
         "Qt 基础"},
        {"QStackedWidget 适合解决什么界面问题？",
         "它可以在多个页面之间切换，同一时间只显示一个页面，适合做主界面与子页面的切换。",
         "Qt Widgets"},
        {"SQLite 中为什么推荐使用 bindValue？",
         "bindValue 可以避免直接拼接 SQL 带来的转义问题，也能让 SQL 语句更清晰。",
         "数据库"}
    };

    m_currentIndex = 0;
    showCurrentCard();
}

void ReviewWidget::showCurrentCard()
{
    if (m_currentIndex >= m_cards.size()) {
        showEmptyState();
        return;
    }

    const ReviewCard &card = m_cards.at(m_currentIndex);
    m_titleLabel->setText("Anki 复习");
    m_statusLabel->setText(QString("第 %1 / %2 张")
                               .arg(m_currentIndex + 1)
                               .arg(m_cards.size()));
    m_tagLabel->setText("标签：" + card.tag);
    m_frontEdit->setPlainText(card.front);
    m_backEdit->setPlainText(card.back);
    m_backEdit->setVisible(false);
    m_showAnswerButton->setVisible(true);
    m_showAnswerButton->setEnabled(true);
    setReviewButtonsVisible(false);
}

void ReviewWidget::showEmptyState()
{
    m_titleLabel->setText("Anki 复习");
    m_statusLabel->setText("今天没有需要复习的卡片");
    m_tagLabel->clear();
    m_frontEdit->setPlainText("今天没有需要复习的卡片");
    m_backEdit->clear();
    m_backEdit->setVisible(false);
    m_showAnswerButton->setVisible(false);
    setReviewButtonsVisible(false);
}

void ReviewWidget::showAnswer()
{
    if (m_currentIndex >= m_cards.size()) {
        return;
    }

    m_backEdit->setVisible(true);
    m_showAnswerButton->setVisible(false);
    setReviewButtonsVisible(true);
}

void ReviewWidget::submitRating(int rating)
{
    Q_UNUSED(rating);

    if (m_currentIndex >= m_cards.size()) {
        showEmptyState();
        return;
    }

    ++m_currentIndex;
    showCurrentCard();
}

void ReviewWidget::setReviewButtonsVisible(bool visible)
{
    m_forgetButton->setVisible(visible);
    m_blurryButton->setVisible(visible);
    m_rememberButton->setVisible(visible);
    m_easyButton->setVisible(visible);
}
