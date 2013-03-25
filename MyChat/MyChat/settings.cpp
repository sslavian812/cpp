#include "settings.h"

Settings::Settings(QString * name, QHostAddress * addr, QWidget *parent, Qt::WFlags flags) : name(name), addr(addr) {
	setupUi(this);

	QList<QHostAddress> addrList = QNetworkInterface::allAddresses();
	QList<QString> strAddrList;
	for(QList<QHostAddress>::Iterator it = addrList.begin(); it != addrList.end(); ++it)
		strAddrList.push_back(it->toString());
	comboBox->addItems(strAddrList);

	connect(pushButton, SIGNAL(clicked()), this, SLOT(setData()));
}

void Settings::setData() {
	QString text = lineEdit->text();
    if(text.isEmpty())
        return;
	*addr = QHostAddress(comboBox->currentText());
	*name = text;
	accept();
}