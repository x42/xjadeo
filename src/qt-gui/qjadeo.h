#ifndef QJADEO_H
#define QJADEO_H
#include "ui_mainwindow.h"
#include <qsettings.h>
#include "prefdialog.h"
#include "importprogress.h"
#include "importdialog.h"

#define MAX_RECENTFILES 5

class QJadeo: public QMainWindow , public Ui::MainWindow
{
	Q_OBJECT

public:

  QJadeo();
  void fileLoad(const QString & filename);
  void initialize();
  QString m_xjadeopath;
  QProcess *xjadeo;

private:

  QStringList m_recentFiles;

  int m_movie_width;
  int m_movie_height;
  int m_updatefps;
  int m_frames;
  int m_offset;
  int m_framerate;
  QString m_osdfont;
  QString m_alsamidiport;
  QString m_jackmidiport;
  QString m_importdir;
  bool m_importdestination;
  QString m_importcodec;
  QString m_mencoderpath;
  QString m_mencoderopts;
  QString m_xjinfopath;
  int m_mididrv;

  void updateRecentFilesMenu();
  void updateRecentFiles(const QString & filename);
  void saveOptions();
  bool testexec(QString exe);

	QAction *recentFileActs[MAX_RECENTFILES];

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

  void zoomAspect();
  void zoomLetterbox(bool);

  void syncJack();
  void syncLTCJack();
  void syncMTCalsa();
  void syncMTCjack();
  void syncOff();

  void setFPS(const QString &);
  void setOffset(const QString &);

  void fileOpenRecent();

  void readFromStdout ();

  void osdFrameToggled(bool);
  void osdSMPTEToggled(bool);
  void osdTextToggled(bool);
  void osdBoxToggled(bool);
  void osdFont();
  void seekBarChanged( int );
  void osdTextOff();

};
#endif
