/*******************************************************************************
**
** Photivo
**
** Copyright (C) 2011 Bernd Schoeler <brjohn@brother-john.net>
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

#ifndef PTRICHRECTINTERACTION_H
#define PTRICHRECTINTERACTION_H

#include <QGraphicsRectItem>
#include <QMouseEvent>
#include <QRectF>

#include "ptImageInteraction.h"

///////////////////////////////////////////////////////////////////////////
//
// Custom types used by ptRichRectInteraction
//
///////////////////////////////////////////////////////////////////////////

// Position of the mouse when button pressed for dragging
enum ptMovingEdge {
  meNone = 0,
  meTop = 1,
  meRight = 2,
  meBottom = 3,
  meLeft = 4,
  meTopLeft = 5,
  meTopRight = 6,
  meBottomLeft = 7,
  meBottomRight = 8,
  meCenter = 9      // move rect instead of resize
};


///////////////////////////////////////////////////////////////////////////
//
// class ptSimpleRectInteraction
//
///////////////////////////////////////////////////////////////////////////
class ptRichRectInteraction : public ptImageInteraction {
Q_OBJECT

///////////////////////////////////////////////////////////////////////////
//
// PUBLIC members
//
///////////////////////////////////////////////////////////////////////////
public:
  explicit ptRichRectInteraction(QGraphicsView* View,
                                 const int x,
                                 const int y,
                                 const int width,
                                 const int height,
                                 const short FixedAspectRatio,
                                 const uint AspectRatioW,
                                 const uint AspectRatioH,
                                 const short CropGuidelines);
  ~ptRichRectInteraction();

  void setAspectRatio(const short FixedAspectRatio,
                      uint AspectRatioW,
                      uint AspectRatioH,
                      const short ImmediateUpdate = 1);

///////////////////////////////////////////////////////////////////////////
//
// PRIVATE members
//
///////////////////////////////////////////////////////////////////////////
private:
  QRectF* m_Rect;
  QGraphicsRectItem* m_RectItem;

  double      m_AspectRatio;        //  m_AspectRatioW / m_AspectRatioH
  uint        m_AspectRatioW;
  uint        m_AspectRatioH;
  short       m_CropGuidelines;
  short       m_FixedAspectRatio;   // 0: fixed AR, 1: no AR restriction

///////////////////////////////////////////////////////////////////////////
//
// PRIVATE slots
//
///////////////////////////////////////////////////////////////////////////
private slots:
  void mouseAction(QMouseEvent* event);

};

#endif // PTRICHRECTINTERACTION_H
