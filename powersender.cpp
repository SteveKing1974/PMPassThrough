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

PowerSender::PowerSender(QObject *parent) :
    m_SetupComplete(false),
    QObject{parent}
{}

int PowerSender::SetUp(const QMap<QBluetoothUuid, QByteArray>& characteristics)
{
    //! [Advertising Data]
    m_AdvertisingData.setDiscoverability(QLowEnergyAdvertisingData::DiscoverabilityGeneral);
    m_AdvertisingData.setIncludePowerLevel(true);
    m_AdvertisingData.setLocalName("PowerMeterServer");
    m_AdvertisingData.setServices(QList<QBluetoothUuid>()
                                  << QBluetoothUuid::ServiceClassUuid::CyclingPower);
//                                  << QBluetoothUuid::ServiceClassUuid::CyclingSpeedAndCadence);
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
    m_PowerServiceData.setType(QLowEnergyServiceData::ServiceTypePrimary);
    m_PowerServiceData.setUuid(QBluetoothUuid::ServiceClassUuid::CyclingPower);

    QLowEnergyCharacteristicData powerMeasurementCharData;
    powerMeasurementCharData.setUuid(QBluetoothUuid::CharacteristicType::CyclingPowerMeasurement);
    powerMeasurementCharData.setValue(QByteArray(4, 0));
    powerMeasurementCharData.setProperties(QLowEnergyCharacteristic::Notify);
    const QLowEnergyDescriptorData pmClientConfig(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration,
                                                  QByteArray(4, 0));
    powerMeasurementCharData.addDescriptor(pmClientConfig);
    m_PowerServiceData.addCharacteristic(powerMeasurementCharData);

    QMapIterator<QBluetoothUuid, QByteArray> i(characteristics);
    while (i.hasNext())
    {
        i.next();
        QLowEnergyCharacteristicData data;
        data.setUuid(i.key());
        data.setValue(i.value());
        data.setProperties(QLowEnergyCharacteristic::Read);
        const QLowEnergyDescriptorData clientConfig(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration,
                                                    i.value());
        data.addDescriptor(clientConfig);
        m_PowerServiceData.addCharacteristic(data);
    }

    /////////////
    m_CSCServiceData.setType(QLowEnergyServiceData::ServiceTypePrimary);
    m_CSCServiceData.setUuid(QBluetoothUuid::ServiceClassUuid::CyclingSpeedAndCadence);

    QLowEnergyCharacteristicData cscMeasurementCharData;
    cscMeasurementCharData.setUuid(QBluetoothUuid::CharacteristicType::CSCMeasurement);
    cscMeasurementCharData.setValue(QByteArray(4, 0));
    cscMeasurementCharData.setProperties(QLowEnergyCharacteristic::Notify);
    const QLowEnergyDescriptorData cscClientConfig(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration,
                                                  QByteArray(4, 0));
    cscMeasurementCharData.addDescriptor(cscClientConfig);
    m_CSCServiceData.addCharacteristic(cscMeasurementCharData);

    //! [Service Data]

    //! [Start Advertising]
    bool errorOccurred = false;
    m_Controller.reset(QLowEnergyController::createPeripheral());
    QObject::connect(m_Controller.get(), &QLowEnergyController::errorOccurred, this, &PowerSender::ControllerError);

    m_PowerService.reset(m_Controller->addService(m_PowerServiceData));
    m_CSCService.reset(m_Controller->addService(m_CSCServiceData));

    m_Controller->startAdvertising(QLowEnergyAdvertisingParameters(), m_AdvertisingData,
                                   m_AdvertisingData);


    QObject::connect(m_Controller.get(), &QLowEnergyController::disconnected, this, &PowerSender::ControllerDisconnected);

    m_SetupComplete = true;
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
        = m_PowerService->characteristic(QBluetoothUuid::CharacteristicType::CyclingPowerMeasurement);
    Q_ASSERT(characteristic.isValid());
    m_PowerService->writeCharacteristic(characteristic, value); // Potentially causes notification.
}

void PowerSender::UpdatePower(const QBluetoothUuid &c,
                              const QByteArray &value)
{
    if (!m_SetupComplete) return;

    QLowEnergyCharacteristic characteristic = m_PowerService->characteristic(c);
    Q_ASSERT(characteristic.isValid());

    QByteArray v;
    static qint16 p = 5;
    QDataStream s(&v, QIODeviceBase::WriteOnly);
    s.setByteOrder(QDataStream::LittleEndian);
    s << quint16(0x0) << p++%300;
    qDebug() << "Sending " << v;
    m_PowerService->writeCharacteristic(characteristic, v);
}

void PowerSender::UpdateCadence(const QBluetoothUuid &c,
                                const QByteArray &value)
{
    if (!m_SetupComplete) return;

    QLowEnergyCharacteristic characteristic = m_CSCService->characteristic(c);
    Q_ASSERT(characteristic.isValid());
    m_CSCService->writeCharacteristic(characteristic, value);
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
    m_PowerService.reset(m_Controller->addService(m_PowerServiceData));
    m_CSCService.reset(m_Controller->addService(m_CSCServiceData));
    m_Controller->startAdvertising(QLowEnergyAdvertisingParameters(),
                                       m_AdvertisingData, m_AdvertisingData);
}
