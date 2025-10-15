#ifndef POWERSENDER_H
#define POWERSENDER_H

#include <QObject>

class PowerSender : public QObject
{
    Q_OBJECT
public:
    explicit PowerSender(QObject *parent = nullptr);
    
    int SetUp();
signals:
};

#endif // POWERSENDER_H
