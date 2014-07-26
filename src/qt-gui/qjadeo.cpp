#include <stdlib.h>
#include <qapplication.h>
#include <qprocess.h>
#include <qtimer.h>
#include <qfiledialog.h>
#include <qmessagebox.h>
#include <qprogressbar.h>
#include <qspinbox.h>
#include <qslider.h>
#include <qlineedit.h> 
#include <qcombobox.h> 
#include <qcheckbox.h> 
#include <qdesktopwidget.h>
#include <qtextcodec.h>
#include <QTranslator>
#include <QTest>

#include "qjadeo.h"
#include "mydirs.h"

#include <math.h>

// Global variables
//QProcess xjadeo;

///////////////////////////////////////////////////////
// QJadeo window
///////////////////////////////////////////////////////

// Constructor

QJadeo::QJadeo()
{
  setupUi(this);
  retranslateUi(this);

  QSettings m_settings("rss", "jadeo");

  int windowWidth = m_settings.value("WindowWidth", 460).toInt();
  int windowHeight = m_settings.value("WindowHeight", 100).toInt();
  int windowX = m_settings.value("WindowX", -1).toInt();
  int windowY = m_settings.value("WindowY", -1).toInt();

  resize(windowWidth, windowHeight);
  if(windowX != -1 || windowY != -1)
    move(windowX, windowY);

  for(int i = 0; i < MAX_RECENTFILES; ++i)
  {
    QString filename = m_settings.value("File" + QString::number(i + 1)).toString();

    if(!filename.isEmpty())
      m_recentFiles.push_back(filename);
  }
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
  if (m_mencoderpath.isEmpty()) m_mencoderpath = QString(MENCODER);
  if (m_xjadeopath.isEmpty()) m_xjadeopath = QString(BINDIR XJREMOTE);
  if (m_xjinfopath.isEmpty()) m_xjinfopath = QString(BINDIR XJINFO);
  m_osdfont = m_settings.value("OSD font").toString();
  fileImportAction->setEnabled(testexec(m_mencoderpath));

  for (int i = 0; i < MAX_RECENTFILES; ++i) {
    recentFileActs[i] = new QAction(this);
    recentFileActs[i]->setVisible(false);
    connect(recentFileActs[i], SIGNAL(triggered()), this, SLOT(fileOpenRecent())); 
    fileMenu->addAction(recentFileActs[i]);
  }

  if(m_recentFiles.size())
    updateRecentFilesMenu();
}

void QJadeo::initialize () 
{
  if (!m_osdfont.isEmpty()) {
    xjadeo->write(QString("osd font " + m_osdfont + "\n").toUtf8());
  }
  xjadeo->write(QByteArray("osd notext\n"));
  xjadeo->write(QByteArray("get width\n"));
  xjadeo->write(QByteArray("get height\n"));
  xjadeo->write(QByteArray("get frames\n"));
  xjadeo->write(QByteArray("get framerate\n"));
  xjadeo->write(QByteArray("get offset\n"));
  xjadeo->write(QByteArray("get osdcfg\n"));
  xjadeo->write(QByteArray("midi driver\n"));
  xjadeo->write(QByteArray("get syncsource\n"));
  xjadeo->write(QByteArray("get position\n"));
  xjadeo->write(QByteArray("get letterbox\n"));
  xjadeo->write(QByteArray("notify frame\n"));
}

/* search for external executable 
 * returns true if file is found in env(PATH) 
 * and is executable.
 */
bool QJadeo::testexec(QString exe)
{
  return 1;
  //int timeout=10;
  QProcess testbin; 
  /* FIXME: there MUST be a better way 
   * than to simply try exeute it .. */
//testbin.setCommunication(0);
  testbin.start(exe,QStringList("-V"));
  if(!testbin.waitForStarted()) {
    testbin.kill();
    return (0);
  }
  //while (testbin.state() && timeout--) usleep (10000);
  //if (timeout<1) testbin.kill();
  //if (testbin.exitStatus() != 0) return(0);
  return(1);
}

// Recent files list

void QJadeo::updateRecentFilesMenu()
{
  int numRecentFiles = qMin(m_recentFiles.size(), MAX_RECENTFILES);

  for (int i = 0; i < numRecentFiles; ++i) {
    QString text = tr("&%1 %2").arg(i + 1).arg(QFileInfo(m_recentFiles[i]).fileName());
    recentFileActs[i]->setText(text);
    recentFileActs[i]->setData(m_recentFiles[i]);
    recentFileActs[i]->setVisible(true);
  }
  for (int j = numRecentFiles; j < MAX_RECENTFILES; ++j)
    recentFileActs[j]->setVisible(false);

  //separatorAct->setVisible(numRecentFiles > 0);
}

void QJadeo::updateRecentFiles(const QString & filename)
{
  m_recentFiles.removeAll(filename);
  m_recentFiles.prepend(filename);
  while (m_recentFiles.size() > MAX_RECENTFILES)
    m_recentFiles.removeLast();
  updateRecentFilesMenu();
}

void QJadeo::fileOpenRecent()
{
  QAction *action = qobject_cast<QAction *>(sender());
  if (action) fileLoad(action->data().toString());
}

void QJadeo::saveOptions()
{
  QSettings m_settings("rss", "jadeo");

  m_settings.setValue("WindowWidth", width());
  m_settings.setValue("WindowHeight", height());
  m_settings.setValue("WindowX", x());
  m_settings.setValue("WindowY", y());
  for(int i = 0; i < int (m_recentFiles.size()); ++i)
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
  QString s = QFileDialog::getOpenFileName(this, tr("Choose video file"), "", 0);

  if(!s.isNull())
  {
    fileLoad(s);
  }
}

void QJadeo::fileDisconnect()
{
  xjadeo->write(QByteArray("notify disable\n"));
  xjadeo->write(QByteArray("exit\n"));
  saveOptions();
  // xjremote will exit and we'll go down with it
  // xjadeo will return a @489 error message 
  //close();
}

void QJadeo::fileExit()
{
  xjadeo->write(QByteArray("quit\n"));
  xjadeo->write(QByteArray("exit\n"));
  saveOptions();
  //close(); // we will terminate when xjadeo/xjremote does.
}

void QJadeo::filePreferences()
{
  PrefDialog *pdialog = new PrefDialog(this);
  if (pdialog) {
    /* set values */
    pdialog->prefLineJackMidi->setText(m_jackmidiport);
    pdialog->prefLineAlsaMidi->setText(m_alsamidiport);
    pdialog->prefLineXjadeo->setText(m_xjadeopath);
    /* exec dialog */
    if( pdialog->exec()) {
    /* apply settings */
      fileImportAction->setEnabled(testexec(m_mencoderpath));
      if (!pdialog->prefLineXjadeo->text().isEmpty())
	m_xjadeopath = pdialog->prefLineXjadeo->text();
      if (!pdialog->prefLineAlsaMidi->text().isEmpty())
	m_alsamidiport = pdialog->prefLineAlsaMidi->text();
      if (!pdialog->prefLineJackMidi->text().isEmpty())
	m_jackmidiport = pdialog->prefLineJackMidi->text();
    /* and save */
      saveOptions();
    }
    delete pdialog;
  }
}

void QJadeo::fileImport()
{
#if 0
  ImportDialog *idialog = new ImportDialog(this);

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
      if (idialog->widthCheckBox->isChecked()) {
	w = idialog->widthSpinBox->value();
        if (idialog->aspectCheckBox->isChecked()) 
	  h = idialog->heightSpinBox->value();
      }
      if (idialog->fpsCheckBox->isChecked()) 
	fps = idialog->fpsComboBox->currentText();
    }
    /* start encoding */
    if(!src.isEmpty() && !dst.isEmpty()) 
      iprog = new ImportProgress(this);
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
#endif
}

void QJadeo::helpAbout()
{
  QMessageBox::about(
    this,
    "About qjadeo",
    "(c) 2006-2010 Robin Gareus & Luis Garrido\n"
    "http://xjadeo.sf.net"
  );
}

void QJadeo::zoom50()
{
  xjadeo->write(QByteArray("window resize 50\n"));
}

void QJadeo::zoom100()
{
  xjadeo->write(QByteArray("window resize 100\n"));
}

void QJadeo::zoom200()
{
  xjadeo->write(QByteArray("window resize 200\n"));
}

void QJadeo::zoomFullScreen()
{
  QDesktopWidget *d = QApplication::desktop();
  xjadeo->write(QByteArray("window position 0x0\n"));
  xjadeo->write(QString(
    "window resize " + QString::number(d->width()) +
    "x" + QString::number(d->height()) + "\n"
  ).toAscii());
}

void QJadeo::zoomAspect()
{
  xjadeo->write(QByteArray("window fixaspect\n"));
}

void QJadeo::zoomLetterbox(bool value)
{
  if (value) {
    xjadeo->write(QByteArray("window letterbox on\n"));
  } else {
    xjadeo->write(QByteArray("window letterbox off\n"));
  }
}

void QJadeo::syncJack()
{
  xjadeo->write(QByteArray("ltc disconnect\n"));
  xjadeo->write(QByteArray("midi disconnect\n"));
  xjadeo->write(QByteArray("jack connect\n"));
  xjadeo->write(QByteArray("midi driver\n"));
  xjadeo->write(QByteArray("get syncsource\n"));
}

void QJadeo::syncLTCJack()
{
  xjadeo->write(QByteArray("midi disconnect\n"));
  xjadeo->write(QByteArray("jack disconnect\n"));
  xjadeo->write(QByteArray("ltc connect\n"));
  xjadeo->write(QByteArray("midi driver\n"));
  xjadeo->write(QByteArray("get syncsource\n"));
}

void QJadeo::syncMTCalsa()
{
  xjadeo->write(QByteArray("ltc disconnect\n"));
  xjadeo->write(QByteArray("jack disconnect\n"));
  xjadeo->write(QByteArray("midi disconnect\n"));
  xjadeo->write(QByteArray("midi driver alsa-seq\n"));
  xjadeo->write(QString("midi connect "+ m_alsamidiport +"\n").toAscii());
  xjadeo->write(QByteArray("midi driver\n"));
  xjadeo->write(QByteArray("get syncsource\n"));
}

void QJadeo::syncMTCjack()
{
  xjadeo->write(QByteArray("ltc disconnect\n"));
  xjadeo->write(QByteArray("jack disconnect\n"));
  xjadeo->write(QByteArray("midi disconnect\n"));
  xjadeo->write(QByteArray("midi driver jack\n"));
  xjadeo->write(QString("midi connect "+ m_jackmidiport +"\n").toAscii());
  xjadeo->write(QByteArray("midi driver\n"));
  xjadeo->write(QByteArray("get syncsource\n"));
}

void QJadeo::syncOff()
{
  xjadeo->write(QByteArray("ltc disconnect\n"));
  xjadeo->write(QByteArray("jack disconnect\n"));
  xjadeo->write(QByteArray("midi disconnect\n"));
  xjadeo->write(QByteArray("midi driver\n"));
  xjadeo->write(QByteArray("get syncsource\n"));
}

void QJadeo::setFPS(const QString &fps)
{
  xjadeo->write(QString("set fps " + fps + "\n").toAscii());
}

void QJadeo::setOffset(const QString &offset)
{
  xjadeo->write(QString("set offset " + offset + "\n").toAscii());
}

void QJadeo::osdFrameToggled(bool value)
{
  if(value)
    xjadeo->write(QByteArray("osd frame 0\n"));
  else
    xjadeo->write(QByteArray("osd frame -1\n"));
}

void QJadeo::osdSMPTEToggled(bool value)
{
  if(value)
    xjadeo->write(QByteArray("osd smpte 100\n"));
  else
    xjadeo->write(QByteArray("osd smpte -1\n"));
}

void QJadeo::osdTextOff()
{
  QJadeo::osdTextToggled(false);
  xjadeo->write(QByteArray("get osdcfg\n"));
}

void QJadeo::osdTextToggled(bool value)
{
  if(value)
    xjadeo->write(QByteArray("osd text\n"));
  else
    xjadeo->write(QByteArray("osd notext\n"));
}

void QJadeo::osdBoxToggled(bool value)
{
  if(value)
    xjadeo->write(QByteArray("osd box\n"));
  else
    xjadeo->write(QByteArray("osd nobox\n"));
}

void QJadeo::seekBarChanged( int value )
{
  int frame=0;
  QString Temp;
  if(m_frames > 0) {
  	frame = value*m_frames/1000;
  }
  Temp.sprintf("seek %i\n",frame);
  xjadeo->write(Temp.toAscii());
}

void QJadeo::osdFont()
{
  QString s = QFileDialog::getOpenFileName(this, tr("Choose a font"), "", "TrueType fonts (*.ttf)");

  if(!s.isNull())
  {
    m_osdfont = s;
    xjadeo->write(QString("osd font " + s + "\n").toUtf8());
    xjadeo->write(QByteArray("osd text \n"));
  }
}

// Called when xjadeo outputs to stdout

void QJadeo::readFromStdout()
{

  while(xjadeo->canReadLine())
  {
    QString response = xjadeo->readLine();
    //qDebug(QString("ResponseLine: '" + response+ "'").toAscii().data());

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
	    xjadeo->write(QByteArray("quit\n"));
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
        int equalsign = response.indexOf('=');
        int comment = response.indexOf('#');
	if (comment > 0) response.truncate(comment);
        QString name = response.mid(5, equalsign - 5);
        QString value = response.right(response.length() - equalsign - 1).trimmed();
        //qDebug(QString("Response: name='" + name + "' value='" +value +"'").toAscii().data());
        if(name == "position")
        {
          if(m_frames > 0) {
            progressBar->setRange(0, m_frames); 
            progressBar->setValue(value.toInt() + 1);
	  }
        }
        else if(name == "filename")
          updateRecentFiles(QString::fromUtf8(value.toAscii().data(),-1));
        else if(name == "movie_width")
          m_movie_width = value.toInt();
        else if(name == "movie_height")
          m_movie_height = value.toInt();
        else if(name == "mididrv")
	{
          m_mididrv = 0;
          if (value == "JACK-MIDI") m_mididrv = 1;
	}
        else if(name == "syncsource")
	{
	  syncJackAction->setChecked(FALSE);
	  syncLTCJackAction->setChecked(FALSE);
	  syncMTCJackAction->setChecked(FALSE);
	  syncMTCAlsaAction->setChecked(FALSE);
	  syncOffAction->setChecked(FALSE);

	  if (value.toInt()==0) { // off
            seekBar->setEnabled(TRUE);
	    syncOffAction->setChecked(TRUE);
	  } else if (value.toInt()==3) { // LTC
	      syncLTCJackAction->setChecked(TRUE);
	  } else if (value.toInt()==2) { // MIDI
            seekBar->setEnabled(FALSE);
	    if (m_mididrv == 1) 
	      syncMTCJackAction->setChecked(TRUE);
	    else
	      syncMTCAlsaAction->setChecked(TRUE);
	  } else {
            seekBar->setEnabled(FALSE); //JACK
	    syncJackAction->setChecked(TRUE);
	  }
	}
        else if(name == "osdfont")
	{
	  ;// set default folder and file in 
	   // File selection dialog.
	}
        else if(name == "letterbox")
	{
	  int v= value.toInt();
	  actionLetterbox->setChecked(v&1);
	}
        else if(name == "osdmode")
	{
	  int v= value.toInt();
	  osdBoxAction->setChecked(v&8);
	  osdTextAction->setChecked(v&4);
	  osdSMPTEAction->setChecked(v&2);
	  osdFrameAction->setChecked(v&1);
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
          xjadeo->write(QString("set fps " + value + "\n").toAscii());
        }
        else if(name == "updatefps")
          m_updatefps = value.toInt();
        break;
      }
      case 1:
        if(status==129) {
	  xjadeo->write(QByteArray("get filename\n"));
	  xjadeo->write(QByteArray("get width\n"));
	  xjadeo->write(QByteArray("get height\n"));
	  xjadeo->write(QByteArray("get frames\n"));
	  xjadeo->write(QByteArray("get framerate\n"));
	  xjadeo->write(QByteArray("notify frame\n"));
	  xjadeo->write(QByteArray("get offset\n"));
	  xjadeo->write(QByteArray("get osdcfg\n"));
	  xjadeo->write(QByteArray("midi driver\n"));
	  xjadeo->write(QByteArray("get syncsource\n"));
	  xjadeo->write(QByteArray("get position\n"));
	  xjadeo->write(QByteArray("get letterbox\n"));
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
  bool showtext = osdTextAction->isChecked();
  xjadeo->write(QString("load " + filename + "\n").toUtf8());
  xjadeo->write(QString("osd text " + filename + "\n").toUtf8());
  xjadeo->write(QByteArray("get filename\n"));
  if (!showtext) {
    QTimer::singleShot(3000, this, SLOT(osdTextOff()));
  }
}

int main(int argc, char **argv)
{

  QApplication a(argc, argv);
  QString sLocale = QLocale::system().name();
  QTranslator translator;

  if (sLocale != "C") {
    QString sLocName = "qjadeo_" + sLocale;
    if (!translator.load(sLocName, ".")) {
        QString sLocPath = CONFIG_PREFIX;
        if (!translator.load(sLocName, sLocPath)) {
            fprintf(stderr, "Warning: no locale found: %s/%s.qm\n", sLocPath.toAscii().data(), sLocName.toAscii().data());
	}
    }
    a.installTranslator(&translator);
  }

  QJadeo w;
  QString xjadeoPath(getenv("XJREMOTE"));

  if(xjadeoPath.isEmpty())
    xjadeoPath = w.m_xjadeopath;
  else w.m_xjadeopath = xjadeoPath;

  if(xjadeoPath.isEmpty())
    xjadeoPath = BINDIR XJREMOTE;

  w.xjadeo = new QProcess(&w);
  w.xjadeo->start(xjadeoPath, QStringList("-R"));
  if(!w.xjadeo->waitForStarted())
  {
    QMessageBox::QMessageBox::critical( &w, "qjadeo", "can not execute xjadeo/xjremote." ,"Exit", QString::null, QString::null, 0, -1);
    //qFatal("Could not start xjadeo executable: " + xjadeoPath);
    //qFatal("Try to set the XJREMOTE environment variable to point to xjadeo.");
    exit(1);
  }
  w.xjadeo->closeReadChannel(QProcess::StandardError);
  w.xjadeo->setReadChannel(QProcess::StandardOutput);

  w.connect(w.xjadeo, SIGNAL(readyReadStandardOutput()), &w, SLOT(readFromStdout()));
  w.connect(w.xjadeo, SIGNAL(finished(int, QProcess::ExitStatus)), &w, SLOT(close()));

  w.show();
  a.connect(&a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()));
  w.initialize();
  a.exec();

  if (w.xjadeo->state()){
    qDebug("xjadeo is still running.\n");
    w.xjadeo->write(QByteArray("notify disable\n"));
    w.xjadeo->write(QByteArray("exit\n"));
    QTest::qWait(1000);
  }
  while (w.xjadeo->state()){
    qDebug("terminate xjadeo/xjremote process.\n");
    w.xjadeo->close();
    QTest::qWait(1000); 
    w.xjadeo->kill();
  }
}

/* vi:set ts=8 sts=2 sw=2: */
