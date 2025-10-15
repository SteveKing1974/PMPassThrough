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
    //! [devicediscovery-1]
    m_deviceDiscoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
    m_deviceDiscoveryAgent->setLowEnergyDiscoveryTimeout(125000);

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
                    pickNext();
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

void PowerReceiver::startMeasurement()
{
    if (alive()) {
        m_start = QDateTime::currentDateTime();
        m_min = 0;
        m_max = 0;
        m_avg = 0;
        m_sum = 0;
        m_calories = 0;
        m_measuring = true;
        m_measurements.clear();
        emit measuringChanged();
    }
}

void PowerReceiver::stopMeasurement()
{
    m_measuring = false;
    emit measuringChanged();
}

//! [Filter HeartRate service 1]
void PowerReceiver::serviceDiscovered(const QBluetoothUuid &gatt)
{
    qDebug() << "gatt is " << gatt;
    if (gatt == QBluetoothUuid(QBluetoothUuid::ServiceClassUuid::HeartRate)) {
        qDebug() << "Heart Rate service discovered. Waiting for service scan to be done...";
        m_foundHeartRateService = true;
    }
}
//! [Filter HeartRate service 1]

void PowerReceiver::serviceScanDone()
{
    qDebug() << "Service scan done.";

    // Delete old service if available
    if (m_service) {
        delete m_service;
        m_service = nullptr;
    }

//! [Filter HeartRate service 2]
    // If heartRateService found, create new service
    if (m_foundHeartRateService)
        m_service = m_control->createServiceObject(QBluetoothUuid(QBluetoothUuid::ServiceClassUuid::HeartRate), this);

    if (m_service) {
        connect(m_service, &QLowEnergyService::stateChanged, this, &PowerReceiver::serviceStateChanged);
        connect(m_service, &QLowEnergyService::characteristicChanged, this, &PowerReceiver::updateHeartRateValue);
        connect(m_service, &QLowEnergyService::descriptorWritten, this, &PowerReceiver::confirmedDescriptorWrite);
        m_service->discoverDetails();
    } else {
        qDebug() << "Heart Rate Service not found.";
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

        const QLowEnergyCharacteristic hrChar =
                m_service->characteristic(QBluetoothUuid(QBluetoothUuid::CharacteristicType::HeartRateMeasurement));
        if (!hrChar.isValid()) {
            qDebug() << "HR Data not found.";
            break;
        }

        m_notificationDesc = hrChar.descriptor(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
        if (m_notificationDesc.isValid())
            m_service->writeDescriptor(m_notificationDesc, QByteArray::fromHex("0100"));

        break;
    }
    default:
        //nothing for now
        break;
    }

    emit aliveChanged();
}
//! [Find HRM characteristic]

//! [Reading value]
void PowerReceiver::updateHeartRateValue(const QLowEnergyCharacteristic &c, const QByteArray &value)
{
    // ignore any other characteristic change -> shouldn't really happen though
    if (c.uuid() != QBluetoothUuid(QBluetoothUuid::CharacteristicType::HeartRateMeasurement))
        return;

    auto data = reinterpret_cast<const quint8 *>(value.constData());
    quint8 flags = *data;

    //Heart Rate
    int hrvalue = 0;
    if (flags & 0x1) // HR 16 bit? otherwise 8 bit
        hrvalue = static_cast<int>(qFromLittleEndian<quint16>(data[1]));
    else
        hrvalue = static_cast<int>(data[1]);

    addMeasurement(hrvalue);
}
//! [Reading value]

void PowerReceiver::updateDemoHR()
{
    int randomValue = 0;
    if (m_currentValue < 30) // Initial value
        randomValue = 55 + QRandomGenerator::global()->bounded(30);
    else if (!m_measuring) // Value when relax
        randomValue = qBound(55, m_currentValue - 2 + QRandomGenerator::global()->bounded(5), 75);
    else // Measuring
        randomValue = m_currentValue + QRandomGenerator::global()->bounded(10) - 2;

    addMeasurement(randomValue);
}

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
    m_foundHeartRateService = false;

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

bool PowerReceiver::measuring() const
{
    return m_measuring;
}

bool PowerReceiver::alive() const
{
    if (m_service)
        return m_service->state() == QLowEnergyService::RemoteServiceDiscovered;

    return false;
}

int PowerReceiver::hr() const
{
    return m_currentValue;
}

int PowerReceiver::time() const
{
    return m_start.secsTo(m_stop);
}

int PowerReceiver::maxHR() const
{
    return m_max;
}

int PowerReceiver::minHR() const
{
    return m_min;
}

float PowerReceiver::average() const
{
    return m_avg;
}

float PowerReceiver::calories() const
{
    return m_calories;
}

void PowerReceiver::addMeasurement(int value)
{
    m_currentValue = value;

    // If measuring and value is appropriate
    if (m_measuring && value > 30 && value < 250) {

        m_stop = QDateTime::currentDateTime();
        m_measurements << value;

        m_min = m_min == 0 ? value : qMin(value, m_min);
        m_max = qMax(value, m_max);
        m_sum += value;
        m_avg = (double)m_sum / m_measurements.size();
        m_calories = ((-55.0969 + (0.6309 * m_avg) + (0.1988 * 94) + (0.2017 * 24)) / 4.184)
                * 60 * time() / 3600;
    }

    emit statsChanged();
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
    //pickNext();
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
