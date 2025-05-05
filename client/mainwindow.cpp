#include "mainwindow.h"
#include <QDateTime>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>
#include <QDebug>
#include <QtCharts/QScatterSeries>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    
    httpManager = new HttpManager(this);
    connect(httpManager, &HttpManager::dataReceived, this, &MainWindow::onDataReceived);
    connect(httpManager, &HttpManager::errorOccurred, this, &MainWindow::onErrorOccurred);
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUi()
{
    // Set up the main window
    setWindowTitle("Data Plotter");
    resize(800, 600);
    
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    QGroupBox *inputGroup = new QGroupBox("Request Parameters");
    QVBoxLayout *inputLayout = new QVBoxLayout(inputGroup);

    QHBoxLayout *projectLayout = new QHBoxLayout();
    QLabel *projectLabel = new QLabel("Project ID:");
    projectIdInput = new QLineEdit();
    projectLayout->addWidget(projectLabel);
    projectLayout->addWidget(projectIdInput);
    inputLayout->addLayout(projectLayout);

    QHBoxLayout *tagsLayout = new QHBoxLayout();
    QLabel *tagsLabel = new QLabel("Tags:");
    tagsInput = new QTextEdit();
    tagsInput->setPlaceholderText("Enter tags separated by commas");
    tagsInput->setMaximumHeight(60);
    tagsLayout->addWidget(tagsLabel);
    tagsLayout->addWidget(tagsInput);
    inputLayout->addLayout(tagsLayout);

    QHBoxLayout *intervalLayout = new QHBoxLayout();
    QLabel *intervalLabel = new QLabel("Interval (seconds):");
    intervalInput = new QSpinBox();
    intervalInput->setMinimum(1);
    intervalInput->setMaximum(86400); // Max 1 day in seconds
    intervalInput->setValue(3600);    // Default 1 hour
    intervalLayout->addWidget(intervalLabel);
    intervalLayout->addWidget(intervalInput);
    inputLayout->addLayout(intervalLayout);

    fetchButton = new QPushButton("Fetch Data");
    inputLayout->addWidget(fetchButton);
    
    mainLayout->addWidget(inputGroup);

    chart = new QChart();
    chart->setTitle("Data Points");
    
    chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setMinimumHeight(400);
    
    mainLayout->addWidget(chartView);

    connect(fetchButton, &QPushButton::clicked, this, &MainWindow::onFetchButtonClicked);
}

void MainWindow::onFetchButtonClicked()
{
    QString projectId = projectIdInput->text().trimmed();
    if (projectId.isEmpty()) {
        QMessageBox::warning(this, "Missing Information", "Please enter a Project ID");
        return;
    }
    
    QStringList tags;
    QString tagsText = tagsInput->toPlainText().trimmed();
    if (!tagsText.isEmpty()) {
        tags = tagsText.split(',', Qt::SkipEmptyParts);
        for (QString &tag : tags) {
            tag = tag.trimmed();
        }
    }
    
    qint64 interval = intervalInput->value();

    fetchButton->setEnabled(false);
    fetchButton->setText("Loading...");

    httpManager->fetchData(projectId, tags, interval);
}

void MainWindow::onDataReceived(const QVector<DataPoint> &dataPoints)
{
    // Re-enable button
    fetchButton->setEnabled(true);
    fetchButton->setText("Fetch Data");
    
    if (dataPoints.isEmpty()) {
        QMessageBox::information(this, "No Data", "No data points were returned from the server");
        return;
    }
    
    plotData(dataPoints);
}

void MainWindow::onErrorOccurred(const QString &errorMessage)
{
    fetchButton->setEnabled(true);
    fetchButton->setText("Fetch Data");
    
    QMessageBox::critical(this, "Error", "Failed to fetch data: " + errorMessage);
}

void MainWindow::plotData(const QVector<DataPoint> &dataPoints)
{
    chart->removeAllSeries();
    
    if (dataPoints.isEmpty()) {
        chart->setTitle("No data available for the provided parameters");
        qDebug() << "No data points to plot";
        return;
    }
    
    QLineSeries *lineSeries = new QLineSeries();
    lineSeries->setName("Data Series");
    
    QPen linePen = lineSeries->pen();
    linePen.setWidth(2);
    lineSeries->setPen(linePen);
    
    QScatterSeries *scatterSeries = new QScatterSeries();
    scatterSeries->setName("Data Points");
    scatterSeries->setMarkerSize(10);
    scatterSeries->setColor(Qt::red);
    
    QDateTime minDate = dataPoints.first().timestamp;
    QDateTime maxDate = dataPoints.first().timestamp;
    double minValue = dataPoints.first().value;
    double maxValue = dataPoints.first().value;
    
    for (const DataPoint &point : dataPoints) {
        qint64 msecsSinceEpoch = point.timestamp.toMSecsSinceEpoch();
        lineSeries->append(msecsSinceEpoch, point.value);
        scatterSeries->append(msecsSinceEpoch, point.value);

        if (point.timestamp < minDate) minDate = point.timestamp;
        if (point.timestamp > maxDate) maxDate = point.timestamp;
        if (point.value < minValue) minValue = point.value;
        if (point.value > maxValue) maxValue = point.value;
    }
    
    qDebug() << "Added" << dataPoints.size() << "points to series";
    
    double yPadding = (maxValue - minValue) * 0.1;
    if (yPadding == 0) yPadding = 1.0;
    
    chart->addSeries(lineSeries);
    chart->addSeries(scatterSeries);
    
    QDateTimeAxis *axisX = nullptr;
    QValueAxis *axisY = nullptr;
    
    if (chart->axes(Qt::Horizontal).isEmpty() || chart->axes(Qt::Vertical).isEmpty()) {
        axisX = new QDateTimeAxis;
        axisX->setTickCount(10);
        axisX->setFormat("MM/dd HH:mm");
        axisX->setTitleText("Time");
        
        axisY = new QValueAxis;
        axisY->setLabelFormat("%.2f");
        axisY->setTitleText("Value");
        
        chart->addAxis(axisX, Qt::AlignBottom);
        chart->addAxis(axisY, Qt::AlignLeft);
    } else {
        axisX = qobject_cast<QDateTimeAxis*>(chart->axes(Qt::Horizontal).first());
        axisY = qobject_cast<QValueAxis*>(chart->axes(Qt::Vertical).first());
    }
    
    lineSeries->attachAxis(axisX);
    lineSeries->attachAxis(axisY);
    scatterSeries->attachAxis(axisX);
    scatterSeries->attachAxis(axisY);
    
    axisX->setRange(minDate, maxDate);
    axisY->setRange(minValue - yPadding, maxValue + yPadding);
    
    chart->setTitle(QString("Data for Project: %1 (%2 data points)").arg(projectIdInput->text()).arg(dataPoints.size()));
}
