#ifndef IMPORTPROGRESS_H
#define IMPORTPROGRESS_H

#include <qprocess.h>
#include "ui_importprogress.h"
class ImportProgress: public QDialog , public Ui::ImportProgress
{
	Q_OBJECT
	public:
		ImportProgress(QWidget *parent = 0);
	public slots:
		int mencode(QString commandpath);
		int setEncoderFiles( QString src, QString dst);
		void setEncoderArgs( QString codec, QString fps, int w, int h );
		void setExtraArgs( QString args );

		void ImportCancel();
		void readFromStderr();
		void readFromStdout();
		void encodeFinished(int, QProcess::ExitStatus);
	private:
		QProcess encoder;
		QString enc_codec;
		QString enc_xargs;
		QString enc_fps;
		int enc_w, enc_h;
		QString enc_dst;
		QString enc_src;

};

#endif
