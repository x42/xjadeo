#include <qapplication.h>
#include <qprocess.h>
#include <qtimer.h>
#include <qfiledialog.h>
#include <qmessagebox.h>
#include <qpopupmenu.h>
#include <qstatusbar.h>
#include <qprogressbar.h>
#include <qspinbox.h>
#include <qslider.h>
#include <qlineedit.h> 
#include <qcombobox.h> 
#include <qcheckbox.h> 
#include <qdesktopwidget.h>

#include "qjadeo.h"
#include "bindir.h"
// Global variables

QProcess xjadeo;

///////////////////////////////////////////////////////
// QJadeo window
///////////////////////////////////////////////////////

// Constructor

#define MAX_RECENTFILES 5

QJadeo::QJadeo()
{

  // Shamelessly stolen from qjackctl. Qt doc misleading.
  m_settings.beginGroup("/qjadeo");

  int windowWidth = m_settings.readNumEntry("/WindowWidth", 460);
  int windowHeight = m_settings.readNumEntry("WindowHeight", 530);
  int windowX = m_settings.readNumEntry("WindowX", -1);
  int windowY = m_settings.readNumEntry("WindowY", -1);

  resize(windowWidth, windowHeight);
  if(windowX != -1 || windowY != -1)
    move(windowX, windowY);

  for(int i = 0; i < MAX_RECENTFILES; ++i)
  {
    QString filename = m_settings.readEntry("File" + QString::number(i + 1));

    if(!filename.isEmpty())
      m_recentFiles.push_back(filename);
  }
  if(m_recentFiles.count())
    updateRecentFilesMenu();

  m_midiport = m_settings.readEntry("MIDI port");
  // TODO: detect portmidi / alsamidi default. 'midi library' 
  if (m_midiport.isEmpty()) m_midiport = QString("24");
  m_importdir = m_settings.readEntry("Import Directory");
  m_importdestination = m_settings.readBoolEntry("Import Destination");
  m_importcodec = m_settings.readEntry("Import Codec");
  if (m_importcodec.isEmpty()) m_importcodec = QString("mpeg4");
  m_osdfont = m_settings.readEntry("OSD font");
  xjadeo.writeToStdin("osd font " + m_osdfont + "\nosd text \n");

  statusBar()->hide();

}

// Recent files list

void QJadeo::updateRecentFilesMenu()
{
  for(int i = 0; i < MAX_RECENTFILES; ++i)
  {
    if(fileMenu->findItem(i))
      fileMenu->removeItem(i);
    if(i < int (m_recentFiles.count()))
      fileMenu->insertItem(QString("&%1 %2").arg(i + 1).arg(m_recentFiles[i]),
			   this, SLOT(fileOpenRecent(int)), 0, i);
  }
}

void QJadeo::updateRecentFiles(const QString & filename)
{
  if(m_recentFiles.find(filename) != m_recentFiles.end())
    return;

  m_recentFiles.push_back(filename);
  if(m_recentFiles.count() > MAX_RECENTFILES)
    m_recentFiles.pop_front();

  updateRecentFilesMenu();
}

void QJadeo::fileOpenRecent(int index)
{
  fileLoad(m_recentFiles[index]);
}

// Slots

void QJadeo::saveOptions()
{
  m_settings.writeEntry("WindowWidth", width());
  m_settings.writeEntry("WindowHeight", height());
  m_settings.writeEntry("WindowX", x());
  m_settings.writeEntry("WindowY", y());
  for(int i = 0; i < int (m_recentFiles.count()); ++i)
    m_settings.writeEntry("File" + QString::number(i + 1), m_recentFiles[i]);
  m_settings.writeEntry("OSD font", m_osdfont);
  m_settings.writeEntry("MIDI port", m_midiport);
  m_settings.writeEntry("Import Command", m_importcodec);
  m_settings.writeEntry("Import Directory", m_importdir);
  m_settings.writeEntry("Import Destination", m_importdestination);
}

void QJadeo::fileOpen()
{
  QString s = QFileDialog::getOpenFileName("",
					   0,
					   this,
					   "Browse file dialog",
					   "Choose a file");

  if(!s.isNull())
  {
    fileLoad(s);
  }

}

void QJadeo::fileDisconnect()
{
  saveOptions();
  close();
}

void QJadeo::fileExit()
{
  xjadeo.writeToStdin(QString("quit\n"));
  xjadeo.writeToStdin(QString("exit\n"));
  saveOptions();
  //close(); // we will terminate when xjadeo/xjremote does.
}

void QJadeo::filePreferences()
{
  PrefDialog *pdialog = new PrefDialog::PrefDialog(this);
  if (pdialog) {
    pdialog->prefLineMidi->setText(m_midiport);
    pdialog->codecComboBox->setCurrentText(m_importcodec);
    pdialog->destDirLineEdit->setText(m_importdir);
    if (m_importdestination)
      pdialog->prefDirCheckBox->toggle();
    pdialog->destDirLineEdit->setEnabled(m_importdestination);
    if( pdialog->exec()) {
      m_importcodec = pdialog->codecComboBox->currentText();
      if (!pdialog->prefLineMidi->text().isEmpty())
	m_midiport = pdialog->prefLineMidi->text();
      if (pdialog->prefDirCheckBox->isOn() && !pdialog->destDirLineEdit->text().isEmpty())
	m_importdir = pdialog->destDirLineEdit->text();
      m_importdestination = pdialog->prefDirCheckBox->isOn();
      saveOptions();
    }
    delete pdialog;
  }
}

void QJadeo::fileImport()
{
  ImportDialog *idialog = new ImportDialog::ImportDialog(this);
  qDebug("Import: ");

  if (idialog) {
    QString src, dst;
    QString fps = QString("");
    ImportProgress *iprog = NULL;
    int w,h;
    w=h=0;
    if (m_importdestination) 
      idialog->dstDir = m_importdir;
    /* get settings */
    if(idialog->exec()) {
      src = idialog->SourceLineEdit->text();
      dst = idialog->DestLineEdit->text();
      if (idialog->widthCheckBox->isOn()) {
	w = idialog->widthSpinBox->value();
        if (idialog->aspectCheckBox->isOn()) 
	  h = idialog->heightSpinBox->value();
      }
      if (idialog->fpsCheckBox->isOn()) 
	fps = idialog->fpsComboBox->currentText();
    }
    /* start encoding */
    if(!src.isEmpty() && !dst.isEmpty()) 
      iprog = new ImportProgress::ImportProgress(this);
    if(iprog) { 
      if(!iprog->encode(src,dst,w,h,fps,m_importcodec)) {
	iprog->setModal(TRUE);
	iprog->show(); 
      } else
	delete iprog;
    }
    delete idialog;
  }
}

void QJadeo::helpAbout()
{
  QMessageBox::about(
    this,
    "About qjadeo",
    "(c) 2006 Robin Gareus & Luis Garrido\n"
    "http://xjadeo.sf.net"
  );
}

void QJadeo::zoom50()
{
  xjadeo.writeToStdin(QString("window resize 50\n"));
}

void QJadeo::zoom100()
{
  xjadeo.writeToStdin(QString("window resize 100\n"));
}

void QJadeo::zoom200()
{
  xjadeo.writeToStdin(QString("window resize 200\n"));
}

void QJadeo::zoomFullScreen()
{
  QDesktopWidget *d = QApplication::desktop();
  xjadeo.writeToStdin(QString("window position 0x0\n"));
  xjadeo.writeToStdin(
    "window resize " + QString::number(d->width()) +
    "x" + QString::number(d->height()) + "\n"
  );
}

void QJadeo::syncJack()
{
  xjadeo.writeToStdin(QString("midi disconnect\n"));
  xjadeo.writeToStdin(QString("jack connect\n"));
  xjadeo.writeToStdin(QString("get syncsource\n"));
}

void QJadeo::syncMTC()
{
  xjadeo.writeToStdin(QString("jack disconnect\n"));
  //xjadeo.writeToStdin(QString("midi reconnect\n"));
  xjadeo.writeToStdin(QString("midi connect "+ m_midiport +"\n"));
  xjadeo.writeToStdin(QString("get syncsource\n"));
}

void QJadeo::syncOff()
{
  xjadeo.writeToStdin(QString("jack disconnect\n"));
  xjadeo.writeToStdin(QString("midi disconnect\n"));
  xjadeo.writeToStdin(QString("get syncsource\n"));
}

void QJadeo::setFPS(const QString &fps)
{
  xjadeo.writeToStdin("set fps " + fps + "\n");
}

void QJadeo::setOffset(const QString &offset)
{
  xjadeo.writeToStdin("set offset " + offset + "\n");
}

void QJadeo::osdFrameToggled(bool value)
{
  if(value)
    xjadeo.writeToStdin(QString("osd frame 0\n"));
  else
    xjadeo.writeToStdin(QString("osd frame -1\n"));
}

void QJadeo::osdSMPTEToggled(bool value)
{
  if(value)
    xjadeo.writeToStdin(QString("osd smpte 100\n"));
  else
    xjadeo.writeToStdin(QString("osd smpte -1\n"));
}

void QJadeo::seekBarChanged( int value )
{
  int frame=0;
  QString Temp;
  if(m_frames > 0) {
  	frame = value*m_frames/1000;	
  }
  Temp.sprintf("seek %i\n",frame);
  xjadeo.writeToStdin(QString(Temp));
}

void QJadeo::osdFont()
{
  QString s = QFileDialog::getOpenFileName("",
					   "TrueType fonts (*.ttf)",
					   this,
					   "Browse font dialog",
					   "Choose a font");

  if(!s.isNull())
  {
    m_osdfont = s;
    xjadeo.writeToStdin("osd font " + s + "\n");
  }
}

// Called when xjadeo outputs to stdout

void QJadeo::readFromStdout()
{

  while(xjadeo.canReadLineStdout())
  {
    QString response = xjadeo.readLineStdout();
    //qDebug("Response: " + response);

    int status = response.mid(1, 3).toInt();

    switch(status / 100)
    {
      case 4:
      {
        switch(status)
        {
          case 410:   // get filename -> no open video file
            break;
          default:
            QMessageBox::critical(
              this,
              "qjadeo",
              "Error " + response, "&Close"
            );
        }
        break;
      }
      case 2:
      case 3:
      {
        int equalsign = response.find('=');
        QString name = response.mid(5, equalsign - 5);
        QString value = response.right(response.length() - equalsign - 1);
        if(name == "position")
        {
          if(m_frames > 0)
            progressBar->setProgress(value.toInt() + 1, m_frames);
        }
        else if(name == "filename")
          updateRecentFiles(value);
        else if(name == "movie_width")
          m_movie_width = value.toInt();
        else if(name == "movie_height")
          m_movie_height = value.toInt();
        else if(name == "syncsource")
	{
	  Sync->setItemChecked(Sync->idAt(0),FALSE);
	  Sync->setItemChecked(Sync->idAt(1),FALSE);
	  Sync->setItemChecked(Sync->idAt(2),FALSE);

	  if (value.toInt()==0) { // off
            seekBar->setEnabled(TRUE);
	    Sync->setItemChecked(Sync->idAt(2),TRUE);
	  } else if (value.toInt()==2) { // MIDI
            seekBar->setEnabled(FALSE);
	    Sync->setItemChecked(Sync->idAt(1),TRUE);
	  } else {
            seekBar->setEnabled(FALSE); //JACK
	    Sync->setItemChecked(Sync->idAt(0),TRUE);
	  }
	}
        else if(name == "osdfont")
	{
	  ;// set default folder and file in 
	   // File selection dialog.
	}
        else if(name == "osdmode")
	{
	  int v= value.toInt();
	  OSD->setItemChecked(OSD->idAt(1),v&1);
	  OSD->setItemChecked(OSD->idAt(2),v&2);
	}
        else if(name == "frames")
	{
          m_frames = value.toInt();
	}
        else if(name == "offset")
	{
          m_offset = value.toInt();
          offsetSpinBox->setValue(m_offset);
        }
	else if(name == "framerate")
        {
          m_framerate = value.toInt();
          fpsSpinBox->setValue(m_framerate);
          xjadeo.writeToStdin("set fps " + value + "\n");
        }
        else if(name == "updatefps")
          m_updatefps = value.toInt();
        break;
      }
      case 1:
        if(status==129) {
	  xjadeo.writeToStdin(QString("get filename\n"));
	  xjadeo.writeToStdin(QString("get width\n"));
	  xjadeo.writeToStdin(QString("get height\n"));
	  xjadeo.writeToStdin(QString("get frames\n"));
	  xjadeo.writeToStdin(QString("get framerate\n"));
	  xjadeo.writeToStdin(QString("notify frame\n"));
	  xjadeo.writeToStdin(QString("get offset\n"));
	  xjadeo.writeToStdin(QString("get osdcfg\n"));
	  xjadeo.writeToStdin(QString("get syncsource\n"));
	  xjadeo.writeToStdin(QString("get position\n"));
	}
    }
  }
}

void QJadeo::fileLoad(const QString & filename)
{

  m_movie_width = 0;
  m_movie_height = 0;
  m_updatefps = 0;
  m_frames = 0;
  m_offset = 0;
  m_framerate = 0;
  xjadeo.writeToStdin("load " + filename + "\n");
  xjadeo.writeToStdin(QString("get filename\n"));
  xjadeo.writeToStdin(QString("get width\n"));
  xjadeo.writeToStdin(QString("get height\n"));
  xjadeo.writeToStdin(QString("get frames\n"));
  xjadeo.writeToStdin(QString("get framerate\n"));
  xjadeo.writeToStdin(QString("notify frame\n"));
  xjadeo.writeToStdin(QString("get offset\n"));
  xjadeo.writeToStdin(QString("get position\n"));

}

// Program entry point

int main(int argc, char **argv)
{

  QApplication a(argc, argv);

// Launch xjadeo

  QString xjadeoPath(getenv("XJADEO"));

  // TODO: use "/xjadeo" ifndef HAVE_MQ  
  // change env(XJADEO) -> env(XJREMOTE)
  if(xjadeoPath.isEmpty())
    xjadeoPath = BINDIR "/xjremote";

  xjadeo.addArgument(xjadeoPath);
  xjadeo.addArgument("-R");

  if(!xjadeo.start())
  {
    qFatal("Could not start xjadeo executable: " + xjadeoPath);
  }


  QJadeo w;

  w.connect(&xjadeo, SIGNAL(readyReadStdout()), &w, SLOT(readFromStdout()));
  w.connect(&xjadeo, SIGNAL(processExited()), &w, SLOT(close()));

  w.show();
  a.connect(&a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()));
  xjadeo.writeToStdin(QString("get width\n"));
  xjadeo.writeToStdin(QString("get height\n"));
  xjadeo.writeToStdin(QString("get frames\n"));
  xjadeo.writeToStdin(QString("get framerate\n"));
  xjadeo.writeToStdin(QString("get offset\n"));
  xjadeo.writeToStdin(QString("get osdcfg\n"));
  xjadeo.writeToStdin(QString("get syncsource\n"));
  xjadeo.writeToStdin(QString("get position\n"));
  xjadeo.writeToStdin(QString("notify frame\n"));

  a.exec();

  xjadeo.writeToStdin(QString("notify off\n"));

  xjadeo.tryTerminate();
  QTimer::singleShot(5000, &xjadeo, SLOT(kill()));

}

/* vi:set ts=8 sts=2 sw=2: */
