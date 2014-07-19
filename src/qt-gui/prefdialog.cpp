#include "prefdialog.h"

#include <qvariant.h>
#include <qimage.h>
#include <qpixmap.h>
/*
 *  Constructs a PrefDialog as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  true to construct a modal dialog.
 */
PrefDialog::PrefDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi(this);
		retranslateUi(this);
}

/*
 *  Destroys the object and frees any allocated resources
 */
PrefDialog::~PrefDialog()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
void PrefDialog::languageChange()
{
    retranslateUi(this);
}
 */

//#include <qfiledialog.h>

void PrefDialog::selPrefsDest()
{
#if 0
  QString s = QFileDialog::getExistingDirectory(this, 
			tr("Select default destination folder"),
		  "",
			QFileDialog::ShowDirsOnly| QFileDialog::DontResolveSymlinks);
  }
#endif
}


void PrefDialog::prefDirEnable(bool /*t*/)
{
#if 0
  destDirLineEdit->setEnabled(t);
  selDestPushButton->setEnabled(t);
#endif
}

