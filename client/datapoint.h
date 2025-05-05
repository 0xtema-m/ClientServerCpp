#ifndef DATAPOINT_H
#define DATAPOINT_H

#include <QDateTime>

struct DataPoint {
    QDateTime timestamp;
    double value;
};

#endif // DATAPOINT_H
