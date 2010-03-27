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


#include <QGraphicsWidget>
#include <QGraphicsSimpleTextItem>
#include <QPainter>
#include <QCursor>

#include "class_list.h"

#include <cmath>

class mapCompass : public QGraphicsWidget
{ Q_OBJECT
    public:
        mapCompass(Projection * proj,MainWindow * main,myCentralWidget * parent);
        double getWindAngle(void) { return wind_angle; }
        bool tryMoving(int x, int y);
        bool hasCompassLine(void) { return drawCompassLine; }


    public slots:
        void slot_compassLine(int mouse_x, int mouse_y);
        void slot_compassCenterBoat();
        void slot_compassCenterWp();
        void slot_stopCompassLine(void);
        void slot_paramChanged(void);
        QPainterPath shape() const;
        QRectF boundingRect() const;
        void slot_shHidden(){hide();}
        void slot_shCom();
        void slot_shPol();
    protected:
        void paint(QPainter * pnt, const QStyleOptionGraphicsItem * , QWidget * );

        void mousePressEvent(QGraphicsSceneMouseEvent *);
        void mouseReleaseEvent(QGraphicsSceneMouseEvent *e);
        void slotMouseDblClicked(QGraphicsSceneMouseEvent * e);
        //void contextMenuEvent(QGraphicsSceneContextMenuEvent * event);

    private:
        int size;
        QPolygon poly;
        int mouse_x;
        int mouse_y;
        bool isMoving;

        bool mouseEvt;
        Projection * proj;
        myCentralWidget * parent;
        MainWindow * main;
        QCursor enterCursor;
        double wind_angle;
        double wind_speed;
        float bvmg_up;
        float bvmg_down;

        /* Compass Line */        
        bool drawCompassLine;
        orthoSegment * compassLine;
        QGraphicsTextItem * hdg_label;
        QGraphicsTextItem * windAngle_label;
        void updateCompassLineLabels(int x, int y);
};

#endif // MAPCOMPASS_H
