#include "reviewwidget.h"

#include "carddialog.h"
#include "databasemanager.h"

#include <QColor>
#include <QFileDialog>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMap>
#include <QMessageBox>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QEasingCurve>
#include <QGraphicsOpacityEffect>

ReviewWidget::ReviewWidget(DatabaseManager *databaseManager, QWidget *parent)
    : QWidget(parent),
      m_databaseManager(databaseManager),
      m_currentGroup("默认分组"),
      m_currentDeck("默认牌组"),
      m_currentIndex(0),
      m_titleLabel(new QLabel("Anki 牌库", this)),
      m_statusLabel(new QLabel(this)),
      m_tagLabel(new QLabel(this)),
      m_libraryWidget(new QWidget(this)),
      m_searchEdit(new QLineEdit(this)),
      m_groupTabsLayout(new QHBoxLayout),
      m_deckScrollArea(new QScrollArea(this)),
      m_deckListWidget(new QWidget(this)),
      m_deckListLayout(new QVBoxLayout),
      m_newGroupButton(new QPushButton("+", this)),
      m_newDeckButton(new QPushButton("新建牌组", this)),
      m_deleteGroupButton(new QPushButton("删除分组", this)),
      m_addCardButton(new QPushButton("添加卡片", this)),
      m_importCardButton(new QPushButton("导入 Anki 卡片", this)),
      m_backToPlannerButton(new QPushButton("返回计划", this)),
      m_deckDetailWidget(new QWidget(this)),
      m_cardListScrollArea(new QScrollArea(this)),
      m_cardListWidget(new QWidget(this)),
      m_cardListLayout(new QVBoxLayout),
      m_backFromDetailButton(new QPushButton("返回牌库", this)),
      m_startReviewFromDetailButton(new QPushButton("开始复习", this)),
      m_reviewAreaWidget(new QWidget(this)),
      m_backSideTitleLabel(new QLabel("背面", this)),
      m_cardFrame(new QFrame(this)),
      m_cardShadowEffect(new QGraphicsDropShadowEffect(this)),
      m_frontOpacityEffect(new QGraphicsOpacityEffect(this)),
      m_backTitleOpacityEffect(new QGraphicsOpacityEffect(this)),
      m_backOpacityEffect(new QGraphicsOpacityEffect(this)),
      m_frontEdit(new QTextEdit(this)),
      m_backEdit(new QTextEdit(this)),
      m_backToLibraryButton(new QPushButton("返回牌库", this)),
      m_showAnswerButton(new QPushButton("显示答案", this)),
      m_forgetButton(new QPushButton("忘了", this)),
      m_blurryButton(new QPushButton("模糊", this)),
      m_rememberButton(new QPushButton("记住了", this)),
      m_easyButton(new QPushButton("很熟", this))
{
    setupUi();
    setupStyleSheet();
    connectSignals();
    showLibrary();
}

void ReviewWidget::setupUi()
{
    /*
     * 页面分成两块：
     * - m_libraryWidget：类似截图里的牌库首页，显示搜索、分组标签和实体牌组卡片。
     * - m_reviewAreaWidget：点进某个牌组后的复习页面。
     */
    m_titleLabel->setObjectName("reviewTitle");
    m_statusLabel->setObjectName("reviewStatus");
    m_tagLabel->setObjectName("reviewTag");

    m_searchEdit->setObjectName("deckSearchEdit");
    m_searchEdit->setPlaceholderText("发现牌组");
    m_searchEdit->setMinimumHeight(42);

    m_newGroupButton->setObjectName("groupAddButton");
    m_newDeckButton->setProperty("buttonRole", "normal");
    m_deleteGroupButton->setProperty("buttonRole", "danger");
    m_addCardButton->setProperty("buttonRole", "primary");
    m_importCardButton->setProperty("buttonRole", "accent");
    m_backToPlannerButton->setProperty("buttonRole", "ghost");
    m_backFromDetailButton->setProperty("buttonRole", "ghost");
    m_startReviewFromDetailButton->setProperty("buttonRole", "primary");
    m_backToLibraryButton->setProperty("buttonRole", "ghost");

    const QList<QPushButton *> buttons = {
        m_newGroupButton,
        m_newDeckButton,
        m_deleteGroupButton,
        m_addCardButton,
        m_importCardButton,
        m_backToPlannerButton,
        m_backFromDetailButton,
        m_startReviewFromDetailButton,
        m_backToLibraryButton,
        m_showAnswerButton,
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

    auto *searchLayout = new QHBoxLayout;
    searchLayout->setContentsMargins(0, 0, 0, 0);
    searchLayout->setSpacing(10);
    searchLayout->addWidget(m_searchEdit, 1);
    searchLayout->addWidget(m_newDeckButton);
    searchLayout->addWidget(m_deleteGroupButton);
    searchLayout->addWidget(m_addCardButton);
    searchLayout->addWidget(m_importCardButton);

    m_groupTabsLayout->setContentsMargins(0, 0, 0, 0);
    m_groupTabsLayout->setSpacing(14);

    m_deckListWidget->setLayout(m_deckListLayout);
    m_deckListLayout->setContentsMargins(0, 0, 0, 0);
    m_deckListLayout->setSpacing(12);

    m_deckScrollArea->setObjectName("deckScrollArea");
    m_deckScrollArea->setWidgetResizable(true);
    m_deckScrollArea->setFrameShape(QFrame::NoFrame);
    m_deckScrollArea->setWidget(m_deckListWidget);

    auto *libraryLayout = new QVBoxLayout(m_libraryWidget);
    libraryLayout->setContentsMargins(0, 0, 0, 0);
    libraryLayout->setSpacing(14);
    libraryLayout->addLayout(searchLayout);
    libraryLayout->addLayout(m_groupTabsLayout);
    libraryLayout->addWidget(m_deckScrollArea, 1);

    m_cardListWidget->setLayout(m_cardListLayout);
    m_cardListLayout->setContentsMargins(0, 0, 0, 0);
    m_cardListLayout->setSpacing(12);

    m_cardListScrollArea->setObjectName("cardListScrollArea");
    m_cardListScrollArea->setWidgetResizable(true);
    m_cardListScrollArea->setFrameShape(QFrame::NoFrame);
    m_cardListScrollArea->setWidget(m_cardListWidget);

    auto *detailTopLayout = new QHBoxLayout;
    detailTopLayout->setContentsMargins(0, 0, 0, 0);
    detailTopLayout->setSpacing(10);
    detailTopLayout->addWidget(m_backFromDetailButton);
    detailTopLayout->addStretch();
    detailTopLayout->addWidget(m_startReviewFromDetailButton);

    auto *detailLayout = new QVBoxLayout(m_deckDetailWidget);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->setSpacing(12);
    detailLayout->addLayout(detailTopLayout);
    detailLayout->addWidget(m_cardListScrollArea, 1);

    m_cardFrame->setObjectName("flashCardFrame");
    m_cardFrame->setMinimumHeight(360);

    m_cardShadowEffect->setBlurRadius(28);
    m_cardShadowEffect->setOffset(0, 10);
    m_cardShadowEffect->setColor(QColor(38, 50, 68, 45));
    m_cardFrame->setGraphicsEffect(m_cardShadowEffect);

    m_frontEdit->setObjectName("cardText");
    m_frontEdit->setReadOnly(true);
    m_frontEdit->setMinimumHeight(150);
    m_frontEdit->setFrameShape(QFrame::NoFrame);
    m_frontOpacityEffect->setOpacity(1.0);
    m_frontEdit->setGraphicsEffect(m_frontOpacityEffect);

    m_backEdit->setObjectName("answerText");
    m_backEdit->setReadOnly(true);
    m_backEdit->setMinimumHeight(150);
    m_backEdit->setFrameShape(QFrame::NoFrame);
    m_backOpacityEffect->setOpacity(1.0);
    m_backEdit->setGraphicsEffect(m_backOpacityEffect);

    auto *frontTitleLabel = new QLabel("正面", this);
    frontTitleLabel->setObjectName("cardSideTitle");
    m_backSideTitleLabel->setObjectName("cardSideTitle");
    m_backTitleOpacityEffect->setOpacity(1.0);
    m_backSideTitleLabel->setGraphicsEffect(m_backTitleOpacityEffect);
    m_showAnswerButton->setObjectName("showAnswerButton");

    auto *cardLayout = new QVBoxLayout(m_cardFrame);
    cardLayout->setContentsMargins(28, 24, 28, 24);
    cardLayout->setSpacing(10);
    cardLayout->addWidget(frontTitleLabel);
    cardLayout->addWidget(m_frontEdit, 1);
    cardLayout->addWidget(m_backSideTitleLabel);
    cardLayout->addWidget(m_backEdit, 1);

    auto *reviewTopLayout = new QHBoxLayout;
    reviewTopLayout->setContentsMargins(0, 0, 0, 0);
    reviewTopLayout->addWidget(m_backToLibraryButton);
    reviewTopLayout->addStretch();

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

    auto *reviewLayout = new QVBoxLayout(m_reviewAreaWidget);
    reviewLayout->setContentsMargins(0, 0, 0, 0);
    reviewLayout->setSpacing(12);
    reviewLayout->addLayout(reviewTopLayout);
    reviewLayout->addWidget(m_cardFrame, 1);
    reviewLayout->addLayout(answerButtonLayout);
    reviewLayout->addLayout(ratingLayout);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(22, 20, 22, 20);
    mainLayout->setSpacing(12);
    mainLayout->addLayout(headerLayout);
    mainLayout->addWidget(m_libraryWidget, 1);
    mainLayout->addWidget(m_deckDetailWidget, 1);
    mainLayout->addWidget(m_reviewAreaWidget, 1);
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

        QLineEdit#deckSearchEdit {
            background: #ffffff;
            border: 1px solid #dfe6f1;
            border-radius: 16px;
            padding: 0 16px;
            color: #263244;
            font-size: 15px;
        }

        QLineEdit#deckSearchEdit:focus {
            border-color: #42b9a7;
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

        QPushButton[groupTab="true"] {
            border: none;
            background: transparent;
            color: #64748b;
            padding: 8px 4px;
            font-size: 17px;
        }

        QPushButton[groupTab="true"]:hover {
            color: #2f9f90;
            background: transparent;
        }

        QPushButton[groupTab="true"][active="true"] {
            color: #2f9f90;
            font-weight: 800;
        }

        QPushButton#groupAddButton {
            min-width: 40px;
            border: none;
            background: transparent;
            color: #475569;
            font-size: 24px;
            padding: 2px 8px;
        }

        QPushButton#groupAddButton:hover {
            color: #2f9f90;
            background: transparent;
        }

        QPushButton[buttonRole="primary"] {
            background: #2f6fed;
            border-color: #2f6fed;
            color: #ffffff;
        }

        QPushButton[buttonRole="primary"]:hover {
            background: #255fd0;
            border-color: #255fd0;
        }

        QPushButton[buttonRole="ghost"] {
            background: #f8fafd;
            color: #475569;
        }

        QPushButton[buttonRole="normal"] {
            background: #ffffff;
            color: #475569;
        }

        QPushButton[buttonRole="accent"] {
            background: #eef8f4;
            border-color: #a9d8c4;
            color: #1f7a55;
        }

        QPushButton[buttonRole="accent"]:hover {
            background: #def1e9;
            border-color: #86c8ac;
        }

        QPushButton[buttonRole="danger"] {
            background: #fff1f1;
            border-color: #f1b8b8;
            color: #c73d3d;
        }

        QPushButton[buttonRole="danger"]:hover {
            background: #ffe4e4;
            border-color: #e99292;
        }

        QScrollArea#deckScrollArea {
            background: transparent;
        }

        QScrollArea#cardListScrollArea {
            background: transparent;
        }

        QFrame#deckCard {
            background: #ffffff;
            border: 1px solid #e2e8f0;
            border-radius: 16px;
        }

        QFrame#deckCard:hover {
            border-color: #b9d7ce;
            background: #fbfefd;
        }

        QLabel#deckTitle {
            color: #263244;
            font-size: 20px;
            font-weight: 800;
        }

        QLabel#deckMeta {
            color: #7b8798;
            font-size: 13px;
        }

        QProgressBar#deckProgressBar {
            height: 8px;
            border: none;
            border-radius: 4px;
            background: #edf2f7;
        }

        QProgressBar#deckProgressBar::chunk {
            border-radius: 4px;
            background: #42b9a7;
        }

        QLabel#deckDueBadge {
            background: #f97357;
            color: #ffffff;
            border-radius: 12px;
            padding: 3px 9px;
            font-weight: 800;
        }

        QLabel#emptyLibraryLabel {
            color: #64748b;
            background: #ffffff;
            border: 1px dashed #cbd5e1;
            border-radius: 14px;
            padding: 30px;
            font-size: 15px;
        }

        QFrame#cardPreview {
            background: #ffffff;
            border: 1px solid #e2e8f0;
            border-radius: 12px;
        }

        QLabel#cardPreviewFront {
            color: #1f2d3d;
            font-size: 16px;
            font-weight: 800;
        }

        QLabel#cardPreviewBack {
            color: #475569;
            font-size: 13px;
        }

        QLabel#cardPreviewMeta {
            color: #7b8798;
            font-size: 12px;
        }

        QLabel#cardSideTitle {
            color: #7b8798;
            font-size: 12px;
            font-weight: 700;
        }

        QFrame#flashCardFrame {
            background: #ffffff;
            border: 1px solid #d9e2f1;
            border-radius: 18px;
        }

        QTextEdit#cardText,
        QTextEdit#answerText {
            background: transparent;
            border: none;
            padding: 8px 0;
            selection-background-color: #dce8ff;
            font-size: 18px;
        }

        QTextEdit#answerText {
            color: #334155;
            font-size: 16px;
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
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ReviewWidget::reloadDeckLibrary);
    connect(m_newGroupButton, &QPushButton::clicked, this, &ReviewWidget::createGroup);
    connect(m_newDeckButton, &QPushButton::clicked, this, &ReviewWidget::createDeck);
    connect(m_deleteGroupButton, &QPushButton::clicked, this, &ReviewWidget::deleteCurrentGroup);
    connect(m_addCardButton, &QPushButton::clicked, this, &ReviewWidget::addCard);
    connect(m_importCardButton, &QPushButton::clicked, this, &ReviewWidget::importCardsFromFile);
    connect(m_backToPlannerButton, &QPushButton::clicked, this, &ReviewWidget::backRequested);
    connect(m_backFromDetailButton, &QPushButton::clicked, this, &ReviewWidget::showLibrary);
    connect(m_startReviewFromDetailButton, &QPushButton::clicked, this, [this]() {
        startDeckReview(m_currentGroup, m_currentDeck);
    });
    connect(m_backToLibraryButton, &QPushButton::clicked, this, &ReviewWidget::showLibrary);
    connect(m_showAnswerButton, &QPushButton::clicked, this, &ReviewWidget::showAnswer);
    connect(m_forgetButton, &QPushButton::clicked, this, [this]() { submitRating(0); });
    connect(m_blurryButton, &QPushButton::clicked, this, [this]() { submitRating(1); });
    connect(m_rememberButton, &QPushButton::clicked, this, [this]() { submitRating(2); });
    connect(m_easyButton, &QPushButton::clicked, this, [this]() { submitRating(3); });
}

void ReviewWidget::reloadCards()
{
    /*
     * MainWindow 每次切到 Anki 页面都会调用这里。
     * 入口默认回到牌库首页，而不是直接进入某个旧牌组。
     */
    reloadGroups();
    reloadDecks();
    showLibrary();
}

void ReviewWidget::reloadGroups()
{
    m_groups.clear();
    if (m_databaseManager != nullptr) {
        m_groups = m_databaseManager->getFlashCardGroups();
    }

    if (m_groups.isEmpty()) {
        m_groups.append("默认分组");
    }
    if (!m_groups.contains(m_currentGroup)) {
        m_currentGroup = m_groups.first();
    }
}

void ReviewWidget::reloadDecks()
{
    m_decks.clear();
    if (m_databaseManager != nullptr) {
        m_decks = m_databaseManager->getFlashCardDecks(m_currentGroup);
    }
}

void ReviewWidget::reloadDeckLibrary()
{
    clearDeckList();

    while (QLayoutItem *item = m_groupTabsLayout->takeAt(0)) {
        if (item->widget() != nullptr) {
            if (item->widget() == m_newGroupButton) {
                m_newGroupButton->setParent(this);
            } else {
                item->widget()->deleteLater();
            }
        }
        delete item;
    }

    for (const QString &group : m_groups) {
        auto *groupButton = new QPushButton(group, this);
        groupButton->setProperty("groupTab", true);
        groupButton->setProperty("active", group == m_currentGroup);
        groupButton->setCursor(Qt::PointingHandCursor);
        connect(groupButton, &QPushButton::clicked, this, [this, group]() {
            m_currentGroup = group;
            m_currentDeck.clear();
            reloadDecks();
            reloadDeckLibrary();
        });
        m_groupTabsLayout->addWidget(groupButton);
    }
    m_groupTabsLayout->addWidget(m_newGroupButton);
    m_groupTabsLayout->addStretch();

    if (m_databaseManager == nullptr) {
        auto *emptyLabel = new QLabel("数据库还没有初始化，暂时不能读取牌库。", m_deckListWidget);
        emptyLabel->setObjectName("emptyLibraryLabel");
        emptyLabel->setAlignment(Qt::AlignCenter);
        m_deckListLayout->addWidget(emptyLabel);
        m_deckListLayout->addStretch();
        return;
    }

    const QVector<DeckOverview> overviews =
        m_databaseManager->getFlashCardDeckOverviews(m_currentGroup, m_searchEdit->text());

    if (overviews.isEmpty()) {
        auto *emptyLabel = new QLabel("这个分组里还没有牌组，点击“新建牌组”开始整理卡片。", m_deckListWidget);
        emptyLabel->setObjectName("emptyLibraryLabel");
        emptyLabel->setAlignment(Qt::AlignCenter);
        m_deckListLayout->addWidget(emptyLabel);
        m_deckListLayout->addStretch();
        return;
    }

    for (const DeckOverview &overview : overviews) {
        addDeckCard(overview);
    }
    m_deckListLayout->addStretch();
}

void ReviewWidget::clearDeckList()
{
    while (QLayoutItem *item = m_deckListLayout->takeAt(0)) {
        if (item->widget() != nullptr) {
            item->widget()->deleteLater();
        }
        delete item;
    }
}

void ReviewWidget::clearCardList()
{
    while (QLayoutItem *item = m_cardListLayout->takeAt(0)) {
        if (item->widget() != nullptr) {
            item->widget()->deleteLater();
        }
        delete item;
    }
}

void ReviewWidget::addCardPreview(const FlashCard &cardData)
{
    auto *card = new QFrame(m_cardListWidget);
    card->setObjectName("cardPreview");
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *colorStripe = new QFrame(card);
    colorStripe->setFixedWidth(10);
    colorStripe->setStyleSheet(QString("background: %1; border-radius: 5px;")
                                   .arg(cardData.cardColor));

    auto *frontLabel = new QLabel(cardData.front, card);
    frontLabel->setObjectName("cardPreviewFront");
    frontLabel->setWordWrap(true);

    auto *backLabel = new QLabel(cardData.back.isEmpty()
                                     ? QString("暂无背面内容")
                                     : cardData.back,
                                 card);
    backLabel->setObjectName("cardPreviewBack");
    backLabel->setWordWrap(true);

    const QString tagText = cardData.tag.isEmpty() ? QString("未分类") : cardData.tag;
    auto *metaLabel = new QLabel(QString("标签：%1    已复习：%2 次    间隔：%3 天    下次复习：%4")
                                     .arg(tagText)
                                     .arg(cardData.reviewCount)
                                     .arg(cardData.intervalDays)
                                     .arg(cardData.dueTime),
                                 card);
    metaLabel->setObjectName("cardPreviewMeta");
    metaLabel->setWordWrap(true);

    auto *textLayout = new QVBoxLayout;
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(8);
    textLayout->addWidget(frontLabel);
    textLayout->addWidget(backLabel);
    textLayout->addWidget(metaLabel);

    auto *cardLayout = new QHBoxLayout(card);
    cardLayout->setContentsMargins(14, 12, 16, 12);
    cardLayout->setSpacing(12);
    cardLayout->addWidget(colorStripe);
    cardLayout->addLayout(textLayout, 1);

    m_cardListLayout->addWidget(card);
}

void ReviewWidget::addDeckCard(const DeckOverview &overview)
{
    auto *card = new QFrame(m_deckListWidget);
    card->setObjectName("deckCard");
    card->setMinimumHeight(96);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *shadowEffect = new QGraphicsDropShadowEffect(card);
    shadowEffect->setBlurRadius(18);
    shadowEffect->setOffset(0, 6);
    shadowEffect->setColor(QColor(38, 50, 68, 26));
    card->setGraphicsEffect(shadowEffect);

    auto *colorStripe = new QFrame(card);
    colorStripe->setFixedWidth(84);
    colorStripe->setStyleSheet(QString("background: %1; border-radius: 14px;").arg(overview.color));

    const int progress = overview.totalCards > 0
                             ? qMin(100, overview.reviewedCards * 100 / overview.totalCards)
                             : 0;

    auto *titleLabel = new QLabel(overview.deck, card);
    titleLabel->setObjectName("deckTitle");

    auto *metaLabel = new QLabel(QString("卡片 %1 张  |  已复习 %2 张  |  完成度 %3%")
                                     .arg(overview.totalCards)
                                     .arg(overview.reviewedCards)
                                     .arg(progress),
                                 card);
    metaLabel->setObjectName("deckMeta");

    auto *progressBar = new QProgressBar(card);
    progressBar->setObjectName("deckProgressBar");
    progressBar->setRange(0, 100);
    progressBar->setValue(progress);
    progressBar->setTextVisible(false);

    const QString dueText = overview.dueCards > 0
                                ? QString("今日待复习 %1 张").arg(overview.dueCards)
                                : QString("今天没有到期卡片");
    auto *dueLabel = new QLabel(dueText, card);
    dueLabel->setObjectName("deckMeta");

    auto *badgeLabel = new QLabel(QString::number(overview.dueCards), card);
    badgeLabel->setObjectName("deckDueBadge");
    badgeLabel->setAlignment(Qt::AlignCenter);
    badgeLabel->setVisible(overview.dueCards > 0);

    auto *startButton = new QPushButton("开始复习", card);
    startButton->setProperty("buttonRole", "primary");
    startButton->setCursor(Qt::PointingHandCursor);
    startButton->setEnabled(overview.dueCards > 0);
    connect(startButton, &QPushButton::clicked, this, [this, overview]() {
        startDeckReview(overview.group, overview.deck);
    });

    auto *viewButton = new QPushButton("查看卡片", card);
    viewButton->setProperty("buttonRole", "normal");
    viewButton->setCursor(Qt::PointingHandCursor);
    connect(viewButton, &QPushButton::clicked, this, [this, overview]() {
        showDeckDetail(overview.group, overview.deck);
    });

    auto *deleteButton = new QPushButton("删除牌组", card);
    deleteButton->setProperty("buttonRole", "danger");
    deleteButton->setCursor(Qt::PointingHandCursor);
    connect(deleteButton, &QPushButton::clicked, this, [this, overview]() {
        deleteDeck(overview);
    });

    auto *textLayout = new QVBoxLayout;
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(8);
    textLayout->addWidget(titleLabel);
    textLayout->addWidget(metaLabel);
    textLayout->addWidget(progressBar);
    textLayout->addWidget(dueLabel);

    auto *actionLayout = new QVBoxLayout;
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(10);
    actionLayout->addWidget(badgeLabel, 0, Qt::AlignRight);
    actionLayout->addStretch();
    actionLayout->addWidget(viewButton);
    actionLayout->addWidget(startButton);
    actionLayout->addWidget(deleteButton);

    auto *cardLayout = new QHBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 18, 0);
    cardLayout->setSpacing(18);
    cardLayout->addWidget(colorStripe);
    cardLayout->addLayout(textLayout, 1);
    cardLayout->addLayout(actionLayout);

    m_deckListLayout->addWidget(card);
}

void ReviewWidget::showLibrary()
{
    reloadGroups();
    reloadDecks();

    m_titleLabel->setText("Anki 牌库");
    m_statusLabel->setText("按分组整理牌组，选择一个牌组开始今日复习");
    m_tagLabel->setText("当前分组：" + m_currentGroup);
    m_cards.clear();
    m_currentIndex = 0;

    m_libraryWidget->setVisible(true);
    m_deckDetailWidget->setVisible(false);
    m_reviewAreaWidget->setVisible(false);
    reloadDeckLibrary();
}

void ReviewWidget::showDeckDetail(const QString &group, const QString &deck)
{
    if (m_databaseManager == nullptr) {
        QMessageBox::warning(this, "提示", "数据库还没有初始化，暂时不能查看牌组。");
        return;
    }

    m_currentGroup = group.trimmed().isEmpty() ? QString("默认分组") : group.trimmed();
    m_currentDeck = deck.trimmed().isEmpty() ? QString("默认牌组") : deck.trimmed();
    reloadDecks();
    clearCardList();

    m_deckCards = m_databaseManager->getFlashCardsByDeck(m_currentDeck, m_currentGroup);
    const QVector<FlashCard> dueCards = m_databaseManager->getDueFlashCards(m_currentDeck, m_currentGroup);

    m_titleLabel->setText(m_currentDeck);
    m_statusLabel->setText(QString("分组：%1    共 %2 张卡片    今日待复习 %3 张")
                               .arg(m_currentGroup)
                               .arg(m_deckCards.size())
                               .arg(dueCards.size()));
    m_tagLabel->setText("查看这个牌组里的具体卡片");
    m_startReviewFromDetailButton->setText(QString("开始复习 (%1)").arg(dueCards.size()));
    m_startReviewFromDetailButton->setEnabled(!dueCards.isEmpty());

    if (m_deckCards.isEmpty()) {
        auto *emptyLabel = new QLabel("这个牌组还没有卡片，返回牌库后可以点击“添加卡片”。", m_cardListWidget);
        emptyLabel->setObjectName("emptyLibraryLabel");
        emptyLabel->setAlignment(Qt::AlignCenter);
        m_cardListLayout->addWidget(emptyLabel);
    } else {
        for (const FlashCard &cardData : m_deckCards) {
            addCardPreview(cardData);
        }
    }
    m_cardListLayout->addStretch();

    m_libraryWidget->setVisible(false);
    m_deckDetailWidget->setVisible(true);
    m_reviewAreaWidget->setVisible(false);
}

void ReviewWidget::showReviewArea()
{
    m_libraryWidget->setVisible(false);
    m_deckDetailWidget->setVisible(false);
    m_reviewAreaWidget->setVisible(true);
}

void ReviewWidget::startDeckReview(const QString &group, const QString &deck)
{
    if (m_databaseManager == nullptr) {
        QMessageBox::warning(this, "提示", "数据库还没有初始化，暂时不能开始复习。");
        return;
    }

    m_currentGroup = group.trimmed().isEmpty() ? QString("默认分组") : group.trimmed();
    m_currentDeck = deck.trimmed().isEmpty() ? QString("默认牌组") : deck.trimmed();
    reloadDecks();

    m_cards = m_databaseManager->getDueFlashCards(m_currentDeck, m_currentGroup);
    m_currentIndex = 0;
    showReviewArea();
    showCurrentCard();
}

QString ReviewWidget::selectedGroupForNewCards() const
{
    return m_currentGroup.trimmed().isEmpty() ? QString("默认分组") : m_currentGroup.trimmed();
}

QString ReviewWidget::selectedDeckForNewCards() const
{
    const QString deckName = m_currentDeck.trimmed();
    if (!deckName.isEmpty() && m_decks.contains(deckName)) {
        return deckName;
    }

    if (!m_decks.isEmpty()) {
        return m_decks.first();
    }

    return QString();
}

void ReviewWidget::applyCardColor(const QString &cardColor)
{
    const QString color = cardColor.trimmed().isEmpty() ? QString("#ffffff") : cardColor.trimmed();
    m_cardFrame->setStyleSheet(QString(R"(
        QFrame#flashCardFrame {
            background: %1;
            border: 1px solid #d9e2f1;
            border-radius: 18px;
        }
    )").arg(color));
}

void ReviewWidget::createGroup()
{
    if (m_databaseManager == nullptr) {
        QMessageBox::warning(this, "提示", "数据库还没有初始化，暂时不能新建分组。");
        return;
    }

    bool ok = false;
    const QString groupName = QInputDialog::getText(this,
                                                    "新建分组",
                                                    "分组名称:",
                                                    QLineEdit::Normal,
                                                    QString(),
                                                    &ok).trimmed();
    if (!ok) {
        return;
    }

    if (groupName.isEmpty()) {
        QMessageBox::warning(this, "提示", "分组名称不能为空。");
        return;
    }

    if (!m_databaseManager->addFlashCardGroup(groupName)) {
        QMessageBox::warning(this, "提示", "新建分组失败，请查看调试输出。");
        return;
    }

    m_currentGroup = groupName;
    m_currentDeck.clear();
    reloadGroups();
    reloadDecks();
    reloadDeckLibrary();
}

void ReviewWidget::createDeck()
{
    if (m_databaseManager == nullptr) {
        QMessageBox::warning(this, "提示", "数据库还没有初始化，暂时不能新建牌组。");
        return;
    }

    bool ok = false;
    const QString deckName = QInputDialog::getText(this,
                                                   "新建牌组",
                                                   QString("在“%1”中新建牌组:").arg(selectedGroupForNewCards()),
                                                   QLineEdit::Normal,
                                                   QString(),
                                                   &ok).trimmed();
    if (!ok) {
        return;
    }

    if (deckName.isEmpty()) {
        QMessageBox::warning(this, "提示", "牌组名称不能为空。");
        return;
    }

    if (!m_databaseManager->addFlashCardDeck(deckName, selectedGroupForNewCards())) {
        QMessageBox::warning(this, "提示", "新建牌组失败，请查看调试输出。");
        return;
    }

    m_currentDeck = deckName;
    reloadDecks();
    reloadDeckLibrary();
}

void ReviewWidget::deleteCurrentGroup()
{
    if (m_databaseManager == nullptr) {
        QMessageBox::warning(this, "提示", "数据库还没有初始化，暂时不能删除分组。");
        return;
    }

    const QString groupName = selectedGroupForNewCards();
    const QMessageBox::StandardButton result =
        QMessageBox::question(this,
                              "删除分组",
                              QString("确定删除分组“%1”吗？\n\n这个操作会同时删除该分组下的所有牌组和复习卡片。")
                                  .arg(groupName),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No);
    if (result != QMessageBox::Yes) {
        return;
    }

    if (!m_databaseManager->deleteFlashCardGroup(groupName)) {
        QMessageBox::warning(this, "提示", "删除分组失败，请查看调试输出。");
        return;
    }

    m_currentGroup = "默认分组";
    m_currentDeck.clear();
    showLibrary();
}

void ReviewWidget::deleteDeck(const DeckOverview &overview)
{
    if (m_databaseManager == nullptr) {
        QMessageBox::warning(this, "提示", "数据库还没有初始化，暂时不能删除牌组。");
        return;
    }

    const QMessageBox::StandardButton result =
        QMessageBox::question(this,
                              "删除牌组",
                              QString("确定删除“%1 / %2”吗？\n\n这个操作会删除该牌组里的 %3 张卡片。")
                                  .arg(overview.group)
                                  .arg(overview.deck)
                                  .arg(overview.totalCards),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No);
    if (result != QMessageBox::Yes) {
        return;
    }

    if (!m_databaseManager->deleteFlashCardDeck(overview.deck, overview.group)) {
        QMessageBox::warning(this, "提示", "删除牌组失败，请查看调试输出。");
        return;
    }

    if (m_currentGroup == overview.group && m_currentDeck == overview.deck) {
        m_currentDeck.clear();
    }
    showLibrary();
}

void ReviewWidget::addCard()
{
    if (m_databaseManager == nullptr) {
        QMessageBox::warning(this, "提示", "数据库还没有初始化，暂时不能添加卡片。");
        return;
    }

    reloadDecks();
    if (m_decks.isEmpty()) {
        QMessageBox::warning(this,
                             "提示",
                             QString("“%1”分组里还没有牌组，请先新建牌组，再添加卡片。")
                                 .arg(selectedGroupForNewCards()));
        return;
    }

    CardDialog dialog(this);
    QMap<QString, QStringList> decksByGroup;
    for (const QString &group : m_groups) {
        decksByGroup.insert(group, m_databaseManager->getFlashCardDecks(group));
    }
    dialog.setDecksByGroup(decksByGroup, selectedGroupForNewCards(), selectedDeckForNewCards());
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const bool success = m_databaseManager->addFlashCard(dialog.getFront(),
                                                         dialog.getBack(),
                                                         dialog.getTag(),
                                                         dialog.getGroup(),
                                                         dialog.getDeck(),
                                                         dialog.getCardColor());
    if (!success) {
        QMessageBox::warning(this, "提示", "添加复习卡片失败，请查看调试输出。");
        return;
    }

    m_currentGroup = dialog.getGroup();
    m_currentDeck = dialog.getDeck();
    reloadGroups();
    reloadDecks();
    showLibrary();
}

void ReviewWidget::importCardsFromFile()
{
    if (m_databaseManager == nullptr) {
        QMessageBox::warning(this, "提示", "数据库还没有初始化，暂时不能导入卡片。");
        return;
    }

    reloadDecks();
    if (m_decks.isEmpty()) {
        QMessageBox::warning(this,
                             "提示",
                             QString("“%1”分组里还没有牌组，请先新建牌组，再导入卡片。")
                                 .arg(selectedGroupForNewCards()));
        return;
    }

    const QString filePath = QFileDialog::getOpenFileName(this,
                                                          "选择 Anki 文本文件",
                                                          QString(),
                                                          "文本文件 (*.txt *.tsv);;所有文件 (*)");
    if (filePath.isEmpty()) {
        return;
    }

    const QString groupName = selectedGroupForNewCards();
    const QString deckName = selectedDeckForNewCards();
    const int importedCount =
        m_databaseManager->importFlashCardsFromTextFile(filePath, groupName, deckName, "#ffffff");
    if (importedCount < 0) {
        QMessageBox::warning(this, "导入失败", "导入 Anki 文本文件失败，请查看调试输出。");
        return;
    }

    QMessageBox::information(this,
                             "导入完成",
                             QString("已导入 %1 张复习卡片到“%2 / %3”。")
                                 .arg(importedCount)
                                 .arg(groupName)
                                 .arg(deckName));
    showLibrary();
}

void ReviewWidget::showCurrentCard()
{
    if (m_currentIndex >= m_cards.size()) {
        showEmptyState();
        return;
    }

    const FlashCard &card = m_cards.at(m_currentIndex);
    m_titleLabel->setText("Anki 复习");
    m_statusLabel->setText(QString("第 %1 / %2 张    分组：%3    牌组：%4    已复习 %5 次")
                               .arg(m_currentIndex + 1)
                               .arg(m_cards.size())
                               .arg(card.group)
                               .arg(card.deck)
                               .arg(card.reviewCount));
    m_tagLabel->setText("标签：" + (card.tag.isEmpty() ? QString("未分类") : card.tag));
    applyCardColor(card.cardColor);
    m_frontEdit->setPlainText(card.front);
    m_backEdit->setPlainText(card.back);
    m_backSideTitleLabel->setVisible(false);
    m_backEdit->setVisible(false);
    m_showAnswerButton->setVisible(true);
    m_showAnswerButton->setEnabled(true);
    setReviewButtonsVisible(false);
    playCardEnterAnimation();
}

void ReviewWidget::showEmptyState()
{
    m_titleLabel->setText("Anki 复习");
    m_statusLabel->setText(QString("“%1 / %2”今天没有需要复习的卡片")
                               .arg(selectedGroupForNewCards())
                               .arg(selectedDeckForNewCards()));
    m_tagLabel->clear();
    applyCardColor("#ffffff");
    m_frontEdit->setPlainText("今天没有需要复习的卡片，可以返回牌库添加卡片或选择其他牌组。");
    m_backEdit->clear();
    m_backSideTitleLabel->setVisible(false);
    m_backEdit->setVisible(false);
    m_showAnswerButton->setVisible(false);
    setReviewButtonsVisible(false);
    playCardEnterAnimation();
}

void ReviewWidget::showAnswer()
{
    if (m_currentIndex >= m_cards.size()) {
        return;
    }

    m_backSideTitleLabel->setVisible(true);
    m_backEdit->setVisible(true);
    m_showAnswerButton->setVisible(false);
    setReviewButtonsVisible(true);
    playAnswerRevealAnimation();
}

void ReviewWidget::submitRating(int rating)
{
    if (m_currentIndex >= m_cards.size()) {
        showEmptyState();
        return;
    }

    if (m_databaseManager == nullptr) {
        QMessageBox::warning(this, "提示", "数据库还没有初始化，无法保存复习结果。");
        return;
    }

    const FlashCard &card = m_cards.at(m_currentIndex);
    if (!m_databaseManager->updateFlashCardAfterReview(card.id, rating)) {
        QMessageBox::warning(this, "提示", "保存复习结果失败，请查看调试输出。");
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

void ReviewWidget::playCardEnterAnimation()
{
    if (!m_cardFrame->isVisible()) {
        return;
    }

    const QPoint endPosition = m_cardFrame->pos();
    const QPoint startPosition = endPosition + QPoint(0, 18);
    m_cardFrame->move(startPosition);

    auto *moveAnimation = new QPropertyAnimation(m_cardFrame, "pos", this);
    moveAnimation->setDuration(220);
    moveAnimation->setStartValue(startPosition);
    moveAnimation->setEndValue(endPosition);
    moveAnimation->setEasingCurve(QEasingCurve::OutCubic);
    moveAnimation->start(QAbstractAnimation::DeleteWhenStopped);

    m_frontOpacityEffect->setOpacity(0.0);
    auto *frontOpacityAnimation = new QPropertyAnimation(m_frontOpacityEffect, "opacity", this);
    frontOpacityAnimation->setDuration(200);
    frontOpacityAnimation->setStartValue(0.0);
    frontOpacityAnimation->setEndValue(1.0);
    frontOpacityAnimation->setEasingCurve(QEasingCurve::OutCubic);
    frontOpacityAnimation->start(QAbstractAnimation::DeleteWhenStopped);

    m_cardShadowEffect->setBlurRadius(14);
    auto *shadowAnimation = new QPropertyAnimation(m_cardShadowEffect, "blurRadius", this);
    shadowAnimation->setDuration(240);
    shadowAnimation->setStartValue(14);
    shadowAnimation->setEndValue(28);
    shadowAnimation->setEasingCurve(QEasingCurve::OutCubic);
    shadowAnimation->start(QAbstractAnimation::DeleteWhenStopped);
}

void ReviewWidget::playAnswerRevealAnimation()
{
    m_backTitleOpacityEffect->setOpacity(0.0);
    m_backOpacityEffect->setOpacity(0.0);

    auto *titleAnimation = new QPropertyAnimation(m_backTitleOpacityEffect, "opacity", this);
    titleAnimation->setDuration(160);
    titleAnimation->setStartValue(0.0);
    titleAnimation->setEndValue(1.0);
    titleAnimation->setEasingCurve(QEasingCurve::OutCubic);
    titleAnimation->start(QAbstractAnimation::DeleteWhenStopped);

    auto *answerAnimation = new QPropertyAnimation(m_backOpacityEffect, "opacity", this);
    answerAnimation->setDuration(220);
    answerAnimation->setStartValue(0.0);
    answerAnimation->setEndValue(1.0);
    answerAnimation->setEasingCurve(QEasingCurve::OutCubic);
    answerAnimation->start(QAbstractAnimation::DeleteWhenStopped);
}
