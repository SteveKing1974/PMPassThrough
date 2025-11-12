#ifndef POWERSENDER_H
#define POWERSENDER_H

#include <QObject>
#include <QLowEnergyController>
#include <QLowEnergyServiceData>

class QLowEnergyService;

class PowerSender : public QObject
{
    Q_OBJECT
public:
    explicit PowerSender(QObject *parent = nullptr);
    
    int SetUp(const QMap<QBluetoothUuid, QByteArray>& characteristics);

public slots:
    void UpdatePower(quint16 power, quint16 cadence, quint16 time);
    void UpdatePower(const QBluetoothUuid &c,
                     const QByteArray &value);
    void UpdateCadence(const QBluetoothUuid &c,
                       const QByteArray &value);

private slots:
    void ControllerError(QLowEnergyController::Error newError);
    void ControllerDisconnected();

private:
    std::unique_ptr<QLowEnergyService> m_PowerService;
    std::unique_ptr<QLowEnergyService> m_CSCService;
    std::unique_ptr<QLowEnergyController> m_Controller;

    QLowEnergyAdvertisingData m_AdvertisingData;
    QLowEnergyServiceData m_PowerServiceData;
    QLowEnergyServiceData m_CSCServiceData;

    bool m_SetupComplete;
};

#endif // POWERSENDER_H
