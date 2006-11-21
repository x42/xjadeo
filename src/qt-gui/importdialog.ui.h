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

#include <qfiledialog.h>
#include <qlineedit.h> 
#include <qfileinfo.h> 
#include <qprocess.h>  
#include <qmessagebox.h>  

void ImportDialog::importSrcSelect()
{
  QString s = QFileDialog::getOpenFileName("",
					   0,
					   this,
					   "import video file",
					   "select video file to convert");
  if(!s.isNull()) {
    SourceLineEdit->setText(s);
    if (DestLineEdit->text().isEmpty())	 {
    	QFileInfo qfi = QFileInfo(s);
	QString dstfolder;
  	if (dstDir.isEmpty())
		dstfolder = qfi.dirPath();
	else
		dstfolder = dstDir;
    	DestLineEdit->setText(dstfolder+"/"+qfi.baseName()+"-xj.avi");
    }
  }
}

void ImportDialog::importDstSelect()
{
  QString s = QFileDialog::getSaveFileName("",
					   0,
					   this,
					   "save converted file",
					   "choose a destinaton filename");
  if(!s.isNull()) {
    DestLineEdit->setText(s);
  }
}


void ImportDialog::widthCheckBox_toggled( bool t)
{
  widthSpinBox->setEnabled(t);
  if (!t)heightSpinBox->setEnabled(t);
  else if (aspectCheckBox->isOn()) 
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
  avInfoLabel->setText( infoproc->readStdout());
}

void ImportDialog::importAvInfo()
{
  infoproc = new QProcess( this );
  infoproc->addArgument( xjinfo );
  infoproc->addArgument( "-v" ); 
  infoproc->addArgument( SourceLineEdit->text() );
  connect( infoproc, SIGNAL(readyReadStdout()), this, SLOT(readFromStdout()) );
  connect( infoproc, SIGNAL(processExited()), this, SLOT(infoFinished()));
  if ( !infoproc->start() ) {
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
