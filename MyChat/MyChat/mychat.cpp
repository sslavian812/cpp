#include <QtGui>
#include "mychat.h"

#include "settings.h"
#include <QtNetwork>

MyChat::MyChat(QWidget *parent, Qt::WFlags flags) : QMainWindow(parent, flags)
{
	QHostAddress addr;
	Settings st(&myNick, &addr);
	st.exec();

	setupUi(this);
	lineEdit->setFocusPolicy(Qt::StrongFocus);
    textEdit->setReadOnly(true);
    listWidget->setFocusPolicy(Qt::NoFocus);

	engine = new NetworkEngine(myNick, addr);

	connect(lineEdit, SIGNAL(returnPressed()), this, SLOT(returnPressed()));
    connect(engine, SIGNAL(newMessage(QString,QString)), this, SLOT(appendMessage(QString,QString)));
    connect(engine, SIGNAL(newUser(QString)), this, SLOT(newUser(QString)));
    connect(engine, SIGNAL(userLeft(QString)), this, SLOT(userLeft(QString)));

	tableFormat.setBorder(0);
	newUser(myNick);
}

void MyChat::appendMessage(const QString from, const QString message)
{
    QTextCursor cursor(textEdit->textCursor());
    cursor.movePosition(QTextCursor::End);
    QTextTable *table = cursor.insertTable(1, 2, tableFormat);
    table->cellAt(0, 0).firstCursorPosition().insertText('<' + from + ">: ");
    table->cellAt(0, 1).firstCursorPosition().insertText(message);
    QScrollBar *bar = textEdit->verticalScrollBar();
    bar->setValue(bar->maximum());
}


void MyChat::returnPressed()
{
    QString text = lineEdit->text();
    if (text.isEmpty())
        return;

    engine->_sendMessage(text);
    appendMessage(myNick, text);

    lineEdit->clear();
}


void MyChat::newUser(const QString nick)
{
    QColor color = textEdit->textColor();
    textEdit->setTextColor(Qt::gray);
	textEdit->append(tr("#%1 has joined").arg(nick));
    textEdit->setTextColor(color);
    listWidget->addItem(nick);
}

void MyChat::userLeft(const QString nick)
{
    QList<QListWidgetItem *> items = listWidget->findItems(nick, Qt::MatchExactly);
    if (items.isEmpty())
        return;

    delete items.at(0);
    QColor color = textEdit->textColor();
    textEdit->setTextColor(Qt::gray);
	textEdit->append(tr("#%1 has left").arg(nick));
    textEdit->setTextColor(color);
}