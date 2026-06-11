#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include "ankipackageimporter.h"
#include "flashcard.h"
#include "task.h"

#include <QList>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStringList>
#include <QVector>

/*
 * DeckOverview 是牌库首页需要展示的一张“牌组卡片”。
 *
 * 它不是数据库表，只是把 SQL 统计结果打包给 ReviewWidget 使用：
 * totalCards 表示牌组总卡片数，dueCards 表示今天到期数量，
 * reviewedCards 表示至少复习过一次的卡片数。
 */
class DeckOverview
{
public:
    QString group;
    QString deck;
    QString color;
    int totalCards = 0;
    int dueCards = 0;
    int reviewedCards = 0;
};

/*
 * DatabaseManager 是项目里的数据库访问层。
 *
 * MainWindow、TaskDialog 等界面类不应该关心 SQL 语句怎么写，
 * 它们只需要调用 addTask()、getAllTasks() 这类函数即可。
 *
 * 这样拆分有两个好处：
 * 1. 数据库代码集中在一个地方，之后想修改表结构或 SQL 更容易。
 * 2. 界面代码更干净，适合初学者逐个模块学习。
 */
class DatabaseManager
{
public:
    DatabaseManager();
    ~DatabaseManager();

    // 打开 planner.db，并确保 tasks 表和 flashcards 表存在。
    bool initialize();

    // 基本增删改查接口。
    bool addTask(const Task &task);
    bool deleteTask(int id);
    bool updateTask(const Task &task);
    Task getTaskById(int id);

    // 查询接口：全部任务和按日期筛选任务。
    QList<Task> getAllTasks();
    QList<Task> getTasksByDate(const QString &date);

    // 将任务在 0% 未完成和 100% 已完成之间切换。
    bool toggleTaskDone(int id);

    // 调试接口：清空所有业务数据并重置自增 ID，仅供管理员调试界面调用。
    bool clearAllDataForDebug();

    // Anki 风格复习卡片接口。
    bool addFlashCard(const QString &front,
                      const QString &back,
                      const QString &tag,
                      const QString &group = "默认分组",
                      const QString &deck = "默认牌组",
                      const QString &cardColor = "#ffffff");
    QVector<FlashCard> getDueFlashCards(const QString &deck = QString(),
                                         const QString &group = QString());
    QVector<FlashCard> getFlashCardsByDeck(const QString &deck,
                                           const QString &group = QString());
    QStringList getFlashCardGroups();
    QStringList getFlashCardDecks(const QString &group = QString());
    QVector<DeckOverview> getFlashCardDeckOverviews(const QString &group = QString(),
                                                    const QString &keyword = QString());
    bool addFlashCardGroup(const QString &group);
    bool addFlashCardDeck(const QString &deck, const QString &group = "默认分组");
    bool deleteFlashCardGroup(const QString &group);
    bool deleteFlashCardDeck(const QString &deck, const QString &group = "默认分组");
    bool updateFlashCardAfterReview(int cardId, int rating);
    bool deleteFlashCard(int cardId);
    int importFlashCardsFromTextFile(const QString &filePath,
                                     const QString &group = "默认分组",
                                     const QString &deck = "默认牌组",
                                     const QString &cardColor = "#ffffff");
    FlashCardImportResult importFlashCardsFromTextFileWithResult(const QString &filePath,
                                                                 const QString &group = "默认分组",
                                                                 const QString &deck = "默认牌组",
                                                                 const QString &cardColor = "#ffffff");
    FlashCardImportResult importFlashCardsFromAnkiPackage(const QVector<ImportedAnkiCard> &cards,
                                                          const QString &group = "默认分组",
                                                          const QString &deck = "默认牌组",
                                                          const QString &cardColor = "#ffffff");

private:
    // 初始化数据库时调用；如果表已存在，SQL 不会重复创建。
    bool createTasksTable();

    // 初始化复习卡片表；不会影响已有任务表。
    bool createFlashcardsTable();

    // 初始化牌库分组表；允许空分组先创建出来。
    bool createFlashCardGroupsTable();

    // 初始化牌组表；允许没有卡片的牌组也能保存。
    bool createFlashCardDecksTable();

    // 为常用查询字段创建索引，提升到期复习、牌组筛选和导入去重速度。
    bool createFlashCardIndexes();

    // 兼容旧版本数据库：如果 flashcards 表还没有导入来源字段，就自动添加。
    bool ensureFlashCardSourceColumns();

    // 兼容旧版本数据库：如果 flashcards 表还没有 deck 字段，就自动添加。
    bool ensureFlashCardDeckColumn();

    // 兼容旧版本数据库：如果 flashcards 表还没有 card_group 字段，就自动添加。
    bool ensureFlashCardGroupColumn();

    // 兼容旧版本数据库：如果 flashcards 表还没有 card_color 字段，就自动添加。
    bool ensureFlashCardColorColumn();

    // 兼容旧版本数据库：如果 flashcard_decks 表还没有 group_name 字段，就自动添加。
    bool ensureFlashCardDeckGroupColumn();

    // 兼容旧版本数据库：如果 tasks 表还没有 progress 字段，就自动添加。
    bool ensureProgressColumn();

    // 兼容旧版本数据库：如果 tasks 表还没有 end_date 字段，就自动添加。
    bool ensureDateRangeColumn();

    // 兼容旧版本数据库：如果 tasks 表还没有 task_type 字段，就自动添加。
    bool ensureTaskTypeColumn();

    // 兼容旧版本数据库：如果 tasks 表还没有 course_time 字段，就自动添加。
    // 字段名沿用 course_time，但业务含义是“课程总时长”。
    bool ensureCourseTimeColumn();

    // 兼容旧版本数据库：如果 tasks 表还没有 course_hours 字段，就自动添加。
    bool ensureClassHoursColumn();

    // 把 QSqlQuery 当前行转换成 Task 对象，减少重复取字段代码。
    Task taskFromQuery(const QSqlQuery &query) const;

    // 把 QSqlQuery 当前行转换成 FlashCard 对象。
    FlashCard flashCardFromQuery(const QSqlQuery &query) const;

    // Qt 的数据库连接对象。这里使用一个固定连接名，避免重复 addDatabase。
    QSqlDatabase m_database;
};

#endif // DATABASEMANAGER_H
