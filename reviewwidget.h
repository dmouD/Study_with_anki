#ifndef REVIEWWIDGET_H
#define REVIEWWIDGET_H

#include "flashcard.h"

#include <QStringList>
#include <QWidget>
#include <QVector>

class DatabaseManager;
class DeckOverview;
class QFrame;
class QGraphicsDropShadowEffect;
class QGraphicsOpacityEffect;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QTextEdit;
class QVBoxLayout;

/*
 * ReviewWidget 是 DStudy 里的 Anki 风格复习页面。
 *
 * 它现在包含两个层次：
 * 1. 牌库首页：按“分组 -> 牌组”展示可视化牌组卡片。
 * 2. 复习页面：显示正面、背面和四个反馈按钮。
 *
 * ReviewWidget 只负责界面和流程，所有 SQL 都交给 DatabaseManager。
 */
class ReviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ReviewWidget(DatabaseManager *databaseManager, QWidget *parent = nullptr);

    // MainWindow 切到 Anki 页面时调用，默认回到牌库首页并刷新统计。
    void reloadCards();

signals:
    // 用户点击“返回计划”时通知 MainWindow 切回原来的计划软件页面。
    void backRequested();

private:
    void setupUi();
    void setupStyleSheet();
    void connectSignals();

    void reloadGroups();
    void reloadDecks();
    void reloadDeckLibrary();
    void clearDeckList();
    void addDeckCard(const DeckOverview &overview);
    void clearCardList();
    void addCardPreview(const FlashCard &card);

    void showLibrary();
    void showDeckDetail(const QString &group, const QString &deck);
    void showReviewArea();
    void startDeckReview(const QString &group, const QString &deck);

    QString selectedGroupForNewCards() const;
    QString selectedDeckForNewCards() const;
    void applyCardColor(const QString &cardColor);

    void createGroup();
    void createDeck();
    void deleteCurrentGroup();
    void deleteDeck(const DeckOverview &overview);
    void addCard();
    void importCardsFromFile();

    void showCurrentCard();
    void showEmptyState();
    void showAnswer();
    void submitRating(int rating);
    void setReviewButtonsVisible(bool visible);
    void playCardEnterAnimation();
    void playAnswerRevealAnimation();

    DatabaseManager *m_databaseManager;
    QVector<FlashCard> m_cards;
    QVector<FlashCard> m_deckCards;
    QStringList m_groups;
    QStringList m_decks;
    QString m_currentGroup;
    QString m_currentDeck;
    int m_currentIndex;

    QLabel *m_titleLabel;
    QLabel *m_statusLabel;
    QLabel *m_tagLabel;

    QWidget *m_libraryWidget;
    QLineEdit *m_searchEdit;
    QHBoxLayout *m_groupTabsLayout;
    QScrollArea *m_deckScrollArea;
    QWidget *m_deckListWidget;
    QVBoxLayout *m_deckListLayout;
    QPushButton *m_newGroupButton;
    QPushButton *m_newDeckButton;
    QPushButton *m_deleteGroupButton;
    QPushButton *m_addCardButton;
    QPushButton *m_importCardButton;
    QPushButton *m_backToPlannerButton;

    QWidget *m_deckDetailWidget;
    QScrollArea *m_cardListScrollArea;
    QWidget *m_cardListWidget;
    QVBoxLayout *m_cardListLayout;
    QPushButton *m_backFromDetailButton;
    QPushButton *m_startReviewFromDetailButton;

    QWidget *m_reviewAreaWidget;
    QLabel *m_backSideTitleLabel;
    QFrame *m_cardFrame;
    QGraphicsDropShadowEffect *m_cardShadowEffect;
    QGraphicsOpacityEffect *m_frontOpacityEffect;
    QGraphicsOpacityEffect *m_backTitleOpacityEffect;
    QGraphicsOpacityEffect *m_backOpacityEffect;
    QTextEdit *m_frontEdit;
    QTextEdit *m_backEdit;
    QPushButton *m_backToLibraryButton;
    QPushButton *m_showAnswerButton;
    QPushButton *m_forgetButton;
    QPushButton *m_blurryButton;
    QPushButton *m_rememberButton;
    QPushButton *m_easyButton;
};

#endif // REVIEWWIDGET_H
