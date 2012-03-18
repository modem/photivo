/*******************************************************************************
**
** Photivo
**
** Copyright (C) 2012 Bernd Schoeler <brjohn@brother-john.net>
**
** This file is part of Photivo.
**
** Photivo is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License version 3
** as published by the Free Software Foundation.
**
** Photivo is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Photivo.  If not, see <http://www.gnu.org/licenses/>.
**
*******************************************************************************/

#ifndef PTIMAGESPOTMODEL_H
#define PTIMAGESPOTMODEL_H

//==============================================================================

#include <QStandardItemModel>
#include <QList>
#include <QString>
#include <QSettings>

#include "ptImageSpot.h"

//==============================================================================

class ptImageSpotModel: public QStandardItemModel {
  Q_OBJECT

public:
  /*! Constructs an empty \c ptImageSpotModel object.
    \param ASizeHint
      The size hint used for each item in the model. Width should be \c 0 and height
      large enough to contain the editor widget.
    \param APtsSectionName
      Name of the pts inifile section for this spot type. Must be unique!
    \param AParent
      The models parent widget.
  */
  ptImageSpotModel(const QSize ASizeHint,
                   const QString &APtsSectionName,
                   QObject *AParent = nullptr);
  ~ptImageSpotModel();

  /*! Appends a new spot to the end of the spot list.
   \param ANewSpot
     A pointer to the spot object. The model takes ownership of the spot. */
  void appendSpot(ptImageSpot *ANewSpot);

  /*! Reimplemented from parent class. */
  virtual Qt::ItemFlags flags(const QModelIndex &index) const;

  /*! Loads a list of spots from a pts file. The existing list is replaced.
      \param APtsFile
        A pointer to the \c QSettings object representing the pts file. */
  void ReadFromFile(QSettings *APtsFile);

  /*! Update an item and the underlying spot repair data.  */
  virtual bool setData(const QModelIndex &index,
                       const QVariant    &value,
                       int               role = Qt::EditRole);

  /*! Assigns new \data to a spot.
      \param AIndex
        The list index of the spot.
      \param ASpotData
        A pointer to the new spot object. The old data object is deleted and \c ASpotData
        reparented to the model. */
  virtual void setSpot(const int AIndex, ptImageSpot *ASpotData);

  /*! Returns a pointer to the spot at list position \c AIndex. */
  ptImageSpot *spot(const int AIndex) { return FSpotList->at(AIndex); }

  /*! Reimplemented from parent class. */
  virtual Qt::DropActions supportedDropActions() const;

  /*! Remove one or more items from the model and delete the underlying repair spots. */
  bool removeRows(int row, int count, const QModelIndex &parent);

  /*! Saves complete spotlist to pts file.
      \param APtsFile
        A Pointer to a \c QSettings object that represents the pts file. */
  void WriteToFile(QSettings *APtsFile);

//------------------------------------------------------------------------------

protected:
  void              ClearList();

  /*! Synchronises the model to the spot list.
      Included data: name of repair algorithm, enabled state. */
  void              RebuildModel();

  QString               FPtsName;
  QSize                 FSizeHint;
  QList<ptImageSpot*>  *FSpotList;

//------------------------------------------------------------------------------

private:
  /*! Returns a spot’s name and coordinates in the form "spotname\t@x,y".
      This formatting is needed for the ListView caption. */
  QString AppendCoordsToName(const ptImageSpot *ASpot);


};
#endif // PTIMAGESPOTMODEL_H