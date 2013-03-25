#include "mychat.h"
#include <QtGui/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	MyChat w;
	w.show();
	return a.exec();
}
