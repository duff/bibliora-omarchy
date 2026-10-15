#pragma once

#include <QLocalServer>
#include <QString>

#include <functional>

// One Bibliora at a time (adapted from Billy's). The running one listens on a
// per-user name in Linux's abstract socket namespace, which has no file to go
// stale. A later launch asks it to come forward, passing along where to open
// (`bibliora jn 3:16`, a bibliora:// link), and exits; bin/relaunch's new
// Bibliora (--replace) instead waits for the old one to quit and takes over.
class Instance
{
public:
    using RaiseFn = std::function<void(const QString &activationToken, const QString &open)>;

    Instance();

    // True when this process is now the Bibliora. False after handing the
    // launch (its XDG activation token and where to open) to the one running.
    bool claim(const RaiseFn &onRaise, const QString &activationToken, const QString &open);

    // Listens as soon as the Bibliora being replaced lets go of the name.
    void takeOver(const RaiseFn &onRaise);

private:
    bool ping(const QString &activationToken, const QString &open);
    void serve();

    const QString m_name;
    QLocalServer m_server;
    RaiseFn m_onRaise;
};
