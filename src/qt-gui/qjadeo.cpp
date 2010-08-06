
#include <stdlib.h>

#include <qapplication.h>
#include <qprocess.h>
#include <qtimer.h>
#include <qfiledialog.h>
#include <qmessagebox.h>
//#include <qpopupmenu.h>
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

  int windowWidth = m_settings.value("/WindowWidth", 460).toInt();
  int windowHeight = m_settings.value("WindowHeight", 530).toInt();
  int windowX = m_settings.value("WindowX", -1).toInt();
  int windowY = m_settings.value("WindowY", -1).toInt();

# if 0
  resize(windowWidth, windowHeight);
  if(windowX != -1 || windowY != -1)
    move(windowX, windowY);
#endif
  for(int i = 0; i < MAX_RECENTFILES; ++i)
  {
    QString filename = m_settings.value("File" + QString::number(i + 1)).toString();

    if(!filename.isEmpty())
      m_recentFiles.push_back(filename);
  }
  if(m_recentFiles.count())
    updateRecentFilesMenu();

  m_alsamidiport = m_settings.value("ALSA MIDI port").toString();
  m_jackmidiport = m_settings.value("JACK MIDI port").toString();
  m_importdir = m_settings.value("Import Directory").toString();
  m_importdestination = m_settings.value("Import Destination").toBool();
  m_importcodec = m_settings.value("Import Codec").toString();
  m_xjadeopath = m_settings.value("XJADEO Path").toString();
  m_mencoderpath = m_settings.value("MENCODER Path").toString();
  m_mencoderopts = m_settings.value("MENCODER Options").toString();
  m_xjinfopath = m_settings.value("XJINFO Path").toString();

  // TODO: detect portmidi / alsamidi default. 'midi library' 
  if (m_alsamidiport.isEmpty()) m_alsamidiport = QString("24");
  if (m_jackmidiport.isEmpty()) m_jackmidiport = QString("");
  if (m_importcodec.isEmpty()) m_importcodec = QString("mpeg4");
  if (m_mencoderpath.isEmpty()) m_mencoderpath = QString("mencoder");
  if (m_xjadeopath.isEmpty()) m_xjadeopath = QString(BINDIR "xjremote");
  if (m_xjinfopath.isEmpty()) m_xjinfopath = QString(BINDIR "xjinfo");
  m_osdfont = m_settings.value("OSD font").toString();
#if 0 // QT4
  fileMenu->setItemEnabled(fileMenu->idAt(1),testexec(m_mencoderpath));
  statusBar()->hide();
#endif

}

void QJadeo::initialize () 
{
  xjadeo.write(QString("osd font " + m_osdfont + "\nosd text \n").toAscii());
  xjadeo.write(QByteArray("get width\n"));
  xjadeo.write(QByteArray("get height\n"));
  xjadeo.write(QByteArray("get frames\n"));
  xjadeo.write(QByteArray("get framerate\n"));
  xjadeo.write(QByteArray("get offset\n"));
  xjadeo.write(QByteArray("get osdcfg\n"));
  xjadeo.write(QByteArray("midi driver\n"));
  xjadeo.write(QByteArray("get syncsource\n"));
  xjadeo.write(QByteArray("get position\n"));
  xjadeo.write(QByteArray("get seekmode\n"));
  xjadeo.write(QByteArray("notify frame\n"));
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
#if 0 // QT4
  for(int i = 0; i < MAX_RECENTFILES; ++i)
  {
    if(fileMenu->findItem(i))
      fileMenu->removeItem(i);
    if(i < int (m_recentFiles.count()))
      fileMenu->insertItem(QString("&%1 %2").arg(i + 1).arg(m_recentFiles[i]),
			   this, SLOT(fileOpenRecent(int)), 0, i);
  }
#endif
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
  m_settings.setValue("WindowWidth", width());
  m_settings.setValue("WindowHeight", height());
  m_settings.setValue("WindowX", x());
  m_settings.setValue("WindowY", y());
  for(int i = 0; i < int (m_recentFiles.count()); ++i)
    m_settings.setValue("File" + QString::number(i + 1), m_recentFiles[i]);
  m_settings.setValue("OSD font", m_osdfont);
  m_settings.setValue("JACK MIDI port", m_jackmidiport);
  m_settings.setValue("ALSA MIDI port", m_alsamidiport);
  m_settings.setValue("Import Codec", m_importcodec);
  m_settings.setValue("Import Directory", m_importdir);
  m_settings.setValue("Import Destination", m_importdestination);
  m_settings.setValue("XJADEO Path", m_xjadeopath);
  m_settings.setValue("XJINFO Path", m_xjinfopath);
  m_settings.setValue("MENCODER Path", m_mencoderpath);
  m_settings.setValue("MENCODER Options", m_mencoderopts);
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
  xjadeo.write(QByteArray("notify disable\n"));
  xjadeo.write(QByteArray("exit\n"));
  saveOptions();
  // xjremote will exit and we'll go down with it
  // xjadeo will return a @489 error message 
  //close();
}

void QJadeo::fileExit()
{
  xjadeo.write(QByteArray("quit\n"));
  xjadeo.write(QByteArray("exit\n"));
  saveOptions();
  //close(); // we will terminate when xjadeo/xjremote does.
}

void QJadeo::filePreferences()
{
  PrefDialog *pdialog = new PrefDialog::PrefDialog(this);
  if (pdialog) {
    /* set values */
    pdialog->prefLineJackMidi->setText(m_jackmidiport);
    pdialog->prefLineAlsaMidi->setText(m_alsamidiport);
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
      if (!pdialog->prefLineAlsaMidi->text().isEmpty())
	m_alsamidiport = pdialog->prefLineAlsaMidi->text();
      if (!pdialog->prefLineJackMidi->text().isEmpty())
	m_jackmidiport = pdialog->prefLineJackMidi->text();
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
  xjadeo.write(QByteArray("set seekmode 2\n"));
}


void QJadeo::seekContinuously()
{
  xjadeo.write(QByteArray("set seekmode 1\n"));
}


void QJadeo::seekKeyFrames()
{
  xjadeo.write(QByteArray("set seekmode 3\n"));
}

void QJadeo::zoom50()
{
  xjadeo.write(QByteArray("window resize 50\n"));
}

void QJadeo::zoom100()
{
  xjadeo.write(QByteArray("window resize 100\n"));
}

void QJadeo::zoom200()
{
  xjadeo.write(QByteArray("window resize 200\n"));
}

void QJadeo::zoomFullScreen()
{
  QDesktopWidget *d = QApplication::desktop();
  xjadeo.write(QByteArray("window position 0x0\n"));
  xjadeo.write(QString(
    "window resize " + QString::number(d->width()) +
    "x" + QString::number(d->height()) + "\n"
  ).toAscii());
}

void QJadeo::syncJack()
{
  xjadeo.write(QByteArray("midi disconnect\n"));
  xjadeo.write(QByteArray("jack connect\n"));
  xjadeo.write(QByteArray("midi driver\n"));
  xjadeo.write(QByteArray("get syncsource\n"));
}

void QJadeo::syncMTCalsa()
{
  xjadeo.write(QByteArray("jack disconnect\n"));
  xjadeo.write(QByteArray("midi disconnect\n"));
  xjadeo.write(QByteArray("midi driver alsa-seq\n"));
  xjadeo.write(QString("midi connect "+ m_alsamidiport +"\n").toAscii());
  xjadeo.write(QByteArray("midi driver\n"));
  xjadeo.write(QByteArray("get syncsource\n"));
}

void QJadeo::syncMTCjack()
{
  xjadeo.write(QByteArray("jack disconnect\n"));
  xjadeo.write(QByteArray("midi disconnect\n"));
  xjadeo.write(QByteArray("midi driver jack\n"));
  xjadeo.write(QString("midi connect "+ m_jackmidiport +"\n").toAscii());
  xjadeo.write(QByteArray("midi driver\n"));
  xjadeo.write(QByteArray("get syncsource\n"));
}

void QJadeo::syncOff()
{
  xjadeo.write(QByteArray("jack disconnect\n"));
  xjadeo.write(QByteArray("midi disconnect\n"));
  xjadeo.write(QByteArray("midi driver\n"));
  xjadeo.write(QByteArray("get syncsource\n"));
}

void QJadeo::setFPS(const QString &fps)
{
  xjadeo.write(QString("set fps " + fps + "\n").toAscii());
}

void QJadeo::setOffset(const QString &offset)
{
  xjadeo.write(QString("set offset " + offset + "\n").toAscii());
}

void QJadeo::osdFrameToggled(bool value)
{
  if(value)
    xjadeo.write(QByteArray("osd frame 0\n"));
  else
    xjadeo.write(QByteArray("osd frame -1\n"));
}

void QJadeo::osdSMPTEToggled(bool value)
{
  if(value)
    xjadeo.write(QByteArray("osd smpte 100\n"));
  else
    xjadeo.write(QByteArray("osd smpte -1\n"));
}

void QJadeo::osdBoxToggled(bool value)
{
  if(value)
    xjadeo.write(QByteArray("osd box\n"));
  else
    xjadeo.write(QByteArray("osd nobox\n"));
}

void QJadeo::seekBarChanged( int value )
{
  int frame=0;
  QString Temp;
  if(m_frames > 0) {
  	frame = value*m_frames/1000;	
  }
  Temp.sprintf("seek %i\n",frame);
  xjadeo.write(Temp.toAscii());
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
    xjadeo.write(QString("osd font " + s + "\n").toAscii());
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
	    xjadeo.write(QByteArray("quit\n"));
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
        else if(name == "mididrv")
	{
          m_mididrv = 0;
          if (value == "JACK-MIDI") m_mididrv = 1;
	}
        else if(name == "syncsource")
	{
#if 0 // QT4
	  Sync->setItemChecked(Sync->idAt(0),FALSE);
	  Sync->setItemChecked(Sync->idAt(1),FALSE);
	  Sync->setItemChecked(Sync->idAt(2),FALSE);
	  Sync->setItemChecked(Sync->idAt(3),FALSE);
#endif

	  if (value.toInt()==0) { // off
            seekBar->setEnabled(TRUE);
	    Sync->setItemChecked(Sync->idAt(3),TRUE);
	  } else if (value.toInt()==2) { // MIDI
            seekBar->setEnabled(FALSE);
	    if (m_mididrv == 1) 
	      Sync->setItemChecked(Sync->idAt(1),TRUE); // JACK midi
	    else
	      Sync->setItemChecked(Sync->idAt(2),TRUE); // ALSA midi
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
          xjadeo.write(QString("set fps " + value + "\n").toAscii());
        }
        else if(name == "updatefps")
          m_updatefps = value.toInt();
        break;
      }
      case 1:
        if(status==129) {
	  xjadeo.write(QByteArray("get filename\n"));
	  xjadeo.write(QByteArray("get width\n"));
	  xjadeo.write(QByteArray("get height\n"));
	  xjadeo.write(QByteArray("get frames\n"));
	  xjadeo.write(QByteArray("get framerate\n"));
	  xjadeo.write(QByteArray("notify frame\n"));
	  xjadeo.write(QByteArray("get offset\n"));
	  xjadeo.write(QByteArray("get osdcfg\n"));
	  xjadeo.write(QByteArray("midi driver\n"));
	  xjadeo.write(QByteArray("get syncsource\n"));
	  xjadeo.write(QByteArray("get position\n"));
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
  m_mididrv = 0;
  xjadeo.write(QString("load " + filename + "\n").toAscii());
  xjadeo.write(QByteArray("get filename\n"));
  xjadeo.write(QByteArray("get width\n"));
  xjadeo.write(QByteArray("get height\n"));
  xjadeo.write(QByteArray("get frames\n"));
  xjadeo.write(QByteArray("get framerate\n"));
  xjadeo.write(QByteArray("notify frame\n"));
  xjadeo.write(QByteArray("get offset\n"));
  xjadeo.write(QByteArray("get position\n"));

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
                fprintf(stderr, "Warning: no locale found: %s/%s.qm\n", sLocPath.toAscii(), sLocName.toAscii());
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
    //qFatal("Could not start xjadeo executable: " + xjadeoPath);
    //qFatal("Try to set the XJREMOTE environment variable to point to xjadeo.");
    exit(1);
  }

  w.connect(&xjadeo, SIGNAL(readyReadStdout()), &w, SLOT(readFromStdout()));
  w.connect(&xjadeo, SIGNAL(processExited()), &w, SLOT(close()));

  w.show();
  a.connect(&a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()));
  w.initialize();
  a.exec();

  // clean up - 

  xjadeo.write(QByteArray("notify disable\n"));
  xjadeo.write(QByteArray("exit\n"));
  //sleep(1); // TODO: flush pipe to xjadeo/xjremote !

  xjadeo.tryTerminate();
  QTimer::singleShot(5000, &xjadeo, SLOT(kill()));
  //FIXME : does the kill-timeout remain when we exit here??
}

/* vi:set ts=8 sts=2 sw=2: */
