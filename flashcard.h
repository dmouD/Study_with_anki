#ifndef FLASHCARD_H
#define FLASHCARD_H

#include <QString>

/*
 * FlashCard 是一张 Anki 风格复习卡片的数据结构。
 *
 * 它只保存数据库中的一行记录，不负责界面显示，也不直接执行 SQL。
 * 数据库读写统一放在 DatabaseManager 里。
 */
class FlashCard
{
public:
    FlashCard()
        : id(-1),
          intervalDays(1),
          easeFactor(2.5),
          reviewCount(0),
          deck("默认牌组"),
          cardColor("#ffffff"),
          source("manual")
    {
    }

    int id;
    QString front;
    QString back;
    QString tag;
    QString deck;
    QString cardColor;
    QString dueTime;
    int intervalDays;
    double easeFactor;
    int reviewCount;
    QString createdAt;
    QString source;
    QString sourceId;
};

#endif // FLASHCARD_H
