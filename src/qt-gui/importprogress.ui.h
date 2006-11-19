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

int ImportProgress::encode( QString src, QString dst, int w, int h, QString fps, QString vcodec )
{
  QFileInfo qfi = QFileInfo(dst);
  if ( qfi.exists() && QMessageBox::question( this, "Overwrite File?", "File exists. Do you want to overwrite it?", "&Yes", "&No", QString::null, 0, 1 ) ) return (1);
  QDir d( qfi.dirPath() );       
  if (!d.exists()) d.mkdir(qfi.dirPath(),TRUE);
  encoder.addArgument("mencoder");
  encoder.addArgument("-idx");
  encoder.addArgument("-ovc");
  encoder.addArgument("lavc");
  encoder.addArgument("-lavcopts");
  encoder.addArgument("vcodec="+vcodec);
  if (fps.toInt() > 0) {
    encoder.addArgument("-ofps");
    encoder.addArgument(fps); 
    qDebug("MENCODER OPTION: -ofps "+fps);
  }
  encoder.addArgument("-nosound");
  if (w > 0 && h == 0) {
    QString Temp;
    encoder.addArgument("-vf");
    encoder.addArgument("scale");
    encoder.addArgument("-zoom");
    encoder.addArgument("-xy");
    Temp.sprintf("%i",w);
    encoder.addArgument(Temp);
  } else if (w > 0 && h > 0) {
    QString Temp;
    encoder.addArgument("-vf");
    Temp.sprintf("scale=%i:%i",w,h);
    encoder.addArgument(Temp);
    qDebug("MENCODER OPTION: "+Temp);
  }
  encoder.addArgument(src);
  encoder.addArgument("-o");
  encoder.addArgument(dst);
  encoder.setCommunication ( QProcess::Stdout|QProcess::Stderr );

  if(!encoder.start()) {
    qDebug("Could not start encoder!");
    //  QMessageBox::critical( 0, "Could not start encoder!");
    return(1);
  } else {
    qDebug("started encoder..");
    connect(&encoder, SIGNAL(readyReadStdout()), this, SLOT(readFromStdout()));
    connect(&encoder, SIGNAL(readyReadStderr()), this, SLOT(readFromStderr()));
    connect(&encoder, SIGNAL(processExited()), this, SLOT(encodeFinished()));
    return(0);
  }
}

void ImportProgress::readFromStderr()
{
  qDebug( encoder.readStderr() );
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
  qDebug("encoding done.");
  close();
}

/* vi:set ts=8 sts=2 sw=2: */



void ImportProgress::newFunction()
{

}
