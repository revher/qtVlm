#ifndef VLMPOINTGRAPHIC_H
#define VLMPOINTGRAPHIC_H
#include <QGraphicsScene>
#include <QGraphicsWidget>
#include <QPainter>
#include <QObject>
#include "Projection.h"
#include "class_list.h"

class vlmPointGraphic : public QGraphicsWidget
{ Q_OBJECT
public:
    vlmPointGraphic(ROUTAGE * routage,int isoNb, int pointIsoNb, double lon, double lat, Projection * proj, QGraphicsScene * myScene,int z_level);
    ~vlmPointGraphic();
    void setIsoNb(int n){this->isoNb=n;}
    void setPointIsoNb(int n){this->pointIsoNb=n;}
    void setEta(time_t eta);
    void shown(bool b){if (b) show(); else hide();}
    void setDebug(QString f){this->debug=f;}
    void createPopupMenu();
    ROUTAGE * getRoutage(){return routage;}
protected:
    void paint(QPainter * pnt, const QStyleOptionGraphicsItem * , QWidget * );
    QRectF boundingRect() const;
//    void  hoverEnterEvent ( QGraphicsSceneHoverEvent * event );
//    void  hoverLeaveEvent ( QGraphicsSceneHoverEvent * event );
    void contextMenuEvent(QGraphicsSceneContextMenuEvent * e);

public slots:
    void drawWay(void);
    void slot_showMe(void);
    void slot_updateTip(int i,int n, QString t);
private:
    QGraphicsScene * myScene;
    Projection * proj;
    ROUTAGE * routage;
    int isoNb;
    int pointIsoNb;
    double lon,lat;
    time_t eta;
    QString debug;
};

#endif // VLMPOINTGRAPHIC_H
