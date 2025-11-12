#include "bleservice.h"
#include <QLowEnergyService>

BLEService::BLEService(QLowEnergyService* service,
                       const QBluetoothUuid& notification_char,
                       QObject *parent)
    : m_service(service),m_notification_char(notification_char),QObject{parent}
{
    if (m_service) {
        m_service->setParent(this);
        connect(m_service, &QLowEnergyService::stateChanged, this, &BLEService::serviceStateChanged);
        connect(m_service, &QLowEnergyService::characteristicChanged, this, &BLEService::confirmedCharactoristicValueChanged);
        connect(m_service, &QLowEnergyService::characteristicRead, this, &BLEService::confirmedCharactoristicRead);
        connect(m_service, &QLowEnergyService::descriptorWritten, this, &BLEService::confirmedDescriptorWrite);
        m_service->discoverDetails();
    }
}

QList<QLowEnergyCharacteristic> BLEService::characteristics() const
{
    return m_service->characteristics();
}

bool BLEService::read_value(const QBluetoothUuid &c)
{
    const QLowEnergyCharacteristic read_char = m_service->characteristic(c);

    if (!read_char.isValid()) {
        qDebug() << "Wrong read char" << c;
        return false;
    }

    m_service->readCharacteristic(read_char);

    return true;
}

void BLEService::disable_notifications()
{
    if (m_notificationDesc.isValid() && m_service
        && m_notificationDesc.value() == QByteArray::fromHex("0100")) {
        m_service->writeDescriptor(m_notificationDesc, QByteArray::fromHex("0000"));
    }
}


void BLEService::confirmedDescriptorWrite(const QLowEnergyDescriptor &d, const QByteArray &value)
{
    if (d.isValid() && d == m_notificationDesc && value == QByteArray::fromHex("0000")) {
        //disabled notifications -> assume disconnect intent
        emit disconnect();
    }
}

void BLEService::confirmedCharactoristicRead(const QLowEnergyCharacteristic &c, const QByteArray &value)
{
    emit value_read(c.uuid(), value);
}

void BLEService::confirmedCharactoristicValueChanged(const QLowEnergyCharacteristic &c, const QByteArray &value)
{
    emit value_changed(c.uuid(), value);
}

void BLEService::serviceStateChanged(QLowEnergyService::ServiceState s)
{
    switch (s) {
    case QLowEnergyService::RemoteServiceDiscovering:
        qDebug() << "Discovering services...";
        break;
    case QLowEnergyService::RemoteServiceDiscovered:
    {
        qDebug() << m_notification_char << "Service discovered.";

        const QLowEnergyCharacteristic notify_char =
            m_service->characteristic(m_notification_char);

        if (!notify_char.isValid()) {
            qDebug() << "Wrong notify char" << m_notification_char;
            break;
        }

        m_notificationDesc = notify_char.descriptor(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
        if (m_notificationDesc.isValid())
            m_service->writeDescriptor(m_notificationDesc, QByteArray::fromHex("0100"));

        emit discovery_complete();
        break;
    }
    default:
        //nothing for now
        break;
    }

}
