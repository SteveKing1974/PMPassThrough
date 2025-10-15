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

#include "powerreceiver.h"

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

    PowerReceiver rcv;
    rcv.startSearch();


    const int retval = QCoreApplication::exec();
    return retval;
}
