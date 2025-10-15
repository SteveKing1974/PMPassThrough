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
    void pickNext();
    void setAddressType(AddressType type);
    AddressType addressType() const;

    bool measuring() const;
    bool alive() const;
    int m_picked;


signals:
    void exit();

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
    void connectToService(const QString &address);
    bool scanning() const;
    QVariant devices();
    //QLowEnergyService
    void serviceStateChanged(QLowEnergyService::ServiceState s);
    void updatePowerValue(const QLowEnergyCharacteristic &c,
                              const QByteArray &value);
    void confirmedDescriptorWrite(const QLowEnergyDescriptor &d,
                                  const QByteArray &value);

    void updateDemoHR();

private:
    void addMeasurement(int value);
    QBluetoothDeviceDiscoveryAgent *m_deviceDiscoveryAgent;
    QLowEnergyController *m_control = nullptr;
    QLowEnergyService *m_service = nullptr;
    QLowEnergyDescriptor m_notificationDesc;
    QBluetoothDeviceInfo* m_currentDevice;

    QSet<QBluetoothUuid> m_gatts;
    bool m_measuring = false;
    int m_currentValue = 0, m_min = 0, m_max = 0, m_sum = 0;
    float m_avg = 0, m_calories = 0;

    // Statistics
    QDateTime m_start;
    QDateTime m_stop;

    QList<int> m_measurements;
    QLowEnergyController::RemoteAddressType m_addressType = QLowEnergyController::PublicAddress;
    QList<QBluetoothDeviceInfo*> m_devices;
};

#endif // DEVICEHANDLER_H
