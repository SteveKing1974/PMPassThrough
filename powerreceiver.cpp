// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include "powerreceiver.h"

#include <QtEndian>
#include <QRandomGenerator>
#include <QBluetoothPermission>
#include <qcoreapplication.h>

PowerReceiver::PowerReceiver(QObject *parent) :
    QObject(parent)
{
    QTimer powermeterTimer;
    QObject::connect(&powermeterTimer, &QTimer::timeout, qApp, &QCoreApplication::quit);
    powermeterTimer.start(10000);

    //! [devicediscovery-1]
    m_deviceDiscoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
    m_deviceDiscoveryAgent->setLowEnergyDiscoveryTimeout(5000);

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
    m_devices.clear();
}

void PowerReceiver::setAddressType(AddressType type)
{
    switch (type) {
    case PowerReceiver::AddressType::PublicAddress:
        m_addressType = QLowEnergyController::PublicAddress;
        break;
    case PowerReceiver::AddressType::RandomAddress:
        m_addressType = QLowEnergyController::RandomAddress;
        break;
    }
}

PowerReceiver::AddressType PowerReceiver::addressType() const
{
    if (m_addressType == QLowEnergyController::RandomAddress)
        return PowerReceiver::AddressType::RandomAddress;

    return PowerReceiver::AddressType::PublicAddress;
}

void PowerReceiver::setDevice(QBluetoothDeviceInfo *device)
{
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
        m_control->setRemoteAddressType(m_addressType);
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

void PowerReceiver::pickNext()
{
    m_picked++;
    m_picked = m_picked%m_devices.count();
    setDevice(m_devices[m_picked]);
}

//! [Filter HeartRate service 1]
void PowerReceiver::serviceDiscovered(const QBluetoothUuid &gatt)
{
    m_gatts.insert(gatt);
}
//! [Filter HeartRate service 1]

void PowerReceiver::serviceScanDone()
{
    qDebug() << "Service scan done.";
    if (m_gatts.contains(QBluetoothUuid(QBluetoothUuid::ServiceClassUuid::CyclingPower)) &&
                         m_gatts.contains(QBluetoothUuid(QBluetoothUuid::ServiceClassUuid::CyclingSpeedAndCadence)))
    {

    }
    else
    {
        qDebug() << "Power and Cadence service not found.";
    }

    // Delete old service if available
    if (m_service) {
        delete m_service;
        m_service = nullptr;
    }

//! [Filter HeartRate service 2]
    // If heartRateService found, create new service
    if (m_foundPowerService)
        m_service = m_control->createServiceObject(QBluetoothUuid(QBluetoothUuid::ServiceClassUuid::CyclingPower), this);

    if (m_service) {
        connect(m_service, &QLowEnergyService::stateChanged, this, &PowerReceiver::serviceStateChanged);
        connect(m_service, &QLowEnergyService::characteristicChanged, this, &PowerReceiver::updatePowerValue);
        connect(m_service, &QLowEnergyService::descriptorWritten, this, &PowerReceiver::confirmedDescriptorWrite);
        m_service->discoverDetails();
    } else {
        qDebug() << "Power Service not found.";
        pickNext();
    }
//! [Filter HeartRate service 2]
}

// Service functions
//! [Find HRM characteristic]
void PowerReceiver::serviceStateChanged(QLowEnergyService::ServiceState s)
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
//! [Find HRM characteristic]

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
//! [Reading value]


void PowerReceiver::confirmedDescriptorWrite(const QLowEnergyDescriptor &d, const QByteArray &value)
{
    if (d.isValid() && d == m_notificationDesc && value == QByteArray::fromHex("0000")) {
        //disabled notifications -> assume disconnect intent
        m_control->disconnectFromDevice();
        delete m_service;
        m_service = nullptr;
    }
}

void PowerReceiver::disconnectService()
{
    m_foundPowerService = false;

    //disable notifications
    if (m_notificationDesc.isValid() && m_service
            && m_notificationDesc.value() == QByteArray::fromHex("0100")) {
        m_service->writeDescriptor(m_notificationDesc, QByteArray::fromHex("0000"));
    } else {
        if (m_control)
            m_control->disconnectFromDevice();

        delete m_service;
        m_service = nullptr;
    }
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
    qDeleteAll(m_devices);
    m_devices.clear();

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

    // If device is LowEnergy-device, add it to the list
    if (device.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration) {
        auto devInfo = new QBluetoothDeviceInfo(device);
        auto it = std::find_if(m_devices.begin(), m_devices.end(),
                               [devInfo](QBluetoothDeviceInfo *dev) {
                                   return devInfo->address() == dev->address();
                               });
        if (it == m_devices.end()) {
            m_devices.append(devInfo);
        } else {
            auto oldDev = *it;
            *it = devInfo;
            delete oldDev;
        }
    }
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
    if (m_devices.isEmpty()) {
        qDebug() << "No Low Energy devices found.";
    } else {
        qDebug() << "Scanning done.";
    }

    m_picked = -1;
    pickNext();
}


void PowerReceiver::connectToService(const QString &address)
{
    m_deviceDiscoveryAgent->stop();

    QBluetoothDeviceInfo *currentDevice = nullptr;
    for (QBluetoothDeviceInfo *entry : std::as_const(m_devices)) {
        if (entry && entry->address() == QBluetoothAddress(address)) {
            currentDevice = entry;
            break;
        }
    }

    if (currentDevice)
        setDevice(currentDevice);

}

bool PowerReceiver::scanning() const
{
    return m_deviceDiscoveryAgent->isActive();
}

QVariant PowerReceiver::devices()
{
    return QVariant::fromValue(m_devices);
}
