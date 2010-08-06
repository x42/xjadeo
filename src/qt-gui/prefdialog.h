#ifndef PREFDIALOG_H
#define PREFDIALOG_H
#include "ui_prefdialog.h"
class PrefDialog: public QDialog , public Ui::PrefDialog
{
	Q_OBJECT
	public:
		PrefDialog(QWidget *parent = 0);
		~PrefDialog();
	private slots:
		void selPrefsDest();
		void prefDirEnable( bool t);
};

#endif
