#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QTextEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QMessageBox>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QVector>
#include "httpmanager.h"
#include "datapoint.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onFetchButtonClicked();
    void onDataReceived(const QVector<DataPoint> &dataPoints);
    void onErrorOccurred(const QString &errorMessage);

private:
    // UI Elements
    QWidget *centralWidget;
    QLineEdit *projectIdInput;
    QTextEdit *tagsInput;
    QSpinBox *intervalInput;
    QPushButton *fetchButton;
    QChartView *chartView;
    QChart *chart;

    // Data Management
    HttpManager *httpManager;
    
    void setupUi();
    void plotData(const QVector<DataPoint> &dataPoints);
};

#endif // MAINWINDOW_H
