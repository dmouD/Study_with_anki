#include "deepseekclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

DeepSeekClient::DeepSeekClient(QObject *parent)
    : QObject(parent),
      m_networkManager(new QNetworkAccessManager(this))
{
}

void DeepSeekClient::analyzePlan(const QString &apiKey, const QString &prompt)
{
    const QString trimmedKey = apiKey.trimmed();
    if (trimmedKey.isEmpty()) {
        emit analysisFailed("请先输入 DeepSeek API Key。");
        return;
    }

    if (prompt.trimmed().isEmpty()) {
        emit analysisFailed("当前没有可分析的任务。");
        return;
    }

    /*
     * DeepSeek API 兼容 OpenAI Chat Completions 格式。
     * 请求地址：https://api.deepseek.com/chat/completions
     */
    QNetworkRequest request(QUrl("https://api.deepseek.com/chat/completions"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", ("Bearer " + trimmedKey).toUtf8());

    QJsonArray messages;
    messages.append(QJsonObject{
        {"role", "system"},
        {"content", "你是一个严谨、实用的学习计划助手。请根据用户的任务列表，给出清晰、可执行的学习安排建议。"}
    });
    messages.append(QJsonObject{
        {"role", "user"},
        {"content", prompt}
    });

    QJsonObject body;
    body["model"] = "deepseek-v4-flash";
    body["messages"] = messages;
    body["stream"] = false;
    body["max_tokens"] = 1200;
    body["temperature"] = 0.4;
    body["thinking"] = QJsonObject{{"type", "disabled"}};

    QNetworkReply *reply = m_networkManager->post(request, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleReply(reply);
        reply->deleteLater();
    });
}

void DeepSeekClient::handleReply(QNetworkReply *reply)
{
    const QByteArray responseData = reply->readAll();

    if (reply->error() != QNetworkReply::NoError) {
        const QString message = responseData.isEmpty()
                                    ? reply->errorString()
                                    : QString::fromUtf8(responseData);
        emit analysisFailed("DeepSeek 请求失败：" + message);
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(responseData, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit analysisFailed("DeepSeek 返回内容不是有效 JSON。");
        return;
    }

    const QJsonObject root = document.object();
    const QJsonArray choices = root.value("choices").toArray();
    if (choices.isEmpty()) {
        emit analysisFailed("DeepSeek 返回结果中没有 choices。");
        return;
    }

    const QJsonObject firstChoice = choices.first().toObject();
    const QJsonObject message = firstChoice.value("message").toObject();
    const QString content = message.value("content").toString().trimmed();

    if (content.isEmpty()) {
        emit analysisFailed("DeepSeek 返回的分析内容为空。");
        return;
    }

    emit analysisFinished(content);
}
