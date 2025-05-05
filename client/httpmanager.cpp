#include "httpmanager.h"
#include <QUrl>
#include <QDebug>

HttpManager::HttpManager(QObject *parent) : QObject(parent)
{
    manager = new QNetworkAccessManager(this);
    connect(manager, &QNetworkAccessManager::finished, this, &HttpManager::onRequestFinished);
}

void HttpManager::fetchData(const QString &projectId, const QStringList &tags, qint64 interval)
{
    QUrl url("http://127.0.0.1:9999/get");

    QJsonObject requestBody;
    requestBody["project_id"] = projectId;
    requestBody["interval_seconds"] = interval;
    requestBody["metric_type"] = "SPEED";
    
    qInfo() << projectId;
    qInfo() << interval;

    QJsonArray tagsArray;
    if (!tags.isEmpty()) {
        for (const QString &tag : tags) {
            tagsArray.append(tag);
        }
    }
    requestBody["tags"] = tagsArray;

    QJsonDocument doc(requestBody);
    QByteArray jsonData = doc.toJson();

    QNetworkRequest request(url);

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::ContentLengthHeader, QByteArray::number(jsonData.size()));

    manager->get(request, jsonData);
}

void HttpManager::onRequestFinished(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        qDebug() << "Response received:" << responseData;
        
        QVector<DataPoint> dataPoints = parseResponse(responseData);
        qDebug() << "Parsed" << dataPoints.size() << "data points";
        
        emit dataReceived(dataPoints);
    } else {
        qDebug() << "Network error:" << reply->errorString();
        emit errorOccurred(reply->errorString());
    }
    reply->deleteLater();
}

QVector<DataPoint> HttpManager::parseResponse(const QByteArray &data)
{
    QVector<DataPoint> dataPoints;
    QJsonDocument doc = QJsonDocument::fromJson(data);

    // qInfo() << "Parsing: " << data;
    if (doc.isObject()) {
        // qInfo() << "It is an object";
        QJsonArray array = doc.object()["metrics"].toArray();
        
        // qInfo() << "Array size: " << array.size();
        for (const QJsonValue &value : array) {
            if (value.isObject()) {
                QJsonObject obj = value.toObject();
                
                DataPoint point;

                qint64 timestamp = obj["timestamp"].toVariant().toLongLong();
                point.timestamp = QDateTime::fromMSecsSinceEpoch(timestamp);

                point.value = obj["value"].toDouble();
                
                dataPoints.append(point);
            }
        }
    }
    
    return dataPoints;
}
