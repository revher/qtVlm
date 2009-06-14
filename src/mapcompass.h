/**********************************************************************
qtVlm: Virtual Loup de mer GUI
Copyright (C) 2008 - Christophe Thomas aka Oxygen77

http://qtvlm.sf.net

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
***********************************************************************/

#ifndef MAPCOMPASS_H
#define MAPCOMPASS_H

#include <QWidget>
#include <QPainter>

#include "Projection.h"

class mapCompass : public QWidget
{
    public:
        mapCompass(Projection * proj,QWidget *parentWindow=NULL);

    private:
        int size;
        bool isMoving;
        int mouse_x,mouse_y;
        bool mouseEvt;
        Projection * proj;

        void  paintEvent(QPaintEvent *event);
        void  mousePressEvent(QMouseEvent *);
        void  mouseReleaseEvent(QMouseEvent *e);
        void  mouseMoveEvent (QMouseEvent * e);
};

#endif // MAPCOMPASS_H
