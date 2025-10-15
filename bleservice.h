#ifndef BLESERVICE_H
#define BLESERVICE_H

#include <QObject>
#include <QBluetoothUuid>

class BLEService : public QObject
{
    Q_OBJECT
public:
    explicit BLEService(const QBluetoothUuid& service_class,
                        const QBluetoothUuid& notification_char,
                        QObject *parent = nullptr);

signals:
};

#endif // BLESERVICE_H
