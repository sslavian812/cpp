#ifndef PEER_CONNECTION_H
#define PEER_CONNECTION_H

#include <QTimer>
#include <QString>
#include <QHostAddress>

class PeerConnection : public QObject {

	Q_OBJECT

public:
	static const int pongTimeout = 600 * 1000;
	PeerConnection(QString const & nick, QHostAddress const & addr);
	QString getNick() const { return nick; }
	QHostAddress getAddr() const { return addr; }
	PeerConnection(const PeerConnection & other) : nick(other.getNick()), addr(other.getAddr()) {}

public slots:
	void resetPongTimer();

private:
	QString nick;
    QTimer pongTimer;
	QHostAddress addr;
};

#endif // PEER_CONNECTION_H