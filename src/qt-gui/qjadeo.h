#include "mainwindow.h"
#include <qsettings.h>

class QJadeo: public MainWindow
{

	Q_OBJECT

public:

  QJadeo();

private:

  QStringList m_recentFiles;
  QSettings m_settings;

  int m_movie_width;
  int m_movie_height;
  int m_updatefps;
  int m_frames;
  int m_framerate;
  QString m_osdfont;

  void updateRecentFilesMenu();
  void updateRecentFiles(const QString & filename);
  void saveOptions();
  void fileLoad(const QString & filename);

public slots:

  void fileOpen();
  void fileExit();
  void helpAbout();
  void zoom50();
  void zoom100();
  void zoom200();
  void zoomFullScreen();

  void syncJack();

  void setFPS(const QString &);
  void setOffset(const QString &);

  void fileOpenRecent(int index);

  void readFromStdout ();

  void osdFrameToggled(bool);
  void osdSMPTEToggled(bool);
  void osdFont();

};
