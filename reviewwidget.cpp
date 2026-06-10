#include "reviewwidget.h"

#include "carddialog.h"
#include "databasemanager.h"

#include <QColor>
#include <QComboBox>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTextEdit>
#include <QVBoxLayout>

ReviewWidget::ReviewWidget(DatabaseManager *databaseManager, QWidget *parent)
    : QWidget(parent),
      m_databaseManager(databaseManager),
      m_currentIndex(0),
      m_titleLabel(new QLabel("Anki 复习", this)),
      m_statusLabel(new QLabel(this)),
      m_tagLabel(new QLabel(this)),
      m_deckLabel(new QLabel("当前牌组:", this)),
      m_backSideTitleLabel(new QLabel("背面", this)),
      m_deckCombo(new QComboBox(this)),
      m_cardFrame(new QFrame(this)),
      m_frontEdit(new QTextEdit(this)),
      m_backEdit(new QTextEdit(this)),
      m_showAnswerButton(new QPushButton("显示答案", this)),
      m_newDeckButton(new QPushButton("新建牌组", this)),
      m_addCardButton(new QPushButton("添加卡片", this)),
      m_importCardButton(new QPushButton("导入 Anki 卡片", this)),
      m_backToPlannerButton(new QPushButton("返回计划", this)),
      m_forgetButton(new QPushButton("忘了", this)),
      m_blurryButton(new QPushButton("模糊", this)),
      m_rememberButton(new QPushButton("记住了", this)),
      m_easyButton(new QPushButton("很熟", this))
{
    setupUi();
    setupStyleSheet();
    connectSignals();
    showEmptyState();
}

void ReviewWidget::setupUi()
{
    /*
     * 页面结构：
     * 顶部是标题、牌组筛选、添加/导入按钮和返回按钮。
     * 中间是实体化卡片，底部是显示答案和四个反馈按钮。
     */
    m_titleLabel->setObjectName("reviewTitle");
    m_statusLabel->setObjectName("reviewStatus");
    m_tagLabel->setObjectName("reviewTag");
    m_deckLabel->setObjectName("deckLabel");

    m_deckCombo->setObjectName("deckCombo");
    m_deckCombo->setMinimumWidth(180);
    m_deckCombo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_cardFrame->setObjectName("flashCardFrame");
    m_cardFrame->setMinimumHeight(360);

    auto *shadowEffect = new QGraphicsDropShadowEffect(m_cardFrame);
    shadowEffect->setBlurRadius(28);
    shadowEffect->setOffset(0, 10);
    shadowEffect->setColor(QColor(38, 50, 68, 45));
    m_cardFrame->setGraphicsEffect(shadowEffect);

    m_frontEdit->setObjectName("cardText");
    m_frontEdit->setReadOnly(true);
    m_frontEdit->setMinimumHeight(150);
    m_frontEdit->setFrameShape(QFrame::NoFrame);

    m_backEdit->setObjectName("answerText");
    m_backEdit->setReadOnly(true);
    m_backEdit->setMinimumHeight(150);
    m_backEdit->setFrameShape(QFrame::NoFrame);

    m_showAnswerButton->setObjectName("showAnswerButton");
    m_newDeckButton->setProperty("buttonRole", "normal");
    m_addCardButton->setProperty("buttonRole", "primary");
    m_importCardButton->setProperty("buttonRole", "accent");
    m_backToPlannerButton->setProperty("buttonRole", "ghost");

    const QList<QPushButton *> buttons = {
        m_showAnswerButton,
        m_newDeckButton,
        m_addCardButton,
        m_importCardButton,
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

    auto *controlLayout = new QHBoxLayout;
    controlLayout->setContentsMargins(0, 0, 0, 0);
    controlLayout->setSpacing(10);
    controlLayout->addWidget(m_deckLabel);
    controlLayout->addWidget(m_deckCombo);
    controlLayout->addWidget(m_newDeckButton);
    controlLayout->addStretch();
    controlLayout->addWidget(m_addCardButton);
    controlLayout->addWidget(m_importCardButton);

    auto *frontTitleLabel = new QLabel("正面", this);
    frontTitleLabel->setObjectName("cardSideTitle");
    m_backSideTitleLabel->setObjectName("cardSideTitle");

    auto *cardLayout = new QVBoxLayout(m_cardFrame);
    cardLayout->setContentsMargins(28, 24, 28, 24);
    cardLayout->setSpacing(10);
    cardLayout->addWidget(frontTitleLabel);
    cardLayout->addWidget(m_frontEdit, 1);
    cardLayout->addWidget(m_backSideTitleLabel);
    cardLayout->addWidget(m_backEdit, 1);

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
    mainLayout->addLayout(controlLayout);
    mainLayout->addWidget(m_cardFrame, 1);
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

        QLabel#deckLabel {
            color: #64748b;
            font-size: 13px;
            font-weight: 600;
        }

        QLabel#cardSideTitle {
            color: #7b8798;
            font-size: 12px;
            font-weight: 700;
        }

        QComboBox#deckCombo {
            background: #ffffff;
            border: 1px solid #dfe6f1;
            border-radius: 8px;
            padding: 8px 10px;
            color: #263244;
        }

        QComboBox#deckCombo:focus {
            border-color: #2f6fed;
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

        QPushButton[buttonRole="normal"]:hover {
            background: #f0f5ff;
            border-color: #adc3ea;
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
    connect(m_newDeckButton, &QPushButton::clicked, this, &ReviewWidget::createDeck);
    connect(m_addCardButton, &QPushButton::clicked, this, &ReviewWidget::addCard);
    connect(m_importCardButton, &QPushButton::clicked, this, &ReviewWidget::importCardsFromFile);
    connect(m_deckCombo, &QComboBox::currentTextChanged, this, [this]() {
        reloadCards();
    });
    connect(m_backToPlannerButton, &QPushButton::clicked, this, &ReviewWidget::backRequested);
    connect(m_showAnswerButton, &QPushButton::clicked, this, &ReviewWidget::showAnswer);
    connect(m_forgetButton, &QPushButton::clicked, this, [this]() { submitRating(0); });
    connect(m_blurryButton, &QPushButton::clicked, this, [this]() { submitRating(1); });
    connect(m_rememberButton, &QPushButton::clicked, this, [this]() { submitRating(2); });
    connect(m_easyButton, &QPushButton::clicked, this, [this]() { submitRating(3); });
}

void ReviewWidget::reloadDecks()
{
    const QString currentDeck = m_deckCombo->currentText();

    m_decks.clear();
    if (m_databaseManager != nullptr) {
        m_decks = m_databaseManager->getFlashCardDecks();
    }

    QSignalBlocker blocker(m_deckCombo);
    m_deckCombo->clear();
    m_deckCombo->addItem("全部牌组");
    for (const QString &deck : m_decks) {
        if (!deck.trimmed().isEmpty() && m_deckCombo->findText(deck) < 0) {
            m_deckCombo->addItem(deck);
        }
    }

    const int restoredIndex = m_deckCombo->findText(currentDeck);
    if (restoredIndex >= 0) {
        m_deckCombo->setCurrentIndex(restoredIndex);
    } else {
        m_deckCombo->setCurrentIndex(0);
    }
}

QString ReviewWidget::selectedDeckFilter() const
{
    const QString deck = m_deckCombo->currentText().trimmed();
    return deck == "全部牌组" ? QString() : deck;
}

QString ReviewWidget::selectedDeckForNewCards() const
{
    const QString deck = selectedDeckFilter();
    return deck.isEmpty() ? QString("默认牌组") : deck;
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

void ReviewWidget::createDeck()
{
    if (m_databaseManager == nullptr) {
        QMessageBox::warning(this, "提示", "数据库还没有初始化，暂时不能新建牌组。");
        return;
    }

    bool ok = false;
    const QString deckName = QInputDialog::getText(this,
                                                   "新建牌组",
                                                   "牌组名称:",
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

    if (!m_databaseManager->addFlashCardDeck(deckName)) {
        QMessageBox::warning(this, "提示", "新建牌组失败，请查看调试输出。");
        return;
    }

    reloadDecks();
    const int index = m_deckCombo->findText(deckName);
    if (index >= 0) {
        m_deckCombo->setCurrentIndex(index);
    }
    reloadCards();
}

void ReviewWidget::reloadCards()
{
    /*
     * 复习页每次显示前都会重新查询数据库。
     * getDueFlashCards() 已经在 SQL 中限制 due_time <= 当前时间。
     */
    m_cards.clear();
    reloadDecks();

    if (m_databaseManager != nullptr) {
        m_cards = m_databaseManager->getDueFlashCards(selectedDeckFilter());
    }

    m_currentIndex = 0;
    showCurrentCard();
}

void ReviewWidget::addCard()
{
    if (m_databaseManager == nullptr) {
        QMessageBox::warning(this, "提示", "数据库还没有初始化，暂时不能添加卡片。");
        return;
    }

    CardDialog dialog(this);
    dialog.setDecks(m_decks, selectedDeckForNewCards());
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const bool success = m_databaseManager->addFlashCard(dialog.getFront(),
                                                         dialog.getBack(),
                                                         dialog.getTag(),
                                                         dialog.getDeck(),
                                                         dialog.getCardColor());
    if (!success) {
        QMessageBox::warning(this, "提示", "添加复习卡片失败，请查看调试输出。");
        return;
    }

    reloadCards();
}

void ReviewWidget::importCardsFromFile()
{
    if (m_databaseManager == nullptr) {
        QMessageBox::warning(this, "提示", "数据库还没有初始化，暂时不能导入卡片。");
        return;
    }

    const QString filePath = QFileDialog::getOpenFileName(this,
                                                          "选择 Anki 文本文件",
                                                          QString(),
                                                          "文本文件 (*.txt *.tsv);;所有文件 (*)");
    if (filePath.isEmpty()) {
        return;
    }

    const QString deckName = selectedDeckForNewCards();
    const int importedCount = m_databaseManager->importFlashCardsFromTextFile(filePath, deckName, "#ffffff");
    if (importedCount < 0) {
        QMessageBox::warning(this, "导入失败", "导入 Anki 文本文件失败，请查看调试输出。");
        return;
    }

    QMessageBox::information(this,
                             "导入完成",
                             QString("已导入 %1 张复习卡片到“%2”。").arg(importedCount).arg(deckName));
    reloadCards();
}

void ReviewWidget::showCurrentCard()
{
    if (m_currentIndex >= m_cards.size()) {
        showEmptyState();
        return;
    }

    const FlashCard &card = m_cards.at(m_currentIndex);
    m_titleLabel->setText("Anki 复习");
    m_statusLabel->setText(QString("第 %1 / %2 张    牌组：%3    已复习 %4 次    到期时间：%5")
                               .arg(m_currentIndex + 1)
                               .arg(m_cards.size())
                               .arg(card.deck)
                               .arg(card.reviewCount)
                               .arg(card.dueTime));
    m_tagLabel->setText("标签：" + (card.tag.isEmpty() ? QString("未分类") : card.tag));
    applyCardColor(card.cardColor);
    m_frontEdit->setPlainText(card.front);
    m_backEdit->setPlainText(card.back);
    m_backSideTitleLabel->setVisible(false);
    m_backEdit->setVisible(false);
    m_showAnswerButton->setVisible(true);
    m_showAnswerButton->setEnabled(true);
    setReviewButtonsVisible(false);
}

void ReviewWidget::showEmptyState()
{
    m_titleLabel->setText("Anki 复习");
    m_statusLabel->setText("今天没有需要复习的卡片");
    if (!selectedDeckFilter().isEmpty()) {
        m_statusLabel->setText("这个牌组今天没有需要复习的卡片");
    }
    m_tagLabel->clear();
    applyCardColor("#ffffff");
    m_frontEdit->setPlainText(selectedDeckFilter().isEmpty()
                                  ? "今天没有需要复习的卡片"
                                  : "这个牌组今天没有需要复习的卡片");
    m_backEdit->clear();
    m_backSideTitleLabel->setVisible(false);
    m_backEdit->setVisible(false);
    m_showAnswerButton->setVisible(false);
    setReviewButtonsVisible(false);
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
