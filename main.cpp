// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

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

int main(int argc, char *argv[])
{
    // QLoggingCategory::setFilterRules(QStringLiteral("qt.bluetooth* = true"));
#if defined(Q_OS_ANDROID) || defined(Q_OS_DARWIN)
    QGuiApplication app(argc, argv);
#else
    QCoreApplication app(argc, argv);
#endif

#if QT_CONFIG(permissions)
    //! [Check Bluetooth Permission]
    auto permissionStatus = app.checkPermission(QBluetoothPermission{});
    //! [Check Bluetooth Permission]

    //! [Request Bluetooth Permission]
    if (permissionStatus == Qt::PermissionStatus::Undetermined) {
        qInfo("Requesting Bluetooth permission ...");
        app.requestPermission(QBluetoothPermission{}, [&permissionStatus](const QPermission &permission){
            qApp->exit();
            permissionStatus = permission.status();
        });
        // Now, wait for permission request to resolve.
        app.exec();
    }
    //! [Request Bluetooth Permission]

    if (permissionStatus == Qt::PermissionStatus::Denied) {
        // Either explicitly denied by a user, or Bluetooth is off.
        qWarning("This application cannot use Bluetooth, the permission was denied");
        return -1;
    }

#endif
    //! [Advertising Data]
    QLowEnergyAdvertisingData advertisingData;
    advertisingData.setDiscoverability(QLowEnergyAdvertisingData::DiscoverabilityGeneral);
    advertisingData.setIncludePowerLevel(true);
    advertisingData.setLocalName("PowerMeterServer");
    advertisingData.setServices(QList<QBluetoothUuid>() << QBluetoothUuid::ServiceClassUuid::CyclingPower);
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
    QLowEnergyServiceData serviceData;
    serviceData.setType(QLowEnergyServiceData::ServiceTypePrimary);
    serviceData.setUuid(QBluetoothUuid::ServiceClassUuid::CyclingPower);

    QLowEnergyCharacteristicData powerMeasurementCharData;
    powerMeasurementCharData.setUuid(QBluetoothUuid::CharacteristicType::CyclingPowerMeasurement);
    powerMeasurementCharData.setValue(QByteArray(4, 0));
    powerMeasurementCharData.setProperties(QLowEnergyCharacteristic::Notify);
    const QLowEnergyDescriptorData pmClientConfig(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration,
                                                QByteArray(4, 0));
    powerMeasurementCharData.addDescriptor(pmClientConfig);
    serviceData.addCharacteristic(powerMeasurementCharData);

    QLowEnergyCharacteristicData featureCharData;
    featureCharData.setUuid(QBluetoothUuid::CharacteristicType::CyclingPowerFeature);
    featureCharData.setValue(QByteArray(2, 0));
    featureCharData.setProperties(QLowEnergyCharacteristic::Read);
    const QLowEnergyDescriptorData clientConfig(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration,
                                                QByteArray(2, 0));
    featureCharData.addDescriptor(clientConfig);
    serviceData.addCharacteristic(featureCharData);

    QLowEnergyCharacteristicData sensorLocationCharData;
    sensorLocationCharData.setUuid(QBluetoothUuid::CharacteristicType::SensorLocation);
    sensorLocationCharData.setValue(QByteArray(1, 13));
    sensorLocationCharData.setProperties(QLowEnergyCharacteristic::Read);
    const QLowEnergyDescriptorData locClientConfig(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration,
                                                QByteArray(1, 13));
    sensorLocationCharData.addDescriptor(locClientConfig);
    serviceData.addCharacteristic(sensorLocationCharData);

    //! [Service Data]

    //! [Start Advertising]
    bool errorOccurred = false;
    const std::unique_ptr<QLowEnergyController> leController(QLowEnergyController::createPeripheral());
    auto errorHandler = [&leController, &errorOccurred](QLowEnergyController::Error errorCode) {
        qWarning().noquote().nospace() << errorCode << " occurred: "
                                       << leController->errorString();
        if (errorCode != QLowEnergyController::RemoteHostClosedError) {
            qWarning("Heartrate-server quitting due to the error.");
            errorOccurred = true;
            QCoreApplication::quit();
        }
    };
    QObject::connect(leController.get(), &QLowEnergyController::errorOccurred, errorHandler);

    std::unique_ptr<QLowEnergyService> service(leController->addService(serviceData));
    leController->startAdvertising(QLowEnergyAdvertisingParameters(), advertisingData,
                                   advertisingData);
    if (errorOccurred)
        return -1;
    //! [Start Advertising]

    //! [Provide Heartbeat]
    QTimer powermeterTimer;
    qint16 currentPower = 60;
    enum ValueChange { ValueUp, ValueDown } valueChange = ValueUp;
    const auto powerProvider = [&service, &currentPower, &valueChange]() {
        QByteArray value;
        QDataStream s(&value, QIODeviceBase::WriteOnly);
        s << quint16(0) << currentPower;
        QLowEnergyCharacteristic characteristic
            = service->characteristic(QBluetoothUuid::CharacteristicType::CyclingPowerMeasurement);
        Q_ASSERT(characteristic.isValid());
        service->writeCharacteristic(characteristic, value); // Potentially causes notification.
        if (currentPower == 60)
            valueChange = ValueUp;
        else if (currentPower == 100)
            valueChange = ValueDown;
        if (valueChange == ValueUp)
            ++currentPower;
        else
            --currentPower;
    };
    QObject::connect(&powermeterTimer, &QTimer::timeout, powerProvider);
    powermeterTimer.start(1000);
    //! [Provide Heartbeat]

    auto reconnect = [&leController, advertisingData, &service, serviceData]() {
        service.reset(leController->addService(serviceData));
        if (service) {
            leController->startAdvertising(QLowEnergyAdvertisingParameters(),
                                           advertisingData, advertisingData);
        }
    };
    QObject::connect(leController.get(), &QLowEnergyController::disconnected, reconnect);

    const int retval = QCoreApplication::exec();
    return errorOccurred ? -1 : retval;
}
