#include "network_engine.h"

NetworkEngine::NetworkEngine(QString const & myNick, QHostAddress const & myAddr) {
	functions.insert(std::make_pair("MESSAGE", &NetworkEngine::recvMessage));
	functions.insert(std::make_pair("HELLO", &NetworkEngine::recvHello));
	functions.insert(std::make_pair("QUIT", &NetworkEngine::recvQuit));
	functions.insert(std::make_pair("RESPONSE", &NetworkEngine::recvResponse));
	functions.insert(std::make_pair("JOIN", &NetworkEngine::recvJoin));
	functions.insert(std::make_pair("ACCEPTED", &NetworkEngine::recvAccepted));
	functions.insert(std::make_pair("GET", &NetworkEngine::recvGet));
	functions.insert(std::make_pair("KEEPALIVE", &NetworkEngine::recvKeepAlive));

	currentUser = new PeerConnection(myNick, myAddr);

	udpSocket = new QUdpSocket(this);

	udpSocket->bind(myAddr, port);
	connect(udpSocket, SIGNAL(readyRead()), this, SLOT(processPendingDatagrams()));

	pingTimer.start(pingTimeout);
	connect(&pingTimer, SIGNAL(timeout()), this, SLOT(sendKeepAlive()));

	tryToConnect();
	connectionTimer.start(connectTimeout);
	connect(&connectionTimer, SIGNAL(timeout()), this, SLOT(tryToConnect()));
}

NetworkEngine::~NetworkEngine() {
	sendQuit(*currentUser);
	delete udpSocket;
	delete currentUser;
}

void NetworkEngine::processPendingDatagrams() {
	while(udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(udpSocket->pendingDatagramSize());
		QHostAddress addr;
        udpSocket->readDatagram(datagram.data(), datagram.size(), &addr);
		QString addrStr = addr.toString();
        QString buffer = datagram;	
     
		buffer.chop(2);

		QStringList strList = buffer.split(" ");
		std::map<QString, funcPtr>::iterator it = functions.find(strList[0]);
			if(it != functions.end())
				(this->*(it->second))(buffer, addr);
	 }
}

std::list<PeerConnection>::iterator NetworkEngine::findUser(QHostAddress const & addr) {
	for(std::list<PeerConnection>::iterator it = users.begin(); it != users.end(); ++it)
		if(it->getAddr() == addr)
			return it;
	return users.end();
}

std::list<PeerConnection>::iterator NetworkEngine::findUser(QString const & nick) {
	for(std::list<PeerConnection>::iterator it = users.begin(); it != users.end(); ++it)
		if(it->getNick() == nick)
			return it;
	return users.end();
}

void NetworkEngine::tryToConnect() {
	if(users.size() > 0)
		connectionTimer.stop();
	else
		sendHello();
}

void NetworkEngine::deliveryChecking(QString const nick, Message const message) {
	for(std::list<Message>::iterator jt = sentMessages[nick].begin(); jt != sentMessages[nick].end(); ++jt)
		if(jt->time == message.time) {
			QTimer::singleShot(sentMessageTimeout, this, SLOT(deliveryChecking(nick, message)));
			sendMessage(message.message, message.user, message.time, message.sender);
			return;
		}
}

void NetworkEngine::recvHello(QString const & command, QHostAddress const & addr) {
	QString s = addr.toString();
	QString addrStr = addr.toString();
	QString myAddrStr = currentUser->getAddr().toString();
	if(addr == currentUser->getAddr())
		return;
	QStringList strList = command.split(" ");
	PeerConnection user(strList[1], addr);
	if(findUser(addr) == users.end()) {
		users.push_back(user);
		history[strList[1]];
		sentMessages[strList[1]];
		sendResponse(user);
		sendJoin(user);
		newUser(strList[1]);
	}
}

void NetworkEngine::recvResponse(QString const & command, QHostAddress const & addr) {
	QStringList strList = command.split(" ");
	if(findUser(addr) == users.end()) {
		users.push_back(PeerConnection(strList[1], addr));
		history[strList[1]];
		sentMessages[strList[1]];
		newUser(strList[1]);
	}
	for(size_t i = 2; i < strList.size(); i += 2) {
		if(findUser(QHostAddress(strList[i])) == users.end() && (*currentUser).getAddr().toString() != strList[i]) {
			users.push_back(PeerConnection(strList[i+1], QHostAddress(strList[i])));
			history[strList[i+1]];
			sentMessages[strList[i+1]];
			newUser(strList[i+1]);
		}
	}
}

void NetworkEngine::recvMessage(QString const & command, QHostAddress const & addr) {
	QStringList strList = command.split(" ");
	if(strList.size() < 5)
		return;
	std::list<PeerConnection>::iterator it = findUser(QHostAddress(strList[1]));
	if(it != users.end()) {
		for(std::list<unsigned long>::iterator jt = history[strList[2]].begin(); jt != history[strList[2]].end(); ++jt)
			if(*jt == strList[3].toInt()) {
				sendAccepted(*it, strList[3].toUInt());
				return;
			}
		QString msg = strList[4];
		for(size_t i = 5; i < strList.size(); ++i)
			msg += " " + strList[i];
		history[strList[2]].push_front(strList[3].toUInt());
		newMessage(strList[2], QString::fromUtf8(msg.toStdString().c_str(), msg.size()));
		sendMessage(QString::fromUtf8(msg.toStdString().c_str(), msg.size()), *it, strList[3].toUInt(), addr);
		sendAccepted(*it, strList[3].toInt());
	}
}

void NetworkEngine::recvQuit(QString const & command, QHostAddress const & addr) {
	QStringList strList = command.split(" ");
	std::list<PeerConnection>::iterator it = findUser(QHostAddress(strList[1]));
	if(it != users.end()) {
		history.erase(it->getNick());
		sentMessages.erase(it->getNick());
		userLeft(it->getNick());
		sendQuit(*it);
		users.erase(it);
	}
}

void NetworkEngine::recvJoin(QString const & command, QHostAddress const & addr) {
	QStringList strList = command.split(" ");
	if(findUser(QHostAddress(strList[1])) == users.end()) {
		users.push_back(PeerConnection(strList[2], QHostAddress(strList[1])));
		history[strList[2]];
		sentMessages[strList[2]];
		newUser(strList[2]);
		sendJoin(PeerConnection(strList[2], QHostAddress(strList[1])));
	}
}

void NetworkEngine::recvAccepted(QString const & command, QHostAddress const & addr) {
	QStringList strList = command.split(" ");
	for(std::list<Message>::iterator jt = sentMessages[strList[1]].begin(); jt != sentMessages[strList[1]].end(); ++jt)
		if(jt->time == strList[2].toUInt()) {
			sentMessages[strList[1]].erase(jt);
			return;
		}
}

void NetworkEngine::recvGet(QString const & command, QHostAddress const & addr) {
	QStringList strList = command.split(" ");
	sendResponse(PeerConnection("", addr));
}

void NetworkEngine::recvKeepAlive(QString const & command, QHostAddress const & addr) {
	std::list<PeerConnection>::iterator it = findUser(addr);
	if(it != users.end())
		it->resetPongTimer();
}

void NetworkEngine::sendHello() {
	QString str = "HELLO " + currentUser->getNick() + "\r\n";
	QByteArray datagram(str.toStdString().c_str(), str.size());
	udpSocket->writeDatagram(datagram, datagram.size(), QHostAddress::Broadcast, port);
}

void NetworkEngine::sendResponse(PeerConnection const & user) {
	QString str = "RESPONSE " + currentUser->getNick();
	for(std::list<PeerConnection>::iterator it = users.begin(); it != users.end(); ++it)
		str += " " + it->getAddr().toString() + " " + it->getNick();
	str += "\r\n";
	QByteArray datagram(str.toStdString().c_str(), str.size());
	udpSocket->writeDatagram(datagram, datagram.size(), user.getAddr(), port);
}

void NetworkEngine::sendJoin(PeerConnection const & user) {
	QString str = "JOIN " + user.getAddr().toString() + " " + user.getNick() + "\r\n";
	QByteArray datagram(str.toStdString().c_str(), str.size());
	for(std::list<PeerConnection>::iterator it = users.begin(); it != users.end(); ++it)
		udpSocket->writeDatagram(datagram, datagram.size(), it->getAddr(), port);
}

void NetworkEngine::sendMessage(QString const & message, PeerConnection const & user, unsigned long time, QHostAddress const & sender) {
	QString str = "MESSAGE " + user.getAddr().toString() + " " + user.getNick() + " " + QString().setNum(time) + " " + message.toUtf8() + "\r\n";
	QByteArray datagram(str.toStdString().c_str(), str.size());
	for(std::list<PeerConnection>::iterator it = users.begin(); it != users.end(); ++it)
		if(it->getAddr() != sender)
			udpSocket->writeDatagram(datagram, datagram.size(), it->getAddr(), port);
	Message msg(message, user, time, sender);
	sentMessages[user.getNick()].push_front(msg);
	history[user.getNick()].push_front(time);
	QTimer::singleShot(sentMessageTimeout, this, SLOT(deliveryChecking(user.getNick(), msg)));
}

void NetworkEngine::sendAccepted(PeerConnection const & user, unsigned long time) {
	QString str = "ACCEPTED " + user.getNick() + " " + QString().setNum(time) + "\r\n";
	QByteArray datagram(str.toStdString().c_str(), str.size());
	udpSocket->writeDatagram(datagram, datagram.size(), user.getAddr(), port);
}

void NetworkEngine::sendQuit(PeerConnection const & user) {
	QString str = "QUIT " + user.getAddr().toString() + "\r\n";
	QByteArray datagram(str.toStdString().c_str(), str.size());
	for(std::list<PeerConnection>::iterator it = users.begin(); it != users.end(); ++it)
		udpSocket->writeDatagram(datagram, datagram.size(), it->getAddr(), port);
}

void NetworkEngine::sendKeepAlive() {
	QString str = "KEEPALIVE\r\n";
	QByteArray datagram(str.toStdString().c_str(), str.size());
	for(std::list<PeerConnection>::iterator it = users.begin(); it != users.end(); ++it)
		udpSocket->writeDatagram(datagram, datagram.size(), it->getAddr(), port);
}