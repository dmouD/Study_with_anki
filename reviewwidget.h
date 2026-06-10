#ifndef REVIEWWIDGET_H
#define REVIEWWIDGET_H

#include "flashcard.h"

#include <QStringList>
#include <QWidget>
#include <QVector>

class DatabaseManager;
class QComboBox;
class QFrame;
class QLabel;
class QPushButton;
class QTextEdit;

/*
 * ReviewWidget 是一个轻量版 Anki 复习子界面。
 *
 * 它负责复习流程和添加卡片入口，但不直接写 SQL。
 * 所有卡片数据都通过 DatabaseManager 读取和保存。
 */
class ReviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ReviewWidget(DatabaseManager *databaseManager, QWidget *parent = nullptr);

    // 每次打开复习页时重新读取今日到期卡片，避免显示旧数据。
    void reloadCards();

signals:
    // 用户点击“返回计划”时通知 MainWindow 切回原来的计划软件页面。
    void backRequested();

private:
    void setupUi();
    void setupStyleSheet();
    void connectSignals();
    void reloadDecks();
    QString selectedDeckFilter() const;
    QString selectedDeckForNewCards() const;
    void applyCardColor(const QString &cardColor);
    void createDeck();
    void addCard();
    void importCardsFromFile();
    void showCurrentCard();
    void showEmptyState();
    void showAnswer();
    void submitRating(int rating);
    void setReviewButtonsVisible(bool visible);

    DatabaseManager *m_databaseManager;
    QVector<FlashCard> m_cards;
    QStringList m_decks;
    int m_currentIndex;

    QLabel *m_titleLabel;
    QLabel *m_statusLabel;
    QLabel *m_tagLabel;
    QLabel *m_deckLabel;
    QLabel *m_backSideTitleLabel;
    QComboBox *m_deckCombo;
    QFrame *m_cardFrame;
    QTextEdit *m_frontEdit;
    QTextEdit *m_backEdit;
    QPushButton *m_showAnswerButton;
    QPushButton *m_newDeckButton;
    QPushButton *m_addCardButton;
    QPushButton *m_importCardButton;
    QPushButton *m_backToPlannerButton;
    QPushButton *m_forgetButton;
    QPushButton *m_blurryButton;
    QPushButton *m_rememberButton;
    QPushButton *m_easyButton;
};

#endif // REVIEWWIDGET_H
