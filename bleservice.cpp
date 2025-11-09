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
        connect(m_service, &QLowEnergyService::characteristicChanged, this, &BLEService::value_changed);
        connect(m_service, &QLowEnergyService::descriptorWritten, this, &BLEService::confirmedDescriptorWrite);
        m_service->discoverDetails();
    }
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

void BLEService::serviceStateChanged(QLowEnergyService::ServiceState s)
{
    switch (s) {
    case QLowEnergyService::RemoteServiceDiscovering:
        qDebug() << "Discovering services...";
        break;
    case QLowEnergyService::RemoteServiceDiscovered:
    {
        qDebug() << "Service discovered.";

        const QLowEnergyCharacteristic powerChar1 =
            m_service->characteristic(QBluetoothUuid(QBluetoothUuid::CharacteristicType::CyclingPowerFeature));
        qDebug() << powerChar1.value();

        const QLowEnergyCharacteristic powerChar =
            m_service->characteristic(QBluetoothUuid(QBluetoothUuid::CharacteristicType::CyclingPowerMeasurement));

        if (!powerChar.isValid()) {
            qDebug() << "Power Data not found.";
            break;
        }

        m_notificationDesc = powerChar.descriptor(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
        if (m_notificationDesc.isValid())
            m_service->writeDescriptor(m_notificationDesc, QByteArray::fromHex("0100"));

        const QLowEnergyCharacteristic cscChar =
            m_service->characteristic(QBluetoothUuid(QBluetoothUuid::CharacteristicType::CSCMeasurement));

        if (!cscChar.isValid()) {
            qDebug() << "CSC Data not found.";
            break;
        }

        m_notificationDesc = cscChar.descriptor(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
        if (m_notificationDesc.isValid())
            m_service->writeDescriptor(m_notificationDesc, QByteArray::fromHex("0100"));

        break;
    }
    default:
        //nothing for now
        break;
    }

}
