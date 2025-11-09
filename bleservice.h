#ifndef BLESERVICE_H
#define BLESERVICE_H

#include <QObject>
#include <QBluetoothUuid>
#include <QLowEnergyDescriptor>
#include <QLowEnergyService>

class QLowEnergyCharacteristic;

class BLEService : public QObject
{
    Q_OBJECT
public:
    explicit BLEService(QLowEnergyService* service,
                        const QBluetoothUuid& notification_char,
                        QObject *parent = nullptr);

public:
    void disable_notifications();

signals:
    void disconnected();
    void value_changed(const QLowEnergyCharacteristic &c,
                       const QByteArray &value);

private slots:
    void confirmedDescriptorWrite(const QLowEnergyDescriptor &d, const QByteArray &value);
    void serviceStateChanged(QLowEnergyService::ServiceState s);

private:
    QLowEnergyService* m_service;
    QBluetoothUuid m_notification_char;
    QLowEnergyDescriptor m_notificationDesc;
};

#endif // BLESERVICE_H
