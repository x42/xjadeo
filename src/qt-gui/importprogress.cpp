#include <qprocess.h>  
#include <qtimer.h>  
#include <qdir.h>  
#include <qmessagebox.h>  
#include "qjadeo.h"
#include "importprogress.h"

ImportProgress::ImportProgress(QWidget* parent):
  QDialog(parent)
{
  setupUi(this);
  retranslateUi(this);
}

void ImportProgress::ImportCancel()
{
  encoder.terminate();
  QTimer::singleShot(5000, &encoder, SLOT(kill()));
  //TODO: unlink file ??
  close();
}

int ImportProgress::mencode(QString commandpath)
{
  QStringList argv;
  argv.append("-idx");
  argv.append("-ovc");
  argv.append("lavc");
  argv.append("-lavcopts");
  argv.append("vcodec="+enc_codec);
  if (!enc_fps.isEmpty()) {
    argv.append("-ofps");
    argv.append(enc_fps); 
//  qDebug("MENCODER OPTION: -ofps "+enc_fps);
  } 
  argv.append("-nosound");
  if (enc_w > 0 && enc_h == 0) {
    QString Temp;
    argv.append("-vf");
    argv.append("scale");
    argv.append("-zoom");
    argv.append("-xy");
    Temp.sprintf("%i",enc_w);
    argv.append(Temp);
  } else if (enc_w > 0 && enc_h > 0) {
    QString Temp;
    argv.append("-vf");
    Temp.sprintf("scale=%i:%i",enc_w,enc_h);
    argv.append(Temp);
//  qDebug("MENCODER OPTION: -vf "+Temp);
  }

  { // split enc_xargs by space and add as arguments
  QStringList xargs = enc_xargs.simplified().split(" ",QString::SkipEmptyParts);
   for ( QStringList::Iterator it = xargs.begin(); it != xargs.end(); ++it ) {
    argv.append(*it);
  //qDebug("XOPTION: "+ *it);
   }
  }
  argv.append("-o");
  argv.append(enc_dst);
  argv.append(enc_src);
  //encoder.setCommunication ( QProcess::Stdout|QProcess::Stderr );
  //qDebug(argv.join(" ").toAscii().data());

  encoder.start(commandpath, argv);
  if(!encoder.waitForStarted()) {
    QMessageBox::QMessageBox::warning( this, "Import Error","Could not launch encoder.","OK", QString::null, QString::null, 0, -1);
    return(1);
  } else {
    connect(&encoder, SIGNAL(readyReadStandardOutput ()), this, SLOT(readFromStdout()));
    connect(&encoder, SIGNAL(readyReadStandardError()), this, SLOT(readFromStderr()));
    connect(&encoder, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(encodeFinished(int, QProcess::ExitStatus)));
    return(0);
  }
}

void ImportProgress::readFromStderr()
{
  //qDebug("mencoder:"+ encoder.readStderr());
  encoder.setReadChannel(QProcess::StandardError);
  while(encoder.canReadLine()) {
    QString msg = encoder.readLine();
    //qDebug(msg.toAscii().data());
  }
}

void ImportProgress::readFromStdout()
{
    encoder.setReadChannel(QProcess::StandardOutput);
    QString response = encoder.readLine();
    QString output_text = response.simplified();
    ushort indx = output_text.indexOf( '%');
    indx = output_text.mid( indx - 2, 2 ).toInt();
    if( indx > 99) indx = 100;
    importProgressBar->setRange(0,100 ); // XXX
    importProgressBar->setValue( indx );
}

void ImportProgress::encodeFinished(int /*exitCode*/, QProcess::ExitStatus /*exitStatus*/)
{
  if (encoder.exitStatus() != 0) {
    QMessageBox::QMessageBox::warning(this, 
	"Import failed",
	"Transcode failed.","mmh.",QString::null,QString::null,0,-1);
  } else if ( QMessageBox::QMessageBox::information(this, 
	"Import Finished",
	"Transcoding video file completed.","OK", "Open it",
	QString::null, 0, 0) == 1 ) {
    QJadeo *qj = (QJadeo*) parent();
    qj->fileLoad(enc_dst);
  }
  close();
}

int ImportProgress::setEncoderFiles( QString src, QString dst )
{
  if (!QFile::exists(src)) {
  	QMessageBox::QMessageBox::warning(this, "Import Error","Source file does not exist","OK", QString::null, QString::null, 0, -1);
	return(1);
  }
  QFileInfo qfi = QFileInfo(dst);
  if ( qfi.exists() && QMessageBox::question(this, "Overwrite File?", "File exists. Do you want to overwrite it?", "&Yes", "&No", QString::null, 1, 1 ) ) return (1);
  QDir d( qfi.path() );       
  if (!d.exists()) d.mkpath(qfi.path());
  enc_src=src;
  enc_dst=dst;
  return(0);
}

void ImportProgress::setEncoderArgs( QString codec, QString fps, int w, int h )
{
  enc_fps=fps;
  enc_codec=codec;
  enc_w=w; enc_h=h;
}

void ImportProgress::setExtraArgs( QString args )
{
  enc_xargs=args;
}

/* vi:set ts=8 sts=2 sw=2: */
