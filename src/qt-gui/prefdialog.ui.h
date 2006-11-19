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

void PrefDialog::selPrefsDest()
{
  QString s = QFileDialog::getExistingDirectory("",
	this,
	"default directory",
	"select default destination folder", TRUE);
  if (!s.isEmpty()) {
      destDirLineEdit->setText(s);
  }
}


void PrefDialog::prefDirEnable( bool t)
{
  destDirLineEdit->setEnabled(t);
  selDestPushButton->setEnabled(t);
}
