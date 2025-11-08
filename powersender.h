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
    
    int SetUp();

public slots:
    void UpdatePower(quint16 power, quint16 cadence, quint16 time);
    void UpdateCadence();

private slots:
    void ControllerError(QLowEnergyController::Error newError);
    void ControllerDisconnected();

private:
    std::unique_ptr<QLowEnergyService> m_Service;
    std::unique_ptr<QLowEnergyController> m_Controller;

    QLowEnergyAdvertisingData m_AdvertisingData;
    QLowEnergyServiceData m_ServiceData;
};

#endif // POWERSENDER_H
