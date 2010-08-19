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

#include <QDebug>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QGraphicsSceneMouseEvent>
#include <cmath>

#include "mapcompass.h"
#include "Util.h"
#include "Orthodromie.h"
#include "settings.h"
#include "mycentralwidget.h"
#include "orthoSegment.h"
#include "MainWindow.h"
#include "Grib.h"
#include "Projection.h"
#include "boatAccount.h"

#define COMPASS_MARGIN 30
#define CROSS_SIZE 8
#define MARK_SIZE 12
#define DELTA 4

mapCompass::mapCompass(Projection * proj,MainWindow * main,myCentralWidget *parent) : QGraphicsWidget()
{
    busy=false;
    isMoving=false;
    drawCompassLine=false;
    this->proj=proj;
    this->parent=parent;
    this->main=main;

    setPos(200,200);
    setZValue(Z_VALUE_COMPASS);
    setData(0,COMPASS_WTYPE);

    /* compassLine */
    compassLine = new orthoSegment(proj,parent->getScene(),Z_VALUE_COMPASS);
    connect(parent,SIGNAL(stopCompassLine()),this,SLOT(slot_stopCompassLine()));
    connect(proj,SIGNAL(projectionUpdated()),this,SLOT(slot_projectionUpdated()));
    QPen penLine(QColor(Qt::white));
    penLine.setWidthF(1.6);
    compassLine->setLinePen(penLine);

    hdg_label = new QGraphicsTextItem();
    hdg_label->setZValue(Z_VALUE_COMPASS);
    hdg_label->setHtml("Hdg:");
    hdg_label->hide();
    QFont fnt=hdg_label->font();
    fnt.setBold(true);
    hdg_label->setFont(fnt);
    parent->getScene()->addItem(hdg_label);

    windAngle_label = new QGraphicsTextItem();
    windAngle_label->setZValue(Z_VALUE_COMPASS);
    windAngle_label->setPlainText("Twa/Bs:");
    windAngle_label->setAcceptHoverEvents(false);
    windAngle_label->hide();
    windAngle_label->setFont(fnt);
    parent->getScene()->addItem(windAngle_label);    

    //   size = 310;
    size = 620;
    poly.resize(0);
    boundingR=QRectF(0,0,0,0);
    slot_paramChanged();
    myLon=-1;
    myLat=-1;
}
mapCompass::~mapCompass()
{
}
void mapCompass::slot_paramChanged(void)
{
    if(!(Settings::getSetting("showCompass",1).toInt()==1) && !(Settings::getSetting("showPolar",1).toInt()==1))
       hide();
    else
       show();
    this->slot_compassCenterBoat();
    update();
}

QRectF mapCompass::boundingRect() const
{
    if(polarModeVac)
    {
        return boundingR.united(QRectF(0,0,size,size));
    }
    else
        return QRectF(0,0,size,size);
}
QPainterPath mapCompass::shape() const
{
    QPainterPath path;
    if((Settings::getSetting("compassCenterBoat","0")=="1") || parent->getCompassFollow()!=NULL)
        path.addEllipse(QRectF(size/10,size/10,size/10,size/10));
    else
        path.addEllipse(QRectF(size/4,size/4,size/2,size/2));
    return path;
}

void  mapCompass::paint(QPainter * pnt, const QStyleOptionGraphicsItem * , QWidget * )
{
    if(busy) return;
    busy=true;
    pnt->setRenderHint(QPainter::Antialiasing);
    QFontMetrics fm(pnt->font());
    int str_w,str_h,x,y;
    double lat,lon;
    double WP_lat,WP_lon;
    float WP_angle=-1;
    double WP_dist=-1;
    float angle;
    QString str;    
    proj->screen2map(this->x()+size/2,this->y()+size/2,&lon,&lat);

    QPen pen(QColor(Qt::black));

    str_h=fm.height();
    int diam=(size/2-2*COMPASS_MARGIN)/2;
    pnt->fillRect(0,0, size,size, QBrush(QColor(255,255,255,00)));

    pen.setWidth(1);
    pnt->setPen(pen);

    /* move center to center of circle */
    pnt->translate(size/2,size/2);

    /* center cross */

    pnt->drawLine(-(CROSS_SIZE/2),0,(CROSS_SIZE/2),0);
    pnt->drawLine(0,-(CROSS_SIZE/2),0,(CROSS_SIZE/2));
    if(Settings::getSetting("showCompass",1).toInt()==1)
    {
        /* external compass : direction */

        /* external numbers */
        for(int i=0;i<360;i+=10)
        {
            str=QString().setNum(i);
            str_w=fm.width(str);
            angle=degToRad(i-90);
            x=(int)((diam+12)*cos(angle)-(float)(str_w/2));
            y=(int)((diam+12)*sin(angle)+(float)(str_h/2));
            pnt->drawText(x,y,str);
        }

        /* External marks */
        for(int i=0;i<360;i++)
        {
            if(i%10==0) // big marks
            {
                pen.setWidth(2);
                pnt->setPen(pen);
                pnt->drawLine(0,-diam+MARK_SIZE,0,-diam);
            }
            else if(i%5==0) // medium marks
            {
                pen.setWidth(1);
                pnt->setPen(pen);
                pnt->drawLine(0,-diam+MARK_SIZE,0,-diam+6);
            }
            else if(i%1==0) // small marks
            {
                pen.setWidth(1);
                pnt->setPen(pen);
                pnt->drawLine(0,-diam+MARK_SIZE,0,-diam+10);
            }
            pnt->rotate(1);
        }

        /* WP info */
        main->getBoatWP(&WP_lat,&WP_lon);
        if(WP_lat!=-1 && WP_lat!=0 && WP_lon!=-1 && WP_lon!=0)
        {
            Orthodromie orth(lon,lat,WP_lon,WP_lat);
            WP_angle=qRound(orth.getAzimutDeg());
            WP_dist=orth.getDistance();
            pnt->rotate(WP_angle);
            pen.setColor(Qt::white);
            pen.setWidth(4);
            pnt->setPen(pen);
            pnt->drawLine(0,-diam+MARK_SIZE,0,-diam+MARK_SIZE+DELTA);
            pnt->rotate(-WP_angle);
        }

        /* Internal compas : WIND */

        /* internal numbers */
        /*compute start angle = wind angle*/
        wind_angle=0;
        Grib * grib = parent->getGrib();
        if(grib)
            grib->getInterpolatedValue_byDates(lon,lat,grib->getCurrentDate(),&wind_speed,&wind_angle);
        wind_angle=radToDeg(wind_angle);
        main->getBoatBvmg(&bvmg_up,&bvmg_down,wind_speed);
        pen.setColor(Qt::black);
        pnt->setPen(pen);

        for(int i=0;i<360;i+=20)
        {
            str=QString().setNum(i>180?i-360:i);
            str_w=fm.width(str);
            angle=degToRad((i+wind_angle)-90);
            x=(int)((diam-2*MARK_SIZE-4*DELTA)*cos(angle)-(float)(str_w/2));
            y=(int)((diam-2*MARK_SIZE-4*DELTA)*sin(angle)+(float)(str_h/2));
            pnt->drawText(x,y,str);
        }

        /* internal marks */
        pnt->rotate(wind_angle);
        int i_180;
        for(int i=0;i<360;i++)
        {
            if(i>180)
                i_180=360-i;
            else
                i_180=i;
            if((i_180<qRound(bvmg_up)  || i_180>qRound(bvmg_down)) && qRound(bvmg_up)!=-1)
                pen.setColor(Qt::red);
            else
                pen.setColor(Qt::black);
            pen.setStyle(Qt::SolidLine);
            if(i_180==qRound(bvmg_up)  || i_180==qRound(bvmg_down))
            {
                pen.setColor(Qt::red);
                pen.setStyle(Qt::DashLine);
                pen.setWidth(1);
                pnt->setPen(pen);
                pnt->drawLine(0,0,0,-diam-30);
            }
            else if(i%10==0) // big marks
            {
                pen.setWidth(2);
                pnt->setPen(pen);
                pnt->drawLine(0,-diam+DELTA+MARK_SIZE,0,-diam+DELTA+2*MARK_SIZE);
            }
            else if(i%5==0) // medium marks
            {
                pen.setWidth(1);
                pnt->setPen(pen);
                pnt->drawLine(0,-diam+DELTA+MARK_SIZE,0,-diam+DELTA+2*MARK_SIZE-6);
            }
            else if(i%1==0) // small marks
            {
                pen.setWidth(1);
                pnt->setPen(pen);
                pnt->drawLine(0,-diam+DELTA+MARK_SIZE,0,-diam+DELTA+2*MARK_SIZE-10);
            }
            pnt->rotate(1);
        }
        pnt->rotate(-wind_angle);
    }
    else
    {
        wind_angle=0;
        wind_speed=0;
        main->getBoatBvmg(&bvmg_up,&bvmg_down,wind_speed);
    }
    /* draw Polar */
    polarModeVac=false;
    if(bvmg_up!=-1 && Settings::getSetting("showPolar",1).toInt()==1)
    {
        if(Settings::getSetting("showCompass",1).toInt()==0)
        {
            Grib * grib = parent->getGrib();
            if(grib)
            {
                grib->getInterpolatedValue_byDates(lon,lat,grib->getCurrentDate(),&wind_speed,&wind_angle);
                wind_angle=radToDeg(wind_angle);
            }
        }
        main->getBoatBvmg(&bvmg_up,&bvmg_down,wind_speed);
        float speeds[181];
        for(int i=0;i<=180;i++)
        {
            speeds[i]=main->getBoatPolarSpeed(wind_speed,i);
        }
        poly.resize(361);
        int polVac=0;
        if(Settings::getSetting("scalePolar",0).toInt()==0)
        {
            polVac=Settings::getSetting("polVac",12).toInt();
            polarModeVac=true;
        }
        else if(Settings::getSetting("scalePolar",0).toInt()==2)
        {
            polVac=Settings::getSetting("estimeVac",12).toInt();
            polarModeVac=true;
        }
        double lon1,lat1,lon2,lat2;
        int X,Y;
        proj->screen2map(0,0, &lon1, &lat1);
        Util::getCoordFromDistanceAngle(lat1,lon1,100,90,&lat2,&lon2);
        Util::computePos(proj,(float)lat2,(float)lon2, &Y, &X);
        float oneMile=qAbs(QLineF(0,0,X,Y).length())/100;
        int vacLen=parent->getSelectedBoat()->getVacLen()/60.0;
        for(int i=0;i<=180;i++)
        {
            float temp=0;
            if(!polarModeVac)
                temp=speeds[i]*(size/3)/50.000;
            else
                temp=(speeds[i]/60.0*vacLen*polVac)*oneMile;
//                temp=speeds[i]*(size/3)/main->getBoatPolarMaxSpeed();
            poly[i]=QPointF(temp*sin(degToRad(i)),-temp*cos(degToRad(i)));
            if(i!=180)
                poly[360-i]=QPointF(-temp*sin(degToRad(i)),-temp*cos(degToRad(i)));
        }
        QMatrix matrix;
        matrix.rotate(wind_angle);
        poly=matrix.map(poly);
        pen.setWidth(2);
        pen.setColor(Qt::darkGreen);
        pnt->setPen(pen);
        pnt->drawPolyline(poly);
        QPolygonF polyRed(0);
        pen.setColor(Qt::red);
        pnt->setPen(pen);
        for (int n=0;n<=qRound(bvmg_up);n++)
            polyRed.append(poly[qRound(360-bvmg_up)+n]);
//        polyRed.putPoints(0,qRound(bvmg_up),poly,qRound(360-bvmg_up));
        for (int n=0;n<=qRound(bvmg_up);n++)
            polyRed.append(poly[n]);
//        polyRed.putPoints(qRound(bvmg_up),qRound(bvmg_up),poly,0);
        pnt->drawPolyline(polyRed);
        polyRed.resize(0);
        for (int n=0;n<=qRound(180-bvmg_down);n++)
            polyRed.append(poly[qRound(bvmg_down)+n]);
//        polyRed.putPoints(0,qRound(180-bvmg_down),poly,qRound(bvmg_down));
        for (int n=0;n<=qRound(180-bvmg_down);n++)
            polyRed.append(poly[180+n]);
//        polyRed.putPoints(qRound(180-bvmg_down),qRound(180-bvmg_down),poly,180);
        pnt->drawPolyline(polyRed);
    }
    else
    {
        poly.resize(0);
    }
    pen.setWidth(1);
    pen.setColor(Qt::black);
    pnt->setPen(pen);
//    if (poly.size()!=0)
//    {
//        QRectF r1=poly.boundingRect();
//        QRectF r2=QRectF(-size/2,-size/2,size,size);
//        pnt->drawRect(r1);
//        pnt->drawRect(r2);
//        pnt->drawRect(r1.united(r2));
//    }

    /* text de lattitude/longitude*/
    if(isMoving)
    {
        QString lat_txt,lon_txt,wind_txt,s,z,WP_txt;
        int lat_w,wind_w,WP_w;

        QFont fnt=pnt->font();
        fnt.setBold(true);
        QFontMetrics fm2(fnt);
        pnt->setFont(fnt);
        str_h=fm.height();

        lat_txt=Util::pos2String(TYPE_LAT,lat);
        lon_txt=Util::pos2String(TYPE_LON,lon);
        s.sprintf("%.1f", wind_angle);
        z.sprintf("%.1f", wind_speed);
        wind_txt = tr("Vent: ") + s + tr("°")+", "+z+tr("nds");
        lat_w=fm2.width(lat_txt);
        wind_w=fm2.width(wind_txt);

        pnt->drawText(-lat_w-2,-str_h/2,lat_txt);
        pnt->drawText(2,-str_h/2,lon_txt);
        pnt->drawText(-wind_w/2,-str_h/2-str_h,wind_txt);

        /* WP Data */
        if(WP_angle!=-1)
        {
            double WP_windAngle=WP_angle-wind_angle;
            if(qAbs(WP_windAngle)>180)
            {
                if(WP_windAngle<0)
                    WP_windAngle=360+WP_windAngle;
                else
                    WP_windAngle=WP_windAngle-360;
            }
            s.sprintf("%.1f", WP_angle);
            WP_txt=tr("WP: ") + s + tr("°");
            WP_w=fm2.width(WP_txt);
            pnt->drawText(-WP_w/2,str_h,WP_txt);

            s.sprintf("%.1f", WP_windAngle);
            WP_txt=tr("WP%vent: ") + s + tr("°");
            WP_w=fm2.width(WP_txt);
            pnt->drawText(-WP_w/2,2*str_h,WP_txt);

            s.sprintf("%.1f", WP_dist);
            WP_txt=tr("WP: ") + s + tr(" nm");
            WP_w=fm2.width(WP_txt);
            pnt->drawText(-WP_w/2,3*str_h,WP_txt);

        }
    }
//    pnt->drawRect(QRectF(0,0,size,size));
    boundingR=poly.boundingRect();
    QPointF center=boundingR.center();
    boundingR.setWidth(boundingR.width()*2);
    boundingR.setHeight(boundingR.height()*2);
    boundingR.moveCenter(center);
    busy=false;
#if 0 //draw the bloody boundingRect
    pnt->drawRect(boundingR.toRect());
#endif
}

void  mapCompass::mousePressEvent(QGraphicsSceneMouseEvent * e)
{
    if (e->button() == Qt::LeftButton)
    {
        if(!drawCompassLine && !(Settings::getSetting("compassCenterBoat","0")=="1") && parent->getCompassFollow()==NULL)
        {
            isMoving=true;
            mouseEvt=false;
            mouse_x=e->scenePos().x();
            mouse_y=e->scenePos().y();
            poly.resize(0);
            update();
        }
    }
    if (e->button() == Qt::MidButton)
    {
        slot_compassCenterBoat();
    }
}

//-------------------------------------------------------------------------------
void mapCompass::slot_shCom()
{
    /* si non visible � l'ecran => on le rend visible et show() */
    bool shCom;
    if(!proj->isPointVisible(myLon,myLat))
    {
        myLon=proj->getCX();
        myLat=proj->getCY();
        parent->setCompassFollow(NULL);
        this->compassCenter(myLon,myLat);
        shCom=true;
    }
    else
        shCom=!(Settings::getSetting("showCompass",1).toInt()==1);
    Settings::setSetting("showCompass",shCom?1:0);
    if(!shCom && !Settings::getSetting("showPolar",1).toInt()==1)
        hide();
    else
    {
       /* if(!proj->isPointVisible(myLon,myLat))
        {
            myLon=proj->getCX();
            myLat=proj->getCY();
            parent->setCompassFollow(NULL);
            this->compassCenter(myLon,myLat);
        }*/
        show();
    }
    update();
}
void mapCompass::slot_shPol()
{
    bool shPol;
    if(!proj->isPointVisible(myLon,myLat))
    {
        myLon=proj->getCX();
        myLat=proj->getCY();
        parent->setCompassFollow(NULL);
        this->compassCenter(myLon,myLat);
        shPol=true;
    }
    else
        shPol=!(Settings::getSetting("showPolar",1).toInt()==1);

    Settings::setSetting("showPolar",shPol?1:0);
    if(!shPol && !Settings::getSetting("showCompass",1).toInt()==1)
        hide();
    else
    {
        /*if(!proj->isPointVisible(myLon,myLat))
        {
            myLon=proj->getCX();
            myLat=proj->getCY();
            parent->setCompassFollow(NULL);
            this->compassCenter(myLon,myLat);
        }*/
        show();
    }
    update();
}
void mapCompass::slot_projectionUpdated()
{
    int new_x=0;
    int new_y=0;
    if((myLon==-1 && myLat==-1)||(myLon==0 && myLat==0))
    {
        main->get_selectedBoatPos(&myLat,&myLon);
        if((myLon==-1 && myLat==-1)||(myLon==0 && myLat==0)) return;
    }
    Util::computePos(proj,myLat, myLon, &new_x, &new_y);
    new_x=new_x-size/2;
    new_y=new_y-size/2;
    prepareGeometryChange();
    setPos(new_x, new_y);
}
void mapCompass::slot_compassCenterBoat()
{
    if(Settings::getSetting("compassCenterBoat","0")=="0") return;
    parent->slot_releaseCompassFollow();
    double lat,lon;
    main->get_selectedBoatPos(&lat,&lon);
    if(lat==-1 && lon==-1) return;
    myLon=lon;
    myLat=lat;
    int new_x=0;
    int new_y=0;
    Util::computePos(proj,lat, lon, &new_x, &new_y);
    new_x=new_x-size/2;
    new_y=new_y-size/2;
    prepareGeometryChange();
    setPos(new_x, new_y);
}
void mapCompass::compassCenter(double lon, double lat)
{
    myLon=lon;
    myLat=lat;
    int new_x=0;
    int new_y=0;
    Util::computePos(proj,lat, lon, &new_x, &new_y);
    new_x=new_x-size/2;
    new_y=new_y-size/2;
    prepareGeometryChange();
    setPos(new_x, new_y);
}
void mapCompass::slot_compassCenterWp()
{
        double lat,lon;
        main->getBoatWP(&lat,&lon);
        parent->slot_releaseCompassFollow();
        if(lat==-1 && lon==-1) return;
        myLon=lon;
        myLat=lat;
        int new_x=0;
        int new_y=0;
        Util::computePos(proj,lat, lon, &new_x, &new_y);
        new_x=new_x-size/2;
        new_y=new_y-size/2;
        prepareGeometryChange();
        setPos(new_x, new_y);
}
void  mapCompass::mouseReleaseEvent(QGraphicsSceneMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
    {
        if(!drawCompassLine)
        {
            isMoving=false;
            prepareGeometryChange();
            update();
        }
    }
}

bool mapCompass::tryMoving(int x, int y)
{
    if(drawCompassLine)
    {
        // show heading and wind angle label
        compassLine->show();
        updateCompassLineLabels(x, y);

        compassLine->moveSegment(x,y);
        return true;
    }

    if(isMoving)
    {
        int new_x=this->x()+(x-mouse_x);
        int new_y=this->y()+(y-mouse_y);

//        if(new_x<=-size/2)
//            new_x=-size/2;
//        if(new_y<=-size/2)
//            new_y=-size/2;
//        if(new_x>=parent->width()-size/2)
//            new_x=parent->width()-size/2;
//        if(new_y>=parent->height()-size/2)
//            new_y=parent->height()-size/2;
        proj->screen2map(new_x+size/2,new_y+size/2, &myLon, &myLat);
        prepareGeometryChange();
        setPos(new_x,new_y);
        mouse_x=x;
        mouse_y=y;
        return true;
    }
    else
        return false;
}

void mapCompass::updateCompassLineLabels(int x, int y)
{
    double pos_angle,pos_wind_angle,pos_distance;
    double xa,xb,ya,yb;
    int XX,YY;
    compassLine->getStartPoint(&XX,&YY);
    proj->screen2map(XX,YY,&xa,&ya);
    proj->screen2map(x,y,&xb,&yb);
    Orthodromie orth(xa,ya,xb,yb);
    pos_angle=orth.getAzimutDeg();
    pos_distance=orth.getDistance();
    bool drawWindAngle=true;
    if(this->isVisible())
    {
        hdg_label->setHtml(QString().sprintf("Hdg: %.1f %c, Tws: %.1f nds",pos_angle,176,wind_speed)+"<br>"+
                       "Distance: "+Util::formatDistance(pos_distance));
    /* attention wind_angle, bvmg_up et bvmg_down sont calcules dans paint */
        drawWindAngle=true;
    }
    else
    {
        double loxo_angle=orth.getLoxoCap();
        if(loxo_angle<0) loxo_angle+=360;
        double loxo_dist=orth.getLoxoDistance();
        hdg_label->setDefaultTextColor(Qt::darkRed);
        hdg_label->setHtml(QString().sprintf("<b><big>Ortho->Hdg: %.1f%c Dist: %.2f NM",pos_angle,176,pos_distance)+"<br>"+
                       QString().sprintf("<b><big>Loxo-->Hdg: %.1f%c Dist: %.2f NM",loxo_angle,176,loxo_dist));
        drawWindAngle=false;
    }
    if(!parent->getGrib())
        drawWindAngle=false;

    if(drawWindAngle)
    {
        pos_wind_angle=pos_angle-wind_angle;
        if(qAbs(pos_wind_angle)>180)
        {
            if(pos_wind_angle<0)
                pos_wind_angle=360+pos_wind_angle;
            else
                pos_wind_angle=pos_wind_angle-360;
        }

//        if(qAbs(pos_wind_angle)< bvmg_up || qAbs(pos_wind_angle)> bvmg_down)
//        {
//            windAngle_label->setDefaultTextColor(QColor(Qt::red));
//        }
//        else
//        {
            windAngle_label->setDefaultTextColor(QColor(Qt::black));
//        }
        float bs=main->getBoatPolarSpeed(wind_speed,qAbs(pos_wind_angle));
        QString s_eta;
        if(bs>0)
        {
            float ts=pos_distance/bs*60*60;
            float days=ts/86400.0000;
            if(qRound(days)>days)
                days=qRound(days)-1;
            else
                days=qRound(days);
            float hours=(ts-days*86400)/3600.0000;
            if(qRound(hours)>hours)
                hours=qRound(hours)-1;
            else
                hours=qRound(hours);
            float mins=(ts-days*86400-hours*3600)/60.0000;
            if(qRound(mins)>mins)
                mins=qRound(mins)-1;
            else
                mins=qRound(mins);
            float secs=qRound(ts-days*86400-hours*3600-mins*60);
            s_eta="Estime: "+QString::number((int)days)+" "+tr("jours")+" "+QString::number((int)hours)+" "+tr("heures")+" "+
                    QString::number((int)mins)+" "+tr("minutes")+" "+
                    QString::number((int)secs)+" "+tr("secondes");
        }
        else
            s_eta="Estime non calculable";
        windAngle_label->setHtml(QString().sprintf("Twa: %.1f %c, Bs: %.2f nds",pos_wind_angle,176,bs)+"<br>"+
                               QString().sprintf("Vmg-Vent: %.2f nds",bs*cos(degToRad(pos_wind_angle)))+"<br>"+
                               s_eta);
        windAngle_label->show();
    }
    else
    {
        windAngle_label->hide();
    }
    if(!drawWindAngle)
    {
        if(pos_angle>=270 || pos_angle<=90) /* upper */
            if(pos_angle>=270) /* right */
                hdg_label->setPos(x,y-hdg_label->boundingRect().height());
            else /* left */
                hdg_label->setPos(x-hdg_label->boundingRect().width(),y-hdg_label->boundingRect().height());
        else /*lower*/
            if (pos_angle>=180)
                hdg_label->setPos(x,y+hdg_label->boundingRect().height());
            else
                hdg_label->setPos(x-hdg_label->boundingRect().width(),y+hdg_label->boundingRect().height());
    }
    else
    {
        if( (pos_angle>285 && pos_angle<=360) || (pos_angle>=0 && pos_angle<75) || (pos_angle>105 && pos_angle<255)) /* horizontal */
        {
//            if(drawWindAngle)
//            {
                 hdg_label->setPos(x-hdg_label->boundingRect().width()-10,y-hdg_label->boundingRect().height()/2);
                 windAngle_label->setPos(x+10,y-windAngle_label->boundingRect().height()/2);
//            }
//             else
//                 hdg_label->setPos(x-hdg_label->boundingRect().width()/2,y-hdg_label->boundingRect().height()/2);
        }
        else /* vertical */
        {
//            if(drawWindAngle)
//            {
                hdg_label->setPos(x-hdg_label->boundingRect().width()/2,y-hdg_label->boundingRect().height()-10);
                windAngle_label->setPos(x-windAngle_label->boundingRect().width()/2,y+10);
//            }
//            else
//                hdg_label->setPos(x-hdg_label->boundingRect().width()/2,y-hdg_label->boundingRect().height()-10);
        }
    }
}

void mapCompass::slot_stopCompassLine(void)
{
    if(drawCompassLine) /* toggle compass line only if it is ON */
        slot_compassLine(0,0);
}

void mapCompass::slot_compassLine(int click_x, int click_y)
{
    drawCompassLine=!drawCompassLine;
    if(drawCompassLine)
    {
        windAngle_label->show();
        hdg_label->show();
        compassLine->show();
        if(!this->isVisible())
        {
            updateCompassLineLabels(click_x,click_y);
            compassLine->setAlsoDrawLoxo(true);
            compassLine->initSegment(click_x, click_y, click_x+10,click_y);
        }
        else
        {
            updateCompassLineLabels(click_x+10,click_y);
            compassLine->initSegment(this->x()+size/2,this->y()+size/2, click_x,click_y);
            compassLine->setAlsoDrawLoxo(false);
        }
    }
    else
    {
        windAngle_label->hide();
        hdg_label->hide();
        compassLine->hideSegment();
    }
}
