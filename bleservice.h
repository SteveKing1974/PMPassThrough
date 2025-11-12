#ifndef BLESERVICE_H
#define BLESERVICE_H

#include <QObject>
#include <QBluetoothUuid>
#include <QLowEnergyDescriptor>
#include <QLowEnergyService>
#include <QLowEnergyCharacteristic>

class BLEService : public QObject
{
    Q_OBJECT
public:
    explicit BLEService(QLowEnergyService* service,
                        const QBluetoothUuid& notification_char,
                        QObject *parent = nullptr);

public:
    void disable_notifications();
    bool read_value(const QBluetoothUuid &c);
    QList<QLowEnergyCharacteristic> characteristics() const;

signals:
    void disconnected();
    void discovery_complete();
    void value_changed(const QBluetoothUuid &c,
                       const QByteArray &value);
    void value_read(const QBluetoothUuid &c,
                       const QByteArray &value);
private slots:
    void confirmedDescriptorWrite(const QLowEnergyDescriptor &d, const QByteArray &value);
    void confirmedCharactoristicRead(const QLowEnergyCharacteristic &c, const QByteArray &value);
    void confirmedCharactoristicValueChanged(const QLowEnergyCharacteristic &dc, const QByteArray &value);

    void serviceStateChanged(QLowEnergyService::ServiceState s);

private:
    QLowEnergyService* m_service;
    QBluetoothUuid m_notification_char;
    QLowEnergyDescriptor m_notificationDesc;
};

#endif // BLESERVICE_H
