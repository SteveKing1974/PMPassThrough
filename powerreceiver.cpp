// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include "powerreceiver.h"

#include <QtEndian>
#include <QRandomGenerator>
#include <QBluetoothPermission>
#include <qcoreapplication.h>

PowerReceiver::PowerReceiver(QObject *parent) :
    m_currentDevice(nullptr),
    QObject(parent)
{
    QTimer* powermeterTimer = new QTimer(this);
    QObject::connect(powermeterTimer, &QTimer::timeout, qApp, &QCoreApplication::quit);
    powermeterTimer->start(35000);

    //! [devicediscovery-1]
    m_deviceDiscoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
    m_deviceDiscoveryAgent->setLowEnergyDiscoveryTimeout(20000);

    connect(this, &PowerReceiver::exit, qApp, &QCoreApplication::quit);
    connect(m_deviceDiscoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &PowerReceiver::addDevice);
    connect(m_deviceDiscoveryAgent, &QBluetoothDeviceDiscoveryAgent::errorOccurred,
            this, &PowerReceiver::scanError);

    connect(m_deviceDiscoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &PowerReceiver::scanFinished);
    connect(m_deviceDiscoveryAgent, &QBluetoothDeviceDiscoveryAgent::canceled,
            this, &PowerReceiver::scanFinished);
    //! [devicediscovery-1]
}

PowerReceiver::~PowerReceiver()
{
    delete m_currentDevice;
}

void PowerReceiver::setDevice(QBluetoothDeviceInfo *device)
{
    delete m_currentDevice;
    m_currentDevice = device;

    // Disconnect and delete old connection
    if (m_control) {
        m_control->disconnectFromDevice();
        delete m_control;
        m_control = nullptr;
    }

    // Create new controller and connect it if device available
    if (m_currentDevice) {
        qDebug() << "Device: " << m_currentDevice->name();

        // Make connections
        //! [Connect-Signals-1]
        m_control = QLowEnergyController::createCentral(*m_currentDevice, this);
        //! [Connect-Signals-1]
        m_control->setRemoteAddressType(QLowEnergyController::PublicAddress);
        //! [Connect-Signals-2]
        connect(m_control, &QLowEnergyController::serviceDiscovered,
                this, &PowerReceiver::serviceDiscovered);
        connect(m_control, &QLowEnergyController::discoveryFinished,
                this, &PowerReceiver::serviceScanDone);

        connect(m_control, &QLowEnergyController::errorOccurred, this,
                [this](QLowEnergyController::Error error) {
                    Q_UNUSED(error);
                    qDebug() << "Cannot connect to remote device.";
                    emit exit();
                });
        connect(m_control, &QLowEnergyController::connected, this, [this]() {
           qDebug() << "Controller connected. Search services...";
            m_control->discoverServices();
        });
        connect(m_control, &QLowEnergyController::disconnected, this, [this]() {
            qDebug() << "LowEnergy controller disconnected";
        });

        // Connect
        m_control->connectToDevice();
        //! [Connect-Signals-2]
    }
}

//! [Filter HeartRate service 1]
void PowerReceiver::serviceDiscovered(const QBluetoothUuid &gatt)
{
    m_gatts.insert(gatt);
}
//! [Filter HeartRate service 1]

void PowerReceiver::serviceScanDone()
{
    qDebug() << "Service scan done." << m_gatts;
    qDebug() << "Cycling power" << m_gatts.contains(QBluetoothUuid(QBluetoothUuid::ServiceClassUuid::CyclingPower));
    qDebug() << "Speed cadence" << m_gatts.contains(QBluetoothUuid(QBluetoothUuid::ServiceClassUuid::CyclingSpeedAndCadence));

    if (m_gatts.contains(QBluetoothUuid(QBluetoothUuid::ServiceClassUuid::CyclingPower)))
    {
        QLowEnergyService* service = m_control->createServiceObject(QBluetoothUuid(QBluetoothUuid::ServiceClassUuid::CyclingPower));
        m_power_service = new BLEService(service, QBluetoothUuid(QBluetoothUuid::CharacteristicType::CyclingPowerMeasurement), this);
        connect(m_power_service, &BLEService::value_changed, this, &PowerReceiver::updatePowerValue);
        connect(m_power_service, &BLEService::disconnected, this, &PowerReceiver::disconnectService);
    }
    if (m_gatts.contains(QBluetoothUuid(QBluetoothUuid::ServiceClassUuid::CyclingSpeedAndCadence)))
    {
        QLowEnergyService* service = m_control->createServiceObject(QBluetoothUuid(QBluetoothUuid::ServiceClassUuid::CyclingSpeedAndCadence));
        m_cadence_service = new BLEService(service, QBluetoothUuid(QBluetoothUuid::CharacteristicType::CSCMeasurement), this);
        connect(m_cadence_service, &BLEService::value_changed, this, &PowerReceiver::updateCadenceValue);
        connect(m_cadence_service, &BLEService::disconnected, this, &PowerReceiver::disconnectService);
    }
}


//! [Reading value]
void PowerReceiver::updatePowerValue(const QLowEnergyCharacteristic &c, const QByteArray &value)
{
    // ignore any other characteristic change -> shouldn't really happen though
    qDebug() << "Update value " << c.uuid();
    // if (c.uuid() != QBluetoothUuid(QBluetoothUuid::CharacteristicType::HeartRateMeasurement))
    //     return;

    qDebug() << value;
    // auto data = reinterpret_cast<const quint8 *>(value.constData());
    // quint8 flags = *data;

    // //Heart Rate
    // int hrvalue = 0;
    // if (flags & 0x1) // HR 16 bit? otherwise 8 bit
    //     hrvalue = static_cast<int>(qFromLittleEndian<quint16>(data[1]));
    // else
    //     hrvalue = static_cast<int>(data[1]);

    // addMeasurement(hrvalue);
}

void PowerReceiver::updateCadenceValue(const QLowEnergyCharacteristic &c, const QByteArray &value)
{
    qDebug() << "Update value " << c.uuid();
    // if (c.uuid() != QBluetoothUuid(QBluetoothUuid::CharacteristicType::HeartRateMeasurement))
    //     return;

    qDebug() << value;
}
//! [Reading value]


void PowerReceiver::disconnectService()
{
    if (m_power_service) m_power_service->disable_notifications();
    if (m_cadence_service) m_cadence_service->disable_notifications();

    m_control->disconnectFromDevice();

    delete m_power_service;
    m_power_service = nullptr;
    delete m_cadence_service;
    m_cadence_service = nullptr;
}


void PowerReceiver::startSearch()
{
#if QT_CONFIG(permissions)
    //! [permissions]
    QBluetoothPermission permission{};
    permission.setCommunicationModes(QBluetoothPermission::Access);
    switch (qApp->checkPermission(permission)) {
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(permission, this, &PowerReceiver::startSearch);
        return;
    case Qt::PermissionStatus::Denied:
        qDebug() << "Bluetooth permissions not granted!";
        return;
    case Qt::PermissionStatus::Granted:
        break; // proceed to search
    }
    //! [permissions]
#endif // QT_CONFIG(permissions)
    setDevice(nullptr);
    //! [devicediscovery-2]
    m_deviceDiscoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
    //! [devicediscovery-2]

    qDebug() << "Scanning for devices...";
}

//! [devicediscovery-3]
void PowerReceiver::addDevice(const QBluetoothDeviceInfo &device)
{
    qDebug() << "Low Energy device found: " << device.name() << " Scanning more...";
    if (device.name() != "Victory") return;

    setDevice(new QBluetoothDeviceInfo(device));
    //...
}
//! [devicediscovery-4]

void PowerReceiver::scanError(QBluetoothDeviceDiscoveryAgent::Error error)
{
    if (error == QBluetoothDeviceDiscoveryAgent::PoweredOffError)
        qDebug() << "The Bluetooth adaptor is powered off.";
    else if (error == QBluetoothDeviceDiscoveryAgent::InputOutputError)
        qDebug() << "Writing or reading from the device resulted in an error.";
    else
        qDebug() << "An unknown error has occurred.";
}

void PowerReceiver::scanFinished()
{

}
