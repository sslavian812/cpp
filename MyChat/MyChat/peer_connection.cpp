#include "peer_connection.h"

PeerConnection::PeerConnection(QString const & nick, QHostAddress const & addr) : nick(nick), addr(addr) {
}

void PeerConnection::resetPongTimer() {
	pongTimer.start(pongTimeout);
	connect(&pongTimer, SIGNAL(timeout()), QObject::sender(), SLOT(recvQuit("QUIT " + nick, addr)));
}