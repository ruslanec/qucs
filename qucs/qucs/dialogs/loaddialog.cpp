/***************************************************************************
 * Copyright (C) 2014 Guilherme Brondani Torri <guitorri@gmail.com>        *
 *                                                                         *
 * Modified from SaveDialog and LibraryDialog                              *
 *                                                                         *
 * This is free software; you can redistribute it and/or modify            *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 2, or (at your option)     *
 * any later version.                                                      *
 *                                                                         *
 * This software is distributed in the hope that it will be useful,        *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with this package; see the file COPYING.  If not, write to        *
 * the Free Software Foundation, Inc., 51 Franklin Street - Fifth Floor,   *
 * Boston, MA 02110-1301, USA.                                             *
 ***************************************************************************/

#include <QtGui>
#include <QVariant>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QDebug>
#include <QFileDialog>

#include <QScriptEngine>
#include <QScriptValue>
#include <QScriptValueIterator>

#include "loaddialog.h"
#include "qucs.h"
#include "qucsdoc.h"
#include "components/components.h"
#include "components/vacomponent.h" // getData, JSON
#include "main.h" // QucsSettings

LoadDialog::LoadDialog( QWidget* parent, const char* name, bool modal, Qt::WFlags fl )
   : QDialog( parent, name, modal, fl )
{
   if ( !name )
      setWindowTitle( tr( "Load Verilog-A symbols" ) );
   app = 0l;
//   initDialog();
}

LoadDialog::~LoadDialog()
{
}

void LoadDialog::setApp(QucsApp *a)
{
   app = a;
}

void LoadDialog::initDialog()
{
   QVBoxLayout *all = new QVBoxLayout(this);
   all->setMargin(5);
   all->setSpacing(6);

   // hold group of files / group icon and checkboxes
   QHBoxLayout *hGroups = new QHBoxLayout();
   all->addLayout(hGroups);

   // ---
   QGroupBox *group1 = new QGroupBox( tr( "Choose Verilog-A symbol files:" ) );
   hGroups->addWidget(group1);

   QScrollArea *scrollArea = new QScrollArea(group1);
   scrollArea->setWidgetResizable(true);

   fileView = new QListWidget(this);

   scrollArea->setWidget(fileView);

   QVBoxLayout *areaLayout = new QVBoxLayout();
   areaLayout->addWidget(scrollArea);
   group1->setLayout(areaLayout);


   // ...........................................................
   QGridLayout *gridButts = new QGridLayout();
   all->addLayout(gridButts);

   ButtSelectAll = new QPushButton(tr("Select All"));
   gridButts->addWidget(ButtSelectAll, 0, 0);
   connect(ButtSelectAll, SIGNAL(clicked()), SLOT(slotSelectAll()));
   ButtSelectNone = new QPushButton(tr("Deselect All"));
   gridButts->addWidget(ButtSelectNone, 0, 1);
   connect(ButtSelectNone, SIGNAL(clicked()), SLOT(slotSelectNone()));
   // ...........................................................
   ButtCancel = new QPushButton(tr("Cancel"));
   gridButts->addWidget(ButtCancel, 1, 0);
   connect(ButtCancel, SIGNAL(clicked()), SLOT(reject()));
   ButtOk = new QPushButton(tr("Ok"));
   gridButts->addWidget(ButtOk, 1, 1);
   connect(ButtOk, SIGNAL(clicked()), SLOT(loadSelected()));
   ButtOk->setDefault(true);


   QVBoxLayout *iconLayout = new QVBoxLayout();
   hGroups->addLayout(iconLayout);

   QGroupBox *group2 = new QGroupBox( );

   QVBoxLayout *group2Layout = new QVBoxLayout();

   iconPixmap = new QLabel();
   iconPixmap->setSizePolicy(QSizePolicy::Expanding,
                             QSizePolicy::Expanding);
   iconPixmap->setAlignment(Qt::AlignCenter);
   group2Layout->addWidget(iconPixmap);

   group2->setLayout(group2Layout);
   iconLayout->addWidget(group2);

   ButtChangeIcon = new QPushButton(tr("Change Icon"));
   iconLayout->addWidget(ButtChangeIcon);
   connect(ButtChangeIcon,SIGNAL(clicked()),this,SLOT(slotChangeIcon()));

   ButtInclude = new QPushButton(tr("Include Model"));
   ButtInclude->setToolTip(tr("Include SPICE .model parameters"));
   iconLayout->addWidget(ButtInclude);
   connect(ButtInclude,SIGNAL(clicked()),this,SLOT(slotIncludeModel()));

   // group checkboxes
   QGroupBox *group3 = new QGroupBox();
   QVBoxLayout *group3Layout = new QVBoxLayout();
   group3->setLayout(group3Layout);
   iconLayout->addWidget(group3);

   //
   QCheckBox *autoLoadSelCheck = new QCheckBox(tr("auto-load selected"));
   autoLoadSelCheck->setToolTip(
               tr("Load the selected symbols when opening the project."));

   autoLoadSelCheck->setDisabled(true); //disabled for now

   group3Layout->addWidget(autoLoadSelCheck);

   /*
   QCheckBox *autoLoadAllwaysCheck = new QCheckBox(tr("auto-load all"));
   autoLoadAllwaysCheck->setToolTip(
               tr("Load all symbols."));
   group3Layout->addWidget(autoLoadAllwaysCheck);
   */

   connect(fileView, SIGNAL(itemPressed(QListWidgetItem*)),
           this, SLOT(slotSymbolFileClicked(QListWidgetItem*)));

//   qDebug() << "files " << symbolFiles;

   for(int i=0; i < symbolFiles.size(); i++){
       QListWidgetItem *item = new QListWidgetItem(symbolFiles.at(i), fileView);
     item->setFlags( item->flags() | Qt::ItemIsUserCheckable );
     item->setCheckState(Qt::Checked);

     //set first as selected, one need to be selected to assign bitmap
     fileView->setCurrentRow(0);
   }

   // update icon
   this->slotSymbolFileClicked(fileView->currentItem());

   fileView->installEventFilter(this);
   fileView->setFocus();

}

void LoadDialog::slotSelectAll()
{
    for(int i = 0; i < fileView->count(); ++i)
    {
        QListWidgetItem* item = fileView->item(i);
        item->setCheckState(Qt::Checked);
//        qDebug() << "select" << item->text();
    }
}

void LoadDialog::slotSelectNone()
{
    for(int i = 0; i < fileView->count(); ++i)
    {
        QListWidgetItem* item = fileView->item(i);
        item->setCheckState(Qt::Unchecked);
//        qDebug() << "unselect" << item->text();
    }
}

void LoadDialog::slotSymbolFileClicked(QListWidgetItem* item)
{
//  qDebug() << "pressed" << item->text();
 // get bitmap, try to plot
 // similar to QucsApp::slotSetCompView
  QString JSON = projDir.filePath(item->text());

//  qDebug() << "read " << JSON;

  // Just need path to bitmap, do not create an object
  QString Name, vaBitmap;
  Component * c = (Component *)
          vacomponent::info (Name, vaBitmap, false, JSON);
  if (c) delete c;

//  qDebug() << "slotSymbolFileClicked" << Name << vaBitmap;

  // check if icon exists, fall back to default
  QString iconPath = QString(projDir.absFilePath(vaBitmap+".png"));
  QFile iconFile(iconPath);

  if(iconFile.exists())
  {
    // load bitmap defined on the JSON symbol file
    iconPixmap->setPixmap(QPixmap(iconPath));
  }
  else
  {
    QMessageBox::information(this, tr("Info"),
                 tr("Icon not found:\n %1.png").arg(vaBitmap));
    // default icon
    iconPixmap->setPixmap(QPixmap(":/bitmaps/editdelete.png"));
    }
}

/*!
 * \brief LoadDialog::slotIncludeModel Load a include file with model parameters.
 * It overrides the default parameters
 *
 *  \todo find way to enable/disable include?
 */
void LoadDialog::slotIncludeModel()
{
  qDebug() << "Include what?";

  // current JSON symbol file
  QListWidgetItem *item = fileView->selectedItems()[0];
  QString symbolJSON = item->text();
  symbolJSON = QucsSettings.QucsWorkDir.filePath(symbolJSON);
  qDebug() << "overriding" << symbolJSON;


  //
  QString includeName =
          QFileDialog::getOpenFileName(this,
                                        tr("Open File"),
                                        QString(projDir.absolutePath()),
                                        tr("Include model (*.*)"));

  QString newInclude =  QFileInfo(includeName).baseName();

  qDebug() << newInclude;

  // load file, read line by line
  // parse +
  // split =
  // trim
  // store key, value

  // store the Key, Value  for the include model file
  QHash<QString, QString> includeParam;

  QFile file(includeName);
//  QByteArray ba;
//  ba.clear();
  if (!file.open(QIODevice::ReadWrite | QIODevice::Text)){
    QMessageBox::critical(this, tr("Error"),
                          tr("File not found: %1").arg(includeName));
  }
  else {
    QTextStream in(&file);
    while ( !in.atEnd() )
    {
      QString line = in.readLine();

      /// \todo read model name, check consistency
      if (line.contains(".model")) {
          qDebug() << line;

      }

      // read parameters
      // save where? direct override?
      /*
      at this point "_symbol.json" is already created. it was created during symbol save.

      need to laod it again, check for override, do override and save ??? where?


      */

      if (line.contains("+")) {

        QStringList KeyVal = line.section("+",1).split("=");

        QString Key = KeyVal[0].stripWhiteSpace();
        QString Val = KeyVal[1].stripWhiteSpace();

       // qDebug() << Key << Val;
        includeParam[Key] = Val;

        /// add to hash table
      }

    }
    qDebug() << "card" <<includeParam.keys().size();
  }
  file.close();

  ///=====================================================
  /// load default properties
  ///
//  QString data = getData("/Users/guitorri/qucs_qt3/bsim6_prj/bsim6MOD_symbol.json");
  QString data = getData(symbolJSON);


  /// \todo check if JSON is error free
  /// \todo Need to destroy engine?
  QScriptEngine engine;
  QScriptValue vadata = engine.evaluate("(" + data + ")");

  // check model name??
  QString Description = getString(vadata, "description");
  qDebug() << Description;

  // grab properties
  QScriptValue jsonProps = vadata.property("property");

  // iterator
  QScriptValueIterator it(jsonProps);

  // store the Key, Value  for the include model file
  QHash<QString, QString> defaultParam;

  while (it.hasNext()) {
    it.next();

    QScriptValue entry = it.value();

    // skip length named iterate
    if (it.name().compare("length")) {
      QString name = getString(entry, "name");
      QString value = getString(entry, "value");

      // add to default hash
      defaultParam[name]=value;

      /// can overide here here already...
      /// QScriptEngine does not have serialization... find workaroud..
      // do includision?
    }
  }

  // try to save vadata....
  /*! the issue: QStriptengine does not stream out...
  need to write a serializer.... argh!*/

  // load original into QString,
  // search replace,
  // save modified

//  QFile f1 ("/Users/guitorri/qucs_qt3/bsim6_prj/bsim6MOD_symbol.json");
  QFile f1 (symbolJSON);
  f1.open(QIODevice::ReadOnly | QIODevice::Text);
  QString dat1 = QString(f1.readAll());
  f1.close();


  /// got includeParam / defaultParam
  /// override >>

  QString defin;
  QString inclu;
  QList<QString> Keys = includeParam.keys();
  foreach(QString Key, Keys) {

      // model has parameters we want to replace?
      if (defaultParam.keys().contains(Key)) {

          // is the value different?
//          if ( abs (includeParam[Key].toDouble() - defaultParam[Key].toDouble()) > 1e-12 ) {
          if ( includeParam[Key].toDouble() != defaultParam[Key].toDouble() ) {

            defin = QString("\"name\" : \"%1\", \"value\" : \"%2\"").arg(Key).arg(defaultParam[Key]);
            inclu = QString("\"name\" : \"%1\", \"value\" : \"%2\"").arg(Key).arg(includeParam[Key]);

            qDebug() << " mismatch ==>" << Key
                     << "\n defin" << defaultParam[Key] << defin
                     << "\n inclu" << includeParam[Key] << inclu;

            if (dat1.find(defin)) {
                qDebug() << "foun - >replace";
                // to replacemnt
                dat1.replace(dat1.indexOf(defin), defin.size(), inclu);
            }
            /// \todo error if not found...

        }
    }
  } // for


  /// stream data to json
//  QString data2 = "/Users/guitorri/qucs_qt3/bsim6_prj/bsim6MOD_symbol.json";
  QFile f3(symbolJSON);
  f3.open(QIODevice::WriteOnly | QIODevice::Text);
  QTextStream out(&f3);
  out << dat1;
  f3.close();

}

void LoadDialog::reject()
{
    done(AbortClosing);
}

//
void LoadDialog::loadSelected()
{
  // build list vaComponentds
  // hand it down to main app

  selectedComponents.clear();

  for(int i = 0; i < fileView->count(); ++i)
  {
    QListWidgetItem* item = fileView->item(i);

    if (item->checkState() == Qt::Checked){
        QString key = item->text().split("_symbol.json").at(0);
        QString value = projDir.absoluteFilePath(item->text());

        qDebug() << "key" << key;
        qDebug() << "file " << value;

        selectedComponents[key] = value;
    }
  }

  accept();
}


/*
 * Browse for icon image
 * Save image path to JSON symbol file
 */
void LoadDialog::slotChangeIcon()
{
//  qDebug() << "slotChangeIcon";
  QString iconFileName =
          QFileDialog::getOpenFileName(this,
                                        tr("Open File"),
                                        QString(projDir.absolutePath()),
                                        tr("Icon image (*.png)"));

  QString newIcon =  QFileInfo(iconFileName).baseName();
//  qDebug() << "icon "<< newIcon;

  QString filename = fileView->currentItem()->text();
  filename = projDir.absoluteFilePath(filename);
//  qDebug() << "for " <<  filename;

  // open json
  // change property
  // save&close
  // Try to open the JSON file, can use QScriptEngine for this?
  //
  QFile file(filename);
  QByteArray ba;
  ba.clear();
  if (!file.open(QIODevice::ReadWrite | QIODevice::Text)){
    QMessageBox::critical(this, tr("Error"),
                          tr("File not found: %1").arg(filename));
  }
  else {
    QTextStream in(&file);
    while ( !in.atEnd() )
    {
      QString line = in.readLine();
      if (line.contains("BitmapFile")){
          QString change =
                  QString("  \"BitmapFile\" : \"%1\",").arg(newIcon);
          ba.append(change+"\n");
      }
      else{
          ba.append(line+"\n");
      }
    }
  }

  // write back to the same file, clear it first
  file.resize(0);
  file.write(ba);
  file.close();

  // update icon
  this->slotSymbolFileClicked(fileView->currentItem());
}

bool LoadDialog::eventFilter(QObject *obj, QEvent *event)
{
  if (event->type() == QEvent::KeyPress) {
    QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
//    qDebug() << "type" << keyEvent->key() << fileView->count();
    if (keyEvent->key() == Qt::Key_Up) {

        fileView->setCurrentRow(std::max(0, fileView->currentRow()-1));
        this->slotSymbolFileClicked(fileView->currentItem());
        return true;
    }
    if (keyEvent->key() == Qt::Key_Down) {
        fileView->setCurrentRow(std::min(fileView->count()-1, fileView->currentRow()+1));
        this->slotSymbolFileClicked(fileView->currentItem());
        return true;
    }
  }
  return false;
}
