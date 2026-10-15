#include "Instance.h"

#include <QEventLoop>
#include <QLocalSocket>
#include <QTimer>

#include <unistd.h>

Instance::Instance()
    : m_name(QString("bibliora-%1").arg(getuid()))
{
    m_server.setSocketOptions(QLocalServer::AbstractNamespaceOption);
}

bool Instance::ping(const QString &activationToken, const QString &open)
{
    QLocalSocket probe;
    probe.setSocketOptions(QLocalSocket::AbstractNamespaceOption);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&probe, &QLocalSocket::connected, &loop, &QEventLoop::quit);
    QObject::connect(&probe, &QLocalSocket::errorOccurred, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    probe.connectToServer(m_name);
    timer.start(1000);
    if (probe.state() == QLocalSocket::ConnectingState)
        loop.exec();
    if (probe.state() != QLocalSocket::ConnectedState)
        return false;
    // "raise <token>\t<where to open>": neither holds a tab or a newline.
    QString where = open;
    where.replace('\t', ' ').replace('\n', ' ');
    probe.write("raise " + activationToken.toUtf8() + "\t" + where.toUtf8() + "\n");
    probe.flush();
    QEventLoop reply;
    QTimer replyTimer;
    replyTimer.setSingleShot(true);
    QObject::connect(&probe, &QLocalSocket::readyRead, &reply, &QEventLoop::quit);
    QObject::connect(&probe, &QLocalSocket::disconnected, &reply, &QEventLoop::quit);
    QObject::connect(&replyTimer, &QTimer::timeout, &reply, &QEventLoop::quit);
    replyTimer.start(1000);
    if (probe.bytesAvailable() == 0)
        reply.exec();
    return true;
}

bool Instance::claim(const RaiseFn &onRaise, const QString &activationToken, const QString &open)
{
    m_onRaise = onRaise;
    // Two launches at once both find nobody. Only one of them can listen, and
    // the other asks that one on its second pass.
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (ping(activationToken, open))
            return false;
        if (m_server.listen(m_name))
            break;
    }
    if (m_server.isListening())
        serve();
    return true;
}

void Instance::takeOver(const RaiseFn &onRaise)
{
    m_onRaise = onRaise;
    if (m_server.listen(m_name)) {
        serve();
        return;
    }
    auto *retry = new QTimer(&m_server);
    retry->setInterval(200);
    QObject::connect(retry, &QTimer::timeout, &m_server, [this, retry] {
        if (!m_server.listen(m_name))
            return;
        retry->deleteLater();
        serve();
    });
    retry->start();
}

void Instance::serve()
{
    QObject::connect(&m_server, &QLocalServer::newConnection, &m_server, [this] {
        while (QLocalSocket *socket = m_server.nextPendingConnection()) {
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [this, socket] {
                if (!socket->canReadLine())
                    return;
                const QByteArray line = socket->readLine().trimmed();
                if (!line.startsWith("raise"))
                    return;
                const QString rest = QString::fromUtf8(line.mid(5));
                if (m_onRaise)
                    m_onRaise(rest.section('\t', 0, 0).trimmed(), rest.section('\t', 1).trimmed());
                socket->write("ok\n");
                socket->flush();
                socket->disconnectFromServer();
            });
        }
    });
}
