#include <qfiledialog.h>
#include <qlineedit.h> 
#include <qfileinfo.h> 
#include <qprocess.h>  
#include <qmessagebox.h>  
#include "importdialog.h"

ImportDialog::ImportDialog(QWidget* parent):
  QDialog(parent)
{
  setupUi(this);
  retranslateUi(this);
}

void ImportDialog::importSrcSelect()
{
  QString s = QFileDialog::getOpenFileName(this, tr("select video file to convert"));
  if(!s.isNull()) {
    SourceLineEdit->setText(s);
    if (DestLineEdit->text().isEmpty())	 {
    	QFileInfo qfi = QFileInfo(s);
	QString dstfolder;
  	if (dstDir.isEmpty())
		dstfolder = qfi.path();
	else
		dstfolder = dstDir;
    	DestLineEdit->setText(dstfolder+"/"+qfi.baseName()+"-xj.avi");
    }
  }
}

void ImportDialog::importDstSelect()
{
  QString s = QFileDialog::getSaveFileName(this, tr("choose a destinaton filename"));
  if(!s.isNull()) {
    DestLineEdit->setText(s);
  }
}


void ImportDialog::widthCheckBox_toggled( bool t)
{
  widthSpinBox->setEnabled(t);
  if (!t)heightSpinBox->setEnabled(t);
  else if (aspectCheckBox->isChecked()) 
      heightSpinBox->setEnabled(t);
  aspectCheckBox->setEnabled(t);
}


void ImportDialog::fpsCheckBox_toggled( bool t )
{
  fpsComboBox->setEnabled(t);
}


void ImportDialog::aspectCheckBox_toggled( bool t)
{
  heightSpinBox->setEnabled(t);
}


void ImportDialog::readFromStdout()
{
  avInfoLabel->setText( infoproc->readLine());
}

void ImportDialog::importAvInfo()
{
  infoproc = new QProcess( this );
  QStringList argv;
  argv.append( "-v" ); 
  argv.append( SourceLineEdit->text() );
  infoproc->closeReadChannel(QProcess::StandardError);
  infoproc->setReadChannel(QProcess::StandardOutput);
  connect( infoproc, SIGNAL(readyReadStandardOutput ()), this, SLOT(readFromStdout()) );
  connect( infoproc, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(infoFinished()));
  infoproc->start(xjinfo, argv);
  if (!infoproc->waitForStarted()) {
     QMessageBox::warning( 0, "Warning", "Could not start the xjinfo command.", "OK" ); 
  }
}

void ImportDialog::infoFinished()
{
  if (infoproc->exitStatus() != 0) {
    avInfoLabel->setText(" ");
    QMessageBox::QMessageBox::warning(this, 
	"xjinfo failed",
	"Error occured while detecting file informtaion.","aha.",QString::null,QString::null,0,-1);
  }
}

/* vi:set ts=8 sts=2 sw=2: */
