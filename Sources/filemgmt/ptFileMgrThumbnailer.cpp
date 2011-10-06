/*******************************************************************************
**
** Photivo
**
** Copyright (C) 2011 Bernd Schoeler <brjohn@brother-john.net>
** Copyright (C) 2011 Michael Munzert <mail@mm-log.com>
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

#include <QDir>
#include <QStringList>
#include <QGraphicsTextItem>
#include <QApplication>

#include <Magick++.h>

#include "../ptDcRaw.h"
#include "../ptError.h"
#include "../ptCalloc.h"
#include "../ptDefines.h"
#include "../ptSettings.h"
#include "ptFileMgrThumbnailer.h"

extern ptSettings* Settings;
extern QStringList FileExtsRaw;
extern QStringList FileExtsBitmap;

//==============================================================================

ptFileMgrThumbnailer::ptFileMgrThumbnailer()
: QThread()
{
  m_Cache = NULL;
  m_Queue = NULL;
  m_Dir = "";
}

//==============================================================================

void ptFileMgrThumbnailer::setCache(ptThumbnailCache* cache) {
  if (!this->isRunning() && cache != NULL)
  {
    m_Cache = cache;
  }
}

//==============================================================================

void ptFileMgrThumbnailer::setDir(const QString dir) {
  if (!this->isRunning()) {
    m_Dir = dir;
  }
}

//==============================================================================

void ptFileMgrThumbnailer::setQueue(QQueue<ptGraphicsThumbGroup*>* queue) {
  if (!this->isRunning() && queue != NULL) {
    m_Queue = queue;
  }
}

//==============================================================================

void ptFileMgrThumbnailer::run() {
  QDir thumbsDir = QDir(m_Dir);

  // Check for properly set directory, cache and buffer
  if (!thumbsDir.exists() || m_Queue == NULL /*|| m_Cache == NULL*/) {
    return;
  }

  int thumbsSize = Settings->GetInt("ThumbnailSize");
  thumbsDir.setSorting(QDir::DirsFirst | QDir::Name);
  thumbsDir.setFilter(QDir::AllDirs | QDir::NoDot | QDir::Files);
  thumbsDir.setNameFilters(FileExtsRaw + FileExtsBitmap);
  QFileInfoList files = thumbsDir.entryInfoList();
  for (uint i = 0; i < (uint)files.count(); i++) {
    ptGraphicsThumbGroup* thumbGroup = new ptGraphicsThumbGroup;
    QGraphicsPixmapItem* thumbPixmap = new QGraphicsPixmapItem;

    // we have a directory
    if (files.at(i).isDir()) {
      if (files.at(i).fileName() == "..") {
        thumbPixmap->setPixmap(QPixmap(QString::fromUtf8(":/photivo/FileManager/up.png")));
      } else {
        thumbPixmap->setPixmap(QPixmap(QString::fromUtf8(":/photivo/FileManager/folder.png")));
      }

    // we have a file, see if we can get a thumbnail image
    } else {
      ptDcRaw dcRaw;
      if (dcRaw.Identify(files.at(i).absoluteFilePath()) == 0 ) {

        // we have a raw image ...
        QPixmap* px = new QPixmap;
        if (dcRaw.thumbnail(px)) {
          thumbPixmap->setPixmap(px->scaled(thumbsSize, thumbsSize,
                                            Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        DelAndNull(px);
      } else {
        // ... or a bitmap ...
        try {
          Magick::Image image(files.at(i).absoluteFilePath().toAscii().data());

          // We want 8bit RGB data without alpha channel, scaled to thumbnail size
          image.depth(8);
          image.magick("RGB");
          image.type(Magick::TrueColorType);
          image.zoom(Magick::Geometry(thumbsSize, thumbsSize));

          // TODO: read EXIF orientation and correct image

          // Get the raw image data from GM.
          uint w = image.columns();
          uint h = image.rows();
          uint8_t* ImgBuffer = NULL;
          try {
            ImgBuffer = (uint8_t*)CALLOC(w * h * 4, sizeof(ImgBuffer));
          } catch (std::bad_alloc) {
            // TODO: merge with following catch???
            printf("\n********************\n\nMemory error in thumbnail generator\n\n********************\n\n");
            fflush(stdout);
            throw std::bad_alloc();
          }
          ptMemoryError(ImgBuffer,__FILE__,__LINE__);
          image.write(0, 0, w, h, "BGRA", Magick::CharPixel, ImgBuffer);
          QPixmap px;
          // Detour via QImage necessary because QPixmap does not allow direct
          // access to the pixel data.
          px.convertFromImage(QImage(ImgBuffer, w, h, QImage::Format_RGB32));
          FREE(ImgBuffer);
          thumbPixmap->setPixmap(px);

        // ... or not a supported image file at all
        } catch (Magick::Exception &Error) {
          DelAndNull(thumbPixmap);
          DelAndNull(thumbGroup);
        }
      }
    }

    if (thumbGroup && thumbPixmap) {
      thumbGroup->addItems(thumbPixmap, new QGraphicsTextItem(files.at(i).fileName()));
      m_Queue->enqueue(thumbGroup);
    }

    QApplication::processEvents();

    // Notification signal that new thumbs are in the queue. Emitted every
    // five images. ptFileMgrWindow also reads in five image blocks.
//    if (i % 5 == 0) {
//      emit newThumbsNotify();
//    }
  }

  // final notification to make sure the queue gets completely emptied
  emit newThumbsNotify();
}

//==============================================================================
