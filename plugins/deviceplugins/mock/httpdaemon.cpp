#include "httpdaemon.h"

#include "device.h"
#include "deviceclass.h"
#include "deviceplugin.h"
#include "statetype.h"

#include <QTcpSocket>
#include <QDebug>
#include <QDateTime>
#include <QUrlQuery>

HttpDaemon::HttpDaemon(Device *device, DevicePlugin *parent):
    QTcpServer(parent), disabled(false), m_plugin(parent), m_device(device)
{
    listen(QHostAddress::LocalHost, device->params().value("httpport").toInt());
}

void HttpDaemon::incomingConnection(qintptr socket)
{
    qDebug() << "incoming connection";
    if (disabled)
        return;

    // When a new client connects, the server constructs a QTcpSocket and all
    // communication with the client is done over this QTcpSocket. QTcpSocket
    // works asynchronously, this means that all the communication is done
    // in the two slots readClient() and discardClient().
    QTcpSocket* s = new QTcpSocket(this);
    connect(s, SIGNAL(readyRead()), this, SLOT(readClient()));
    connect(s, SIGNAL(disconnected()), this, SLOT(discardClient()));
    s->setSocketDescriptor(socket);

}

void HttpDaemon::pause()
{
    disabled = true;
}

void HttpDaemon::resume()
{
    disabled = false;
}

void HttpDaemon::readClient()
{
    if (disabled)
        return;

    // This slot is called when the client sent data to the server. The
    // server looks if it was a get request and sends a very simple HTML
    // document back.
    QTcpSocket* socket = (QTcpSocket*)sender();
    if (socket->canReadLine()) {
        QByteArray data = socket->readLine();
        QStringList tokens = QString(data).split(QRegExp("[ \r\n][ \r\n]*"));
        qDebug() << "incoming data" << tokens[1];
        if (tokens[1].contains('?')) {
            QUrl url("http://foo.bar" + tokens[1]);
            QUrlQuery query(url);
            qDebug() << "query is" << url.path();
            if (url.path() == "/setstate") {
                emit setState(QUuid(query.queryItems().first().first), QVariant(query.queryItems().first().second));
            } else if (url.path() == "/generateevent") {
                qDebug() << "got generateevent" << query.queryItemValue("eventid");
                emit triggerEvent(QUuid(query.queryItemValue("eventid")));
            }
        }
        if (tokens[0] == "GET") {
            QTextStream os(socket);
            os.setAutoDetectUnicode(true);
            os << generateWebPage();
            socket->close();

            qDebug() << "Wrote to client";

            if (socket->state() == QTcpSocket::UnconnectedState) {
                delete socket;
                qDebug() << "Connection closed";
            }
        }
    }
}

void HttpDaemon::discardClient()
{
    QTcpSocket* socket = (QTcpSocket*)sender();
    socket->deleteLater();

    qDebug() << "Connection closed";
}

QString HttpDaemon::generateWebPage()
{
    DeviceClass deviceClass = m_plugin->supportedDevices().first();

    QString contentHeader(
        "HTTP/1.0 200 Ok\r\n"
       "Content-Type: text/html; charset=\"utf-8\"\r\n"
       "\r\n"
    );

    QString body = QString(
    "<html>"
        "<body>"
        "<h1>Mock device Controller</h1>\n"
        "Name: %1<br>"
        "ID: %2<br>"
        "DeviceClass ID: %3<br>").arg(m_device->name()).arg(m_device->id().toString()).arg(deviceClass.id().toString());
    body.append("<table>");
    for (int i = 0; i < deviceClass.states().count(); ++i) {
        body.append("<tr>");
        body.append("<form action=\"/setstate\" method=\"get\">");
        const StateType &stateType = deviceClass.states().at(i);
        body.append("<td>" + stateType.name() + "</td>");
        body.append(QString("<td><input type='input'' name='%1' value='%2'></td>").arg(stateType.id().toString()).arg(m_device->states().at(i).value().toString()));
        body.append("<td><input type=submit value='Set State'/></td>");
        body.append("</form>");
        body.append("</tr>");
    }
    body.append("</table>");

    body.append("<table>");
    for (int i = 0; i < deviceClass.events().count(); ++i) {
        const EventType &eventType = deviceClass.events().at(i);
        body.append(QString(
        "<tr>"
        "<form action=\"/generateevent\" method=\"get\">"
        "<td>Event %1<input type='hidden' name='eventid' value='%2'/></td>"
        "<td><input type='submit' value='Generate'/></td>"
        "</form>"
        "</tr>"
        ).arg(eventType.name()).arg(eventType.id().toString()));
    }
    body.append("</table>");

    body.append("</body></html>\n");

    return contentHeader + body;
}