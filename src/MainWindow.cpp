/**********************************************************************
* File:        MainWindow.cpp
* Description: MainWindow functions
* Author:      Marcel Kolodziejczyk
* Created:     2010-01-04
*
* (C) Copyright 2010, Marcel Kolodziejczyk
* (C) Copyright 2011, Zdenko Podobny
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*
**********************************************************************/

#include "include/MainWindow.h"
#include "dialogs/ShortCutsDialog.h"

MainWindow::MainWindow() {
  tabWidget = new QTabWidget;

#if QT_VERSION >= 0x040500
  tabWidget->setTabsClosable(true);
  tabWidget->setMovable(true);
#endif

  connect(tabWidget, SIGNAL(tabCloseRequested(int)), this, SLOT(handleClose(int)));
  connect(tabWidget, SIGNAL(currentChanged(int)), this,
          SLOT(updateMenus()));
  connect(tabWidget, SIGNAL(currentChanged(int)), this,
          SLOT(updateCommandActions()));
  connect(tabWidget, SIGNAL(currentChanged(int)), this,
          SLOT(updateSaveAction()));

  setCentralWidget(tabWidget);

  windowMapper = new QSignalMapper(this);
  connect(windowMapper, SIGNAL(mapped(int)), tabWidget,
          SLOT(setCurrentIndex(int)));

  shortCutsDialog = 0;
  createActions();
  createMenus();
  createToolBars();
  createStatusBar();
  updateMenus();
  updateCommandActions();
  updateSaveAction();
  readSettings();
  setUnifiedTitleAndToolBarOnMac(true);
  setWindowIcon(QIcon(":/images/qbe.png"));
  setWindowTitle(tr("%1 - v%2").arg(SETTING_APPLICATION).arg(VERSION));
}

void MainWindow::closeEvent(QCloseEvent* event) {
  if (closeAllTabs()) {
    writeSettings();
    event->accept();
  } else {
    event->ignore();
  }
}

ChildWidget* MainWindow::activeChild() {
  if (QWidget* currentWidget = tabWidget->currentWidget())
    return qobject_cast<ChildWidget*> (currentWidget);
  return 0;
}

void MainWindow::open() {
  QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                     SETTING_ORGANIZATION, SETTING_APPLICATION);
  // try to get path from settings
  QString last_path = settings.value("last_path").toString();
  QString filetype = "Image files (*.bmp *.png *.jpeg *.jpg *.tif *.tiff);;";
  filetype += "Tiff files (*.tif *.tiff);;All files (*.*)";

  QString imageFile = QFileDialog::getOpenFileName(
                        this,
                        tr("Select image file..."),
                        last_path,
                        filetype);

  addChild(imageFile);
}

void MainWindow::addChild(const QString& imageFileName) {
  if (!imageFileName.isEmpty()) {
    QString canonicalImageFileName =
      QFileInfo(imageFileName).canonicalFilePath();
    for (int i = 0; i < tabWidget->count(); ++i) {
      ChildWidget* child = qobject_cast<ChildWidget*> (tabWidget->widget(i));
      if (canonicalImageFileName == child->canonicalImageFileName()) {
        tabWidget->setCurrentIndex(i);
        return;
      }
    }

    ChildWidget* child = new ChildWidget(this);
    if (child->loadImage(imageFileName)) {
      statusBar()->showMessage(tr("File loaded"), 2000);
      tabWidget->setCurrentIndex(tabWidget->addTab(child,
                                 child->userFriendlyCurrentFile()));
      connect(child, SIGNAL(boxChanged()), this, SLOT(updateCommandActions()));
      connect(child, SIGNAL(modifiedChanged()), this, SLOT(updateTabTitle()));
      connect(child, SIGNAL(modifiedChanged()), this, SLOT(updateSaveAction()));

      // save path of open image file
      QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                         SETTING_ORGANIZATION, SETTING_APPLICATION);
      QString filePath = QFileInfo(imageFileName).absolutePath();
      settings.setValue("last_path", filePath);

      QStringList files = settings.value("recentFileList").toStringList();
      files.removeAll(imageFileName);
      files.prepend(imageFileName);
      while (files.size() > MaxRecentFiles)
        files.removeLast();

      settings.setValue("recentFileList", files);

      foreach(QWidget * widget, QApplication::topLevelWidgets()) {
        MainWindow* mainWin = qobject_cast<MainWindow*>(widget);
        if (mainWin)
          mainWin->updateRecentFileActions();
      }
    } else {
      child->close();
    }
  }
}

void MainWindow::updateRecentFileActions() {
  QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                     SETTING_ORGANIZATION, SETTING_APPLICATION);
  QStringList files = settings.value("recentFileList").toStringList();

  int numRecentFiles = qMin(files.size(), static_cast<int>(MaxRecentFiles));

  for (int i = 0; i < numRecentFiles; ++i) {
    QString text = tr("&%1 %2").arg(i + 1).arg(QFileInfo(files[i]).fileName());

    recentFileActs[i]->setText(text);
    recentFileActs[i]->setData(files[i]);
    recentFileActs[i]->setVisible(true);
  }
  for (int j = numRecentFiles; j < MaxRecentFiles; ++j)
    recentFileActs[j]->setVisible(false);

  fSeparatorAct->setVisible(numRecentFiles > 0);
}

void MainWindow::save() {
  QString fileName = activeChild()->currentBoxFile();

  if (activeChild() && activeChild()->save(fileName))
    statusBar()->showMessage(tr("File saved"), 2000);
}

void MainWindow::saveAs() {
  // Make a copy but do not update title of tab etc.
  // because theere is not correcponding image file
  // or should be SaveAs image?
  QString currentFileName = activeChild()->currentBoxFile();
  QString fileName = QFileDialog::getSaveFileName(this,
                     tr("Save a copy of box file..."),
                     currentFileName,
                     tr("Tesseract-ocr box files (*.box);;All files (*)"));

  if (fileName.isEmpty())
    return;

  if (activeChild() && activeChild()->save(fileName))
    statusBar()->showMessage(tr("File saved"), 2000);
}

void MainWindow::importSym() {
  QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                     SETTING_ORGANIZATION, SETTING_APPLICATION);
  // try to get path from settings
  QString last_path = settings.value("last_path").toString();

  QString textFile = QFileDialog::getOpenFileName(
                       this,
                       tr("Select text file..."),
                       last_path,
                       tr("Text files (*.txt);;All files (*.*)"));
  if (textFile.isEmpty())
    return;

  if (activeChild() && activeChild()->importToChild(textFile))
    statusBar()->showMessage(tr("File saved"), 2000);
}

bool MainWindow::closeActiveTab() {
  if (tabWidget->currentWidget() && tabWidget->currentWidget()->close()) {
    tabWidget->removeTab(tabWidget->currentIndex());
    return true;
  }
  return false;
}

bool MainWindow::closeAllTabs() {
  while (tabWidget->currentWidget()) {
    if (!closeActiveTab())
      return false;
  }
  return true;
}

void MainWindow::openRecentFile() {
  QAction* action = qobject_cast<QAction*>(sender());

  if (action)
    addChild(action->data().toString());
}

void MainWindow::nextTab() {
  if (tabWidget->count() > 0)
    tabWidget->setCurrentIndex((tabWidget->currentIndex() + 1)
                               % tabWidget->count());
}

void MainWindow::previousTab() {
  if (tabWidget->count() > 0) {
    tabWidget->setCurrentIndex((tabWidget->currentIndex() + tabWidget->count()
                                - 1) % tabWidget->count());
  }
}

void MainWindow::bold(bool checked) {
  if (activeChild()) {
    activeChild()->setBolded(checked);
  }
}

void MainWindow::italic(bool checked) {
  if (activeChild()) {
    activeChild()->setItalic(checked);
  }
}

void MainWindow::underline(bool checked) {
  if (activeChild()) {
    activeChild()->setUnderline(checked);
  }
}

void MainWindow::zoomOriginal() {
  if (activeChild()) {
    activeChild()->zoomOriginal();
  }
}

void MainWindow::zoomToSelection() {
  if (activeChild()) {
    activeChild()->zoomToSelection();
  }
}

void MainWindow::zoomIn() {
  if (activeChild()) {
    activeChild()->zoomIn();
  }
}

void MainWindow::zoomOut() {
  if (activeChild()) {
    activeChild()->zoomOut();
  }
}

void MainWindow::zoomToFit() {
  if (activeChild()) {
    activeChild()->zoomToFit();
  }
}

void MainWindow::zoomToHeight() {
  if (activeChild()) {
    activeChild()->zoomToHeight();
  }
}

void MainWindow::zoomToWidth() {
  if (activeChild()) {
    activeChild()->zoomToWidth();
  }
}

void MainWindow::showSymbol() {
  if (activeChild()) {
    activeChild()->showSymbol();
  }
}

void MainWindow::directTypingMode(bool checked) {
  if (activeChild()) {
    activeChild()->setDirectTypingMode(checked);
  }
}

void MainWindow::drawBoxes() {
  if (activeChild()) {
    activeChild()->drawBoxes();
  }
}

void MainWindow::insertSymbol() {
  if (activeChild()) {
    activeChild()->insertSymbol();
  }
}

void MainWindow::splitSymbol() {
  if (activeChild()) {
    activeChild()->splitSymbol();
  }
}

void MainWindow::joinSymbol() {
  if (activeChild()) {
    activeChild()->joinSymbol();
  }
}

void MainWindow::deleteSymbol() {
  if (activeChild()) {
    activeChild()->deleteSymbol();
  }
}

void MainWindow::moveUp() {
  if (activeChild()) {
    activeChild()->moveUp();
  }
}

void MainWindow::moveDown() {
  if (activeChild()) {
    activeChild()->moveDown();
  }
}

void MainWindow::moveTo() {
  if (activeChild()) {
    activeChild()->moveTo();
  }
}

void MainWindow::goToRow() {
  if (activeChild()) {
    activeChild()->goToRow();
  }
}
void MainWindow::slotSettings() {
  runSettingsDialog = new SettingsDialog(this);
  runSettingsDialog->exec();
}

void MainWindow::checkForUpdate() {
  statusBar()->showMessage(tr("Checking for new version..."), 2000);

  QNetworkRequest request;
  request.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml");
  request.setUrl(QUrl(UPDATE_URL));

  // TODO(zdenop): test for proxy, ask auth.
  QNetworkAccessManager* manager = new QNetworkAccessManager();
  QNetworkReply* reply = manager->get(request);

  QEventLoop* loop = new QEventLoop;

  QObject::connect(manager, SIGNAL(finished(QNetworkReply*)),
                   loop, SLOT(quit()));
  loop->exec();

  checkVersion(reply);
  delete manager;
}

void MainWindow::requestFinished(QNetworkReply* reply) {
  checkVersion(reply);
}

void MainWindow::checkVersion(QNetworkReply* reply) {
  if (reply->error() == QNetworkReply::NoError) {
    float current_version = QString(reply->readAll()).toFloat();
    float app_version = (QString("%1").arg(VERSION).replace("dev", "",
                         Qt::CaseInsensitive)).toFloat();

    QString messageText;

    if (app_version == current_version) {
      messageText = tr("<p>No newer version is available.</p>");
    } else if (app_version > current_version) {
      messageText = tr("<p>Your version ('%1') is higher than ").arg(VERSION);
      messageText += tr("released stable version ('%2').").arg(current_version);
      messageText += tr("</p><p>Do you use develepment version? ");
      messageText += tr("Don't forget to install stable version manually!</p>");
    } else {
      messageText = tr("<p>New version '%1' is available!<br/>Please visit ")
                    .arg(current_version);
      messageText +=
        tr("<a href=%1/downloads>downloads on project homepage!</a></p>")
        .arg(PROJECT_URL);
    }

    QMessageBox::information(this, tr("Version info"), messageText);
  } else {
    QMessageBox::critical(this, tr("Network"),
                          tr("ERROR: %1").arg(reply->errorString()));
  }
}

void MainWindow::shortCutList() {
  if (!shortCutsDialog)
    shortCutsDialog = new ShortCutsDialog(this);
  shortCutsDialog -> show();
}

void MainWindow::about() {
  QString abouttext =
    tr("<h1>%1 %3</h1>").arg(SETTING_APPLICATION).arg(VERSION);

  abouttext.append(tr("<p>QT4 editor of tesseract-ocr box files</p>"));
  abouttext.append(tr("<p>Project page: <a href=%1>%2</a></p>").
                   arg(PROJECT_URL).arg(PROJECT_URL_NAME));
  abouttext.append(tr("<p>Copyright 2010 Marcel Kolodziejczyk<br/>"));
  abouttext.append(tr("Copyright 2011 Zdenko Podobný</p>"));
  abouttext.append(tr("<p>This software is released under "
                      "<a href=\"http://www.apache.org/licenses/LICENSE-2.0\">Apache License 2.0</a></p>"));
  QMessageBox::about(this, tr("About application"), abouttext);
}

void MainWindow::aboutQt() {
  QMessageBox::aboutQt(this, tr("About Qt"));
}

void MainWindow::handleClose(int i) {
  if (tabWidget->widget(i) && tabWidget->widget(i)->close())
    tabWidget->removeTab(i);

  if (!activeChild()) {
    _utfCodeLabel->setText("");
    _boxsize->setText("");
    _zoom->setText("");
  }
}

void MainWindow::updateMenus() {
  saveAsAct->setEnabled((activeChild()) != 0);
  importSymAct->setEnabled((activeChild()) != 0);
  closeAct->setEnabled(activeChild() != 0);
  closeAllAct->setEnabled(activeChild() != 0);
  nextAct->setEnabled(activeChild() != 0);
  previousAct->setEnabled(activeChild() != 0);
  separatorAct->setVisible(activeChild() != 0);
  zoomOriginalAct->setEnabled(activeChild() != 0);
  zoomInAct->setEnabled(activeChild() != 0);
  zoomOutAct->setEnabled(activeChild() != 0);
  zoomToFitAct->setEnabled(activeChild() != 0);
  zoomToHeightAct->setEnabled(activeChild() != 0);
  zoomToWidthAct->setEnabled(activeChild() != 0);
  zoomToSelectionAct->setEnabled(activeChild() != 0);
  showSymbolAct->setEnabled(activeChild() != 0);
  goToRowAct->setEnabled(activeChild() != 0);
  drawBoxesAct->setEnabled(activeChild() != 0);
  DirectTypingAct->setEnabled(activeChild() != 0);
}

void MainWindow::updateCommandActions() {
  bool enable = (activeChild()) ? activeChild()->isBoxSelected() : false;

  boldAct->setEnabled(enable);
  boldAct->setChecked((activeChild()) ? activeChild()->isBold() : false);
  italicAct->setEnabled(enable);
  italicAct->setChecked((activeChild()) ? activeChild()->isItalic() : false);
  underlineAct->setEnabled(enable);
  underlineAct->setChecked((activeChild())
                           ? activeChild()->isUnderLine() : false);
  showSymbolAct->setChecked((activeChild())
                            ? activeChild()->isShowSymbol() : false);
  drawBoxesAct->setChecked((activeChild())
                           ? activeChild()->isDrawBoxes() : false);
  moveUpAct->setEnabled(enable);
  moveDownAct->setEnabled(enable);
  moveToAct->setEnabled(enable);
  insertAct->setEnabled(enable);
  splitAct->setEnabled(enable);
  joinAct->setEnabled(enable);
  deleteAct->setEnabled(enable);

  if (activeChild()) {
    _utfCodeLabel->setText(activeChild()->getSymbolHexCode());
    _boxsize->setText(activeChild()->getBoxSize());
    _zoom->setText(activeChild()->getZoom());
  }
}

void MainWindow::updateSaveAction() {
  saveAct->setEnabled((activeChild()) ? activeChild()->isModified() : false);
}

void MainWindow::updateTabTitle() {
  if (activeChild()) {
    QString title = activeChild()->userFriendlyCurrentFile();
    if (activeChild()->isModified())
      title += " *";
    tabWidget->setTabText(tabWidget->currentIndex(), title);
  }
}

void MainWindow::updateViewMenu() {
  viewMenu->clear();
  viewMenu->addAction(nextAct);
  viewMenu->addAction(previousAct);
  viewMenu->addAction(separatorAct);

  separatorAct->setVisible(tabWidget->count() > 0);

  for (int i = 0; i < tabWidget->count(); ++i) {
    ChildWidget* child = qobject_cast<ChildWidget*> (tabWidget->widget(i));

    QString text;
    if (i < 9) {
      text = tr("&%1 %2").arg(i + 1).arg(child->userFriendlyCurrentFile());
    } else {
      text = tr("%1 %2").arg(i + 1).arg(child->userFriendlyCurrentFile());
    }
    QAction* action = viewMenu->addAction(text);
    action->setCheckable(true);
    action->setChecked(child == activeChild());
    connect(action, SIGNAL(triggered()), windowMapper, SLOT(map()));
    windowMapper->setMapping(action, i);
  }

  viewMenu->addSeparator();
  viewMenu->addAction(zoomInAct);
  viewMenu->addAction(zoomOutAct);
  viewMenu->addAction(zoomOriginalAct);
  viewMenu->addAction(zoomToFitAct);
  viewMenu->addAction(zoomToHeightAct);
  viewMenu->addAction(zoomToWidthAct);
  viewMenu->addAction(zoomToSelectionAct);
  viewMenu->addSeparator();
  viewMenu->addAction(showSymbolAct);
  viewMenu->addAction(drawBoxesAct);
}

void MainWindow::createActions() {
  openAct = new QAction(QIcon(":/images/fileopen.png"),
                        tr("&Open..."), this);
  openAct->setShortcuts(QKeySequence::Open);
  openAct->setStatusTip(tr("Open an existing file"));
  connect(openAct, SIGNAL(triggered()), this, SLOT(open()));

  saveAct = new QAction(QIcon(":/images/filesave.png"),
                        tr("&Save"), this);
  saveAct->setShortcuts(QKeySequence::Save);
  saveAct->setStatusTip(tr("Save the document to disk"));
  saveAct->setEnabled(false);
  connect(saveAct, SIGNAL(triggered()), this, SLOT(save()));

  saveAsAct = new QAction(QIcon(":/images/fileopenas.png"),
                          tr("Save &As"), this);
  saveAsAct->setShortcut(tr("Ctrl+Shift+S"));
  saveAsAct->setStatusTip(
    tr("Save document after prompting the user for a file name."));
  saveAsAct->setEnabled(false);
  connect(saveAsAct, SIGNAL(triggered()), this, SLOT(saveAs()));

  importSymAct = new QAction(QIcon(":/images/import.svg"),
                             tr("I&mport symbols..."), this);
  // importSymAct->setShortcuts(QKeySequence::Save);
  importSymAct->setStatusTip(tr("Import symbols from text document"));
  importSymAct->setEnabled(false);
  connect(importSymAct, SIGNAL(triggered()), this, SLOT(importSym()));

  closeAct = new QAction(QIcon(":/images/window-close.png"),
                         tr("Cl&ose"), this);
  closeAct->setShortcut(QKeySequence::Close);
  closeAct->setStatusTip(tr("Close the active tab"));
  connect(closeAct, SIGNAL(triggered()), this, SLOT(closeActiveTab()));

  closeAllAct = new QAction(tr("Close &All"), this);
  closeAllAct->setShortcut(tr("Ctrl+Shift+W"));
  closeAllAct->setStatusTip(tr("Close all the tabs"));
  connect(closeAllAct, SIGNAL(triggered()), this, SLOT(closeAllTabs()));

  separatorAct = new QAction(this);
  separatorAct->setSeparator(true);

  exitAct = new QAction(QIcon(":/images/exit.png"),
                        tr("E&xit"), this);
  exitAct->setShortcut(tr("Ctrl+Q"));
  exitAct->setStatusTip(tr("Exit the application"));
  connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

  boldAct = new QAction(QIcon(":/images/text_bold.png"),
                        tr("&Bold"), this);
  boldAct->setShortcut(QKeySequence::Bold);
  boldAct->setCheckable(true);
  connect(boldAct, SIGNAL(triggered(bool)), this, SLOT(bold(bool)));

  italicAct = new QAction(QIcon(":/images/text_italic.png"),
                          tr("&Italic"), this);
  italicAct->setShortcut(QKeySequence::Italic);
  italicAct->setCheckable(true);
  connect(italicAct, SIGNAL(triggered(bool)), this, SLOT(italic(bool)));

  underlineAct = new QAction(QIcon(":/images/text_under.png"),
                             tr("&Underline"), this);
  underlineAct->setShortcut(QKeySequence::Underline);
  underlineAct->setCheckable(true);
  connect(underlineAct, SIGNAL(triggered(bool)), this, SLOT(underline(bool)));

  zoomInAct = new QAction(QIcon(":/images/zoom-in.png"),
                          tr("Zoom &in"), this);
  zoomInAct->setShortcut(QKeySequence::ZoomIn);
  connect(zoomInAct, SIGNAL(triggered()), this, SLOT(zoomIn()));

  zoomOutAct = new QAction(QIcon(":/images/zoom-out.png"),
                           tr("Zoom &out"), this);
  zoomOutAct->setShortcut(QKeySequence::ZoomOut);
  connect(zoomOutAct, SIGNAL(triggered()), this, SLOT(zoomOut()));

  zoomOriginalAct = new QAction(QIcon(":/images/zoom-original.png"),
                                tr("Zoom &1:1"), this);
  zoomOriginalAct->setShortcut(tr("Ctrl+*"));
  connect(zoomOriginalAct, SIGNAL(triggered()), this, SLOT(zoomOriginal()));

  zoomToFitAct = new QAction(QIcon(":/images/zoom-fit.png"),
                             tr("Zoom to fit"), this);
  zoomToFitAct->setShortcut(tr("Ctrl+."));
  connect(zoomToFitAct, SIGNAL(triggered()), this, SLOT(zoomToFit()));

  zoomToHeightAct = new QAction(QIcon(":/images/zoom-height.png"),
                                tr("Zoom to height"), this);
  zoomToHeightAct->setShortcut(tr("Ctrl+>"));
  connect(zoomToHeightAct, SIGNAL(triggered()), this, SLOT(zoomToHeight()));

  zoomToWidthAct = new QAction(QIcon(":/images/zoom-width.png"),
                               tr("Zoom to width"), this);
  zoomToWidthAct->setShortcut(tr("Ctrl+<"));
  connect(zoomToWidthAct, SIGNAL(triggered()), this, SLOT(zoomToWidth()));

  zoomToSelectionAct = new QAction(QIcon(":/images/zoom-selection.png"),
                                   tr("Zoom to selection"), this);
  zoomToSelectionAct->setShortcut(tr("Ctrl+/"));
  zoomToSelectionAct->setStatusTip(tr("Zoom to selected box"));
  connect(zoomToSelectionAct, SIGNAL(triggered()), this,
          SLOT(zoomToSelection()));

  showSymbolAct = new QAction(QIcon(":/images/showSymbol.png"),
                              tr("S&how symbol"), this);
  showSymbolAct->setCheckable(true);
  showSymbolAct->setShortcut(tr("Ctrl+L"));
  showSymbolAct->setStatusTip(tr("Show/hide symbol over selection rectangle"));
  connect(showSymbolAct, SIGNAL(triggered()), this, SLOT(showSymbol()));

  DirectTypingAct = new QAction(QIcon(":/images/key_bindings.svg"),
                                tr("&Direct type mode"), this);
  DirectTypingAct->setCheckable(true);
  DirectTypingAct->setShortcut(tr("Ctrl+D"));
  connect(DirectTypingAct, SIGNAL(triggered(bool)), this,
          SLOT(directTypingMode(bool)));

  drawBoxesAct = new QAction(QIcon(":/images/drawRect.png"),
                             tr("S&how boxes"), this);
  drawBoxesAct->setCheckable(true);
  drawBoxesAct->setShortcut(tr("Ctrl+H"));
  drawBoxesAct->setStatusTip(tr("Show/hide rectangles for all boxes"));
  connect(drawBoxesAct, SIGNAL(triggered()), this, SLOT(drawBoxes()));

  nextAct = new QAction(QIcon(":/images/next.png"), tr("Ne&xt"), this);
  nextAct->setShortcuts(QKeySequence::NextChild);
  nextAct->setStatusTip(tr("Move the focus to the next window"));
  connect(nextAct, SIGNAL(triggered()), this, SLOT(nextTab()));

  previousAct = new QAction(QIcon(":/images/previous.png"),
                            tr("Pre&vious"), this);
  previousAct->setShortcuts(QKeySequence::PreviousChild);
  previousAct->setStatusTip(tr("Move the focus to the previous window"));
  connect(previousAct, SIGNAL(triggered()), this, SLOT(previousTab()));

  insertAct = new QAction(QIcon(":/images/insertRow.svg"),
                          tr("&Insert symbol"), this);
  insertAct->setShortcut(Qt::Key_Insert);
  connect(insertAct, SIGNAL(triggered()), this, SLOT(insertSymbol()));

  splitAct = new QAction(QIcon(":/images/splitRow.svg"),
                         tr("&Split symbol"), this);
  splitAct->setShortcut(tr("Ctrl+2"));
  connect(splitAct, SIGNAL(triggered()), this, SLOT(splitSymbol()));

  joinAct = new QAction(QIcon(":/images/joinRow.svg"),
                        tr("&Join with Next Symbol"), this);
  joinAct->setShortcut(tr("Ctrl+1"));
  connect(joinAct, SIGNAL(triggered()), this, SLOT(joinSymbol()));

  deleteAct = new QAction(QIcon(":/images/deleteRow.png"),
                          tr("&Delete symbol"), this);
  deleteAct->setShortcut(QKeySequence::Delete);
  connect(deleteAct, SIGNAL(triggered()), this, SLOT(deleteSymbol()));

  moveUpAct = new QAction(QIcon(":/images/up.svg"),
                          tr("Move row &up"), this);
  moveUpAct->setShortcut(Qt::CTRL | Qt::Key_Up);
  connect(moveUpAct, SIGNAL(triggered()), this, SLOT(moveUp()));

  moveDownAct = new QAction(QIcon(":/images/down.svg"),
                            tr("Move row &down"), this);
  moveDownAct->setShortcut(Qt::CTRL | Qt::Key_Down);
  connect(moveDownAct, SIGNAL(triggered()), this, SLOT(moveDown()));

  moveToAct = new QAction(QIcon(":/images/moveTo.svg"),
                          tr("&Move row to…"), this);
  moveToAct->setShortcut(Qt::CTRL | Qt::Key_M);
  connect(moveToAct, SIGNAL(triggered()), this, SLOT(moveTo()));

  goToRowAct = new QAction(QIcon(":/images/gtk-jump-to-ltr.png"),
                           tr("&Go to row…"), this);
  goToRowAct->setShortcut(tr("Ctrl+G"));
  connect(goToRowAct, SIGNAL(triggered()), this, SLOT(goToRow()));

  settingsAct = new QAction(tr("&Settings..."), this);
  settingsAct->setShortcut(tr("Ctrl+T"));
  settingsAct->setStatusTip(tr("Programm settings"));
  connect(settingsAct, SIGNAL(triggered()), this, SLOT(slotSettings()));

  checkForUpdateAct = new QAction(tr("&Check for update"), this);
  checkForUpdateAct->setStatusTip(tr("Check whether a newer version exits."));
  connect(checkForUpdateAct, SIGNAL(triggered()), this, SLOT(checkForUpdate()));

  aboutAct = new QAction(QIcon(":/images/help-about.png"),
                         tr("&About"), this);
  aboutAct->setStatusTip(tr("Show the application's About box"));
  connect(aboutAct, SIGNAL(triggered()), this, SLOT(about()));

  aboutQtAct = new QAction(tr("About &Qt"), this);
  connect(aboutQtAct, SIGNAL(triggered()), this, SLOT(aboutQt()));

  shortCutListAct = new QAction(tr("&Shortcut List"), this);
  shortCutListAct -> setShortcut(tr("F1"));
  connect(shortCutListAct, SIGNAL(triggered()), this, SLOT(shortCutList()));
}

void MainWindow::createMenus() {
  for (int i = 0; i < MaxRecentFiles; ++i) {
    recentFileActs[i] = new QAction(this);
    recentFileActs[i]->setVisible(false);
    connect(recentFileActs[i], SIGNAL(triggered()),
            this, SLOT(openRecentFile()));
  }

  fileMenu = menuBar()->addMenu(tr("&File"));
  fileMenu->addAction(openAct);
  fileMenu->addAction(saveAct);
  fileMenu->addAction(saveAsAct);
  fileMenu->addSeparator();
  fileMenu->addAction(importSymAct);
  fileMenu->addSeparator();
  fileMenu->addAction(closeAct);
  fileMenu->addAction(closeAllAct);
  fSeparatorAct = fileMenu->addSeparator();
  for (int i = 0; i < MaxRecentFiles; ++i)
    fileMenu->addAction(recentFileActs[i]);
  fileMenu->addSeparator();
  fileMenu->addAction(exitAct);
  updateRecentFileActions();

  editMenu = menuBar()->addMenu(tr("&Edit"));
  editMenu->addAction(boldAct);
  editMenu->addAction(italicAct);
  editMenu->addAction(underlineAct);
  editMenu->addSeparator();
  editMenu->addAction(insertAct);
  editMenu->addAction(splitAct);
  editMenu->addAction(joinAct);
  editMenu->addAction(deleteAct);
  editMenu->addSeparator();
  editMenu->addAction(moveUpAct);
  editMenu->addAction(moveDownAct);
  editMenu->addAction(moveToAct);
  editMenu->addAction(goToRowAct);
  editMenu->addSeparator();
  editMenu->addAction(DirectTypingAct);
  editMenu->addSeparator();
  editMenu->addAction(settingsAct);

  viewMenu = menuBar()->addMenu(tr("&View"));
  updateViewMenu();
  connect(viewMenu, SIGNAL(aboutToShow()), this, SLOT(updateViewMenu()));

  menuBar()->addSeparator();

  helpMenu = menuBar()->addMenu(tr("&Help"));
  helpMenu->addAction(checkForUpdateAct);
  helpMenu->addSeparator();
  helpMenu->addAction(shortCutListAct);
  helpMenu->addAction(aboutAct);
  helpMenu->addAction(aboutQtAct);
}

void MainWindow::createToolBars() {
  fileToolBar = addToolBar(tr("File"));
  fileToolBar->setObjectName("fileToolBar");
  fileToolBar->addAction(exitAct);
  fileToolBar->addAction(openAct);
  fileToolBar->addAction(saveAct);
  fileToolBar->addAction(importSymAct);

  viewToolBar = addToolBar(tr("View"));
  viewToolBar->setObjectName("viewToolBar");
  viewToolBar->addAction(previousAct);
  viewToolBar->addAction(nextAct);
  viewToolBar->addSeparator();
  viewToolBar->addAction(zoomInAct);
  viewToolBar->addAction(zoomOutAct);
  viewToolBar->addAction(zoomOriginalAct);
  viewToolBar->addAction(zoomToFitAct);
  viewToolBar->addAction(zoomToHeightAct);
  viewToolBar->addAction(zoomToWidthAct);
  viewToolBar->addAction(zoomToSelectionAct);
  viewToolBar->addSeparator();
  viewToolBar->addAction(showSymbolAct);
  viewToolBar->addAction(drawBoxesAct);
  viewToolBar->addAction(DirectTypingAct);

  editToolBar = addToolBar(tr("Edit"));
  editToolBar->setObjectName("editToolBar");
  editToolBar->addAction(boldAct);
  editToolBar->addAction(italicAct);
  editToolBar->addAction(underlineAct);
}

void MainWindow::createStatusBar() {
  _utfCodeLabel = new QLabel();
  _utfCodeLabel->setToolTip(QString("UTF-8 codes of symbols"));
  _utfCodeLabel->setText("");
  _utfCodeLabel->setIndent(5);

  _boxsize = new QLabel();
  _boxsize->setToolTip(QString("Width&Height of box"));
  _boxsize->setFrameStyle(QFrame::Sunken);
  _boxsize->setAlignment(Qt::AlignHCenter);
  _boxsize->setMaximumWidth(60);

  _zoom = new QLabel();
  _zoom->setToolTip(QString("Zoom factor"));
  _zoom->setFrameStyle(QFrame::Sunken);
  _zoom->setAlignment(Qt::AlignHCenter);
  _zoom->setMaximumWidth(50);

  statusBar()->addWidget(_utfCodeLabel, 3);
  statusBar()->addWidget(_boxsize, 1);
  statusBar()->addWidget(_zoom, 1);
}

void MainWindow::readSettings() {
  QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                     SETTING_ORGANIZATION, SETTING_APPLICATION);

  settings.beginGroup("mainWindow");
  restoreGeometry(settings.value("geometry").toByteArray());
  restoreState(settings.value("state").toByteArray());
  settings.endGroup();
}

void MainWindow::writeSettings() {
  QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                     SETTING_ORGANIZATION, SETTING_APPLICATION);

  settings.beginGroup("mainWindow");
  settings.setValue("geometry", saveGeometry());
  settings.setValue("state", saveState());
  settings.endGroup();
}
