#ifndef NETWORK_ENGINE_H
#define NETWORK_ENGINE_H

#include <map>
#include <list>
#include <ctime>
#include <qudpsocket.h>
#include <QtNetwork>
#include <QTimer>
#include <qbytearray.h>
#include "peer_connection.h"

class NetworkEngine : public QObject {

	Q_OBJECT

public:
	static const quint16 port = 3141;
	static const int pingTimeout = 120 * 1000;
	static const int connectTimeout = 30 * 1000;
	static const int sentMessageTimeout = 60 * 1000;

	NetworkEngine(QString const & myNick, QHostAddress const & myAddr);
	~NetworkEngine();

	void _sendMessage(QString const & text) { sendMessage(text, *currentUser, time(NULL), currentUser->getAddr()); }

private:
	struct Message {
		QString message;
		PeerConnection user;
		unsigned long time;
		QHostAddress sender;

		Message(QString const & message, PeerConnection const & user, unsigned long time, QHostAddress const & sender) : message(message), user(user), time(time), sender(sender) {}
	};

signals:
	void newMessage(QString const from, QString const message);
	void newUser(QString const nick);
	void userLeft(QString const nick);

public slots:
	void recvQuit(QString const & command, QHostAddress const & addr);

private slots:
	void processPendingDatagrams();
	void tryToConnect();
	void deliveryChecking(QString const nick, Message const message);

private:
	void recvHello(QString const & command, QHostAddress const & addr);
	void recvResponse(QString const & command, QHostAddress const & addr);
	void recvJoin(QString const & command, QHostAddress const & addr);
	void recvMessage(QString const & command, QHostAddress const & addr);
	void recvAccepted(QString const & command, QHostAddress const & addr);
	void recvGet(QString const & command, QHostAddress const & addr);
	void recvKeepAlive(QString const & command, QHostAddress const & addr);

	void sendHello();
	void sendResponse(PeerConnection const & user);
	void sendJoin(PeerConnection const & user);
	void sendMessage(QString const & message, PeerConnection const & user, unsigned long time, QHostAddress const & sender);
	void sendAccepted(PeerConnection const & user, unsigned long time);
	void sendQuit(PeerConnection const & user);
	void sendKeepAlive();

	std::list<PeerConnection>::iterator findUser(QHostAddress const & addr);
	std::list<PeerConnection>::iterator findUser(QString const & nick);

	PeerConnection * currentUser;
	std::list<PeerConnection> users;
	std::map<QString, std::list<unsigned long> > history;

	std::map<QString, std::list<Message> > sentMessages;

	typedef void (NetworkEngine::* funcPtr)(QString const &, QHostAddress const &);
	std::map<QString, funcPtr> functions;

	QTimer pingTimer, connectionTimer;
	QUdpSocket * udpSocket;
};

#endif // NETWORK_ENGINE_H