#ifndef PREFDIALOG_H
#define PREFDIALOG_H
#include "ui_prefdialog.h"
class PrefDialog: public QDialog , Ui_PrefDialog
{
	public:
  PrefDialog(QWidget* parent, const char* name, bool modal, Qt::WindowFlags fl);
 ~PrefDialog();
};
#endif
