#ifndef HTTPMANAGER_H
#define HTTPMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <QString>
#include "datapoint.h"

class HttpManager : public QObject
{
    Q_OBJECT
public:
    explicit HttpManager(QObject *parent = nullptr);
    void fetchData(const QString &projectId, const QStringList &tags, qint64 interval);

signals:
    void dataReceived(const QVector<DataPoint> &dataPoints);
    void errorOccurred(const QString &errorMessage);

private slots:
    void onRequestFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *manager;
    QVector<DataPoint> parseResponse(const QByteArray &data);
};

#endif // HTTPMANAGER_H
