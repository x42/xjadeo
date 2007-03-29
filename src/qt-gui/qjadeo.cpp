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
#include <qtextcodec.h>

#include "qjadeo.h"
#include "mydirs.h"

#include <math.h>

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
  m_importdir = m_settings.readEntry("Import Directory");
  m_importdestination = m_settings.readBoolEntry("Import Destination");
  m_importcodec = m_settings.readEntry("Import Codec");
  m_xjadeopath = m_settings.readEntry("XJADEO Path");
  m_mencoderpath = m_settings.readEntry("MENCODER Path");
  m_mencoderopts = m_settings.readEntry("MENCODER Options");
  m_xjinfopath = m_settings.readEntry("XJINFO Path");

  // TODO: detect portmidi / alsamidi default. 'midi library' 
  if (m_midiport.isEmpty()) m_midiport = QString("24");
  if (m_importcodec.isEmpty()) m_importcodec = QString("mpeg4");
  if (m_mencoderpath.isEmpty()) m_mencoderpath = QString("mencoder");
  if (m_xjadeopath.isEmpty()) m_xjadeopath = QString(BINDIR "xjremote");
  if (m_xjinfopath.isEmpty()) m_xjinfopath = QString(BINDIR "xjinfo");
  m_osdfont = m_settings.readEntry("OSD font");

  fileMenu->setItemEnabled(fileMenu->idAt(1),testexec(m_mencoderpath));
  statusBar()->hide();

}

void QJadeo::initialize () 
{
  xjadeo.writeToStdin("osd font " + m_osdfont + "\nosd text \n");
  xjadeo.writeToStdin(QString("get width\n"));
  xjadeo.writeToStdin(QString("get height\n"));
  xjadeo.writeToStdin(QString("get frames\n"));
  xjadeo.writeToStdin(QString("get framerate\n"));
  xjadeo.writeToStdin(QString("get offset\n"));
  xjadeo.writeToStdin(QString("get osdcfg\n"));
  xjadeo.writeToStdin(QString("get syncsource\n"));
  xjadeo.writeToStdin(QString("get position\n"));
  xjadeo.writeToStdin(QString("get seekmode\n"));
  xjadeo.writeToStdin(QString("notify frame\n"));


}

/* search for external executable 
 * returns true if file is found in env(PATH) 
 * and is executable.
 */
bool QJadeo::testexec(QString exe)
{
  int timeout=10;
  QProcess testbin; 
  /* FIXME: there MUST be a better way 
   * than to simply try exeute it .. */
  testbin.addArgument(exe);
  testbin.addArgument("-V");
//testbin.setCommunication(0);
  if(!testbin.start()) return (0);
  while (testbin.isRunning() && timeout--) usleep (10000);
  if (timeout<1) testbin.kill();
//if (testbin.exitStatus() != 0) return(0);
  return(1);
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
  m_settings.writeEntry("Import Codec", m_importcodec);
  m_settings.writeEntry("Import Directory", m_importdir);
  m_settings.writeEntry("Import Destination", m_importdestination);
  m_settings.writeEntry("XJADEO Path", m_xjadeopath);
  m_settings.writeEntry("XJINFO Path", m_xjinfopath);
  m_settings.writeEntry("MENCODER Path", m_mencoderpath);
  m_settings.writeEntry("MENCODER Options", m_mencoderopts);
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
  xjadeo.writeToStdin(QString("notify disable\n"));
  xjadeo.writeToStdin(QString("exit\n"));
  saveOptions();
  // xjremote will exit and we'll go down with it
  // xjadeo will return a @489 error message 
  //close();
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
    /* set values */
    pdialog->prefLineMidi->setText(m_midiport);
    pdialog->prefLineXjadeo->setText(m_xjadeopath);
    pdialog->prefLineXjinfo->setText(m_xjinfopath);
    pdialog->prefLineMencoder->setText(m_mencoderpath);
    pdialog->prefLineMcOptions->setText(m_mencoderopts);
    pdialog->codecComboBox->setCurrentText(m_importcodec);
    pdialog->destDirLineEdit->setText(m_importdir);
    if (m_importdestination)
      pdialog->prefDirCheckBox->toggle();
    pdialog->destDirLineEdit->setEnabled(m_importdestination);
    /* exec dialog */
    if( pdialog->exec()) {
    /* apply settings */
      m_importcodec = pdialog->codecComboBox->currentText();
      m_importdestination = pdialog->prefDirCheckBox->isOn();
      m_mencoderpath = pdialog->prefLineMencoder->text();
      fileMenu->setItemEnabled(fileMenu->idAt(1),testexec(m_mencoderpath));
      m_mencoderopts = pdialog->prefLineMcOptions->text();
      if (!pdialog->prefLineXjadeo->text().isEmpty())
	m_xjadeopath = pdialog->prefLineXjadeo->text();
      if (!pdialog->prefLineXjinfo->text().isEmpty())
	m_xjinfopath = pdialog->prefLineXjinfo->text();
      if (!pdialog->prefLineMidi->text().isEmpty())
	m_midiport = pdialog->prefLineMidi->text();
      if (pdialog->prefDirCheckBox->isOn() && !pdialog->destDirLineEdit->text().isEmpty())
	m_importdir = pdialog->destDirLineEdit->text();
    /* and save */
      saveOptions();
    }
    delete pdialog;
  }
}

void QJadeo::fileImport()
{
  ImportDialog *idialog = new ImportDialog::ImportDialog(this);

  if (idialog) {
    QString src, dst;
    QString fps = QString("");
    QString xargs = QString("");
    ImportProgress *iprog = NULL;
    int w,h;
    w=h=0;
    if (m_importdestination) 
      idialog->dstDir = m_importdir;
    idialog->mcOptionsLineEdit->setText(m_mencoderopts);
    idialog->xjinfo = m_xjinfopath;
    /* get settings */
    if(idialog->exec()) {
      src = idialog->SourceLineEdit->text();
      dst = idialog->DestLineEdit->text();
      xargs = idialog->mcOptionsLineEdit->text();
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
      if(!iprog->setEncoderFiles(src,dst)) {
        iprog->setEncoderArgs(m_importcodec,fps,w,h);
        iprog->setExtraArgs(xargs);
        iprog->mencode(m_mencoderpath);
	iprog->setModal(FALSE);
	iprog->show(); 
      } else delete iprog;
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

void QJadeo::seekAnyFrame()
{
  xjadeo.writeToStdin(QString("set seekmode 2\n"));
}


void QJadeo::seekContinuously()
{
  xjadeo.writeToStdin(QString("set seekmode 1\n"));
}


void QJadeo::seekKeyFrames()
{
  xjadeo.writeToStdin(QString("set seekmode 3\n"));
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
          case 489:   // -> tried "exit" on xjadeo (not xjremote)
	    xjadeo.writeToStdin(QString("quit\n"));
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
        int comment = response.find('#');
	if (comment > 0) response.truncate(comment);
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
        else if(name == "seekmode")
	{
	  Seek->setItemChecked(Seek->idAt(0),FALSE);
	  Seek->setItemChecked(Seek->idAt(1),FALSE);
	  Seek->setItemChecked(Seek->idAt(2),FALSE);
	  if (value.toInt()==3) {  // key
	    Seek->setItemChecked(Seek->idAt(1),TRUE);
	  } else if (value.toInt()==1) {  // cont
	    Seek->setItemChecked(Seek->idAt(2),TRUE);
	  } else {
	    Seek->setItemChecked(Seek->idAt(0),TRUE);
	  }
	}
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
          m_framerate = (int)rint(value.toDouble()); // TODO: round value / allow
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

    // Load translation support.
    QTranslator translator(0);
    QString sLocale = QTextCodec::locale();
    if (sLocale != "C") {
        QString sLocName = "qjadeo_" + sLocale;
        if (!translator.load(sLocName, ".")) {
            QString sLocPath = CONFIG_PREFIX;
            if (!translator.load(sLocName, sLocPath))
                fprintf(stderr, "Warning: no locale found: %s/%s.qm\n", sLocPath.latin1(), sLocName.latin1());
        }
        a.installTranslator(&translator);
    }


  QJadeo w;

// Launch xjadeo

  QString xjadeoPath(getenv("XJREMOTE"));

  if(xjadeoPath.isEmpty())
    xjadeoPath = w.m_xjadeopath;
  else w.m_xjadeopath = xjadeoPath;

  if(xjadeoPath.isEmpty())
    xjadeoPath = BINDIR "/xjremote";

  xjadeo.addArgument(xjadeoPath);
  xjadeo.addArgument("-R");

  if(!xjadeo.start())
  {
     QMessageBox::QMessageBox::critical( &w, "qjadeo","can not execute xjadeo/xjremote.","Exit", QString::null, QString::null, 0, -1);

    qFatal("Could not start xjadeo executable: " + xjadeoPath);
    qFatal("Try to set the XJREMOTE environement variable to point to xjadeo.");
  }

  w.connect(&xjadeo, SIGNAL(readyReadStdout()), &w, SLOT(readFromStdout()));
  w.connect(&xjadeo, SIGNAL(processExited()), &w, SLOT(close()));

  w.show();
  a.connect(&a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()));
  w.initialize();
  a.exec();

  // clean up - 

  xjadeo.writeToStdin(QString("notify disable\n"));
  xjadeo.writeToStdin(QString("exit\n"));
  //sleep(1); // TODO: flush pipe to xjadeo/xjremote !

  xjadeo.tryTerminate();
  QTimer::singleShot(5000, &xjadeo, SLOT(kill()));
  //FIXME : does the kill-timeout remain when we exit here??
}

/* vi:set ts=8 sts=2 sw=2: */
