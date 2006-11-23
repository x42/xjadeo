#ifndef QJADEO_H
#define QJADEO_H
#include "mainwindow.h"
#include <qsettings.h>
#include "prefdialog.h"
#include "importdialog.h"
#include "importprogress.h"

class QJadeo: public MainWindow
{

	Q_OBJECT

public:

  QJadeo();
  void fileLoad(const QString & filename);
  void initialize();
  QString m_xjadeopath;

private:

  QStringList m_recentFiles;
  QSettings m_settings;

  int m_movie_width;
  int m_movie_height;
  int m_updatefps;
  int m_frames;
  int m_offset;
  int m_framerate;
  QString m_osdfont;
  QString m_midiport;
  QString m_importdir;
  bool m_importdestination;
  QString m_importcodec;
  QString m_mencoderpath;
  QString m_mencoderopts;
  QString m_xjinfopath;

  void updateRecentFilesMenu();
  void updateRecentFiles(const QString & filename);
  void saveOptions();
  bool testexec(QString exe);

public slots:

  void fileOpen();
  void fileExit();
  void fileImport();
  void filePreferences();
  void fileDisconnect();
  void helpAbout();
  void zoom50();
  void zoom100();
  void zoom200();
  void zoomFullScreen();

  void syncJack();
  void syncMTC();
  void syncOff();

  void seekContinuously();
  void seekAnyFrame();
  void seekKeyFrames();

  void setFPS(const QString &);
  void setOffset(const QString &);

  void fileOpenRecent(int index);

  void readFromStdout ();

  void osdFrameToggled(bool);
  void osdSMPTEToggled(bool);
  void osdFont();
  void seekBarChanged( int );

};
#endif
