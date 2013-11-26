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

Original code: zyGrib: meteorological GRIB file viewer
Copyright (C) 2008 - Jacques Zaninetti - http://zygrib.free.fr

***********************************************************************/

#ifndef TERRAIN_H
#define TERRAIN_H

#include <QGraphicsWidget>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QToolBar>
#include <QBitmap>
#include <QMutex>

#include "class_list.h"
#include "dataDef.h"

class Terrain : public QGraphicsWidget
{
    Q_OBJECT

public:
    Terrain(myCentralWidget *centralWidget, Projection *proj);

    void  setGSHHS_map(GshhsReader *map);

    void update_mapDataAndLevel(void);

    void setColorMapMode(int dataType, int levelType, int levelValue);
    FCT_GET(int,colorMapMode)
    FCT_GET(int,colorMapLevelType)
    FCT_GET(int,colorMapLevelValue)

    void setFrstArwMode(int mode, int levelType, int levelValue);
    FCT_GET(int,frstArwMode)
    FCT_GET(int,frstArwLevelType)
    FCT_GET(int,frstArwLevelValue)

    void setSecArwMode(int dataType,int levelType, int levelValue);
    FCT_GET(int,secArwMode)
    FCT_GET(int,secArwLevelType)
    FCT_GET(int,secArwLevelValue)

    void updateSize(int width, int height);

    QRectF boundingRect() const;
    QPainterPath shape() const;
    ROUTAGE * getRoutageGrib();
    void setRoutageGrib(ROUTAGE * routage);
    void setScalePos(const int &a, const int &b){this->scalePos=QPoint(a,b);}
    bool daylight(QPainter * pnt, const vlmPoint &coords);

    QSize getSize() const {return QSize(width,height);}

public slots :
    // Map
    void setDrawRivers(bool);
    void setDrawCountriesBorders(bool);
    void setCountriesNames(bool);

    void updateGraphicsParameters();

    void slot_setDrawWindColors    (bool);

    void setColorMapSmooth (bool);
    void setDrawWindArrows    (bool);
    void setDrawWavesArrows (bool b);
    void setBarbules          (bool);
    void setCitiesNamesLevel  (int level);
    void setDrawIsobars       (bool);
    void setDrawIsobarsLabels (bool);
    void setIsobarsStep		  (double step);
    void setPressureMinMax    (bool);
    void show_temperatureLabels(bool b);

    void setDrawIsotherms0       (bool);
    void setDrawIsotherms0Labels (bool);
    void setIsotherms0Step	     (double step);

    void redrawAll();
    void redrawGrib(void);



signals:
    void showContextualMenu(QGraphicsSceneContextMenuEvent * event);
    void mousePress(QGraphicsSceneMouseEvent* e);
    void mouseRelease(QGraphicsSceneMouseEvent* e);
    int  getRaceVacLen(boatVLM *,int*);
    void terrainUpdated();

protected:
    void paint(QPainter * pnt, const QStyleOptionGraphicsItem * , QWidget * );
    void contextMenuEvent(QGraphicsSceneContextMenuEvent * event);
    void mousePressEvent(QGraphicsSceneMouseEvent* e);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* e);

private:    
    //-----------------------------------------------
    int width,height;
    //-----------------------------------------------
    GshhsReader *gshhsReader;
    GisReader   *gisReader;
    Projection  *proj;
    myCentralWidget *centralWidget;

#ifdef __TERRAIN_QIMAGE
    QImage     *imgEarth;
#else
    QPixmap     *imgEarth;
#endif
    QPixmap     *imgWind;
    QPixmap     *imgAll;

    bool        isEarthMapValid;
    bool        isWindMapValid;
    bool        mustRedraw;
    QCursor     enterCursor;

    QColor  seaColor, landColor, backgroundColor, transparentColor;
    QColor  selectColor;

    double isobarsStep;
    bool   showIsobars;
    bool   showIsobarsLabels;
    bool   showPressureMinMax;

    double isotherms0Step;
    bool   showIsotherms0;
    bool   showIsotherms0Labels;

    bool  showTemperatureLabels;

    QPen    seaBordersPen;
    QPen    boundariesPen;
    QPen    riversPen;
    QPen    isobarsPen;
    QPen    isotherms0Pen;


    //-----------------------------------------------
    // Flags indiquant les elements a dessiner
    //-----------------------------------------------
    bool  showCountriesBorders;
    bool  showRivers;
    bool  showOrthodromie;
    bool  interpolateValues;
    bool  windArrowsOnGribGrid;

    bool  showWindColorMap;
    bool  colorMapSmooth;
    bool  showWindArrows;
    bool  showWavesArrows;
    bool  showBarbules;
    int   showCitiesNamesLevel;
    bool  showCountriesNames;

    int	colorMapMode, colorMapLevelType, colorMapLevelValue;
    int frstArwMode, frstArwLevelType, frstArwLevelValue;
    int secArwMode, secArwLevelType, secArwLevelValue;
    //void update_colorMapLevel(int *mode, int *levelType, int *levelValue);
    int compute_dataType(DataManager * dataManager,
                         int currentMode, int defaultMode1, int defaultMode2,
                         QMap<int,QStringList> * allowedMode);
    Couple compute_level(DataManager * dataManager,int newType,int curLevelType, int curLevelValue,
                         QMap<int,QStringList> * allowedLevel);

    //-----------------------------------------------
    void draw_GSHHSandGRIB(void);
    void drawGrib(QPainter &pnt);
    void indicateWaitingMap(void);
    void updateRoutine(void);
    bool toBeRestarted;
    ROUTAGE * routageGrib;
    QMutex mutex;
    QPoint scalePos;
    QTimer * timerUpdated;
};
Q_DECLARE_TYPEINFO(Terrain,Q_MOVABLE_TYPE);

#endif
