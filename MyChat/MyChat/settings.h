#ifndef SETTINGS_H
#define SETTINGS_H

#include <QtNetwork>
#include <qstring.h>
#include "ui_SettingDialog.h"

class Settings : public QDialog, private Ui::dialog {
	Q_OBJECT

public:
	Settings(QString * name, QHostAddress * addr, QWidget *parent = 0, Qt::WFlags flags = 0);
	
private slots:
	void setData();

private:
	QString * name;
	QHostAddress * addr;
};

#endif // SETTINGS_H