#include "powersender.h"
#include <QLowEnergyAdvertisingData>
#include <QLowEnergyAdvertisingParameters>
#include <QLowEnergyCharacteristic>
#include <QLowEnergyCharacteristicData>
#include <QLowEnergyDescriptorData>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QLowEnergyServiceData>

#include <QByteArray>

#if defined(Q_OS_ANDROID) || defined(Q_OS_DARWIN)
#include <QGuiApplication>
#else
#include <QCoreApplication>
#endif

#include <QList>
#include <QLoggingCategory>
#include <QTimer>

#if QT_CONFIG(permissions)
#include <QPermissions>
#endif

#include <memory>

PowerSender::PowerSender(QObject *parent)
    : QObject{parent}
{}

int PowerSender::SetUp()
{
    //! [Advertising Data]
    m_AdvertisingData.setDiscoverability(QLowEnergyAdvertisingData::DiscoverabilityGeneral);
    m_AdvertisingData.setIncludePowerLevel(true);
    m_AdvertisingData.setLocalName("PowerMeterServer");
    m_AdvertisingData.setServices(QList<QBluetoothUuid>() << QBluetoothUuid::ServiceClassUuid::CyclingPower);
    //! [Advertising Data]

    //! [Service Data]
    // SensorLocation = 0x2a5d,
    //     /* 0x2a5e not defined */
    //     /* 0x2a5f not defined */
    //     /* 0x2a60 not defined */
    //     /* 0x2a61 not defined */
    //     /* 0x2a62 not defined */
    //     CyclingPowerMeasurement = 0x2a63,
    //     CyclingPowerVector = 0x2a64,
    //     CyclingPowerFeature = 0x2a65,
    //     CyclingPowerControlPoint = 0x2a66,
    m_ServiceData.setType(QLowEnergyServiceData::ServiceTypePrimary);
    m_ServiceData.setUuid(QBluetoothUuid::ServiceClassUuid::CyclingPower);

    QLowEnergyCharacteristicData powerMeasurementCharData;
    powerMeasurementCharData.setUuid(QBluetoothUuid::CharacteristicType::CyclingPowerMeasurement);
    powerMeasurementCharData.setValue(QByteArray(8, 0));
    powerMeasurementCharData.setProperties(QLowEnergyCharacteristic::Notify | QLowEnergyCharacteristic::Read);
    const QLowEnergyDescriptorData pmClientConfig(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration,
                                                  QByteArray(8, 0));
    powerMeasurementCharData.addDescriptor(pmClientConfig);
    m_ServiceData.addCharacteristic(powerMeasurementCharData);

    QLowEnergyCharacteristicData featureCharData;
    QByteArray ft(4,0);
    ft[0] = 8;
    featureCharData.setUuid(QBluetoothUuid::CharacteristicType::CyclingPowerFeature);
    featureCharData.setValue(ft);
    featureCharData.setProperties(QLowEnergyCharacteristic::Read);
    const QLowEnergyDescriptorData clientConfig(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration,
                                                ft);
    featureCharData.addDescriptor(clientConfig);
    m_ServiceData.addCharacteristic(featureCharData);

    QLowEnergyCharacteristicData sensorLocationCharData;
    sensorLocationCharData.setUuid(QBluetoothUuid::CharacteristicType::SensorLocation);
    sensorLocationCharData.setValue(QByteArray(1, 13));
    sensorLocationCharData.setProperties(QLowEnergyCharacteristic::Read);
    const QLowEnergyDescriptorData locClientConfig(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration,
                                                   QByteArray(1, 13));
    sensorLocationCharData.addDescriptor(locClientConfig);
    m_ServiceData.addCharacteristic(sensorLocationCharData);

    //! [Service Data]

    //! [Start Advertising]
    bool errorOccurred = false;
    m_Controller.reset(QLowEnergyController::createPeripheral());
    QObject::connect(m_Controller.get(), &QLowEnergyController::errorOccurred, this, &PowerSender::ControllerError);

    m_Service.reset(m_Controller->addService(m_ServiceData));

    m_Controller->startAdvertising(QLowEnergyAdvertisingParameters(), m_AdvertisingData,
                                   m_AdvertisingData);


    QObject::connect(m_Controller.get(), &QLowEnergyController::disconnected, this, &PowerSender::ControllerDisconnected);

    return 0;
}

void PowerSender::UpdatePower(quint16 power, quint16 cadence, quint16 time)
{
    QByteArray value;
    QDataStream s(&value, QIODeviceBase::WriteOnly);
    s.setByteOrder(QDataStream::LittleEndian);
    s << quint16(0x20) << power << cadence << time; //(t++)*1024;
    qDebug() << value;
    QLowEnergyCharacteristic characteristic
        = m_Service->characteristic(QBluetoothUuid::CharacteristicType::CyclingPowerMeasurement);
    Q_ASSERT(characteristic.isValid());
    m_Service->writeCharacteristic(characteristic, value); // Potentially causes notification.
}

void PowerSender::UpdateCadence()
{

}

void PowerSender::ControllerError(QLowEnergyController::Error newError)
{
    qWarning().noquote().nospace() << newError << " occurred: "
                                   << m_Controller->errorString();
    if (newError != QLowEnergyController::RemoteHostClosedError) {
        qWarning("PowerSender quitting due to the error.");
        QCoreApplication::quit();
    }
}

void PowerSender::ControllerDisconnected()
{
    m_Service.reset(m_Controller->addService(m_ServiceData));
    if (m_Service) {
        m_Controller->startAdvertising(QLowEnergyAdvertisingParameters(),
                                       m_AdvertisingData, m_AdvertisingData);
    }
}
