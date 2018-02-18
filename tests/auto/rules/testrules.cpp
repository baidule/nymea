/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2015 Simon Stürz <simon.stuerz@guh.io>                   *
 *  Copyright (C) 2014 Michael Zanetti <michael_zanetti@gmx.net>           *
 *                                                                         *
 *  This file is part of guh.                                              *
 *                                                                         *
 *  Guh is free software: you can redistribute it and/or modify            *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, version 2 of the License.                *
 *                                                                         *
 *  Guh is distributed in the hope that it will be useful,                 *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with guh. If not, see <http://www.gnu.org/licenses/>.            *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "guhtestbase.h"
#include "guhcore.h"
#include "devicemanager.h"
#include "mocktcpserver.h"

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QTcpSocket>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QCoreApplication>

using namespace guhserver;

class TestRules: public GuhTestBase
{
    Q_OBJECT

private:
    void cleanupMockHistory();
    void cleanupRules();

    DeviceId addDisplayPinDevice();

    QVariantMap createEventDescriptor(const DeviceId &deviceId, const EventTypeId &eventTypeId);
    QVariantMap createActionWithParams(const DeviceId &deviceId);
    QVariantMap createStateEvaluatorFromSingleDescriptor(const QVariantMap &stateDescriptor);

    void setWritableStateValue(const DeviceId &deviceId, const StateTypeId &stateTypeId, const QVariant &value);

    void verifyRuleExecuted(const ActionTypeId &actionTypeId);
    void verifyRuleNotExecuted();

    QVariant validIntStateBasedRule(const QString &name, const bool &executable, const bool &enabled);

private slots:

    void cleanup();
    void emptyRule();
    void getInvalidRule();

    void addRemoveRules_data();
    void addRemoveRules();

    void editRules_data();
    void editRules();

    void executeRuleActions_data();
    void executeRuleActions();

    void findRule();

    void removeInvalidRule();

    void loadStoreConfig();

    void evaluateEvent();

    void evaluateEventParams();

    void testStateEvaluator_data();
    void testStateEvaluator();

    void testStateEvaluator2_data();
    void testStateEvaluator2();

    void testChildEvaluator_data();
    void testChildEvaluator();

    void testStateChange();

    void enableDisableRule();

    void testEventBasedAction();

    void removePolicyUpdate();
    void removePolicyCascade();

    void testRuleActionParams_data();
    void testRuleActionParams();

    void testInterfaceBasedRule();

    void testHousekeeping_data();
    void testHousekeeping();

};

void TestRules::cleanupMockHistory() {
    QNetworkAccessManager nam;
    QSignalSpy spy(&nam, SIGNAL(finished(QNetworkReply*)));
    QNetworkRequest request(QUrl(QString("http://localhost:%1/clearactionhistory").arg(QString::number(m_mockDevice1Port))));
    QNetworkReply *reply = nam.get(request);
    spy.wait();
    QCOMPARE(spy.count(), 1);
    reply->deleteLater();
}

void TestRules::cleanupRules() {
    QVariant response = injectAndWait("Rules.GetRules");
    foreach (const QVariant &ruleDescription, response.toMap().value("params").toMap().value("ruleDescriptions").toList()) {
        QVariantMap params;
        params.insert("ruleId", ruleDescription.toMap().value("id").toString());
        verifyRuleError(injectAndWait("Rules.RemoveRule", params));
    }
}

DeviceId TestRules::addDisplayPinDevice()
{
    // Discover device
    QVariantList discoveryParams;
    QVariantMap resultCountParam;
    resultCountParam.insert("paramTypeId", resultCountParamTypeId);
    resultCountParam.insert("value", 1);
    discoveryParams.append(resultCountParam);

    QVariantMap params;
    params.insert("deviceClassId", mockDisplayPinDeviceClassId);
    params.insert("discoveryParams", discoveryParams);
    QVariant response = injectAndWait("Devices.GetDiscoveredDevices", params);

    verifyDeviceError(response, DeviceManager::DeviceErrorNoError);

    // Pair device
    DeviceDescriptorId descriptorId = DeviceDescriptorId(response.toMap().value("params").toMap().value("deviceDescriptors").toList().first().toMap().value("id").toString());
    params.clear();
    params.insert("deviceClassId", mockDisplayPinDeviceClassId);
    params.insert("name", "Display pin mock device");
    params.insert("deviceDescriptorId", descriptorId.toString());
    response = injectAndWait("Devices.PairDevice", params);

    verifyDeviceError(response);

    PairingTransactionId pairingTransactionId(response.toMap().value("params").toMap().value("pairingTransactionId").toString());
    QString displayMessage = response.toMap().value("params").toMap().value("displayMessage").toString();

    qDebug() << "displayMessage" << displayMessage;

    params.clear();
    params.insert("pairingTransactionId", pairingTransactionId.toString());
    params.insert("secret", "243681");
    response = injectAndWait("Devices.ConfirmPairing", params);

    verifyDeviceError(response);

    return DeviceId(response.toMap().value("params").toMap().value("deviceId").toString());
}

QVariantMap TestRules::createEventDescriptor(const DeviceId &deviceId, const EventTypeId &eventTypeId)
{
    QVariantMap eventDescriptor;
    eventDescriptor.insert("eventTypeId", eventTypeId);
    eventDescriptor.insert("deviceId", deviceId);
    return eventDescriptor;
}

QVariantMap TestRules::createActionWithParams(const DeviceId &deviceId)
{
    QVariantMap action;
    QVariantList ruleActionParams;
    QVariantMap param1;
    param1.insert("paramTypeId", mockActionParam1ParamTypeId);
    param1.insert("value", 4);
    QVariantMap param2;
    param2.insert("paramTypeId", mockActionParam2ParamTypeId);
    param2.insert("value", true);
    ruleActionParams.append(param1);
    ruleActionParams.append(param2);
    action.insert("deviceId", deviceId);
    action.insert("actionTypeId", mockActionIdWithParams);
    action.insert("ruleActionParams", ruleActionParams);
    return action;
}

QVariantMap TestRules::createStateEvaluatorFromSingleDescriptor(const QVariantMap &stateDescriptor)
{
    QVariantMap stateEvaluator;
    stateEvaluator.insert("stateDescriptor", stateDescriptor);
    return stateEvaluator;
}

void TestRules::setWritableStateValue(const DeviceId &deviceId, const StateTypeId &stateTypeId, const QVariant &value)
{
    QVariantMap params;
    params.insert("deviceId", deviceId);
    params.insert("stateTypeId", stateTypeId);
    QVariant response = injectAndWait("Devices.GetStateValue", params);
    verifyDeviceError(response);

    QVariant currentStateValue = response.toMap().value("params").toMap().value("value");
    bool shouldGetNotification = currentStateValue != value;
    QSignalSpy stateSpy(m_mockTcpServer, SIGNAL(outgoingData(QUuid,QByteArray)));

    QVariantMap paramMap;
    paramMap.insert("paramTypeId", stateTypeId.toString());
    paramMap.insert("value", value);

    params.clear(); response.clear();
    params.insert("deviceId", deviceId);
    params.insert("actionTypeId", stateTypeId.toString());
    params.insert("params", QVariantList() << paramMap);

    printJson(params);

    response = injectAndWait("Actions.ExecuteAction", params);
    verifyDeviceError(response);

    if (shouldGetNotification) {
        stateSpy.wait(200);
        // Wait for state changed notification
        QVariantList stateChangedVariants = checkNotifications(stateSpy, "Devices.StateChanged");
        QVERIFY2(stateChangedVariants.count() == 1, "Did not get Devices.StateChanged notification.");

        QVariantMap notification = stateChangedVariants.first().toMap().value("params").toMap();
        QVERIFY2(notification.contains("deviceId"), "Devices.StateChanged notification does not contain deviceId");
        QVERIFY2(DeviceId(notification.value("deviceId").toString()) == deviceId, "Devices.StateChanged notification does not contain the correct deviceId");
        QVERIFY2(notification.contains("stateTypeId"), "Devices.StateChanged notification does not contain stateTypeId");
        QVERIFY2(StateTypeId(notification.value("stateTypeId").toString()) == stateTypeId, "Devices.StateChanged notification does not contain the correct stateTypeId");
        QVERIFY2(notification.contains("value"), "Devices.StateChanged notification does not contain new state value");
        QVERIFY2(notification.value("value") == QVariant(value), "Devices.StateChanged notification does not contain the new value");
    }

}

void TestRules::verifyRuleExecuted(const ActionTypeId &actionTypeId)
{
    // Verify rule got executed
    QNetworkAccessManager nam;
    QSignalSpy spy(&nam, SIGNAL(finished(QNetworkReply*)));
    QNetworkRequest request(QUrl(QString("http://localhost:%1/actionhistory").arg(QString::number(m_mockDevice1Port))));
    QNetworkReply *reply = nam.get(request);
    spy.wait();
    QCOMPARE(spy.count(), 1);

    QByteArray actionHistory = reply->readAll();
    qDebug() << "have action history" << actionHistory;
    QVERIFY2(actionTypeId == ActionTypeId(actionHistory), "Action not triggered");
    reply->deleteLater();
}

void TestRules::verifyRuleNotExecuted()
{
    QNetworkAccessManager nam;
    QSignalSpy spy(&nam, SIGNAL(finished(QNetworkReply*)));
    QNetworkRequest request(QUrl(QString("http://localhost:%1/actionhistory").arg(QString::number(m_mockDevice1Port))));
    QNetworkReply *reply = nam.get(request);
    spy.wait();
    QCOMPARE(spy.count(), 1);

    QByteArray actionHistory = reply->readAll();
    qDebug() << "have action history" << actionHistory;
    QVERIFY2(actionHistory.isEmpty(), "Action is triggered while it should not have been.");
    reply->deleteLater();
}


/***********************************************************************/

void TestRules::cleanup() {
    cleanupMockHistory();
    cleanupRules();
}

void TestRules::emptyRule()
{
    QVariantMap params;
    params.insert("name", QString());
    params.insert("actions", QVariantList());
    QVariant response = injectAndWait("Rules.AddRule", params);
    verifyRuleError(response, RuleEngine::RuleErrorInvalidRuleFormat);
}

void TestRules::getInvalidRule()
{
    QVariantMap params;
    params.insert("ruleId", QUuid::createUuid());
    QVariant response = injectAndWait("Rules.GetRuleDetails", params);
    verifyRuleError(response, RuleEngine::RuleErrorRuleNotFound);
}

QVariant TestRules::validIntStateBasedRule(const QString &name, const bool &executable, const bool &enabled)
{
    QVariantMap params;

    // StateDescriptor
    QVariantMap stateDescriptor;
    stateDescriptor.insert("stateTypeId", mockIntStateId);
    stateDescriptor.insert("deviceId", m_mockDeviceId);
    stateDescriptor.insert("operator", JsonTypes::valueOperatorToString(Types::ValueOperatorLess));
    stateDescriptor.insert("value", 25);

    // StateEvaluator
    QVariantMap stateEvaluator;
    stateEvaluator.insert("stateDescriptor", stateDescriptor);
    stateEvaluator.insert("operator", JsonTypes::stateOperatorToString(Types::StateOperatorAnd));

    // RuleAction
    QVariantMap action;
    action.insert("actionTypeId", mockActionIdWithParams);
    QVariantList actionParams;
    QVariantMap param1;
    param1.insert("paramTypeId", mockActionParam1ParamTypeId);
    param1.insert("value", 5);
    actionParams.append(param1);
    QVariantMap param2;
    param2.insert("paramTypeId", mockActionParam2ParamTypeId);
    param2.insert("value", true);
    actionParams.append(param2);
    action.insert("deviceId", m_mockDeviceId);
    action.insert("ruleActionParams", actionParams);

    // RuleExitAction
    QVariantMap exitAction;
    exitAction.insert("actionTypeId", mockActionIdNoParams);
    exitAction.insert("deviceId", m_mockDeviceId);
    exitAction.insert("ruleActionParams", QVariantList());

    params.insert("name", name);
    params.insert("enabled", enabled);
    params.insert("executable", executable);
    params.insert("stateEvaluator", stateEvaluator);
    params.insert("actions", QVariantList() << action);
    params.insert("exitActions", QVariantList() << exitAction);

    return params;
}

void TestRules::addRemoveRules_data()
{
    // RuleAction
    QVariantMap validActionNoParams;
    validActionNoParams.insert("actionTypeId", mockActionIdNoParams);
    validActionNoParams.insert("deviceId", m_mockDeviceId);
    validActionNoParams.insert("ruleActionParams", QVariantList());

    QVariantMap invalidAction;
    invalidAction.insert("actionTypeId", ActionTypeId());
    invalidAction.insert("deviceId", m_mockDeviceId);
    invalidAction.insert("ruleActionParams", QVariantList());

    // RuleExitAction
    QVariantMap validExitActionNoParams;
    validExitActionNoParams.insert("actionTypeId", mockActionIdNoParams);
    validExitActionNoParams.insert("deviceId", m_mockDeviceId);
    validExitActionNoParams.insert("ruleActionParams", QVariantList());

    QVariantMap invalidExitAction;
    invalidExitAction.insert("actionTypeId", ActionTypeId());
    invalidExitAction.insert("deviceId", m_mockDeviceId);
    invalidExitAction.insert("ruleActionParams", QVariantList());

    // StateDescriptor
    QVariantMap stateDescriptor;
    stateDescriptor.insert("stateTypeId", mockIntStateId);
    stateDescriptor.insert("deviceId", m_mockDeviceId);
    stateDescriptor.insert("operator", JsonTypes::valueOperatorToString(Types::ValueOperatorLess));
    stateDescriptor.insert("value", 20);

    // StateEvaluator
    QVariantMap validStateEvaluator;
    validStateEvaluator.insert("stateDescriptor", stateDescriptor);
    validStateEvaluator.insert("operator", JsonTypes::stateOperatorToString(Types::StateOperatorAnd));

    QVariantMap invalidStateEvaluator;
    stateDescriptor.remove("deviceId");
    invalidStateEvaluator.insert("stateDescriptor", stateDescriptor);

    // EventDescriptor
    QVariantMap validEventDescriptor1;
    validEventDescriptor1.insert("eventTypeId", mockEvent1Id);
    validEventDescriptor1.insert("deviceId", m_mockDeviceId);
    validEventDescriptor1.insert("paramDescriptors", QVariantList());

    QVariantMap validEventDescriptor2;
    validEventDescriptor2.insert("eventTypeId", mockEvent2Id);
    validEventDescriptor2.insert("deviceId", m_mockDeviceId);
    QVariantList params;
    QVariantMap param1;
    param1.insert("paramTypeId", mockParamIntParamTypeId);
    param1.insert("value", 3);
    param1.insert("operator", JsonTypes::valueOperatorToString(Types::ValueOperatorEquals));
    params.append(param1);
    validEventDescriptor2.insert("paramDescriptors", params);

    QVariantMap validEventDescriptor3;
    validEventDescriptor3.insert("eventTypeId", mockEvent2Id);
    validEventDescriptor3.insert("deviceId", m_mockDeviceId);
    validEventDescriptor3.insert("paramDescriptors", QVariantList());

    // EventDescriptorList
    QVariantList eventDescriptorList;
    eventDescriptorList.append(validEventDescriptor1);
    eventDescriptorList.append(validEventDescriptor2);

    QVariantMap invalidEventDescriptor;
    invalidEventDescriptor.insert("eventTypeId", mockEvent1Id);
    invalidEventDescriptor.insert("deviceId", DeviceId());
    invalidEventDescriptor.insert("paramDescriptors", QVariantList());

    // RuleAction event based
    QVariantMap validActionEventBased;
    validActionEventBased.insert("actionTypeId", mockActionIdWithParams);
    validActionEventBased.insert("deviceId", m_mockDeviceId);
    QVariantMap validActionEventBasedParam1;
    validActionEventBasedParam1.insert("paramTypeId", mockActionParam1ParamTypeId);
    validActionEventBasedParam1.insert("eventTypeId", mockEvent2Id);
    validActionEventBasedParam1.insert("eventParamTypeId", mockParamIntParamTypeId);
    QVariantMap validActionEventBasedParam2;
    validActionEventBasedParam2.insert("paramTypeId", mockActionParam2ParamTypeId);
    validActionEventBasedParam2.insert("value", false);
    validActionEventBased.insert("ruleActionParams", QVariantList() << validActionEventBasedParam1 << validActionEventBasedParam2);

    QVariantMap invalidActionEventBased;
    invalidActionEventBased.insert("actionTypeId", mockActionIdNoParams);
    invalidActionEventBased.insert("deviceId", m_mockDeviceId);
    validActionEventBasedParam1.insert("value", 10);
    invalidActionEventBased.insert("ruleActionParams", QVariantList() << validActionEventBasedParam1);

    QVariantMap invalidActionEventBased2;
    invalidActionEventBased2.insert("actionTypeId", mockActionIdWithParams);
    invalidActionEventBased2.insert("deviceId", m_mockDeviceId);
    QVariantMap invalidActionEventBasedParam2;
    invalidActionEventBasedParam2.insert("paramTypeId", mockActionParam1ParamTypeId);
    invalidActionEventBasedParam2.insert("eventTypeId", mockEvent1Id);
    invalidActionEventBasedParam2.insert("eventParamTypeId", "value");
    QVariantMap invalidActionEventBasedParam3;
    invalidActionEventBasedParam3.insert("paramTypeId", mockActionParam2ParamTypeId);
    invalidActionEventBasedParam3.insert("value", 2);
    invalidActionEventBased2.insert("ruleActionParams", QVariantList() << invalidActionEventBasedParam2 << invalidActionEventBasedParam3);

    QVariantMap invalidActionEventBased3;
    invalidActionEventBased3.insert("actionTypeId", mockActionIdWithParams);
    invalidActionEventBased3.insert("deviceId", m_mockDeviceId);
    QVariantMap invalidActionEventBasedParam4;
    invalidActionEventBasedParam4.insert("paramTypeId", mockActionParam1ParamTypeId);
    invalidActionEventBasedParam4.insert("eventTypeId", mockEvent1Id);
    invalidActionEventBasedParam4.insert("eventParamTypeId", mockParamIntParamTypeId);
    invalidActionEventBased3.insert("ruleActionParams", QVariantList() << invalidActionEventBasedParam4);

    QTest::addColumn<bool>("enabled");
    QTest::addColumn<QVariantMap>("action1");
    QTest::addColumn<QVariantMap>("exitAction1");
    QTest::addColumn<QVariantMap>("eventDescriptor");
    QTest::addColumn<QVariantList>("eventDescriptorList");
    QTest::addColumn<QVariantMap>("stateEvaluator");
    QTest::addColumn<RuleEngine::RuleError>("error");
    QTest::addColumn<bool>("jsonError");
    QTest::addColumn<QString>("name");

    // Rules with event based actions
    QTest::newRow("valid rule. enabled, 1 Action (eventBased), 1 EventDescriptor, name")                << true     << validActionEventBased    << QVariantMap()            << validEventDescriptor3    << QVariantList()       << QVariantMap()            << RuleEngine::RuleErrorNoError << true << "ActionEventRule1";
    QTest::newRow("invalid rule. enabled, 1 Action (eventBased), 1 EventDescriptor, name")              << true     << invalidActionEventBased2 << QVariantMap()            << validEventDescriptor3    << QVariantList()       << QVariantMap()            << RuleEngine::RuleErrorInvalidRuleActionParameter << false << "TestRule";

    QTest::newRow("invalid rule. enabled, 1 Action (eventBased), types not matching, name")             << true     << invalidActionEventBased3 << QVariantMap()            << validEventDescriptor1    << QVariantList()       << QVariantMap()            << RuleEngine::RuleErrorTypesNotMatching << false << "TestRule";

    QTest::newRow("invalid rule. enabled, 1 Action (eventBased), 1 EventDescriptor, name")              << true     << invalidActionEventBased  << QVariantMap()            << validEventDescriptor2    << QVariantList()       << QVariantMap()            << RuleEngine::RuleErrorTypesNotMatching << false << "TestRule";
    QTest::newRow("invalid rule. enabled, 1 Action (eventBased), 1 StateEvaluator, name")               << true     << validActionEventBased    << QVariantMap()            << QVariantMap()            << QVariantList()       << validStateEvaluator      << RuleEngine::RuleErrorInvalidRuleActionParameter << false << "TestRule";
    QTest::newRow("invalid rule. enabled, 1 Action (eventBased), 1 EventDescriptor, name")              << true     << validActionEventBased    << validActionEventBased    << validEventDescriptor2    << QVariantList()       << QVariantMap()            << RuleEngine::RuleErrorInvalidRuleFormat << false << "TestRule";
    QTest::newRow("invalid rule. enabled, 1 Action, 1 ExitAction (EventBased), name")                   << true     << validActionNoParams      << validActionEventBased    << validEventDescriptor2    << QVariantList()       << QVariantMap()            << RuleEngine::RuleErrorInvalidRuleFormat << false << "TestRule";

    // Rules with exit actions
    QTest::newRow("valid rule. enabled, 1 Action, 1 Exit Action,  1 StateEvaluator, name")              << true     << validActionNoParams      << validExitActionNoParams  << QVariantMap()            << QVariantList()       << validStateEvaluator      << RuleEngine::RuleErrorNoError << true << "TestRule";
    QTest::newRow("valid rule. disabled, 1 Action, 1 Exit Action, 1 StateEvaluator, name")              << false    << validActionNoParams      << validExitActionNoParams  << QVariantMap()            << QVariantList()       << validStateEvaluator      << RuleEngine::RuleErrorNoError << true << "TestRule";
    QTest::newRow("invalid rule. disabled, 1 Action, 1 invalid Exit Action, 1 StateEvaluator, name")    << false    << validActionNoParams      << invalidExitAction        << QVariantMap()            << QVariantList()       << validStateEvaluator      << RuleEngine::RuleErrorActionTypeNotFound << false << "TestRule";
    QTest::newRow("invalid rule. 1 Action, 1 Exit Action, 1 EventDescriptor, 1 StateEvaluator, name")   << true     << validActionNoParams      << validExitActionNoParams  << validEventDescriptor1    << QVariantList()       << validStateEvaluator      << RuleEngine::RuleErrorInvalidRuleFormat << false << "TestRule";
    QTest::newRow("invalid rule. 1 Action, 1 Exit Action, eventDescriptorList, 1 StateEvaluator, name") << true     << validActionNoParams      << validExitActionNoParams  << QVariantMap()            << eventDescriptorList  << validStateEvaluator      << RuleEngine::RuleErrorInvalidRuleFormat << false << "TestRule";

    // Rules without exit actions
    QTest::newRow("valid rule. enabled, 1 EventDescriptor, StateEvaluator, 1 Action, name")             << true     << validActionNoParams      << QVariantMap()            << validEventDescriptor1    << QVariantList()       << validStateEvaluator      << RuleEngine::RuleErrorNoError << true << "TestRule";
    QTest::newRow("valid rule. diabled, 1 EventDescriptor, StateEvaluator, 1 Action, name")             << false    << validActionNoParams      << QVariantMap()            << validEventDescriptor1    << QVariantList()       << validStateEvaluator      << RuleEngine::RuleErrorNoError << true << "TestRule";
    QTest::newRow("valid rule. 2 EventDescriptors, 1 Action, name")                                     << true     << validActionNoParams      << QVariantMap()            << QVariantMap()            << eventDescriptorList  << validStateEvaluator      << RuleEngine::RuleErrorNoError << true << "TestRule";
    QTest::newRow("invalid action")                                                                     << true     << invalidAction            << QVariantMap()            << validEventDescriptor1    << QVariantList()       << validStateEvaluator      << RuleEngine::RuleErrorActionTypeNotFound << false << "TestRule";
    QTest::newRow("invalid event descriptor")                                                           << true     << validActionNoParams      << QVariantMap()            << invalidEventDescriptor   << QVariantList()       << validStateEvaluator      << RuleEngine::RuleErrorEventTypeNotFound << false << "TestRule";
    QTest::newRow("invalid StateDescriptor")                                                            << true     << validActionNoParams      << QVariantMap()            << validEventDescriptor1    << QVariantList()       << invalidStateEvaluator    << RuleEngine::RuleErrorInvalidParameter << true << "TestRule";
}

void TestRules::addRemoveRules()
{
    QFETCH(bool, enabled);
    QFETCH(QVariantMap, action1);
    QFETCH(QVariantMap, exitAction1);
    QFETCH(QVariantMap, eventDescriptor);
    QFETCH(QVariantList, eventDescriptorList);
    QFETCH(QVariantMap, stateEvaluator);
    QFETCH(RuleEngine::RuleError, error);
    QFETCH(bool, jsonError);
    QFETCH(QString, name);

    QVariantMap params;
    params.insert("name", name);

    QVariantList actions;
    actions.append(action1);
    params.insert("actions", actions);

    if (!eventDescriptor.isEmpty()) {
        params.insert("eventDescriptors", QVariantList() << eventDescriptor);
    }
    if (!eventDescriptorList.isEmpty()) {
        params.insert("eventDescriptors", eventDescriptorList);
    }
    QVariantList exitActions;
    if (!exitAction1.isEmpty()) {
        exitActions.append(exitAction1);
        params.insert("exitActions", exitActions);
    }
    params.insert("stateEvaluator", stateEvaluator);
    if (!enabled) {
        params.insert("enabled", enabled);
    }
    QVariant response = injectAndWait("Rules.AddRule", params);
    if (!jsonError) {
        verifyRuleError(response, error);
    }

    RuleId newRuleId = RuleId(response.toMap().value("params").toMap().value("ruleId").toString());

    response = injectAndWait("Rules.GetRules");
    QVariantList rules = response.toMap().value("params").toMap().value("ruleDescriptions").toList();

    if (error != RuleEngine::RuleErrorNoError) {
        QVERIFY2(rules.count() == 0, "There should be no rules.");
        return;
    }

    QVERIFY2(rules.count() == 1, "There should be exactly one rule");
    QCOMPARE(RuleId(rules.first().toMap().value("id").toString()), newRuleId);

    params.clear();
    params.insert("ruleId", newRuleId);
    response = injectAndWait("Rules.GetRuleDetails", params);
    QVariantMap rule = response.toMap().value("params").toMap().value("rule").toMap();

    qDebug() << rule.value("name").toString();
    QVERIFY2(rule.value("enabled").toBool() == enabled, "Rule enabled state doesn't match");
    QVariantList eventDescriptors = rule.value("eventDescriptors").toList();
    if (!eventDescriptor.isEmpty()) {
        QVERIFY2(eventDescriptors.count() == 1, "There shoud be exactly one eventDescriptor");
        QVERIFY2(eventDescriptors.first().toMap() == eventDescriptor, "Event descriptor doesn't match");
    } else if (eventDescriptorList.isEmpty()){
        QVERIFY2(eventDescriptors.count() == eventDescriptorList.count(), QString("There shoud be exactly %1 eventDescriptor").arg(eventDescriptorList.count()).toLatin1().data());
        foreach (const QVariant &eventDescriptorVariant, eventDescriptorList) {
            bool found = false;
            foreach (const QVariant &replyEventDescriptorVariant, eventDescriptors) {
                if (eventDescriptorVariant.toMap().value("deviceId") == replyEventDescriptorVariant.toMap().value("deviceId") &&
                        eventDescriptorVariant.toMap().value("eventTypeId") == replyEventDescriptorVariant.toMap().value("eventTypeId")) {
                    found = true;
                    QVERIFY2(eventDescriptorVariant == replyEventDescriptorVariant, "Event descriptor doesn't match");
                }
            }
            QVERIFY2(found, "Missing event descriptor");
        }
    }

    QVariantList replyActions = rule.value("actions").toList();
    QVERIFY2(actions == replyActions, "Actions don't match");

    QVariantList replyExitActions = rule.value("exitActions").toList();
    QVERIFY2(exitActions == replyExitActions, "ExitActions don't match");

    params.clear();
    params.insert("ruleId", newRuleId);
    response = injectAndWait("Rules.RemoveRule", params);
    verifyRuleError(response);

    response = injectAndWait("Rules.GetRules");
    rules = response.toMap().value("params").toMap().value("ruleDescriptions").toList();
    QVERIFY2(rules.count() == 0, "There should be no rules.");
}

void TestRules::editRules_data()
{
    // RuleAction
    QVariantMap validActionNoParams;
    validActionNoParams.insert("actionTypeId", mockActionIdNoParams);
    validActionNoParams.insert("deviceId", m_mockDeviceId);
    validActionNoParams.insert("ruleActionParams", QVariantList());

    QVariantMap invalidAction;
    invalidAction.insert("actionTypeId", ActionTypeId());
    invalidAction.insert("deviceId", m_mockDeviceId);
    invalidAction.insert("ruleActionParams", QVariantList());

    // RuleExitAction
    QVariantMap validExitActionNoParams;
    validExitActionNoParams.insert("actionTypeId", mockActionIdNoParams);
    validExitActionNoParams.insert("deviceId", m_mockDeviceId);
    validExitActionNoParams.insert("ruleActionParams", QVariantList());

    QVariantMap invalidExitAction;
    invalidExitAction.insert("actionTypeId", ActionTypeId());
    invalidExitAction.insert("deviceId", m_mockDeviceId);
    invalidExitAction.insert("ruleActionParams", QVariantList());

    // StateDescriptor
    QVariantMap stateDescriptor;
    stateDescriptor.insert("stateTypeId", mockIntStateId);
    stateDescriptor.insert("deviceId", m_mockDeviceId);
    stateDescriptor.insert("operator", JsonTypes::valueOperatorToString(Types::ValueOperatorLess));
    stateDescriptor.insert("value", 20);

    // StateEvaluator
    QVariantMap validStateEvaluator;
    validStateEvaluator.insert("stateDescriptor", stateDescriptor);
    validStateEvaluator.insert("operator", JsonTypes::stateOperatorToString(Types::StateOperatorAnd));

    QVariantMap invalidStateEvaluator;
    stateDescriptor.remove("deviceId");
    invalidStateEvaluator.insert("stateDescriptor", stateDescriptor);

    // EventDescriptor
    QVariantMap validEventDescriptor1;
    validEventDescriptor1.insert("eventTypeId", mockEvent1Id);
    validEventDescriptor1.insert("deviceId", m_mockDeviceId);
    validEventDescriptor1.insert("paramDescriptors", QVariantList());

    QVariantMap validEventDescriptor2;
    validEventDescriptor2.insert("eventTypeId", mockEvent2Id);
    validEventDescriptor2.insert("deviceId", m_mockDeviceId);
    QVariantList params;
    QVariantMap param1;
    param1.insert("paramTypeId", mockParamIntParamTypeId);
    param1.insert("value", 3);
    param1.insert("operator", JsonTypes::valueOperatorToString(Types::ValueOperatorEquals));
    params.append(param1);
    validEventDescriptor2.insert("paramDescriptors", params);

    QVariantMap validEventDescriptor3;
    validEventDescriptor3.insert("eventTypeId", mockEvent2Id);
    validEventDescriptor3.insert("deviceId", m_mockDeviceId);
    validEventDescriptor3.insert("paramDescriptors", QVariantList());

    // EventDescriptorList
    QVariantList eventDescriptorList;
    eventDescriptorList.append(validEventDescriptor1);
    eventDescriptorList.append(validEventDescriptor2);

    QVariantMap invalidEventDescriptor;
    invalidEventDescriptor.insert("eventTypeId", mockEvent1Id);
    invalidEventDescriptor.insert("deviceId", DeviceId());
    invalidEventDescriptor.insert("paramDescriptors", QVariantList());

    // RuleAction event based
    QVariantMap validActionEventBased;
    validActionEventBased.insert("actionTypeId", mockActionIdWithParams);
    validActionEventBased.insert("deviceId", m_mockDeviceId);
    QVariantMap validActionEventBasedParam1;
    validActionEventBasedParam1.insert("paramTypeId", mockActionParam1ParamTypeId);
    validActionEventBasedParam1.insert("eventTypeId", mockEvent2Id);
    validActionEventBasedParam1.insert("eventParamTypeId", mockParamIntParamTypeId);
    QVariantMap validActionEventBasedParam2;
    validActionEventBasedParam2.insert("paramTypeId", mockActionParam2ParamTypeId);
    validActionEventBasedParam2.insert("value", false);
    validActionEventBased.insert("ruleActionParams", QVariantList() << validActionEventBasedParam1 << validActionEventBasedParam2);

    QVariantMap invalidActionEventBased;
    invalidActionEventBased.insert("actionTypeId", mockActionIdNoParams);
    invalidActionEventBased.insert("deviceId", m_mockDeviceId);
    validActionEventBasedParam1.insert("value", 10);
    invalidActionEventBased.insert("ruleActionParams", QVariantList() << validActionEventBasedParam1);

    QVariantMap invalidActionEventBased2;
    invalidActionEventBased2.insert("actionTypeId", mockActionIdWithParams);
    invalidActionEventBased2.insert("deviceId", m_mockDeviceId);
    QVariantMap invalidActionEventBasedParam2;
    invalidActionEventBasedParam2.insert("paramTypeId", mockActionParam1ParamTypeId);
    invalidActionEventBasedParam2.insert("eventTypeId", mockEvent1Id);
    invalidActionEventBasedParam2.insert("eventParamTypeId", "value");
    QVariantMap invalidActionEventBasedParam3;
    invalidActionEventBasedParam3.insert("paramTypeId", mockActionParam2ParamTypeId);
    invalidActionEventBasedParam3.insert("value", 2);
    invalidActionEventBased2.insert("ruleActionParams", QVariantList() << invalidActionEventBasedParam2 << invalidActionEventBasedParam3);

    QVariantMap invalidActionEventBased3;
    invalidActionEventBased3.insert("actionTypeId", mockActionIdWithParams);
    invalidActionEventBased3.insert("deviceId", m_mockDeviceId);
    QVariantMap invalidActionEventBasedParam4;
    invalidActionEventBasedParam4.insert("paramTypeId", mockActionParam1ParamTypeId);
    invalidActionEventBasedParam4.insert("eventTypeId", mockEvent1Id);
    invalidActionEventBasedParam4.insert("eventParamTypeId", mockParamIntParamTypeId);
    invalidActionEventBased3.insert("ruleActionParams", QVariantList() << invalidActionEventBasedParam4);

    QTest::addColumn<bool>("enabled");
    QTest::addColumn<QVariantMap>("action");
    QTest::addColumn<QVariantMap>("exitAction");
    QTest::addColumn<QVariantMap>("eventDescriptor");
    QTest::addColumn<QVariantList>("eventDescriptorList");
    QTest::addColumn<QVariantMap>("stateEvaluator");
    QTest::addColumn<RuleEngine::RuleError>("error");
    QTest::addColumn<QString>("name");

    // Rules with event based actions
    QTest::newRow("valid rule. enabled, 1 Action (eventBased), 1 EventDescriptor, name")                << true     << validActionEventBased    << QVariantMap()            << validEventDescriptor3    << QVariantList()       << QVariantMap()            << RuleEngine::RuleErrorNoError << "ActionEventRule1";
    QTest::newRow("invalid rule. enabled, 1 Action (eventBased), 1 EventDescriptor, name")              << true     << invalidActionEventBased2 << QVariantMap()            << validEventDescriptor3    << QVariantList()       << QVariantMap()            << RuleEngine::RuleErrorInvalidRuleActionParameter  << "TestRule";

    QTest::newRow("invalid rule. enabled, 1 Action (eventBased), types not matching, name")             << true     << invalidActionEventBased3 << QVariantMap()            << validEventDescriptor1    << QVariantList()       << QVariantMap()            << RuleEngine::RuleErrorTypesNotMatching << "TestRule";

    QTest::newRow("invalid rule. enabled, 1 Action (eventBased), 1 EventDescriptor, name")              << true     << invalidActionEventBased  << QVariantMap()            << validEventDescriptor2    << QVariantList()       << QVariantMap()            << RuleEngine::RuleErrorTypesNotMatching << "TestRule";
    QTest::newRow("invalid rule. enabled, 1 Action (eventBased), 1 StateEvaluator, name")               << true     << validActionEventBased    << QVariantMap()            << QVariantMap()            << QVariantList()       << validStateEvaluator      << RuleEngine::RuleErrorInvalidRuleActionParameter << "TestRule";
    QTest::newRow("invalid rule. enabled, 1 Action (eventBased), 1 EventDescriptor, name")              << true     << validActionEventBased    << validActionEventBased    << validEventDescriptor2    << QVariantList()       << QVariantMap()            << RuleEngine::RuleErrorInvalidRuleFormat << "TestRule";
    QTest::newRow("invalid rule. enabled, 1 Action, 1 ExitAction (EventBased), name")                   << true     << validActionNoParams      << validActionEventBased    << validEventDescriptor2    << QVariantList()       << QVariantMap()            << RuleEngine::RuleErrorInvalidRuleFormat << "TestRule";

    // Rules with exit actions
    QTest::newRow("valid rule. enabled, 1 Action, 1 Exit Action,  1 StateEvaluator, name")              << true     << validActionNoParams      << validExitActionNoParams  << QVariantMap()            << QVariantList()       << validStateEvaluator      << RuleEngine::RuleErrorNoError << "TestRule";
    QTest::newRow("valid rule. disabled, 1 Action, 1 Exit Action, 1 StateEvaluator, name")              << false    << validActionNoParams      << validExitActionNoParams  << QVariantMap()            << QVariantList()       << validStateEvaluator      << RuleEngine::RuleErrorNoError << "TestRule";
    QTest::newRow("invalid rule. 1 Action, 1 Exit Action, 1 EventDescriptor, 1 StateEvaluator, name")   << true     << validActionNoParams      << validExitActionNoParams  << validEventDescriptor1    << QVariantList()       << validStateEvaluator      << RuleEngine::RuleErrorInvalidRuleFormat << "TestRule";
    QTest::newRow("invalid rule. 1 Action, 1 Exit Action, eventDescriptorList, 1 StateEvaluator, name") << true     << validActionNoParams      << validExitActionNoParams  << QVariantMap()            << eventDescriptorList  << validStateEvaluator      << RuleEngine::RuleErrorInvalidRuleFormat << "TestRule";

    // Rules without exit actions
    QTest::newRow("valid rule. enabled, 1 EventDescriptor, StateEvaluator, 1 Action, name")             << true     << validActionNoParams      << QVariantMap()            << validEventDescriptor1    << QVariantList()       << validStateEvaluator      << RuleEngine::RuleErrorNoError << "TestRule";
    QTest::newRow("valid rule. diabled, 1 EventDescriptor, StateEvaluator, 1 Action, name")             << false    << validActionNoParams      << QVariantMap()            << validEventDescriptor1    << QVariantList()       << validStateEvaluator      << RuleEngine::RuleErrorNoError << "TestRule";
    QTest::newRow("valid rule. 2 EventDescriptors, 1 Action, name")                                     << true     << validActionNoParams      << QVariantMap()            << QVariantMap()            << eventDescriptorList  << validStateEvaluator      << RuleEngine::RuleErrorNoError << "TestRule";
}

void TestRules::editRules()
{
    QFETCH(bool, enabled);
    QFETCH(QVariantMap, action);
    QFETCH(QVariantMap, exitAction);
    QFETCH(QVariantMap, eventDescriptor);
    QFETCH(QVariantList, eventDescriptorList);
    QFETCH(QVariantMap, stateEvaluator);
    QFETCH(RuleEngine::RuleError, error);
    QFETCH(QString, name);

    // Add the rule we want to edit
    QVariantList eventParamDescriptors;
    QVariantMap eventDescriptor1;
    eventDescriptor1.insert("eventTypeId", mockEvent1Id);
    eventDescriptor1.insert("deviceId", m_mockDeviceId);
    eventDescriptor1.insert("paramDescriptors", QVariantList());
    QVariantMap eventDescriptor2;
    eventDescriptor2.insert("eventTypeId", mockEvent2Id);
    eventDescriptor2.insert("deviceId", m_mockDeviceId);
    eventDescriptor2.insert("paramDescriptors", QVariantList());
    QVariantMap eventParam1;
    eventParam1.insert("paramTypeId", mockParamIntParamTypeId);
    eventParam1.insert("value", 3);
    eventParam1.insert("operator", JsonTypes::valueOperatorToString(Types::ValueOperatorEquals));
    eventParamDescriptors.append(eventParam1);
    eventDescriptor2.insert("paramDescriptors", eventParamDescriptors);

    QVariantList eventDescriptorList1;
    eventDescriptorList1.append(eventDescriptor1);
    eventDescriptorList1.append(eventDescriptor2);

    QVariantMap stateEvaluator0;
    QVariantMap stateDescriptor1;
    stateDescriptor1.insert("deviceId", m_mockDeviceId);
    stateDescriptor1.insert("operator", JsonTypes::valueOperatorToString(Types::ValueOperatorEquals));
    stateDescriptor1.insert("stateTypeId", mockIntStateId);
    stateDescriptor1.insert("value", 1);
    QVariantMap stateDescriptor2;
    stateDescriptor2.insert("deviceId", m_mockDeviceId);
    stateDescriptor2.insert("operator", JsonTypes::valueOperatorToString(Types::ValueOperatorEquals));
    stateDescriptor2.insert("stateTypeId", mockBoolStateId);
    stateDescriptor2.insert("value", true);
    QVariantMap stateEvaluator1;
    stateEvaluator1.insert("stateDescriptor", stateDescriptor1);
    stateEvaluator1.insert("operator", JsonTypes::stateOperatorToString(Types::StateOperatorAnd));
    QVariantMap stateEvaluator2;
    stateEvaluator2.insert("stateDescriptor", stateDescriptor2);
    stateEvaluator2.insert("operator", JsonTypes::stateOperatorToString(Types::StateOperatorAnd));
    QVariantList childEvaluators;
    childEvaluators.append(stateEvaluator1);
    childEvaluators.append(stateEvaluator2);
    stateEvaluator0.insert("childEvaluators", childEvaluators);
    stateEvaluator0.insert("operator", JsonTypes::stateOperatorToString(Types::StateOperatorAnd));

    QVariantMap action1;
    action1.insert("actionTypeId", mockActionIdNoParams);
    action1.insert("deviceId", m_mockDeviceId);
    action1.insert("ruleActionParams", QVariantList());
    QVariantMap action2;
    action2.insert("actionTypeId", mockActionIdWithParams);
    qDebug() << "got action id" << mockActionIdWithParams;
    action2.insert("deviceId", m_mockDeviceId);
    QVariantList action2Params;
    QVariantMap action2Param1;
    action2Param1.insert("paramTypeId", mockActionParam1ParamTypeId);
    action2Param1.insert("value", 5);
    action2Params.append(action2Param1);
    QVariantMap action2Param2;
    action2Param2.insert("paramTypeId", mockActionParam2ParamTypeId);
    action2Param2.insert("value", true);
    action2Params.append(action2Param2);
    action2.insert("ruleActionParams", action2Params);

    // RuleAction event based
    QVariantMap validActionEventBased;
    validActionEventBased.insert("actionTypeId", mockActionIdWithParams);
    validActionEventBased.insert("deviceId", m_mockDeviceId);
    QVariantMap validActionEventBasedParam1;
    validActionEventBasedParam1.insert("paramTypeId", mockActionParam1ParamTypeId);
    validActionEventBasedParam1.insert("eventTypeId", mockEvent2Id);
    validActionEventBasedParam1.insert("eventParamTypeId", mockParamIntParamTypeId);
    QVariantMap validActionEventBasedParam2;
    validActionEventBasedParam2.insert("paramTypeId", mockActionParam2ParamTypeId);
    validActionEventBasedParam2.insert("value", false);
    validActionEventBased.insert("ruleActionParams", QVariantList() << validActionEventBasedParam1 << validActionEventBasedParam2);

    QVariantList validEventDescriptors3;
    QVariantMap validEventDescriptor3;
    validEventDescriptor3.insert("eventTypeId", mockEvent2Id);
    validEventDescriptor3.insert("deviceId", m_mockDeviceId);
    validEventDescriptor3.insert("paramDescriptors", QVariantList());
    validEventDescriptors3.append(validEventDescriptor3);

    QVariantMap params;
    QVariantList actions;
    actions.append(action1);
    actions.append(action2);
    params.insert("actions", actions);
    params.insert("eventDescriptors", eventDescriptorList1);
    params.insert("stateEvaluator", stateEvaluator0);
    params.insert("name", "TestRule");
    QVariant response = injectAndWait("Rules.AddRule", params);

    RuleId ruleId = RuleId(response.toMap().value("params").toMap().value("ruleId").toString());
    verifyRuleError(response);

    // enable notifications
    QCOMPARE(enableNotifications(), true);

    // now create the new rule and edit the original one
    params.clear();
    params.insert("ruleId", ruleId.toString());
    params.insert("name", name);

    if (!eventDescriptor.isEmpty()) {
        params.insert("eventDescriptors", QVariantList() << eventDescriptor);
    }
    if (!eventDescriptorList.isEmpty()) {
        params.insert("eventDescriptors", eventDescriptorList);
    }
    actions.clear();
    actions.append(action);
    params.insert("actions", actions);

    QVariantList exitActions;
    if (!exitAction.isEmpty()) {
        exitActions.append(exitAction);
        params.insert("exitActions", exitActions);
    }
    params.insert("stateEvaluator", stateEvaluator);
    if (!enabled) {
        params.insert("enabled", enabled);
    }

    // Setup connection to mock client
    QSignalSpy clientSpy(m_mockTcpServer, SIGNAL(outgoingData(QUuid,QByteArray)));
    response.clear();
    response = injectAndWait("Rules.EditRule", params);
    verifyRuleError(response, error);
    if (error == RuleEngine::RuleErrorNoError){
        clientSpy.wait(500);
        // We need to get exactly 2 replies. The actual reply and the Changed notification
        // Make sure there are no other notifications (e.g. RuleAdded or similar)
        QCOMPARE(clientSpy.count(), 2);
        QVariant notification = checkNotification(clientSpy, "Rules.RuleConfigurationChanged");
        QVERIFY2(notification != QVariant(), "not received \"Rules.RuleConfigurationChanged\" notification");

        // now check if the received rule matches the our new rule
        QVariantMap rule = response.toMap().value("params").toMap().value("rule").toMap();

        QVERIFY2(rule.value("enabled").toBool() == enabled, "Rule enabled state doesn't match");
        QVariantList eventDescriptors = rule.value("eventDescriptors").toList();
        if (!eventDescriptor.isEmpty()) {
            QVERIFY2(eventDescriptors.count() == 1, "There shoud be exactly one eventDescriptor");
            QVERIFY2(eventDescriptors.first().toMap() == eventDescriptor, "Event descriptor doesn't match");
        } else if (eventDescriptorList.isEmpty()){
            QVERIFY2(eventDescriptors.count() == eventDescriptorList.count(), QString("There shoud be exactly %1 eventDescriptor").arg(eventDescriptorList.count()).toLatin1().data());
            foreach (const QVariant &eventDescriptorVariant, eventDescriptorList) {
                bool found = false;
                foreach (const QVariant &replyEventDescriptorVariant, eventDescriptors) {
                    if (eventDescriptorVariant.toMap().value("deviceId") == replyEventDescriptorVariant.toMap().value("deviceId") &&
                            eventDescriptorVariant.toMap().value("eventTypeId") == replyEventDescriptorVariant.toMap().value("eventTypeId")) {
                        found = true;
                        QVERIFY2(eventDescriptorVariant == replyEventDescriptorVariant, "Event descriptor doesn't match");
                    }
                }
                QVERIFY2(found, "Missing event descriptor");
            }
        }

        QVariantList replyActions = rule.value("actions").toList();
        QVERIFY2(actions == replyActions, "Actions don't match");

        QVariantList replyExitActions = rule.value("exitActions").toList();
        QVERIFY2(exitActions == replyExitActions, "ExitActions don't match");
    }

    // Remove the rule
    params.clear();
    params.insert("ruleId", ruleId);
    response = injectAndWait("Rules.RemoveRule", params);
    verifyRuleError(response);

    // check if removed
    response = injectAndWait("Rules.GetRules");
    QVariantList rules = response.toMap().value("params").toMap().value("rules").toList();
    QVERIFY2(rules.count() == 0, "There should be no rules.");
}

void TestRules::executeRuleActions_data()
{
    QTest::addColumn<QVariantMap>("params");
    QTest::addColumn<RuleEngine::RuleError>("ruleError");

    QTest::newRow("executable rule, enabled") << validIntStateBasedRule("Executeable", true, true).toMap() << RuleEngine::RuleErrorNoError;
    QTest::newRow("executable rule, disabled") << validIntStateBasedRule("Executeable", true, false).toMap() << RuleEngine::RuleErrorNoError;
    QTest::newRow("not executable rule, enabled") << validIntStateBasedRule("Not Executable", false, true).toMap() << RuleEngine::RuleErrorNotExecutable;
    QTest::newRow("not executable rule, disabled") << validIntStateBasedRule("Not Executable", false, false).toMap() << RuleEngine::RuleErrorNotExecutable;
}

void TestRules::executeRuleActions()
{
    QFETCH(QVariantMap, params);
    QFETCH(RuleEngine::RuleError, ruleError);

    // ADD rule
    QVariant response = injectAndWait("Rules.AddRule", params);
    verifyRuleError(response);

    RuleId ruleId = RuleId(response.toMap().value("params").toMap().value("ruleId").toString());
    QVERIFY(!ruleId.isNull());

    cleanupMockHistory();
    QTest::qWait(200);

    // EEXCUTE action invalid ruleId
    QVariantMap executeParams;
    executeParams.insert("ruleId", QUuid::createUuid().toString());
    response = injectAndWait("Rules.ExecuteActions", executeParams);
    verifyRuleError(response, RuleEngine::RuleErrorRuleNotFound);

    // EXECUTE actions
    qDebug() << "Execute rule actions";
    executeParams.clear();
    executeParams.insert("ruleId", ruleId.toString());
    response = injectAndWait("Rules.ExecuteActions", executeParams);
    verifyRuleError(response, ruleError);

    // give the ruleeingine time to execute the actions
    QTest::qWait(1000);

    if (ruleError == RuleEngine::RuleErrorNoError) {
        verifyRuleExecuted(mockActionIdWithParams);
    } else {
        verifyRuleNotExecuted();
    }

    cleanupMockHistory();
    QTest::qWait(200);

    // EXECUTE exit actions invalid ruleId
    executeParams.clear();
    executeParams.insert("ruleId", QUuid::createUuid().toString());
    response = injectAndWait("Rules.ExecuteExitActions", executeParams);
    verifyRuleError(response, RuleEngine::RuleErrorRuleNotFound);

    // EXECUTE exit actions
    qDebug() << "Execute rule exit actions";
    executeParams.clear();
    executeParams.insert("ruleId", ruleId.toString());
    response = injectAndWait("Rules.ExecuteExitActions", executeParams);
    verifyRuleError(response, ruleError);

    // give the ruleeingine time to execute the actions
    QTest::qWait(1000);

    if (ruleError == RuleEngine::RuleErrorNoError) {
        verifyRuleExecuted(mockActionIdNoParams);
    } else {
        verifyRuleNotExecuted();
    }

    cleanupMockHistory();

    // REMOVE rule
    QVariantMap removeParams;
    removeParams.insert("ruleId", ruleId);
    response = injectAndWait("Rules.RemoveRule", removeParams);
    verifyRuleError(response);
}

void TestRules::findRule()
{
    // ADD rule
    QVariantMap params = validIntStateBasedRule("Executeable", true, true).toMap();
    QVariant response = injectAndWait("Rules.AddRule", params);
    verifyRuleError(response);

    RuleId ruleId = RuleId(response.toMap().value("params").toMap().value("ruleId").toString());
    QVERIFY(!ruleId.isNull());

    params.clear();
    params.insert("deviceId", m_mockDeviceId);
    response = injectAndWait("Rules.FindRules", params);

    QCOMPARE(response.toMap().value("params").toMap().value("ruleIds").toList().count(), 1);
    QCOMPARE(response.toMap().value("params").toMap().value("ruleIds").toList().first().toString(), ruleId.toString());

    // REMOVE rule
    QVariantMap removeParams;
    removeParams.insert("ruleId", ruleId);
    response = injectAndWait("Rules.RemoveRule", removeParams);
    verifyRuleError(response);

}

void TestRules::removeInvalidRule()
{
    QVariantMap params;
    params.insert("ruleId", RuleId::createRuleId());
    QVariant response = injectAndWait("Rules.RemoveRule", params);
    verifyRuleError(response, RuleEngine::RuleErrorRuleNotFound);
}

void TestRules::loadStoreConfig()
{
    QVariantMap eventDescriptor1;
    eventDescriptor1.insert("eventTypeId", mockEvent1Id);
    eventDescriptor1.insert("deviceId", m_mockDeviceId);
    eventDescriptor1.insert("paramDescriptors", QVariantList());

    QVariantMap eventDescriptor2;
    eventDescriptor2.insert("eventTypeId", mockEvent2Id);
    eventDescriptor2.insert("deviceId", m_mockDeviceId);
    eventDescriptor2.insert("paramDescriptors", QVariantList());
    QVariantList eventParamDescriptors;
    QVariantMap eventParam1;
    eventParam1.insert("paramTypeId", mockParamIntParamTypeId);
    eventParam1.insert("value", 3);
    eventParam1.insert("operator", JsonTypes::valueOperatorToString(Types::ValueOperatorEquals));
    eventParamDescriptors.append(eventParam1);
    eventDescriptor2.insert("paramDescriptors", eventParamDescriptors);

    QVariantList eventDescriptorList;
    eventDescriptorList.append(eventDescriptor1);
    eventDescriptorList.append(eventDescriptor2);

    QVariantMap stateEvaluator1;
    QVariantList childEvaluators;

    QVariantMap stateDescriptor2;
    stateDescriptor2.insert("deviceId", m_mockDeviceId);
    stateDescriptor2.insert("operator", JsonTypes::valueOperatorToString(Types::ValueOperatorEquals));
    stateDescriptor2.insert("stateTypeId", mockIntStateId);
    stateDescriptor2.insert("value", 1);
    QVariantMap stateEvaluator2;
    stateEvaluator2.insert("stateDescriptor", stateDescriptor2);
    stateEvaluator2.insert("operator", JsonTypes::stateOperatorToString(Types::StateOperatorAnd));

    QVariantMap stateDescriptor3;
    stateDescriptor3.insert("deviceId", m_mockDeviceId);
    stateDescriptor3.insert("operator", JsonTypes::valueOperatorToString(Types::ValueOperatorEquals));
    stateDescriptor3.insert("stateTypeId", mockBoolStateId);
    stateDescriptor3.insert("value", true);

    QVariantMap stateEvaluator3;
    stateEvaluator3.insert("stateDescriptor", stateDescriptor3);
    stateEvaluator3.insert("operator", JsonTypes::stateOperatorToString(Types::StateOperatorAnd));

    childEvaluators.append(stateEvaluator2);
    childEvaluators.append(stateEvaluator3);
    stateEvaluator1.insert("childEvaluators", childEvaluators);
    stateEvaluator1.insert("operator", JsonTypes::stateOperatorToString(Types::StateOperatorAnd));

    QVariantMap action1;
    action1.insert("actionTypeId", mockActionIdNoParams);
    action1.insert("deviceId", m_mockDeviceId);
    action1.insert("ruleActionParams", QVariantList());

    QVariantMap action2;
    action2.insert("actionTypeId", mockActionIdWithParams);
    qDebug() << "got action id" << mockActionIdWithParams;
    action2.insert("deviceId", m_mockDeviceId);
    QVariantList action2Params;
    QVariantMap action2Param1;
    action2Param1.insert("paramTypeId", mockActionParam1ParamTypeId);
    action2Param1.insert("value", 5);
    action2Params.append(action2Param1);
    QVariantMap action2Param2;
    action2Param2.insert("paramTypeId", mockActionParam2ParamTypeId);
    action2Param2.insert("value", true);
    action2Params.append(action2Param2);
    action2.insert("ruleActionParams", action2Params);

    // RuleAction event based
    QVariantMap validActionEventBased;
    validActionEventBased.insert("actionTypeId", mockActionIdWithParams);
    validActionEventBased.insert("deviceId", m_mockDeviceId);
    QVariantMap validActionEventBasedParam1;
    validActionEventBasedParam1.insert("paramTypeId", mockActionParam1ParamTypeId);
    validActionEventBasedParam1.insert("eventTypeId", mockEvent2Id);
    validActionEventBasedParam1.insert("eventParamTypeId", mockParamIntParamTypeId);
    QVariantMap validActionEventBasedParam2;
    validActionEventBasedParam2.insert("paramTypeId", mockActionParam2ParamTypeId);
    validActionEventBasedParam2.insert("value", false);
    validActionEventBased.insert("ruleActionParams", QVariantList() << validActionEventBasedParam1 << validActionEventBasedParam2);

    QVariantList validEventDescriptors3;
    QVariantMap validEventDescriptor3;
    validEventDescriptor3.insert("eventTypeId", mockEvent2Id);
    validEventDescriptor3.insert("deviceId", m_mockDeviceId);
    validEventDescriptor3.insert("paramDescriptors", QVariantList());
    validEventDescriptors3.append(validEventDescriptor3);

    // rule 1
    QVariantMap params;
    QVariantList actions;
    actions.append(action1);
    actions.append(action2);
    params.insert("actions", actions);
    params.insert("eventDescriptors", eventDescriptorList);
    params.insert("stateEvaluator", stateEvaluator1);
    params.insert("name", "TestRule");
    QVariant response = injectAndWait("Rules.AddRule", params);

    RuleId newRuleId = RuleId(response.toMap().value("params").toMap().value("ruleId").toString());
    verifyRuleError(response);

    // rule 2
    QVariantMap params2;
    QVariantList actions2;
    actions2.append(action1);
    QVariantList exitActions2;
    exitActions2.append(action2);
    params2.insert("actions", actions2);
    params2.insert("exitActions", exitActions2);
    params2.insert("stateEvaluator", stateEvaluator1);
    params2.insert("name", "TestRule2");
    QVariant response2 = injectAndWait("Rules.AddRule", params2);

    RuleId newRuleId2 = RuleId(response2.toMap().value("params").toMap().value("ruleId").toString());
    verifyRuleError(response2);

    // rule 3
    QVariantMap params3;
    QVariantList actions3;
    actions3.append(validActionEventBased);
    params3.insert("actions", actions3);
    params3.insert("eventDescriptors", validEventDescriptors3);
    params3.insert("name", "TestRule3");
    QVariant response3 = injectAndWait("Rules.AddRule", params3);

    RuleId newRuleId3 = RuleId(response3.toMap().value("params").toMap().value("ruleId").toString());
    verifyRuleError(response3);

    response = injectAndWait("Rules.GetRules");
    QVariantList rules = response.toMap().value("params").toMap().value("ruleDescriptions").toList();
    qDebug() << response;

    restartServer();

    response = injectAndWait("Rules.GetRules");
    rules = response.toMap().value("params").toMap().value("ruleDescriptions").toList();

    QVERIFY2(rules.count() == 3, "There should be exactly three rule.");

    QStringList idList;
    foreach (const QVariant &ruleDescription, rules) {
        idList.append(ruleDescription.toMap().value("id").toString());
    }

    QVERIFY2(idList.contains(newRuleId.toString()), "Rule 1 should be in ruleIds list.");
    QVERIFY2(idList.contains(newRuleId2.toString()), "Rule 2 should be in ruleIds list.");
    QVERIFY2(idList.contains(newRuleId3.toString()), "Rule 3 should be in ruleIds list.");

    // Rule 1
    params.clear();
    params.insert("ruleId", newRuleId);
    response.clear();
    response = injectAndWait("Rules.GetRuleDetails", params);

    QVariantMap rule1 = response.toMap().value("params").toMap().value("rule").toMap();

    QVariantList eventDescriptors = rule1.value("eventDescriptors").toList();
    QVERIFY2(eventDescriptors.count() == 2, "There shoud be exactly 2 eventDescriptors");
    foreach (const QVariant &expectedEventDescriptorVariant, eventDescriptorList) {
        bool found = false;
        foreach (const QVariant &replyEventDescriptorVariant, eventDescriptors) {
            if (expectedEventDescriptorVariant.toMap().value("eventTypeId") == replyEventDescriptorVariant.toMap().value("eventTypeId") &&
                    expectedEventDescriptorVariant.toMap().value("deviceId") == replyEventDescriptorVariant.toMap().value("deviceId")) {
                found = true;
                qDebug() << endl << replyEventDescriptorVariant << endl << expectedEventDescriptorVariant;
                QVERIFY2(replyEventDescriptorVariant == expectedEventDescriptorVariant, "EventDescriptor doesn't match");
            }
        }
        QVERIFY2(found, "missing eventdescriptor");
    }

    qDebug() << endl << rule1;

    QVERIFY2(rule1.value("name").toString() == "TestRule", "Loaded wrong name for rule");
    QVariantMap replyStateEvaluator= rule1.value("stateEvaluator").toMap();
    QVariantList replyChildEvaluators = replyStateEvaluator.value("childEvaluators").toList();
    QVERIFY2(replyChildEvaluators.count() == 2, "There shoud be exactly 2 childEvaluators");
    QVERIFY2(replyStateEvaluator.value("operator") == "StateOperatorAnd", "There should be the AND operator.");

    foreach (const QVariant &childEvaluator, replyChildEvaluators) {
        QVERIFY2(childEvaluator.toMap().contains("stateDescriptor"), "StateDescriptor missing in StateEvaluator");
        QVariantMap stateDescriptor = childEvaluator.toMap().value("stateDescriptor").toMap();
        QVERIFY2(stateDescriptor.value("deviceId") == m_mockDeviceId, "DeviceId of stateDescriptor does not match");
        QVERIFY2(stateDescriptor.value("stateTypeId") == mockIntStateId || stateDescriptor.value("stateTypeId") == mockBoolStateId, "StateTypeId of stateDescriptor doesn't match");
    }

    QVariantList replyActions = rule1.value("actions").toList();
    foreach (const QVariant &actionVariant, actions) {
        bool found = false;
        foreach (const QVariant &replyActionVariant, replyActions) {
            if (actionVariant.toMap().value("actionTypeId") == replyActionVariant.toMap().value("actionTypeId") &&
                    actionVariant.toMap().value("deviceId") == replyActionVariant.toMap().value("deviceId")) {
                found = true;
                // Check rule action params
                QVariantList actionParams = actionVariant.toMap().value("ruleActionParams").toList();
                QVariantList replyActionParams = replyActionVariant.toMap().value("ruleActionParams").toList();
                QVERIFY2(actionParams.count() == replyActionParams.count(), "Not the same list size of action params");
                foreach (const QVariant &ruleParam, actionParams) {
                    QVERIFY(replyActionParams.contains(ruleParam));
                }
            }
        }
        QVERIFY2(found, "Action not found after loading from config.");
    }

    // Rule 2
    params.clear();
    params.insert("ruleId", newRuleId2);
    response.clear();
    response = injectAndWait("Rules.GetRuleDetails", params);

    QVariantMap rule2 = response.toMap().value("params").toMap().value("rule").toMap();

    QVERIFY2(rule2.value("name").toString() == "TestRule2", "Loaded wrong name for rule");
    QVariantMap replyStateEvaluator2= rule2.value("stateEvaluator").toMap();
    QVariantList replyChildEvaluators2 = replyStateEvaluator.value("childEvaluators").toList();
    QVERIFY2(replyStateEvaluator2.value("operator") == "StateOperatorAnd", "There should be the AND operator.");
    QVERIFY2(replyChildEvaluators2.count() == 2, "There shoud be exactly 2 childEvaluators");

    foreach (const QVariant &childEvaluator, replyChildEvaluators2) {
        QVERIFY2(childEvaluator.toMap().contains("stateDescriptor"), "StateDescriptor missing in StateEvaluator");
        QVariantMap stateDescriptor = childEvaluator.toMap().value("stateDescriptor").toMap();
        QVERIFY2(stateDescriptor.value("deviceId") == m_mockDeviceId, "DeviceId of stateDescriptor does not match");
        QVERIFY2(stateDescriptor.value("stateTypeId") == mockIntStateId || stateDescriptor.value("stateTypeId") == mockBoolStateId, "StateTypeId of stateDescriptor doesn't match");
    }

    QVariantList replyActions2 = rule2.value("actions").toList();
    QVERIFY2(replyActions2.count() == 1, "Rule 2 should have exactly 1 action");
    foreach (const QVariant &actionVariant, actions2) {
        bool found = false;
        foreach (const QVariant &replyActionVariant, replyActions2) {
            if (actionVariant.toMap().value("actionTypeId") == replyActionVariant.toMap().value("actionTypeId") &&
                    actionVariant.toMap().value("deviceId") == replyActionVariant.toMap().value("deviceId")) {
                found = true;
                // Check rule action params
                QVariantList actionParams = actionVariant.toMap().value("ruleActionParams").toList();
                QVariantList replyActionParams = replyActionVariant.toMap().value("ruleActionParams").toList();
                QVERIFY2(actionParams.count() == replyActionParams.count(), "Not the same list size of action params");
                foreach (const QVariant &ruleParam, actionParams) {
                    QVERIFY(replyActionParams.contains(ruleParam));
                }
            }
        }
        QVERIFY2(found, "Action not found after loading from config.");
    }

    QVariantList replyExitActions2 = rule2.value("exitActions").toList();
    QVERIFY2(replyExitActions2.count() == 1, "Rule 2 should have exactly 1 exitAction");
    foreach (const QVariant &exitActionVariant, replyExitActions2) {
        bool found = false;
        foreach (const QVariant &replyActionVariant, replyExitActions2) {
            if (exitActionVariant.toMap().value("actionTypeId") == replyActionVariant.toMap().value("actionTypeId") &&
                    exitActionVariant.toMap().value("deviceId") == replyActionVariant.toMap().value("deviceId")) {
                found = true;
                // Check rule action params
                QVariantList actionParams = exitActionVariant.toMap().value("ruleActionParams").toList();
                QVariantList replyActionParams = replyActionVariant.toMap().value("ruleActionParams").toList();
                QVERIFY2(actionParams.count() == replyActionParams.count(), "Not the same list size of action params");
                foreach (const QVariant &ruleParam, actionParams) {
                    QVERIFY(replyActionParams.contains(ruleParam));
                }
            }
        }
        QVERIFY2(found, "Exit Action not found after loading from config.");
    }

    // Rule 3
    params.clear();
    params.insert("ruleId", newRuleId3);
    response.clear();
    response = injectAndWait("Rules.GetRuleDetails", params);

    QVariantMap rule3 = response.toMap().value("params").toMap().value("rule").toMap();

    qDebug() << rule3;

    QVariantList eventDescriptors3 = rule3.value("eventDescriptors").toList();
    QVERIFY2(eventDescriptors3.count() == 1, "There shoud be exactly 1 eventDescriptor");
    QVariantMap eventDescriptor = eventDescriptors3.first().toMap();
    QVERIFY2(eventDescriptor.value("eventTypeId").toString() == mockEvent2Id.toString(), "Loaded the wrong eventTypeId in rule 3");
    QVERIFY2(eventDescriptor.value("deviceId").toString() == m_mockDeviceId.toString(), "Loaded the wrong deviceId from eventDescriptor in rule 3");

    QVariantList replyExitActions3 = rule3.value("exitActions").toList();
    QVERIFY2(replyExitActions3.isEmpty(), "Rule 3 should not have any exitAction");

    QVariantList replyActions3 = rule3.value("actions").toList();
    QVERIFY2(replyActions3.count() == 1, "Rule 3 should have exactly 1 action");
    foreach (const QVariant &actionVariant, actions3) {
        bool found = false;
        foreach (const QVariant &replyActionVariant, replyActions3) {
            if (actionVariant.toMap().value("actionTypeId") == replyActionVariant.toMap().value("actionTypeId") &&
                    actionVariant.toMap().value("deviceId") == replyActionVariant.toMap().value("deviceId")) {
                found = true;
                // Check rule action params
                QVariantList actionParams = actionVariant.toMap().value("ruleActionParams").toList();
                QVariantList replyActionParams = replyActionVariant.toMap().value("ruleActionParams").toList();
                QVERIFY2(actionParams.count() == replyActionParams.count(), "Not the same list size of action params");
                foreach (const QVariant &ruleParam, actionParams) {
                    QVERIFY(replyActionParams.contains(ruleParam));
                }
            }
        }
        QVERIFY2(found, "Action not found after loading from config.");
    }

    // Remove Rule1
    params.clear();
    params.insert("ruleId", newRuleId);
    response = injectAndWait("Rules.RemoveRule", params);
    verifyRuleError(response);

    // Remove Rule2
    params2.clear();
    params2.insert("ruleId", newRuleId2);
    response = injectAndWait("Rules.RemoveRule", params2);
    verifyRuleError(response);

    // Remove Rule2
    params3.clear();
    params3.insert("ruleId", newRuleId3);
    response = injectAndWait("Rules.RemoveRule", params3);
    verifyRuleError(response);

    restartServer();

    response = injectAndWait("Rules.GetRules");
    rules = response.toMap().value("params").toMap().value("rules").toList();
    QVERIFY2(rules.count() == 0, "There should be no rules.");
}

void TestRules::evaluateEvent()
{
    // Add a rule
    QVariantMap addRuleParams;
    addRuleParams.insert("name", "TestRule");

    QVariantList events;
    QVariantMap event1;
    event1.insert("eventTypeId", mockEvent1Id);
    event1.insert("deviceId", m_mockDeviceId);
    events.append(event1);
    addRuleParams.insert("eventDescriptors", events);

    QVariantList actions;
    QVariantMap action;
    action.insert("actionTypeId", mockActionIdNoParams);
    action.insert("deviceId", m_mockDeviceId);
    actions.append(action);
    addRuleParams.insert("actions", actions);
    QVariant response = injectAndWait("Rules.AddRule", addRuleParams);
    verifyRuleError(response);

    // Trigger an event
    QNetworkAccessManager nam;
    QSignalSpy spy(&nam, SIGNAL(finished(QNetworkReply*)));

    // trigger event in mock device
    QNetworkRequest request(QUrl(QString("http://localhost:%1/generateevent?eventtypeid=%2").arg(m_mockDevice1Port).arg(mockEvent1Id.toString())));
    QNetworkReply *reply = nam.get(request);
    spy.wait();
    QCOMPARE(spy.count(), 1);
    reply->deleteLater();

    verifyRuleExecuted(mockActionIdNoParams);
}

void TestRules::evaluateEventParams()
{
    // Init bool state to  true
    QNetworkAccessManager nam;
    QSignalSpy spy(&nam, SIGNAL(finished(QNetworkReply*)));
    QNetworkRequest request(QUrl(QString("http://localhost:%1/setstate?%2=%3").arg(m_mockDevice1Port).arg(mockBoolStateId.toString()).arg("true")));
    QNetworkReply *reply = nam.get(request);
    spy.wait();
    QCOMPARE(spy.count(), 1);
    reply->deleteLater();


    // Add a rule
    QVariantMap addRuleParams;
    addRuleParams.insert("name", "TestRule");

    QVariantList params;
    QVariantMap boolParam;
    boolParam.insert("paramTypeId", mockBoolStateId);
    boolParam.insert("operator", "ValueOperatorEquals");
    boolParam.insert("value", true);
    params.append(boolParam);

    QVariantMap event1;
    event1.insert("eventTypeId", mockBoolStateId);
    event1.insert("deviceId", m_mockDeviceId);
    event1.insert("paramDescriptors", params);

    QVariantList events;
    events.append(event1);
    addRuleParams.insert("eventDescriptors", events);

    QVariantList actions;
    QVariantMap action;
    action.insert("actionTypeId", mockActionIdNoParams);
    action.insert("deviceId", m_mockDeviceId);
    actions.append(action);
    addRuleParams.insert("actions", actions);
    QVariant response = injectAndWait("Rules.AddRule", addRuleParams);
    verifyRuleError(response);


    // Trigger a non matching param
    spy.clear();
    request = QNetworkRequest(QUrl(QString("http://localhost:%1/setstate?%2=%3").arg(m_mockDevice1Port).arg(mockBoolStateId.toString()).arg("false")));
    reply = nam.get(request);
    spy.wait();
    QCOMPARE(spy.count(), 1);
    reply->deleteLater();

    verifyRuleNotExecuted();

    // Trigger a matching param
    spy.clear();
    request = QNetworkRequest(QUrl(QString("http://localhost:%1/setstate?%2=%3").arg(m_mockDevice1Port).arg(mockBoolStateId.toString()).arg("true")));
    reply = nam.get(request);
    spy.wait();
    QCOMPARE(spy.count(), 1);
    reply->deleteLater();

    verifyRuleExecuted(mockActionIdNoParams);

    // Reset back to false to not mess with other tests
    spy.clear();
    request = QNetworkRequest(QUrl(QString("http://localhost:%1/setstate?%2=%3").arg(m_mockDevice1Port).arg(mockBoolStateId.toString()).arg("false")));
    reply = nam.get(request);
    spy.wait();
    QCOMPARE(spy.count(), 1);
    reply->deleteLater();
}


void TestRules::testStateChange() {
    // Add a rule
    QVariantMap addRuleParams;
    QVariantMap stateEvaluator;
    QVariantMap stateDescriptor;
    stateDescriptor.insert("deviceId", m_mockDeviceId);
    stateDescriptor.insert("operator", JsonTypes::valueOperatorToString(Types::ValueOperatorGreaterOrEqual));
    stateDescriptor.insert("stateTypeId", mockIntStateId);
    stateDescriptor.insert("value", 42);
    stateEvaluator.insert("stateDescriptor", stateDescriptor);
    addRuleParams.insert("stateEvaluator", stateEvaluator);
    addRuleParams.insert("name", "TestRule");

    QVariantList actions;
    QVariantMap action;
    action.insert("actionTypeId", mockActionIdNoParams);
    action.insert("deviceId", m_mockDeviceId);
    actions.append(action);
    addRuleParams.insert("actions", actions);
    QVariant response = injectAndWait("Rules.AddRule", addRuleParams);
    verifyRuleError(response);


    // Change the state
    QNetworkAccessManager nam;
    QSignalSpy spy(&nam, SIGNAL(finished(QNetworkReply*)));

    // state state to 42
    qDebug() << "setting mock int state to 42";
    QNetworkRequest request(QUrl(QString("http://localhost:%1/setstate?%2=%3").arg(m_mockDevice1Port).arg(mockIntStateId.toString()).arg(42)));
    QNetworkReply *reply = nam.get(request);
    spy.wait();
    QCOMPARE(spy.count(), 1);
    reply->deleteLater();

    verifyRuleExecuted(mockActionIdNoParams);

    cleanupMockHistory();

    // set state to 45
    qDebug() << "setting mock int state to 45";
    spy.clear();
    request.setUrl(QUrl(QString("http://localhost:%1/setstate?%2=%3").arg(m_mockDevice1Port).arg(mockIntStateId.toString()).arg(45)));
    reply = nam.get(request);
    spy.wait();
    QCOMPARE(spy.count(), 1);
    reply->deleteLater();

    verifyRuleNotExecuted();

    cleanupMockHistory();

    // set state to 30
    qDebug() << "setting mock int state to 30";
    spy.clear();
    request.setUrl(QUrl(QString("http://localhost:%1/setstate?%2=%3").arg(m_mockDevice1Port).arg(mockIntStateId.toString()).arg(30)));
    reply = nam.get(request);
    spy.wait();
    QCOMPARE(spy.count(), 1);
    reply->deleteLater();

    verifyRuleNotExecuted();

    cleanupMockHistory();

    // set state to 100
    qDebug() << "setting mock int state to 100";
    spy.clear();
    request.setUrl(QUrl(QString("http://localhost:%1/setstate?%2=%3").arg(m_mockDevice1Port).arg(mockIntStateId.toString()).arg(100)));
    reply = nam.get(request);
    spy.wait();
    QCOMPARE(spy.count(), 1);

    verifyRuleExecuted(mockActionIdNoParams);
    reply->deleteLater();
}

void TestRules::testStateEvaluator_data()
{
    QTest::addColumn<DeviceId>("deviceId");
    QTest::addColumn<StateTypeId>("stateTypeId");
    QTest::addColumn<QVariant>("value");
    QTest::addColumn<Types::ValueOperator>("operatorType");
    QTest::addColumn<bool>("shouldMatch");

    QTest::newRow("invalid stateId") << m_mockDeviceId << StateTypeId::createStateTypeId() << QVariant(10) << Types::ValueOperatorEquals << false;
    QTest::newRow("invalid deviceId") << DeviceId::createDeviceId() << mockIntStateId << QVariant(10) << Types::ValueOperatorEquals << false;

    QTest::newRow("equals, not matching") << m_mockDeviceId << mockIntStateId << QVariant(7777) << Types::ValueOperatorEquals << false;
    QTest::newRow("equals, matching") << m_mockDeviceId << mockIntStateId << QVariant(10) << Types::ValueOperatorEquals << true;

    QTest::newRow("not equal, not matching") << m_mockDeviceId << mockIntStateId << QVariant(10) << Types::ValueOperatorNotEquals << false;
    QTest::newRow("not equal, matching") << m_mockDeviceId << mockIntStateId << QVariant(7777) << Types::ValueOperatorNotEquals << true;

    QTest::newRow("Greater, not matching") << m_mockDeviceId << mockIntStateId << QVariant(7777) << Types::ValueOperatorGreater << false;
    QTest::newRow("Greater, matching") << m_mockDeviceId << mockIntStateId << QVariant(2) << Types::ValueOperatorGreater << true;
    QTest::newRow("GreaterOrEqual, not matching") << m_mockDeviceId << mockIntStateId << QVariant(7777) << Types::ValueOperatorGreaterOrEqual << false;
    QTest::newRow("GreaterOrEqual, matching (greater)") << m_mockDeviceId << mockIntStateId << QVariant(2) << Types::ValueOperatorGreaterOrEqual << true;
    QTest::newRow("GreaterOrEqual, matching (equals)") << m_mockDeviceId << mockIntStateId << QVariant(10) << Types::ValueOperatorGreaterOrEqual << true;

    QTest::newRow("Less, not matching") << m_mockDeviceId << mockIntStateId << QVariant(2) << Types::ValueOperatorLess << false;
    QTest::newRow("Less, matching") << m_mockDeviceId << mockIntStateId << QVariant(7777) << Types::ValueOperatorLess << true;
    QTest::newRow("LessOrEqual, not matching") << m_mockDeviceId << mockIntStateId << QVariant(2) << Types::ValueOperatorLessOrEqual << false;
    QTest::newRow("LessOrEqual, matching (less)") << m_mockDeviceId << mockIntStateId << QVariant(777) << Types::ValueOperatorLessOrEqual << true;
    QTest::newRow("LessOrEqual, matching (equals)") << m_mockDeviceId << mockIntStateId << QVariant(10) << Types::ValueOperatorLessOrEqual << true;
}

void TestRules::testStateEvaluator()
{
    QFETCH(DeviceId, deviceId);
    QFETCH(StateTypeId, stateTypeId);
    QFETCH(QVariant, value);
    QFETCH(Types::ValueOperator, operatorType);
    QFETCH(bool, shouldMatch);

    StateDescriptor descriptor(stateTypeId, deviceId, value, operatorType);
    StateEvaluator evaluator(descriptor);

    QVERIFY2(evaluator.evaluate() == shouldMatch, shouldMatch ? "State should match" : "State shouldn't match");
}

void TestRules::testStateEvaluator2_data()
{
    QTest::addColumn<int>("intValue");
    QTest::addColumn<Types::ValueOperator>("intOperator");

    QTest::addColumn<bool>("boolValue");
    QTest::addColumn<Types::ValueOperator>("boolOperator");

    QTest::addColumn<Types::StateOperator>("stateOperator");

    QTest::addColumn<bool>("shouldMatch");

    QTest::newRow("Y: 10 && false") << 10 << Types::ValueOperatorEquals << false << Types::ValueOperatorEquals << Types::StateOperatorAnd << true;
    QTest::newRow("N: 10 && true") << 10 << Types::ValueOperatorEquals << true << Types::ValueOperatorEquals << Types::StateOperatorAnd << false;
    QTest::newRow("N: 11 && false") << 11 << Types::ValueOperatorEquals << false << Types::ValueOperatorEquals << Types::StateOperatorAnd << false;
    QTest::newRow("Y: 11 || false") << 11 << Types::ValueOperatorEquals << false << Types::ValueOperatorEquals << Types::StateOperatorOr << true;
    QTest::newRow("Y: 10 || false") << 10 << Types::ValueOperatorEquals << false << Types::ValueOperatorEquals << Types::StateOperatorOr << true;
    QTest::newRow("Y: 10 || true") << 10 << Types::ValueOperatorEquals << true << Types::ValueOperatorEquals << Types::StateOperatorOr << true;
    QTest::newRow("N: 11 || true") << 11 << Types::ValueOperatorEquals << true << Types::ValueOperatorEquals << Types::StateOperatorOr << false;
}

void TestRules::testStateEvaluator2()
{
    QFETCH(int, intValue);
    QFETCH(Types::ValueOperator, intOperator);
    QFETCH(bool, boolValue);
    QFETCH(Types::ValueOperator, boolOperator);
    QFETCH(Types::StateOperator, stateOperator);
    QFETCH(bool, shouldMatch);

    StateDescriptor descriptor1(mockIntStateId, m_mockDeviceId, intValue, intOperator);
    StateEvaluator evaluator1(descriptor1);

    StateDescriptor descriptor2(mockBoolStateId, m_mockDeviceId, boolValue, boolOperator);
    StateEvaluator evaluator2(descriptor2);

    QList<StateEvaluator> childEvaluators;
    childEvaluators.append(evaluator1);
    childEvaluators.append(evaluator2);

    StateEvaluator mainEvaluator(childEvaluators);
    mainEvaluator.setOperatorType(stateOperator);

    QVERIFY2(mainEvaluator.evaluate() == shouldMatch, shouldMatch ? "State should match" : "State shouldn't match");
}

void TestRules::testChildEvaluator_data()
{
    cleanup();
    enableNotifications();

    DeviceId testDeviceId = addDisplayPinDevice();
    QVERIFY2(!testDeviceId.isNull(), "Could not add push button device for child evaluators");

    // Create child evaluators
    // Action
    QVariantMap action;
    action.insert("actionTypeId", mockActionIdNoParams);
    action.insert("deviceId", m_mockDeviceId);
    action.insert("ruleActionParams", QVariantList());

    // Exit action (with params)
    QVariantMap exitAction;
    QVariantList actionParams;
    QVariantMap param1;
    param1.insert("paramTypeId", mockActionParam1ParamTypeId);
    param1.insert("value", 12);
    actionParams.append(param1);
    QVariantMap param2;
    param2.insert("paramTypeId", mockActionParam2ParamTypeId);
    param2.insert("value", true);
    actionParams.append(param2);
    exitAction.insert("actionTypeId", mockActionIdWithParams);
    exitAction.insert("deviceId", m_mockDeviceId);
    exitAction.insert("ruleActionParams", actionParams);

    // Stateevaluators
    QVariantMap stateDescriptorPercentage;
    stateDescriptorPercentage.insert("deviceId", testDeviceId);
    stateDescriptorPercentage.insert("operator", JsonTypes::valueOperatorToString(Types::ValueOperatorGreaterOrEqual));
    stateDescriptorPercentage.insert("stateTypeId", percentageStateParamTypeId);
    stateDescriptorPercentage.insert("value", 50);

    QVariantMap stateDescriptorDouble;
    stateDescriptorDouble.insert("deviceId", testDeviceId);
    stateDescriptorDouble.insert("operator", JsonTypes::valueOperatorToString(Types::ValueOperatorEquals));
    stateDescriptorDouble.insert("stateTypeId", doubleStateParamTypeId);
    stateDescriptorDouble.insert("value", 20.5);

    QVariantMap stateDescriptorAllowedValues;
    stateDescriptorAllowedValues.insert("deviceId", testDeviceId);
    stateDescriptorAllowedValues.insert("operator", JsonTypes::valueOperatorToString(Types::ValueOperatorEquals));
    stateDescriptorAllowedValues.insert("stateTypeId", allowedValuesStateParamTypeId);
    stateDescriptorAllowedValues.insert("value", "String value 2");

    QVariantMap stateDescriptorColor;
    stateDescriptorColor.insert("deviceId", testDeviceId);
    stateDescriptorColor.insert("operator", JsonTypes::valueOperatorToString(Types::ValueOperatorEquals));
    stateDescriptorColor.insert("stateTypeId", colorStateParamTypeId);
    stateDescriptorColor.insert("value", "#00FF00");

    QVariantMap firstStateEvaluator;
    firstStateEvaluator.insert("operator", JsonTypes::stateOperatorToString(Types::StateOperatorOr));
    firstStateEvaluator.insert("childEvaluators", QVariantList() << createStateEvaluatorFromSingleDescriptor(stateDescriptorPercentage) << createStateEvaluatorFromSingleDescriptor(stateDescriptorDouble));

    QVariantMap secondStateEvaluator;
    secondStateEvaluator.insert("operator", JsonTypes::stateOperatorToString(Types::StateOperatorAnd));
    secondStateEvaluator.insert("childEvaluators", QVariantList() << createStateEvaluatorFromSingleDescriptor(stateDescriptorAllowedValues) << createStateEvaluatorFromSingleDescriptor(stateDescriptorColor));

    QVariantMap stateEvaluator;
    stateEvaluator.insert("operator", JsonTypes::stateOperatorToString(Types::StateOperatorAnd));
    stateEvaluator.insert("childEvaluators", QVariantList() << firstStateEvaluator << secondStateEvaluator);

    // The rule
    QVariantMap ruleMap;
    ruleMap.insert("name", "Child evaluator rule");
    ruleMap.insert("stateEvaluator", stateEvaluator);
    ruleMap.insert("actions", QVariantList() << action);
    ruleMap.insert("exitActions", QVariantList() << exitAction);

    printJson(ruleMap);


    // (percentage >= 50 || double == 20.5) && (color == #00FF00 && allowedValue == "String value 2") ? action : exit action

    QTest::addColumn<DeviceId>("deviceId");
    QTest::addColumn<QVariantMap>("ruleMap");
    QTest::addColumn<int>("percentageValue");
    QTest::addColumn<double>("doubleValue");
    QTest::addColumn<QString>("allowedValue");
    QTest::addColumn<QString>("colorValue");
    QTest::addColumn<bool>("trigger");
    QTest::addColumn<bool>("active");

    QTest::newRow("Unchanged | 2 | 2.5 | String value 1 | #FF0000") << testDeviceId << ruleMap << 2 << 2.5 << "String value 1" << "#FF0000" << false << false;
    QTest::newRow("Unchanged | 60 | 2.5 | String value 2 | #FF0000") << testDeviceId << ruleMap << 60 << 2.5 << "String value 2" << "#FF0000" << false << false;
    QTest::newRow("Unchanged | 60 | 20.5 | String value 2 | #FF0000") << testDeviceId << ruleMap << 60 << 20.5 << "String value 2" << "#FF0000" << false << false;
    QTest::newRow("Active | 60 | 20.5 | String value 2 | #00FF00") << testDeviceId << ruleMap << 60 << 20.5 << "String value 2" << "#00FF00" << true << true;
    QTest::newRow("Active | 60 | 20.5 | String value 2 | #00FF00") << testDeviceId << ruleMap << 60 << 20.5 << "String value 2" << "#00FF00" << true << true;
}

void TestRules::testChildEvaluator()
{
    QFETCH(DeviceId, deviceId);
    QFETCH(QVariantMap, ruleMap);
    QFETCH(int, percentageValue);
    QFETCH(double, doubleValue);
    QFETCH(QString, allowedValue);
    QFETCH(QString, colorValue);
    QFETCH(bool, trigger);
    QFETCH(bool, active);

    // Init the states
    setWritableStateValue(deviceId, StateTypeId(percentageStateParamTypeId.toString()), QVariant(0));
    setWritableStateValue(deviceId, StateTypeId(doubleStateParamTypeId.toString()), QVariant(0));
    setWritableStateValue(deviceId, StateTypeId(allowedValuesStateParamTypeId.toString()), QVariant("String value 1"));
    setWritableStateValue(deviceId, StateTypeId(colorStateParamTypeId.toString()), QVariant("#000000"));

    // Add rule
    QVariant response = injectAndWait("Rules.AddRule", ruleMap);
    verifyRuleError(response);

    RuleId ruleId = RuleId(response.toMap().value("params").toMap().value("ruleId").toString());

    // Set the states
    setWritableStateValue(deviceId, StateTypeId(percentageStateParamTypeId.toString()), QVariant::fromValue(percentageValue));
    setWritableStateValue(deviceId, StateTypeId(doubleStateParamTypeId.toString()), QVariant::fromValue(doubleValue));
    setWritableStateValue(deviceId, StateTypeId(allowedValuesStateParamTypeId.toString()), QVariant::fromValue(allowedValue));
    setWritableStateValue(deviceId, StateTypeId(colorStateParamTypeId.toString()), QVariant::fromValue(colorValue));

    // Verfiy if the rule executed successfully
    // Actions
    if (trigger && active) {
        verifyRuleExecuted(mockActionIdNoParams);
        cleanupMockHistory();
    }

    // Exit actions
    if (trigger && !active) {
        verifyRuleExecuted(mockActionIdWithParams);
        cleanupMockHistory();
    }

    // Nothing triggert
    if (!trigger) {
        verifyRuleNotExecuted();
    }

    // REMOVE rule
    QVariantMap removeParams;
    removeParams.insert("ruleId", ruleId);
    response = injectAndWait("Rules.RemoveRule", removeParams);
    verifyRuleError(response);
}

void TestRules::enableDisableRule()
{
    // Add a rule
    QVariantMap addRuleParams;
    QVariantList events;
    QVariantMap event1;
    event1.insert("eventTypeId", mockEvent1Id);
    event1.insert("deviceId", m_mockDeviceId);
    events.append(event1);
    addRuleParams.insert("eventDescriptors", events);
    addRuleParams.insert("name", "TestRule");

    QVariantList actions;
    QVariantMap action;
    action.insert("actionTypeId", mockActionIdNoParams);
    action.insert("deviceId", m_mockDeviceId);
    actions.append(action);
    addRuleParams.insert("actions", actions);
    QVariant response = injectAndWait("Rules.AddRule", addRuleParams);
    verifyRuleError(response);
    RuleId id = RuleId(response.toMap().value("params").toMap().value("ruleId").toString());

    // Trigger an event
    QNetworkAccessManager nam;
    QSignalSpy spy(&nam, SIGNAL(finished(QNetworkReply*)));

    // trigger event in mock device
    QNetworkRequest request(QUrl(QString("http://localhost:%1/generateevent?eventtypeid=%2").arg(m_mockDevice1Port).arg(mockEvent1Id.toString())));
    QNetworkReply *reply = nam.get(request);
    spy.wait();
    QCOMPARE(spy.count(), 1);
    reply->deleteLater();

    verifyRuleExecuted(mockActionIdNoParams);

    cleanupMockHistory();

    // Now DISABLE the rule invalid ruleId
    QVariantMap disableParams;
    disableParams.insert("ruleId", QUuid::createUuid().toString());
    response = injectAndWait("Rules.DisableRule", disableParams);
    verifyRuleError(response, RuleEngine::RuleErrorRuleNotFound);

    // Now DISABLE the rule
    disableParams.clear();
    disableParams.insert("ruleId", id.toString());
    response = injectAndWait("Rules.DisableRule", disableParams);
    verifyRuleError(response);

    // trigger event in mock device
    spy.clear();
    request = QNetworkRequest(QUrl(QString("http://localhost:%1/generateevent?eventtypeid=%2").arg(m_mockDevice1Port).arg(mockEvent1Id.toString())));
    reply = nam.get(request);
    spy.wait();
    QCOMPARE(spy.count(), 1);
    reply->deleteLater();

    verifyRuleNotExecuted();

    cleanupMockHistory();

    // Now ENABLE the rule again invald ruleId
    QVariantMap enableParams;
    enableParams.insert("ruleId", QUuid::createUuid().toString());
    response = injectAndWait("Rules.EnableRule", enableParams);
    verifyRuleError(response, RuleEngine::RuleErrorRuleNotFound);

    // Now ENABLE the rule again
    enableParams.clear();
    enableParams.insert("ruleId", id.toString());
    response = injectAndWait("Rules.EnableRule", enableParams);
    verifyRuleError(response);

    // trigger event in mock device
    spy.clear();
    request = QNetworkRequest(QUrl(QString("http://localhost:%1/generateevent?eventtypeid=%2").arg(m_mockDevice1Port).arg(mockEvent1Id.toString())));
    reply = nam.get(request);
    spy.wait();
    QCOMPARE(spy.count(), 1);
    reply->deleteLater();

    verifyRuleExecuted(mockActionIdNoParams);
}

void TestRules::testEventBasedAction()
{
    // Add a rule
    QVariantMap addRuleParams;
    QVariantMap eventDescriptor;
    eventDescriptor.insert("eventTypeId", mockIntStateId);
    eventDescriptor.insert("deviceId", m_mockDeviceId);
    addRuleParams.insert("eventDescriptors", QVariantList() << eventDescriptor);
    addRuleParams.insert("name", "TestRule");
    addRuleParams.insert("enabled", true);

    QVariantList actions;
    QVariantMap action;
    QVariantList ruleActionParams;
    QVariantMap param1;
    param1.insert("paramTypeId", mockActionParam1ParamTypeId);
    param1.insert("eventTypeId", mockIntStateId);
    param1.insert("eventParamTypeId", mockIntStateId);
    QVariantMap param2;
    param2.insert("paramTypeId", mockActionParam2ParamTypeId);
    param2.insert("value", true);
    ruleActionParams.append(param1);
    ruleActionParams.append(param2);
    action.insert("actionTypeId", mockActionIdWithParams);
    action.insert("deviceId", m_mockDeviceId);
    action.insert("ruleActionParams", ruleActionParams);
    actions.append(action);
    addRuleParams.insert("actions", actions);

    qDebug() << addRuleParams;

    QVariant response = injectAndWait("Rules.AddRule", addRuleParams);
    verifyRuleError(response);

    // Change the state
    QNetworkAccessManager nam;
    QSignalSpy spy(&nam, SIGNAL(finished(QNetworkReply*)));

    // state state to 42
    qDebug() << "setting mock int state to 42";
    QNetworkRequest request(QUrl(QString("http://localhost:%1/setstate?%2=%3").arg(m_mockDevice1Port).arg(mockIntStateId.toString()).arg(42)));
    QNetworkReply *reply = nam.get(request);
    spy.wait();
    QCOMPARE(spy.count(), 1);
    reply->deleteLater();

    verifyRuleExecuted(mockActionIdWithParams);
    // TODO: check if this action was realy executed with the int state value 42
}

void TestRules::removePolicyUpdate()
{
    // ADD parent device
    QVariantMap params;
    params.insert("deviceClassId", mockParentDeviceClassId);
    params.insert("name", "Parent device");

    QVariant response = injectAndWait("Devices.AddConfiguredDevice", params);
    verifyDeviceError(response);

    DeviceId parentDeviceId = DeviceId(response.toMap().value("params").toMap().value("deviceId").toString());
    QVERIFY(!parentDeviceId.isNull());

    // find child device
    response = injectAndWait("Devices.GetConfiguredDevices");

    QVariantList devices = response.toMap().value("params").toMap().value("devices").toList();

    DeviceId childDeviceId;
    foreach (const QVariant deviceVariant, devices) {
        QVariantMap deviceMap = deviceVariant.toMap();

        if (deviceMap.value("deviceClassId").toString() == mockChildDeviceClassId.toString()) {
            if (deviceMap.value("parentId") == parentDeviceId.toString()) {
                //qDebug() << QJsonDocument::fromVariant(deviceVariant).toJson();
                childDeviceId = DeviceId(deviceMap.value("id").toString());
            }
        }
    }
    QVERIFY2(!childDeviceId.isNull(), "Could not find child device");

    // Add rule with child device
    QVariantList eventDescriptors;
    eventDescriptors.append(createEventDescriptor(childDeviceId, mockParentChildEventId));
    eventDescriptors.append(createEventDescriptor(parentDeviceId, mockParentChildEventId));
    eventDescriptors.append(createEventDescriptor(m_mockDeviceId, mockEvent1Id));

    params.clear(); response.clear();
    params.insert("name", "RemovePolicy");
    params.insert("eventDescriptors", eventDescriptors);
    params.insert("actions", QVariantList() << createActionWithParams(m_mockDeviceId));

    response = injectAndWait("Rules.AddRule", params);
    verifyRuleError(response);
    RuleId ruleId = RuleId(response.toMap().value("params").toMap().value("ruleId").toString());
    QVERIFY2(!ruleId.isNull(), "Could not get ruleId");

    // Try to remove child device
    params.clear(); response.clear();
    params.insert("deviceId", childDeviceId);
    response = injectAndWait("Devices.RemoveConfiguredDevice", params);
    verifyDeviceError(response, DeviceManager::DeviceErrorDeviceIsChild);

    // Try to remove child device
    params.clear(); response.clear();
    params.insert("deviceId", parentDeviceId);
    response = injectAndWait("Devices.RemoveConfiguredDevice", params);
    verifyDeviceError(response, DeviceManager::DeviceErrorDeviceInRule);

    // Remove policy
    params.clear(); response.clear();
    params.insert("deviceId", parentDeviceId);
    params.insert("removePolicy", "RemovePolicyUpdate");
    response = injectAndWait("Devices.RemoveConfiguredDevice", params);
    verifyDeviceError(response);

    // get updated rule
    params.clear();
    params.insert("ruleId", ruleId);
    response = injectAndWait("Rules.GetRuleDetails", params);
    verifyRuleError(response);

    QVariantMap rule = response.toMap().value("params").toMap().value("rule").toMap();
    qDebug() << QJsonDocument::fromVariant(rule).toJson();
    QVERIFY(rule.value("eventDescriptors").toList().count() == 1);

    // REMOVE rule
    QVariantMap removeParams;
    removeParams.insert("ruleId", ruleId);
    response = injectAndWait("Rules.RemoveRule", removeParams);
    verifyRuleError(response);
}

void TestRules::removePolicyCascade()
{
    // ADD parent device
    QVariantMap params;
    params.insert("deviceClassId", mockParentDeviceClassId);
    params.insert("name", "Parent device");

    QVariant response = injectAndWait("Devices.AddConfiguredDevice", params);
    verifyDeviceError(response);

    DeviceId parentDeviceId = DeviceId(response.toMap().value("params").toMap().value("deviceId").toString());
    QVERIFY(!parentDeviceId.isNull());

    // find child device
    response = injectAndWait("Devices.GetConfiguredDevices");

    QVariantList devices = response.toMap().value("params").toMap().value("devices").toList();

    DeviceId childDeviceId;
    foreach (const QVariant deviceVariant, devices) {
        QVariantMap deviceMap = deviceVariant.toMap();

        if (deviceMap.value("deviceClassId").toString() == mockChildDeviceClassId.toString()) {
            if (deviceMap.value("parentId") == parentDeviceId.toString()) {
                //qDebug() << QJsonDocument::fromVariant(deviceVariant).toJson();
                childDeviceId = DeviceId(deviceMap.value("id").toString());
            }
        }
    }
    QVERIFY2(!childDeviceId.isNull(), "Could not find child device");

    // Add rule with child device
    QVariantList eventDescriptors;
    eventDescriptors.append(createEventDescriptor(childDeviceId, mockParentChildEventId));
    eventDescriptors.append(createEventDescriptor(parentDeviceId, mockParentChildEventId));
    eventDescriptors.append(createEventDescriptor(m_mockDeviceId, mockEvent1Id));

    params.clear(); response.clear();
    params.insert("name", "RemovePolicy");
    params.insert("eventDescriptors", eventDescriptors);
    params.insert("actions", QVariantList() << createActionWithParams(m_mockDeviceId));

    response = injectAndWait("Rules.AddRule", params);
    verifyRuleError(response);
    RuleId ruleId = RuleId(response.toMap().value("params").toMap().value("ruleId").toString());
    QVERIFY2(!ruleId.isNull(), "Could not get ruleId");

    // Try to remove child device
    params.clear(); response.clear();
    params.insert("deviceId", childDeviceId);
    response = injectAndWait("Devices.RemoveConfiguredDevice", params);
    verifyDeviceError(response, DeviceManager::DeviceErrorDeviceIsChild);

    // Try to remove child device
    params.clear(); response.clear();
    params.insert("deviceId", parentDeviceId);
    response = injectAndWait("Devices.RemoveConfiguredDevice", params);
    verifyDeviceError(response, DeviceManager::DeviceErrorDeviceInRule);

    // Remove policy
    params.clear(); response.clear();
    params.insert("deviceId", parentDeviceId);
    params.insert("removePolicy", "RemovePolicyCascade");
    response = injectAndWait("Devices.RemoveConfiguredDevice", params);
    verifyDeviceError(response);

    // get updated rule
    params.clear();
    params.insert("ruleId", ruleId);
    response = injectAndWait("Rules.GetRuleDetails", params);
    verifyRuleError(response, RuleEngine::RuleErrorRuleNotFound);
}

void TestRules::testRuleActionParams_data()
{
    QVariantMap action;
    QVariantList ruleActionParams;
    QVariantMap param1;
    param1.insert("paramTypeId", mockActionParam1ParamTypeId);
    param1.insert("value", 4);
    QVariantMap param2;
    param2.insert("paramTypeId", mockActionParam2ParamTypeId);
    param2.insert("value", true);
    ruleActionParams.append(param1);
    ruleActionParams.append(param2);
    action.insert("actionTypeId", mockActionIdWithParams);
    action.insert("deviceId", m_mockDeviceId);
    action.insert("ruleActionParams", ruleActionParams);

    QVariantMap invalidAction1;
    invalidAction1.insert("actionTypeId", mockActionIdWithParams);
    invalidAction1.insert("deviceId", m_mockDeviceId);
    invalidAction1.insert("ruleActionParams", QVariantList() << param2);

    QVariantMap invalidAction2;
    invalidAction2.insert("actionTypeId", mockActionIdWithParams);
    invalidAction2.insert("deviceId", m_mockDeviceId);
    invalidAction2.insert("ruleActionParams", QVariantList() << param1);


    QTest::addColumn<QVariantMap>("action");
    QTest::addColumn<QVariantMap>("exitAction");
    QTest::addColumn<RuleEngine::RuleError>("error");

    QTest::newRow("valid action params") << action << QVariantMap() << RuleEngine::RuleErrorNoError;
    QTest::newRow("valid action and exit action params") << action << action << RuleEngine::RuleErrorNoError;
    QTest::newRow("invalid action params1") << invalidAction1 << QVariantMap() << RuleEngine::RuleErrorInvalidRuleActionParameter;
    QTest::newRow("invalid action params2") << invalidAction2 << QVariantMap() << RuleEngine::RuleErrorInvalidRuleActionParameter;
    QTest::newRow("valid action and invalid exit action params1") << action << invalidAction1 << RuleEngine::RuleErrorInvalidRuleActionParameter;
    QTest::newRow("valid action and invalid exit action params2") << action << invalidAction2 << RuleEngine::RuleErrorInvalidRuleActionParameter;
}

void TestRules::testRuleActionParams()
{
    QFETCH(QVariantMap, action);
    QFETCH(QVariantMap, exitAction);
    QFETCH(RuleEngine::RuleError, error);

    // Add a rule
    QVariantMap addRuleParams;
    addRuleParams.insert("name", "TestRule");
    addRuleParams.insert("enabled", true);
    if (!action.isEmpty())
        addRuleParams.insert("actions", QVariantList() << action);
    if (!exitAction.isEmpty())
        addRuleParams.insert("exitActions", QVariantList() << exitAction);

    QVariant response = injectAndWait("Rules.AddRule", addRuleParams);
    verifyRuleError(response, error);
}

void TestRules::testInterfaceBasedRule()
{
    QVariantMap powerAction;
    powerAction.insert("interface", "light");
    powerAction.insert("interfaceAction", "power");
    QVariantMap powerActionParam;
    powerActionParam.insert("paramName", "power");
    powerActionParam.insert("value", true);
    powerAction.insert("ruleActionParams", QVariantList() << powerActionParam);

    QVariantMap lowBatteryEvent;
    lowBatteryEvent.insert("interface", "battery");
    lowBatteryEvent.insert("interfaceEvent", "batteryCritical");

    QVariantMap addRuleParams;
    addRuleParams.insert("name", "TestInterfaceBasedRule");
    addRuleParams.insert("enabled", true);
    addRuleParams.insert("actions", QVariantList() << powerAction);
    addRuleParams.insert("eventDescriptors", QVariantList() << lowBatteryEvent);

    QVariant response = injectAndWait("Rules.AddRule", addRuleParams);


    // Change the state
    QNetworkAccessManager nam;
    QSignalSpy spy(&nam, SIGNAL(finished(QNetworkReply*)));

    // state battery critical state to true
    qDebug() << "setting battery critical state to true";
    QNetworkRequest request(QUrl(QString("http://localhost:%1/setstate?%2=%3").arg(m_mockDevice1Port).arg(mockBatteryCriticalStateId.toString()).arg(true)));
    QNetworkReply *reply = nam.get(request);
    spy.wait();
    QCOMPARE(spy.count(), 1);
    reply->deleteLater();

    qDebug() << "response" << response;
}

void TestRules::testHousekeeping_data()
{
    QTest::addColumn<bool>("testAction");
    QTest::addColumn<bool>("testExitAction");
    QTest::addColumn<bool>("testStateEvaluator");
    QTest::addColumn<bool>("testEventDescriptor");

    QTest::newRow("action") << true << false << false << false;
    QTest::newRow("exitAction") << false << true << false << false;
    QTest::newRow("stateDescriptor") << false << false << true << false;
    QTest::newRow("eventDescriptor")<< false << false << false << true;
}

void TestRules::testHousekeeping()
{
    QFETCH(bool, testAction);
    QFETCH(bool, testExitAction);
    QFETCH(bool, testStateEvaluator);
    QFETCH(bool, testEventDescriptor);

    QVariantMap params;
    params.insert("deviceClassId", mockDeviceClassId);
    params.insert("name", "TestDeviceToBeRemoved");
    QVariantList deviceParams;
    QVariantMap httpParam;
    httpParam.insert("paramTypeId", httpportParamTypeId);
    httpParam.insert("value", 6667);
    deviceParams.append(httpParam);
    params.insert("deviceParams", deviceParams);
    QVariant response = injectAndWait("Devices.AddConfiguredDevice", params);
    DeviceId deviceId = DeviceId::fromUuid(response.toMap().value("params").toMap().value("deviceId").toUuid());
    QVERIFY2(!deviceId.isNull(), "Something went wrong creating the device for testing.");

    // Create a rule with this device
    params.clear();
    params.insert("name", "testrule");
    if (testEventDescriptor) {
        QVariantList eventDescriptors;
        QVariantMap eventDescriptor;
        eventDescriptor.insert("eventTypeId", mockEvent1Id);
        eventDescriptor.insert("deviceId", testEventDescriptor ? deviceId : m_mockDeviceId);
        eventDescriptors.append(eventDescriptor);
        params.insert("eventDescriptors", eventDescriptors);
    }

    QVariantMap stateEvaluator;
    QVariantMap stateDescriptor;
    stateDescriptor.insert("stateTypeId", mockIntStateId);
    stateDescriptor.insert("operator", "ValueOperatorGreater");
    stateDescriptor.insert("value", 555);
    stateDescriptor.insert("deviceId", testStateEvaluator ? deviceId : m_mockDeviceId);
    stateEvaluator.insert("stateDescriptor", stateDescriptor);
    params.insert("stateEvaluator", stateEvaluator);

    QVariantList actions;
    QVariantMap action;
    action.insert("actionTypeId", mockActionIdNoParams);
    action.insert("deviceId", testAction ? deviceId : m_mockDeviceId);
    actions.append(action);
    params.insert("actions", actions);

    if (!testEventDescriptor) {
        QVariantList exitActions;
        QVariantMap exitAction;
        exitAction.insert("actionTypeId", mockActionIdNoParams);
        exitAction.insert("deviceId", testExitAction ? deviceId : m_mockDeviceId);
        exitActions.append(exitAction);
        params.insert("exitActions", exitActions);
    }

    response = injectAndWait("Rules.AddRule", params);
    RuleId ruleId = RuleId::fromUuid(response.toMap().value("params").toMap().value("ruleId").toUuid());


    // Verfy that the rule has been created successfully and our device is in there.
    params.clear();
    params.insert("ruleId", ruleId);
    response = injectAndWait("Rules.GetRuleDetails", params);
    if (testEventDescriptor) {
        QVERIFY2(response.toMap().value("params").toMap().value("rule").toMap().value("eventDescriptors").toList().first().toMap().value("deviceId").toUuid().toString() == (testEventDescriptor ? deviceId.toString() : m_mockDeviceId.toString()), "Couldn't find device in eventDescriptor of rule");
    }
    QVERIFY2(response.toMap().value("params").toMap().value("rule").toMap().value("stateEvaluator").toMap().value("stateDescriptor").toMap().value("deviceId").toUuid().toString() == (testStateEvaluator ? deviceId.toString() : m_mockDeviceId.toString()), "Couldn't find device in stateEvaluator of rule");
    QVERIFY2(response.toMap().value("params").toMap().value("rule").toMap().value("actions").toList().first().toMap().value("deviceId").toUuid().toString() == (testAction ? deviceId.toString() : m_mockDeviceId.toString()), "Couldn't find device in actions of rule");
    if (!testEventDescriptor) {
        QVERIFY2(response.toMap().value("params").toMap().value("rule").toMap().value("exitActions").toList().first().toMap().value("deviceId").toUuid().toString() == (testExitAction ? deviceId.toString() : m_mockDeviceId.toString()), "Couldn't find device in exitActions of rule");
    }

    // Manually delete this device from config
    GuhSettings settings(GuhSettings::SettingsRoleDevices);
    settings.beginGroup("DeviceConfig");
    settings.remove(deviceId.toString());
    settings.endGroup();

    restartServer();

    // Now make sure the appropriate entries with our device have disappeared
    response = injectAndWait("Rules.GetRuleDetails", params);
    if (testEventDescriptor) {
        QVERIFY2(response.toMap().value("params").toMap().value("rule").toMap().value("eventDescriptors").toList().count() == (testEventDescriptor ? 0: 1), "EventDescriptor still in rule... should've been removed by housekeeping.");
    }
    QVERIFY2(response.toMap().value("params").toMap().value("rule").toMap().value("stateEvaluator").toMap().value("stateDescriptor").toMap().isEmpty() == (testStateEvaluator ? true : false), "StateEvaluator still in rule... should've been removed by housekeeping.");
    QVERIFY2(response.toMap().value("params").toMap().value("rule").toMap().value("actions").toList().count() == (testAction ? 0 : 1), "Action still in rule... should've been removed by housekeeping.");
    if (!testEventDescriptor) {
        QVERIFY2(response.toMap().value("params").toMap().value("rule").toMap().value("exitActions").toList().count() == (testExitAction ? 0: 1), "ExitAction still in rule... should've been removed by housekeeping.");
    }
}

#include "testrules.moc"
QTEST_MAIN(TestRules)
