#ifndef DEEPSEEKCLIENT_H
#define DEEPSEEKCLIENT_H

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

/*
 * DeepSeekClient 只负责和 DeepSeek API 通信。
 *
 * MainWindow 会把任务列表整理成 prompt，然后调用 analyzePlan()。
 * DeepSeekClient 发送网络请求，收到结果后通过 signal 通知界面。
 *
 * 这里使用 Qt Network 的异步请求，不需要多线程，也不会卡住界面。
 */
class DeepSeekClient : public QObject
{
    Q_OBJECT

public:
    explicit DeepSeekClient(QObject *parent = nullptr);

    // apiKey 由用户在界面输入；prompt 是要发给 AI 的任务摘要和分析要求。
    void analyzePlan(const QString &apiKey, const QString &prompt);

signals:
    void analysisFinished(const QString &result);
    void analysisFailed(const QString &errorMessage);

private:
    void handleReply(QNetworkReply *reply);

    QNetworkAccessManager *m_networkManager;
};

#endif // DEEPSEEKCLIENT_H
