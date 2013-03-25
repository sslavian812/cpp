#ifndef MYCHAT_H
#define MYCHAT_H

#include "ui_mychat.h"
#include "network_engine.h"

class MyChat : public QMainWindow, private Ui::MyChatClass
{
	Q_OBJECT

public:
	MyChat(QWidget *parent = 0, Qt::WFlags flags = 0);
	~MyChat() { delete engine; }

private slots:
    void appendMessage(const QString from, const QString message);
	void returnPressed();
    void newUser(QString const nick);
    void userLeft(QString const nick);

private:
	QString myNick;
	QTextTableFormat tableFormat;
	NetworkEngine * engine;
};

#endif // MYCHAT_H
