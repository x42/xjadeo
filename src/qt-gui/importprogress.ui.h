/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/
#include <qprocess.h>  
#include <qtimer.h>  
#include <qdir.h>  
#include <qmessagebox.h>  

void ImportProgress::ImportCancel()
{
  encoder.tryTerminate();
  QTimer::singleShot(5000, &encoder, SLOT(kill()));
  //TODO: unlink file ??
  close();
}

int ImportProgress::mencode(QString commandpath)
{
  encoder.addArgument(commandpath);
  encoder.addArgument("-idx");
  encoder.addArgument("-ovc");
  encoder.addArgument("lavc");
  encoder.addArgument("-lavcopts");
  encoder.addArgument("vcodec="+enc_codec);
  if (!enc_fps.isEmpty()) {
    encoder.addArgument("-ofps");
    encoder.addArgument(enc_fps); 
//  qDebug("MENCODER OPTION: -ofps "+enc_fps);
  } 
  encoder.addArgument("-nosound");
  if (enc_w > 0 && enc_h == 0) {
    QString Temp;
    encoder.addArgument("-vf");
    encoder.addArgument("scale");
    encoder.addArgument("-zoom");
    encoder.addArgument("-xy");
    Temp.sprintf("%i",enc_w);
    encoder.addArgument(Temp);
  } else if (enc_w > 0 && enc_h > 0) {
    QString Temp;
    encoder.addArgument("-vf");
    Temp.sprintf("scale=%i:%i",enc_w,enc_h);
    encoder.addArgument(Temp);
//  qDebug("MENCODER OPTION: -vf "+Temp);
  }

  { // split enc_xargs by space and add as arguments
  QStringList xargs = QStringList::split(" ",enc_xargs.simplifyWhiteSpace());
   for ( QStringList::Iterator it = xargs.begin(); it != xargs.end(); ++it ) {
    encoder.addArgument(*it);
    qDebug("XOPTION: "+ *it);
   }
  }
  encoder.addArgument("-o");
  encoder.addArgument(enc_dst);
  encoder.addArgument(enc_src);
  encoder.setCommunication ( QProcess::Stdout|QProcess::Stderr );

  if(!encoder.start()) {
    QMessageBox::QMessageBox::warning( this, "Import Error","Could not launch encoder.","OK", QString::null, QString::null, 0, -1);
    return(1);
  } else {
    connect(&encoder, SIGNAL(readyReadStdout()), this, SLOT(readFromStdout()));
    connect(&encoder, SIGNAL(readyReadStderr()), this, SLOT(readFromStderr()));
    connect(&encoder, SIGNAL(processExited()), this, SLOT(encodeFinished()));
    return(0);
  }
}

void ImportProgress::readFromStderr()
{
  //qDebug("mencoder:"+ encoder.readStderr());
  while(encoder.canReadLineStderr()) {
    QString msg = encoder.readLineStderr();
    qDebug("mencoder: " + msg);
  }
}

void ImportProgress::readFromStdout()
{
    QString response = encoder.readStdout();
    QString output_text = response.simplifyWhiteSpace();
    ushort indx = output_text.find( '%', 0, TRUE);
    indx = output_text.mid( indx - 2, 2 ).toInt();
    if( indx > 99) indx = 100;
    if( indx <= 100 && indx > importProgressBar->progress())
      importProgressBar->setProgress( indx );
}

void ImportProgress::encodeFinished()
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
  QDir d( qfi.dirPath() );       
  if (!d.exists()) d.mkdir(qfi.dirPath(),TRUE);
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
