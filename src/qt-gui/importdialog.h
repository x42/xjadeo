#ifndef IMPORTDIALOG_H
#define IMPORTDIALOG_H
#include <qprocess.h>

#include "ui_importdialog.h"
class ImportDialog: public QDialog , public Ui::ImportDialog
{
	Q_OBJECT
	public:
		ImportDialog(QWidget *parent = 0);
		QString xjinfo;
		QString dstDir;

	public slots:
		void importSrcSelect();
		void importDstSelect();
		void widthCheckBox_toggled( bool t);
		void fpsCheckBox_toggled( bool t );
		void aspectCheckBox_toggled( bool t);
		void readFromStdout();
		void importAvInfo();
		void infoFinished();

	private:
		QProcess *infoproc;
};
#endif
