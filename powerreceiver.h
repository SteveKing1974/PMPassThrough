// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef DEVICEHANDLER_H
#define DEVICEHANDLER_H

#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QBluetoothDeviceDiscoveryAgent>

#include <QDateTime>
#include <QList>
#include <QTimer>

#include "bleservice.h"

class DeviceInfo;

class PowerReceiver : public QObject
{
    Q_OBJECT

public:
    enum class AddressType {
        PublicAddress,
        RandomAddress
    };
    Q_ENUM(AddressType)

    PowerReceiver(QObject *parent = nullptr);
    virtual ~PowerReceiver();

    void setDevice( QBluetoothDeviceInfo *device);

signals:
    void exit();
    void PowerUpdated(quint16 power, quint16 cadence, quint16 time);

public slots:
    void disconnectService();
    void addDevice(const QBluetoothDeviceInfo &device);
    void scanError(QBluetoothDeviceDiscoveryAgent::Error error);
    void scanFinished();
    void startSearch();

private:
    //QLowEnergyController
    void serviceDiscovered(const QBluetoothUuid &);
    void serviceScanDone();

    void updatePowerValue(const QLowEnergyCharacteristic &c,
                              const QByteArray &value);
    void updateCadenceValue(const QLowEnergyCharacteristic &c,
                          const QByteArray &value);
private:
    QBluetoothDeviceDiscoveryAgent *m_deviceDiscoveryAgent;
    QLowEnergyController *m_control = nullptr;
    BLEService *m_power_service = nullptr;
    BLEService *m_cadence_service = nullptr;

    QLowEnergyDescriptor m_notificationDesc;
    QBluetoothDeviceInfo* m_currentDevice;

    QSet<QBluetoothUuid> m_gatts;

    QBluetoothDeviceInfo* m_trainer_device;
};

#endif // DEVICEHANDLER_H
