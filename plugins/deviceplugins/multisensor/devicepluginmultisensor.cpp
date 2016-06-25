/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2015 Simon Stuerz <simon.stuerz@guh.guru>                *
 *                                                                         *
 *  This file is part of guh.                                              *
 *                                                                         *
 *  Guh is free software: you can redistribute it and/or modify            *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, version 2 of the License.                *
 *                                                                         *
 *  Guh is distributed in the hope that it will be useful,                 *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the           *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with guh. If not, see <http://www.gnu.org/licenses/>.            *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*!
    \page multisensor.html
    \title MultiSensor
    \brief Plugin for TI SensorTag.

    \ingroup plugins
    \ingroup guh-plugins

    This plugin allows finding and controlling the Bluetooth Low Energy SensorTag from Texas Instruments.

    \chapter Plugin properties
    Following JSON file contains the definition and the description of all available
\l{DeviceClass}{DeviceClasses}
    and \l{Vendor}{Vendors} of this \l{DevicePlugin}.

    For more details on how to read this JSON file please check out the documentation fo
r \l{The plugin JSON File}.

    \quotefile plugins/deviceplugins/multisensor/devicepluginmultisensor.json
*/


#include "devicepluginmultisensor.h"
#include "plugininfo.h"
#include "devicemanager.h"
#include "bluetooth/bluetoothlowenergydevice.h"

/* The constructor of this device plugin. */
DevicePluginMultiSensor::DevicePluginMultiSensor()
{

}

/* This method will be called from the devicemanager to get
 * information about this plugin which device resource will be needed.
 */
DeviceManager::HardwareResources DevicePluginMultiSensor::requiredHardware() const
{
    return DeviceManager::HardwareResourceBluetoothLE;
}

DeviceManager::DeviceError DevicePluginMultiSensor::discoverDevices(const DeviceClassId &deviceClassId, const ParamList &params)
{
    Q_UNUSED(params)

    if (deviceClassId != sensortagDeviceClassId)
        return DeviceManager::DeviceErrorDeviceClassNotFound;

    if (!discoverBluetooth())
        return DeviceManager::DeviceErrorHardwareNotAvailable;

    return DeviceManager::DeviceErrorAsync;
}

void DevicePluginMultiSensor::bluetoothDiscoveryFinished(const QList<QBluetoothDeviceInfo> &deviceInfos)
{
    QList<DeviceDescriptor> deviceDescriptors;
    foreach (QBluetoothDeviceInfo deviceInfo, deviceInfos) {
        if (deviceInfo.name().contains("SensorTag")) {
            if (!verifyExistingDevices(deviceInfo)) {
                DeviceDescriptor descriptor(sensortagDeviceClassId, "SensorTag", deviceInfo.address().toString());
                ParamList params;
                params.append(Param("name", deviceInfo.name()));
                params.append(Param("mac address", deviceInfo.address().toString()));
                descriptor.setParams(params);
                deviceDescriptors.append(descriptor);
            }
        }
    }

    emit devicesDiscovered(sensortagDeviceClassId, deviceDescriptors);
}

/* This method will be called from the devicemanager while he
 * is setting up a new device. Here the developer has the chance to
 * perform the setup on the actual device and report the result.
 */
DeviceManager::DeviceSetupStatus DevicePluginMultiSensor::setupDevice(Device *device)
{
    qCDebug(dcMultiSensor) << "Setting up MultiSensor" << device->name() << device->params();

    if (device->deviceClassId() == sensortagDeviceClassId) {
        QBluetoothAddress address = QBluetoothAddress(device->paramValue("mac address").toString());
        QString name = device->paramValue("name").toString();
        QBluetoothDeviceInfo deviceInfo = QBluetoothDeviceInfo(address, name, 0);

        QPointer<SensorTag> tag = new SensorTag(deviceInfo, QLowEnergyController::PublicAddress, device);
        m_tags.append(tag);

        tag->connectDevice();

        return DeviceManager::DeviceSetupStatusSuccess;
    }
    return DeviceManager::DeviceSetupStatusFailure;
}


void DevicePluginMultiSensor::deviceRemoved(Device *device)
{
    auto pos = std::find_if(
                m_tags.begin(), m_tags.end(),
                [device](QPointer<SensorTag> tag) { return tag->parent() == device; });
    if (pos == m_tags.end())
        return;

    m_tags.erase(pos);
}

bool DevicePluginMultiSensor::verifyExistingDevices(const QBluetoothDeviceInfo &deviceInfo)
{
    foreach (Device *device, myDevices()) {
        if (device->paramValue("mac address").toString() == deviceInfo.address().toString())
            return true;
    }

    return false;
}



