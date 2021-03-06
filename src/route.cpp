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

#include <cassert>
#include <QDateTime>
#include <QMessageBox>
#include <QDebug>
#include <QDomDocument>
#include <QFileDialog>

#include "route.h"

#include "Orthodromie.h"
#include "Projection.h"
#include "mycentralwidget.h"
#include "vlmLine.h"
#include "POI.h"
#include "DialogRoute.h"
#include "boatVLM.h"
#include "boatReal.h"
#include "Polar.h"
#include "Util.h"
#include "settings.h"
#include <QCache>
#include "Terrain.h"
#include "XmlFile.h"

#define USE_VBVMG_VLM

ROUTE::ROUTE(QString name, Projection *proj, DataManager *dataManager, QGraphicsScene * myScene, myCentralWidget *parentWindow)
            : QObject()

{
    this->proj=proj;
    this->name=name;
    this->myscene=myScene;
    this->dataManager=dataManager;
    this->parent=parentWindow;
    this->color=Settings::getSetting("routeLineColor", QColor(Qt::yellow)).value<QColor>();
    this->width=Settings::getSetting("routeLineWidth", 2.0).toDouble();
    this->startFromBoat=true;
    this->startTimeOption=1;
    this->line=new vlmLine(proj,myScene,Z_VALUE_ROUTE);
    connect(line,SIGNAL(hovered()),this,SLOT(hovered()));
    connect(line,SIGNAL(unHovered()),this,SLOT(unHovered()));
    line->setParent(this);
    this->frozen=false;
    this->poiName="";
    this->superFrozen=false;
    this->detectCoasts=true;
    this->optimizing=false;
    this->optimizingPOI=false;
    this->busy=false;
    this->startTime= QDateTime::currentDateTime().toUTC();
    this->has_eta=false;
    this->lastReachedPoi = NULL;
    this->eta=0;
    this->remain=0;
    this->myBoat=NULL;
    this->boatLogin="";
    this->my_poiList.clear();
    this->fastVmgCalc=false;
    this->startPoiName="";
    this->hasStartEta=false;
    this->startEta=0;
    this->hidePois=false;
    this->imported=false;
    this->multVac=1;
    this->useVbvmgVlm=false;
    this->setNewVbvmgVlm(false);
    showInterpolData=false;
    roadInfo=new routeInfo(parent,this);
    roadInfo->hide();
    pen.setColor(color);
    pen.setBrush(color);
    pen.setWidthF(width);
    this->initialized=false;
    connect (parent,SIGNAL(boatPointerHasChanged(boat*)),this,SLOT(slot_boatPointerHasChanged(boat*)));
    this->simplify=false;
    this->optimize=false;
    this->lastKnownSpeed=10e-4;
    this->hidden=false;
    this->temp=false;
    slot_shRou(parentWindow->get_shRoute_st());
    this->speedLossOnTack=1;
    this->autoRemove=Settings::getSetting("autoRemovePoiFromRoute",0).toInt()==1;
    this->autoAt=Settings::getSetting("autoFillPoiHeading",0).toInt()==1;
    this->sortPoisbyName=Settings::getSetting("routeSortByName",1).toInt()==1;
    this->pilototo=false;
    this->initialDist=0;
    this->roadMapInterval=1;
    this->roadMapHDG=0;
    this->useInterval=true;
    this->hypotPos=NULL;
    routeDelay=new QTimer(this);
    routeDelay->setInterval(5);
    routeDelay->setSingleShot(true);
    connect(routeDelay,SIGNAL(timeout()),this,SLOT(slot_recalculate()));
    this->strongSimplify=false;
    delay=10;
    forceComparator=false;
    fastSpeed=NULL;
}

ROUTE::~ROUTE()
{
    //qWarning() << "Deleting route: " << name<<"busy="<<busy;
    delete roadInfo;

    if(line)
    {
        if(!parent->getAboutToQuit())
        {
            disconnect(line,SIGNAL(hovered()),this,SLOT(hovered()));
            disconnect(line,SIGNAL(unHovered()),this,SLOT(unHovered()));
            line->deleteLater();
        }
    }
    if(hypotPos!=NULL)
    {
        delete tanPos;
        delete tanNeg;
        delete hypotPos;
        delete hypotNeg;
    }
    if(fastSpeed)
        delete fastSpeed;
}
void ROUTE::setShowInterpolData(bool b)
{
    this->showInterpolData=b;
    if(!this->initialized) showInterpolData=false;
    if(!this->hidden && showInterpolData)
        roadInfo->show();
    else
        roadInfo->hide();
}
void ROUTE::slot_calculateWithDelay()
{
    //qWarning()<<"delay="<<delay;
    if(!routeDelay->isActive())
    {
        if(delay>30)
            routeDelay->start(delay);
        else
            this->slot_recalculate();
    }
}

void ROUTE::setBoat(boat *curBoat)
{
    this->myBoat=curBoat;
    if(myBoat!=NULL && myBoat->get_boatType()==BOAT_VLM)
        this->boatLogin=((boatVLM*)myBoat)->getBoatPseudo();
    else
        this->boatLogin="";
    if(myBoat!=NULL && myBoat->get_boatType()==BOAT_REAL)
    {
        this->autoAt=false;
        this->autoRemove=false;
    }
    if(frozen)
    {
        setFrozen2(false);
        setFrozen2(true);
    }
}
void ROUTE::setWidth(double width)
{
    this->width=width;
    pen.setWidthF(width);
    line->setLinePen(pen);
}
void ROUTE::setColor(QColor color)
{
    this->color=color;
    pen.setColor(color);
    pen.setBrush(color);
    line->setLinePen(pen);
}
void ROUTE::insertPoi(POI *poi)
{
    my_poiList.append(poi);
    connect(poi,SIGNAL(poiMoving()),this,SLOT(slot_calculateWithDelay()));
    slot_recalculate();
}
void ROUTE::removePoi(POI *poi)
{
    my_poiList.removeAll(poi);
    disconnect(poi,SIGNAL(poiMoving()),this,SLOT(slot_calculateWithDelay()));
    slot_recalculate();
}
void ROUTE::hovered()
{
    foreach(POI* poi, this->my_poiList)
    {
        poi->setLabelTransp(false);
        poi->setZValue(30);
    }
    parent->setRouteToClipboard(this);
}
void ROUTE::unHovered()
{
    foreach(POI* poi, this->my_poiList)
    {
        poi->setLabelTransp(true);
        poi->setZValue(Z_VALUE_POI);
    }
    parent->setRouteToClipboard(NULL);
}

void ROUTE::zoom()
{
   double   xW, xE, yN, yS;

   QList<POI*>&                 route = this->getPoiList();
   QList<POI*>::const_iterator  poi   = route.begin();
   if (poi == route.end()) return;

   if (this->getStartFromBoat()) {
      xW = xE = this->getBoat()->getLon();
      yN = yS = this->getBoat()->getLat();
   } else {
      xW = xE = (*poi)->getLongitude();
      yN = yS = (*poi)->getLatitude();
      ++poi;
   }

   for (; poi != route.end(); ++poi) {
      const double  lon = (*poi)->getLongitude();
      if (lon < xW) {
         if (xW-lon < 180)
            xW = lon;
         else if (lon+360 > xE)
            xE = lon+360;
      } else if (lon > xE) {
         if (lon-xE < 180)
            xE = lon;
         else if (lon-360 < xW)
            xW = lon-360;
      }
      const double  lat = (*poi)->getLatitude();
      if (lat < yS) yS  = lat;
      if (lat > yN) yN  = lat;
   }

   //qWarning() << "Zooming to" << xW << "-" << xE << "by" << yS << "-" << yN;
   proj->zoomOnZone (xW,yN,xE,yS);
   proj->setScale (proj->getScale()*.9);
}

void ROUTE::slot_recalculate(boat * boat)
{
    if(fastSpeed)
    {
        delete fastSpeed;
        fastSpeed=NULL;
    }
    if(temp) return;
    QTime timeTotal;
    timeTotal.start();
    line->setCoastDetection(false);
    //QTime timeDebug;
    //int timeD=0;
    int nbLoop=0;
    if(parent->getAboutToQuit()) return;
    if(busy)
    {
        //busy=false; /*recursion*/
        return;
    }
    if(boat!=NULL && this->myBoat!=boat) return;
    if(myBoat==NULL  || !myBoat->getStatus()) return;
    if(this->hidden) return;
    if (frozen && initialized)
    {
        interpolatePos();
        return;
    }
    if(initialized && (frozen || superFrozen)) return;
    //qWarning()<<"calculating"<<this->name<<"with"<<my_poiList.count()<<"POIs"<<"and autoAt="<<this->autoAt;
    line->deleteAll();
    line->setHasInterpolated(false);
    line->setLinePen(pen);
    line->setCoastDetection(false);
    line->slot_showMe();
    roadMap.clear();
    if(my_poiList.count()==0) return;
    busy=true;
    if(this->sortPoisbyName)
        qSort(my_poiList.begin(),my_poiList.end(),POI::byName);
    else
        qSort(my_poiList.begin(),my_poiList.end(),POI::bySequence);
    if(!parent->getIsStartingUp() && !this->optimizing && !this->optimizingPOI && autoRemove && this->startFromBoat)
    {
        bool foundWP=false;
        int n=0;
        for (n=0;n<this->my_poiList.count();++n)
        {
            if(my_poiList.at(n)->getIsWp())
            {
                foundWP=true;
                break;
            }
        }
        if(foundWP)
        {
            POI * wp=my_poiList.at(n);
            while(my_poiList.first()!=wp)
            {
                my_poiList.first()->setMyLabelHidden(false);
                my_poiList.first()->setRoute(NULL);
            }
            if(my_poiList.count()==0)
            {
                busy=false;
                return;
            }
        }
    }
    bool firstPoint=true;
    double lastTwa=0;
    eta=0;
    has_eta=false;
    time_t now;
    if(myBoat!=NULL && myBoat->getPolarData() && dataManager && dataManager->isOk())
    {
        initialized=true;
        switch(startTimeOption)
        {
            case 1:
                if(myBoat->get_boatType()!=BOAT_VLM)
                    eta=((boatReal*)myBoat)->getLastUpdateTime();
                else
                    eta=((boatVLM*)myBoat)->getPrevVac()/*+((boatVLM*)myBoat)->getVacLen()*/;
                now = (QDateTime::currentDateTime()).toUTC().toTime_t();
//#warning find a better way to identify a boat that has not yet started
/*cas du boat inscrit depuis longtemps mais pas encore parti*/
                //if(eta < now - 2*myBoat->getVacLen() && myBoat->get_boatType()==BOAT_VLM)
                if(myBoat->getLoch()<0.01 && myBoat->get_boatType()==BOAT_VLM)
                    eta=now;
                break;
            case 2:
                eta=dataManager->get_currentDate();
                break;
            case 3:
                eta=startTime.toUTC().toTime_t();
                break;
        }
        has_eta=true;
        Orthodromie orth(0,0,0,0);
        QListIterator<POI*> i (my_poiList);
        QString tip;
        double lon,lat;
        if(startFromBoat)
        {
            lon=myBoat->getLon();
            lat=myBoat->getLat();
            lastReachedPoi = NULL;
        }
        else
        {
            POI * poi;
            poi=i.next();
            lon=poi->getLongitude();
            lat=poi->getLatitude();
            tip="<br>Starting point for route "+name;
#if 0
            if(startTimeOption==3)
                poi->setRouteTimeStamp((int)eta+myBoat->getVacLen()*multVac);
            else
                poi->setRouteTimeStamp((int)eta);
#else
            poi->setRouteTimeStamp((int)eta);
#endif
            poi->setTip(tip);
            lastReachedPoi = poi;
        }
        if (simplify || optimizing || optimizingPOI)
        {
            eta=start;
            lat=startLat;
            lon=startLon;
        }
        else
        {
            start=eta;
            startLat=lat;
            startLon=lon;
        }
        if(parent->getCompassFollow()==this)
            parent->centerCompass(lon,lat);
        if(!optimizing && (!optimizingPOI || !hasStartEta))
        {
            vlmPoint p(lon,lat);
            p.eta=eta;
            p.isPOI=true;
            line->addVlmPoint(p);
        }
        double newSpeed,distanceParcourue,remaining_distance,res_lon,res_lat,cap1,cap2,diff1,diff2;
        remaining_distance=0;
        double previous_remaining_distance=10e6;
        lastKnownSpeed=10e-4;
        double wind_angle,wind_speed,cap,angle,capSaved;
        cap=-1;
        capSaved=cap;
        time_t maxDate=dataManager->get_maxDate();
        QString previousPoiName="";
        time_t previousEta=0;
        time_t lastEta=0;
        time_t gribDate=dataManager->get_currentDate();
        if(this->my_poiList.isEmpty())
            initialDist=0;
        else
        {
            orth.setPoints(lon, lat, my_poiList.last()->getLongitude(),my_poiList.last()->getLatitude());
            initialDist=orth.getDistance();
        }
        double poiNb=-1;
        Orthodromie orth2(lon,lat,lon,lat);
        while(i.hasNext())
        {
            ++poiNb;
            int nbToReach=0;
            POI * poi = i.next();
            if(optimizingPOI && hasStartEta)
            {
                if(sortPoisbyName)
                {
                    if(poi->getName()<startPoiName)
                        continue;
                }
                else
                {
                    if(poi->getSequence()<startPoiName.toInt())
                        continue;
                }
                if ((sortPoisbyName && poi->getName()==startPoiName)||
                    (!sortPoisbyName && poi->getSequence()==startPoiName.toInt()))
                {
                    lon=poi->getLongitude();
                    lat=poi->getLatitude();
                    eta=startEta;
                    vlmPoint p(lon,lat);
                    p.eta=eta;
                    line->addVlmPoint(p);
                    orth2.setPoints(lon,lat,lon,lat);
                    continue;
                }
            }
            if(optimizingPOI)
                orth2.setEndPoint(poi->getLongitude(),poi->getLatitude());
            if(!dataManager->isOk() && !imported)
            {
                tip=tr("<br>Route: ")+name;
                tip=tip+"<br>Estimated ETA: No grib loaded" ;
                poi->setRouteTimeStamp(-1);
                poi->setTip(tip);
                has_eta=false;
//                qWarning()<<"no grib loaded for route!!";
                continue;
            }
            newSpeed=0;
            distanceParcourue=0;
            res_lon=0;
            res_lat=0;
            wind_angle=0;
            wind_speed=0;
            orth.setPoints(lon, lat, poi->getLongitude(),poi->getLatitude());
            remaining_distance=orth.getDistance();
            time_t Eta=0;
            bool engineUsed=false;
            if(has_eta)
            {
                //if(!busy) this->slot_recalculate();
                if(parent->getAboutToQuit()) return;
                do
                {
                    if(imported)
                        eta=poi->getRouteTimeStamp();
                    else
                        eta= eta + myBoat->getVacLen()*multVac;
                    Eta=eta;
                    if(((dataManager->getInterpolatedWind(lon, lat,
                                              eta,&wind_speed,&wind_angle,INTERPOLATION_DEFAULT)
                            && eta<=maxDate) || imported))
                    {
                        wind_angle=radToDeg(wind_angle);
                        double current_speed=-1;
                        double current_angle=0;
                        //calculate surface wind if any current
                        if(dataManager->hasData(DATA_CURRENT_VX,DATA_LV_MSL,0) && dataManager->getInterpolatedCurrent(lon, lat,
                                                  eta,&current_speed,&current_angle,INTERPOLATION_DEFAULT))
                        {
                            current_angle=radToDeg(current_angle);
                            QPointF p=Util::calculateSumVect(wind_angle,wind_speed,current_angle,current_speed);
                            wind_speed=p.x();
                            wind_angle=p.y();
                        }
                        else
                        {
                            current_speed=-1;
                            current_angle=0;
                        }
                        cap=orth.getAzimutDeg();
                        capSaved=cap;
                        double cog=cap;
                        double hdg=cap;
                        double sog=0;
                        double bs=0;
                        if(imported)
                        {
                            res_lon=poi->getLongitude();
                            res_lat=poi->getLatitude();
                        }
                        else
                        {
                            switch (poi->getNavMode())
                            {
                                case 0: //VBVMG
                                {
#if 0
                                    if(useVbvmgVlm && !this->fastVmgCalc && !parent->getIsStartingUp())
                                    {
                                        line->slot_showMe();
                                        QApplication::processEvents();
                                        double h1,h2,w1,w2,t1,t2,d1,d2;
                                        this->do_vbvmg_context(remaining_distance,cap,wind_speed,wind_angle,&h1,&h2,&w1,&w2,&t1,&t2,&d1,&d2);
                                        angle=A180(w1);
                                        cap=h1;
                                    }
#endif
                                    if(useVbvmgVlm && !this->fastVmgCalc && !parent->getIsStartingUp())
                                    {
                                        if(++nbLoop>100)
                                        {
                                            nbLoop=0;
                                            line->slot_showMe();
                                            QApplication::processEvents();
                                        }
                                        double h1,h2,w1,w2,t1,t2,d1,d2;
#if 0
                                        this->do_vbvmg_context(remaining_distance,cap,wind_speed,wind_angle,&h1,&h2,&w1,&w2,&t1,&t2,&d1,&d2);
                                        double w1Save=w1;
                                        double w2Save=w2;
                                        double h1Save=h1;
#endif
                                        //timeDebug.start();
                                        this->do_vbvmg_buffer(remaining_distance,cap,wind_speed,wind_angle,&h1,&h2,&w1,&w2,&t1,&t2,&d1,&d2);
                                        //timeD+=timeDebug.elapsed();
#if 0
                                        if(qRound(w1*1000)!=qRound(w1Save*1000) || qRound(h1*1000)!=qRound(h1Save*1000))
                                        {
                                            qWarning()<<"error in new vbvmg vlm"<<h1<<w1<<w2<<"should be"<<h1Save<<w1Save<<w2Save;
                                            this->do_vbvmg_buffer(remaining_distance,cap,wind_speed,wind_angle,&h1,&h2,&w1,&w2,&t1,&t2,&d1,&d2);
                                        }
#endif
                                        angle=A180(w1);
                                        cap=h1;
                                    }
                                    else
                                    {
                                        angle=A180(cap-wind_angle);
                                        if(qAbs(angle)<myBoat->getPolarData()->getBvmgUp(wind_speed))
                                        {
                                            angle=myBoat->getPolarData()->getBvmgUp(wind_speed);
                                            cap1=Util::A360(wind_angle+angle);
                                            cap2=Util::A360(wind_angle-angle);
                                            diff1=Util::myDiffAngle(cap,cap1);
                                            diff2=Util::myDiffAngle(cap,cap2);
                                            if(diff1<diff2)
                                                cap=cap1;
                                            else
                                                cap=cap2;
                                        }
                                        else if(qAbs(angle)>myBoat->getPolarData()->getBvmgDown(wind_speed))
                                        {
                                            angle=myBoat->getPolarData()->getBvmgDown(wind_speed);
                                            cap1=Util::A360(wind_angle+angle);
                                            cap2=Util::A360(wind_angle-angle);
                                            diff1=Util::myDiffAngle(cap,cap1);
                                            diff2=Util::myDiffAngle(cap,cap2);
                                            if(diff1<diff2)
                                                cap=cap1;
                                            else
                                                cap=cap2;
                                        }
                                    }
                                    break;
                                }

                                case 1: //BVMG
                                    if(fastVmgCalc || myBoat->get_boatType()==BOAT_REAL)
                                        myBoat->getPolarData()->getBvmg((cap-wind_angle),wind_speed,&angle);
                                    else
                                        myBoat->getPolarData()->bvmgWind((cap-wind_angle),wind_speed,&angle);
    #if 0
                                    if(qRound(angle*100.00)!=qRound(angleDebug*100.00))
                                        qWarning()<<"angle="<<angle<<" angleDebug="<<angleDebug;
    #endif
                                    cap=Util::A360(angle+wind_angle);
                                    break;
                                case 2: //ORTHO
                                    angle=A180(cap-wind_angle);
                                    break;
                            }

                            newSpeed=myBoat->getPolarData()->getSpeed(wind_speed,angle,true,&engineUsed);
                            if(engineUsed && poi->getNavMode()==1)
                            {
                                cap=capSaved;
                                angle=A180(cap-wind_angle);
                            }
                            hdg=cap;
                            bs=newSpeed;
                            if(current_speed>0)
                            {
                                QPointF p=Util::calculateSumVect(cap,newSpeed,Util::A360(current_angle+180.0),current_speed);
                                newSpeed=p.x(); //in this case newSpeed is SOG
                                cap=p.y(); //in this case cap is COG
                            }
                            sog=newSpeed;
                            cog=cap;
                            if (firstPoint)
                            {
                                firstPoint=false;
                            }
                            else if (speedLossOnTack!=1)
                            {
                                if ((angle>0 && lastTwa<0)||(angle<0 && lastTwa>0))
                                {
                                    //qWarning()<<"reducing polar because of tack/gybe";
                                    newSpeed=newSpeed*speedLossOnTack;
                                }
                            }
                            lastKnownSpeed=qMax(10e-4,newSpeed);
                            lastTwa=angle;
                            distanceParcourue=newSpeed*myBoat->getVacLen()*multVac/3600.00;

                            if(!imported && nbToReach==0)
                            {
                                if(distanceParcourue>remaining_distance)
                                {
                                    eta=eta-myBoat->getVacLen()*multVac;
                                    Eta=eta;
                                    if(!this->getSimplify() && poi==my_poiList.last())
                                    {
                                        QList<double> roadPoint;
                                        roadPoint.append((double)Eta); // 0
                                        roadPoint.append(0); // 1
                                        roadPoint.append(0); // 2
                                        roadPoint.append(0); //3
                                        roadPoint.append(-1); //4
                                        roadPoint.append(0); //5
                                        roadPoint.append(0); //6
                                        roadPoint.append(0); //7
                                        roadPoint.append(0); //8
                                        roadPoint.append(-1); //9
                                        roadPoint.append(0); //10
                                        roadPoint.append(0); //11
                                        roadPoint.append(-1); //12
                                        roadPoint.append(-1); //13
                                        roadPoint.append(-1); //14
                                        roadPoint.append(-1); //15
                                        roadPoint.append(-1); //16
                                        roadPoint.append(-1); //17
                                        roadPoint.append(-1); //18
                                        roadPoint.append(-1); //19
                                        roadPoint.append(-1); //20
                                        roadPoint.append(-1); //21 waves
                                        roadPoint.append(-1); //22 waves dir
                                        roadPoint.append(-1); //23 waves combined height
                                        roadMap.append(roadPoint);
                                    }
                                    break;
                                }
                            }
                            Util::getCoordFromDistanceAngle(lat, lon, distanceParcourue, cap,&res_lat,&res_lon);
                        }
                        previous_remaining_distance=orth.getDistance();
                        orth.setStartPoint(res_lon, res_lat);
                        remaining_distance=orth.getDistance();
                        lon=res_lon;
                        lat=res_lat;
                        //qWarning() << "" << eta << ";" << wind_angle << ";" << wind_speed << ";" << newSpeed << ";" << distanceParcourue << ";" << remaining_distance;
                        ++nbToReach;
                        if (!optimizing)
                        {
                            vlmPoint p(lon,lat);
                            p.eta=Eta;
                            line->addVlmPoint(p);
                            if(!this->getSimplify())
                            {
                                QList<double> roadPoint;
                                roadPoint.append((double)(Eta-myBoat->getVacLen())); // 0
                                roadPoint.append(poi->getLongitude()); // 1
                                roadPoint.append(poi->getLatitude()); // 2
                                roadPoint.append(Util::A360(hdg-myBoat->getDeclinaison())); //3
                                roadPoint.append(bs); //4
                                roadPoint.append(distanceParcourue); //5
                                roadPoint.append(wind_angle); //6
                                roadPoint.append(wind_speed); //7
                                roadPoint.append(A180(hdg-wind_angle)); //8
                                roadPoint.append(poiNb); //9
                                roadPoint.append(remaining_distance); //10
                                roadPoint.append(Util::A360(capSaved-myBoat->getDeclinaison())); //11
                                roadPoint.append(engineUsed?1:-1); //12
                                roadPoint.append(p.lon); //13
                                roadPoint.append(p.lat); //14
                                roadPoint.append(Util::A360(hdg)); //15
                                roadPoint.append(Util::A360(capSaved)); //16
                                roadPoint.append(Util::A360(cog)); //17
                                roadPoint.append(sog); //18
                                roadPoint.append(current_speed); //19
                                roadPoint.append(Util::A360(current_angle+180.0)); //20
                                if(dataManager->hasData(DATA_WAVES_MAX_HGT,DATA_LV_GND_SURF,0))
                                    roadPoint.append(dataManager->getInterpolatedValue_1D(DATA_WAVES_MAX_HGT,DATA_LV_GND_SURF,0,p.lon,p.lat,roadPoint.at(0))); //21
                                else
                                    roadPoint.append(-1);// 21
                                if(dataManager->hasData(DATA_WAVES_MAX_DIR,DATA_LV_GND_SURF,0))
                                    roadPoint.append(dataManager->getInterpolatedValue_1D(DATA_WAVES_MAX_DIR,DATA_LV_GND_SURF,0,p.lon,p.lat,roadPoint.at(0))); //21
                                else
                                    roadPoint.append(-1);// 22
                                if(dataManager->hasData(DATA_WAVES_SIG_HGT_COMB,DATA_LV_GND_SURF,0))
                                    roadPoint.append(dataManager->getInterpolatedValue_1D(DATA_WAVES_SIG_HGT_COMB,DATA_LV_GND_SURF,0,p.lon,p.lat,roadPoint.at(0))); //21
                                else
                                    roadPoint.append(-1);// 23
                                roadMap.append(roadPoint);
                            }
                            if(lastEta<gribDate && Eta>=gribDate)
                            {
                                if(!this->getSimplify() && this->showInterpolData)
                                {
                                    QList<double> roadPoint=roadMap.last();
                                    vlmPoint p(roadPoint.at(1),roadPoint.at(2));
                                    p.eta=roadPoint.at(0);
                                    bool night=false;
                                    if(parent->getTerre()->daylight(NULL,p))
                                        night=true;
                                    roadInfo->setValues(roadPoint.at(6),roadPoint.at(7),roadPoint.at(8),
                                                        roadPoint.at(4),roadPoint.at(3),roadPoint.at(11),
                                                        roadPoint.at(10),engineUsed,lat<0,roadPoint.at(17),
                                                        roadPoint.at(18),roadPoint.at(19),roadPoint.at(20),roadPoint.at(21),roadPoint.at(22),night,roadPoint.at(23));
                                }
                                if(gribDate>start+1000)
                                {
                                    line->setInterpolated(lon,lat);
                                    line->setHasInterpolated(true);
                                    if(parent->getCompassFollow()==this)
                                        parent->centerCompass(lon,lat);
                                }
                            }
                            lastEta=Eta;
                        }
                    }
                    else
                    {
                        has_eta=false;
                        orth.setPoints(res_lon,res_lat,my_poiList.last()->getLongitude(),my_poiList.last()->getLatitude());
                        remain=orth.getDistance();
                        break;
                    }
                    if(!imported &&(remaining_distance<distanceParcourue  ||
                                    previous_remaining_distance<distanceParcourue))
                    {
                        if(!this->getSimplify() && poi==my_poiList.last())
                        {
                            QList<double> roadPoint;
                            roadPoint.append((double)Eta); // 0
                            roadPoint.append(0); // 1
                            roadPoint.append(0); // 2
                            roadPoint.append(0); //3
                            roadPoint.append(-1); //4
                            roadPoint.append(distanceParcourue); //5
                            roadPoint.append(0); //6
                            roadPoint.append(0); //7
                            roadPoint.append(0); //8
                            roadPoint.append(-1); //9
                            roadPoint.append(0); //10
                            roadPoint.append(0); //11
                            roadPoint.append(-1); //12
                            roadPoint.append(-1); //13
                            roadPoint.append(-1); //14
                            roadPoint.append(-1); //15
                            roadPoint.append(-1); //16
                            roadPoint.append(-1); //17
                            roadPoint.append(-1); //18
                            roadPoint.append(-1); //19
                            roadPoint.append(-1); //20
                            roadPoint.append(-1); //21 waves height
                            roadPoint.append(-1); //22 waves dir
                            roadPoint.append(-1); //23 waves combined height
                            roadMap.append(roadPoint);
                        }
                        break;
                    }
                } while (has_eta && !imported);
            }
            if (has_eta)
                lastReachedPoi = poi;

            // If the target was "reached", this will be the remaining
            // distance between the position at the end of the last
            // vacation and the target. Otherwise, it will be the
            // distance between the end of the route and the last POI
            // of the route.
            remain = orth.getDistance();
            if(this->optimizingPOI && has_eta && !imported)
            {
                double dist1=orth2.getDistance();
                orth2.setEndPoint(lon,lat);
                if(orth2.getDistance()>dist1)
                    remain=-remain; /*case where we went over the target*/
            }
            if(this->autoAt && has_eta)
            {
                poi->setWph(qRound(cap*100)/100.0);
            }
//            else
//                poi->setWph(-1);
//            qWarning()<<"Distance Parcourue="<<distanceParcourue<<" remaining_distance="<<remaining_distance<<" previous_rd="<<previous_remaining_distance;
//            qWarning()<<"newSpeed="<<newSpeed<<" wind_speed="<<wind_speed<<" angle="<<angle;
            line->setLastPointIsPoi();
            tip=tr("<br>Route: ")+name;
            //time_t Eta=eta-myBoat->getVacLen();
            if(!has_eta)
            {
                tip=tip+tr("<br>ETA: Non joignable avec ce fichier GRIB");
                poi->setRouteTimeStamp(-1);
            }
            else if(Eta-start<=0)
            {
                tip=tip+tr("<br>ETA: deja atteint");
                poi->setRouteTimeStamp(Eta);
            }
            else
            {
                if(myBoat->get_boatType()==BOAT_VLM)
                    tip=tip+"<br>"+tr("Note: la date indiquee correspond a la desactivation du WP");
                time_t Start=start;
                if(startTimeOption==1)
                    Start=QDateTime::currentDateTimeUtc().toTime_t();
                //qWarning()<<"eta arrivee "<<eta;
                double days=(Eta-Start)/86400.0000;
                if(qRound(days)>days)
                    days=qRound(days)-1;
                else
                    days=qRound(days);
                double hours=(Eta-Start-days*86400)/3600.0000;
                if(qRound(hours)>hours)
                    hours=qRound(hours)-1;
                else
                    hours=qRound(hours);
                double mins=qRound((Eta-Start-days*86400-hours*3600)/60.0000);
                QString tt;
                QDateTime tm;
                tm.setTimeSpec(Qt::UTC);
                tm.setTime_t(Start);
                //qWarning()<<"tm="<<tm.toString();
                switch(startTimeOption)
                {
                    case 1:
                            tt="<br>"+tr("ETA a partir de maintenant")+" ("+tm.toString("dd MMM-hh:mm")+"):<br>";
                            break;
                    case 2:
                            tt="<br>"+tr("ETA depuis la date Grib")+" ("+tm.toString("dd MMM-hh:mm")+"):<br>";
                            break;
                    case 3:
                            tt="<br>"+tr("ETA depuis la date fixe")+" ("+tm.toString("dd MMM-hh:mm")+"):<br>";
                            break;
                }
                tip=tip+tt+QString::number((int)days)+" "+tr("jours")+" "+QString::number((int)hours)+" "+tr("heures")+" "+
                    QString::number((int)mins)+" "+tr("minutes");
#if 0
                if(myBoat->get_boatType()==BOAT_REAL)
                    poi->setRouteTimeStamp(Eta+myBoat->getVacLen());
                else
                    poi->setRouteTimeStamp(Eta);
#else
                poi->setRouteTimeStamp(Eta);
#endif
                if(poi==this->my_poiList.last())
                    eta=Eta;
            }
            poi->setTip(tip);
//            lon=poi->getLongitude();
//            lat=poi->getLatitude();
            if(optimizingPOI)
            {
                if(previousPoiName==poiName)
                    break;
                if(!hasStartEta)
                {
                    startEta=previousEta;
                    startPoiName=previousPoiName;
                }
                if(sortPoisbyName)
                    previousPoiName=poi->getName();
                else
                    previousPoiName.setNum(poi->getSequence());
                previousEta=Eta;
            }
            if(poi==this->my_poiList.last())
            {
                tip=tr("Route: ")+name;
                if(!has_eta)
                {
                    tip=tip+tr("<br>ETA: Non joignable avec ce fichier GRIB");
                }
                else if(Eta-start<=0)
                {
                    tip=tip+tr("<br>ETA: deja atteint");
                }
                else
                {
                    time_t Start=start;
                    if(startTimeOption==1)
                        Start=QDateTime::currentDateTimeUtc().toTime_t();
                    double days=(Eta-Start)/86400.0000;
                    if(qRound(days)>days)
                        days=qRound(days)-1;
                    else
                        days=qRound(days);
                    double hours=(Eta-Start-days*86400)/3600.0000;
                    if(qRound(hours)>hours)
                        hours=qRound(hours)-1;
                    else
                        hours=qRound(hours);
                    double mins=qRound((Eta-Start-days*86400-hours*3600)/60.0000);
                    QString tt;
                    QDateTime tm;
                    tm.setTimeSpec(Qt::UTC);
                    tm.setTime_t(Start);
                    //qWarning()<<"tm="<<tm.toString();
                    switch(startTimeOption)
                    {
                        case 1:
                                tt="<br>"+tr("ETA a partir de maintenant")+" ("+tm.toString("dd MMM-hh:mm")+"):<br>";
                                break;
                        case 2:
                                tt="<br>"+tr("ETA depuis la date Grib")+" ("+tm.toString("dd MMM-hh:mm")+"):<br>";
                                break;
                        case 3:
                                tt="<br>"+tr("ETA depuis la date fixe")+" ("+tm.toString("dd MMM-hh:mm")+"):<br>";
                                break;
                    }
                    tm.setTime_t(poi->getRouteTimeStamp());
                    tip=tip+tt+tm.toString("dd MMM-hh:mm")+"<br>";
                    tip=tip+QString::number((int)days)+" "+tr("jours")+" "+QString::number((int)hours)+" "+tr("heures")+" "+
                        QString::number((int)mins)+" "+tr("minutes");
#if 0
                    if(myBoat->get_boatType()==BOAT_REAL)
                        poi->setRouteTimeStamp(Eta+myBoat->getVacLen());
                    else
                        poi->setRouteTimeStamp(Eta);
#else
                    poi->setRouteTimeStamp(Eta);
#endif
                    if(poi==this->my_poiList.last())
                        eta=Eta;
                }
                this->line->setTip(tip);
            }
        }
    }

    if(!optimizing)
        line->slot_showMe();
    if(optimizingPOI)
    {
        if(startPoiName!="")
            hasStartEta=true;
    }
    setHidePois(this->hidePois);
    if(this->detectCoasts && !optimizingPOI && !this->imported && !this->frozen)
    {
        line->setCoastDetection(true);
        line->setMcp(parent);
    }
    else
        line->setCoastDetection(false);
    this->slot_shShow();
    line->slot_showMe();
    interpolatePos(); /*to cover the case when grib date has changed during calculations*/
    busy=false;
//    qWarning()<<"VBVMG-VLM calculation time:"<<timeD;
//    qWarning()<<"Route total calculation time:"<<timeTotal.elapsed();
    delay=timeTotal.elapsed();
}
void ROUTE::interpolatePos()
{
    line->setHasInterpolated(false);
    if(!dataManager || !dataManager->isOk())
        return;
    QList<vlmPoint> *list=line->getPoints();
    if (list->count()==0) return;
    time_t lastEta=list->at(0).eta;
    time_t gribDate=dataManager->get_currentDate();
    if(parent->getCompassFollow()==this)
        parent->centerCompass(list->at(0).lon,list->at(0).lat);
    if(gribDate<lastEta+1000) return;
    for (int n=1; n<list->count();++n)
    {
        if(lastEta<gribDate && list->at(n).eta>=gribDate)
        {
            double x1,y1,x2,y2;
            proj->map2screenDouble(list->at(n-1).lon,list->at(n-1).lat,&x1,&y1);
            proj->map2screenDouble(list->at(n).lon,list->at(n).lat,&x2,&y2);
            QLineF segment(x2,y2,x1,y1);
            double ratio=segment.length()/(list->at(n).eta-lastEta);
            //qWarning()<<"ratio="<<ratio<<(ratio*(list->at(n).eta-gribDate))/100.0;
            QPointF p=segment.pointAt((ratio*(list->at(n).eta-gribDate))/100.0);
            double lon,lat;
            proj->screen2mapDouble(p.x(),p.y(),&lon,&lat);
            line->setInterpolated(lon,lat);
            line->setHasInterpolated(true);
            if(parent->getCompassFollow()==this)
                parent->centerCompass(lon,lat);
            break;
        }
        lastEta=list->at(n).eta;
    }
}
double ROUTE::A180(double angle)
{
    if(qAbs(angle)>180)
    {
        if(angle<0)
            angle=360+angle;
        else
            angle=angle-360;
    }
    return angle;
}

bool ROUTE::isPartOfBvmg(POI * poi)
{
    if (poi->getNavMode()==1) return true;
    if (my_poiList.last()==poi) return false;
    if (my_poiList.at(my_poiList.indexOf(poi)+1)->getNavMode()==1) return true;
    return false;
}
void ROUTE::slot_edit()
{
    emit editMe(this);
}

void ROUTE::slot_shRou(bool isHidden) {
    bool toBeHidden=this->hidden || (Settings::getSetting("autoHideRoute",1).toInt()==1 && (this->myBoat==NULL || !this->myBoat->getIsSelected()));
    if(isHidden || toBeHidden) {
        if(line)
            line->hide();
        roadInfo->hide();
        if(toBeHidden)
            setHidePois(hidePois);
    }
    else {
        if(line)
            line->show();
        if(showInterpolData)
            roadInfo->show();
    }
}

void ROUTE::slot_boatPointerHasChanged(boat * acc)
{
    if(acc->get_boatType()==BOAT_VLM)
    {
        if(((boatVLM*)acc)->getBoatPseudo()==this->boatLogin)
            this->setBoat(acc);
    }
    else
    {
        if(myBoat->get_boatType()!=BOAT_VLM)
            this->setBoat(acc);
    }
}
void ROUTE::setHidePois(bool b)
{
    this->hidePois=b;
    QListIterator<POI*> i (my_poiList);
    bool toBeHidden=this->hidden || (Settings::getSetting("autoHideRoute",1).toInt()==1 && (this->myBoat==NULL || !this->myBoat->getIsSelected()));
    while(i.hasNext())
    {
        POI * poi=i.next();
        if ((poi==my_poiList.first() || poi==my_poiList.last()) && !toBeHidden)
            poi->setMyLabelHidden(false);
        else
        {
            if(!toBeHidden)
                poi->setMyLabelHidden(this->hidePois);
            else
                poi->setMyLabelHidden(true);
        }
    }
}
void ROUTE::do_vbvmg_context(double dist,double wanted_heading,
                             double w_speed,double w_angle,
                             double *heading1, double *heading2,
                             double *wangle1, double *wangle2,
                             double *time1, double *time2,
                             double *dist1, double *dist2) {
   double alpha, beta;
   double speed, speed_t1, speed_t2, l1, l2, d1, d2;
   double angle, t, t1, t2, t_min;
   //double wanted_heading;
   //double w_speed, w_angle;
   double tanalpha, d1hypotratio;
   double b_alpha, b_beta, b_t1, b_t2, b_l1, b_l2;
   //double b1_alpha, b1_beta;
   double speed_alpha, speed_beta;
   double vmg_alpha, vmg_beta;
   wanted_heading=degToRad(wanted_heading);
   w_angle=degToRad(w_angle);
   int i,j, min_i, min_j, max_i, max_j;

   b_t1 = b_t2 = b_l1 = b_l2 = b_alpha = b_beta = beta = 0.0;



   /* first compute the time for the "ortho" heading */
   speed=myBoat->getPolarData()->getSpeed(w_speed,A180(radToDeg(w_angle-wanted_heading)));
   //speed = find_speed(aboat, w_speed, w_angle - wanted_heading);
   if (speed > 0.0) {
     t_min = dist / speed;
   } else {
     t_min = 365.0*24.0; /* one year :) */
   }

 #if DEBUG
   printf("VBVMG: Wind %.2fkts %.2f\n", w_speed, radToDeg(w_angle));
   printf("VBVMG Direct road: heading %.2f time %.2f\n",
          radToDeg(wanted_heading), t_min);
   printf("VBVMG Direct road: wind angle %.2f\n",
          radToDeg(w_angle-wanted_heading));
 #endif /* DEBUG */

   angle = w_angle - wanted_heading;
   if (angle < -PI ) {
     angle += TWO_PI;
   } else if (angle > PI) {
     angle -= TWO_PI;
   }
   if (angle < 0.0) {
     min_i = 1;
     min_j = -89;
     max_i = 90;
     max_j = 0;
   } else {
     min_i = -89;
     min_j = 1;
     max_i = 0;
     max_j = 90;
   }

   for (i=min_i; i<max_i; i++) {
     alpha = degToRad((double)i);
     tanalpha = tan(alpha);
     d1hypotratio = hypot(1, tan(alpha));
     speed_t1=myBoat->getPolarData()->getSpeed(w_speed,A180(radToDeg(angle-alpha)));
     //speed_t1 = find_speed(aboat, w_speed, angle-alpha);
     if (speed_t1 <= 0.0) {
       continue;
     }
     for (j=min_j; j<max_j; j++) {
       beta = degToRad((double)j);
       d1 = dist * (tan(-beta) / (tanalpha + tan(-beta)));
       l1 =  d1 * d1hypotratio;
       t1 = l1 / speed_t1;
       if ((t1 < 0.0) || (t1 > t_min)) {
         continue;
       }
       d2 = dist - d1;
       speed_t2=myBoat->getPolarData()->getSpeed(w_speed,A180(radToDeg(angle-beta)));
       //speed_t2 = find_speed(aboat, w_speed, angle-beta);
       if (speed_t2 <= 0.0) {
         continue;
       }
       l2 =  d2 * hypot(1, tan(-beta));
       t2 = l2 / speed_t2;
       if (t2 < 0.0) {
         continue;
       }
       t = t1 + t2;
       if (t < t_min) {
         t_min = t;
         b_alpha = alpha;
         b_beta  = beta;
         b_l1 = l1;
         b_l2 = l2;
         b_t1 = t1;
         b_t2 = t2;
       }
     }
   }
 #if DEBUG
   printf("VBVMG: alpha=%.2f, beta=%.2f\n", radToDeg(b_alpha), radToDeg(b_beta));
 #endif /* DEBUG */

#if 0 /*set 1 to get 0.1 precision*/
     b1_alpha = b_alpha;
     b1_beta = b_beta;
     for (i=-9; i<=9; i++) {
       alpha = b1_alpha + degToRad(((double)i)/10.0);
       tanalpha = tan(alpha);
       d1hypotratio = hypot(1, tan(alpha));
       speed_t1=myBoat->getPolarData()->getSpeed(w_speed,A180(radToDeg(angle-alpha)));
       //speed_t1 = find_speed(aboat, w_speed, angle-alpha);
       if (speed_t1 <= 0.0) {
         continue;
       }
       for (j=-9; j<=9; j++) {
         beta = b1_beta + degToRad(((double)j)/10.0);
         d1 = dist * (tan(-beta) / (tanalpha + tan(-beta)));
         l1 =  d1 * d1hypotratio;
         t1 = l1 / speed_t1;
         if ((t1 < 0.0) || (t1 > t_min)) {
           continue;
         }
         d2 = dist - d1;
         speed_t2=myBoat->getPolarData()->getSpeed(w_speed,A180(radToDeg(angle-beta)));
         //speed_t2 = find_speed(aboat, w_speed, angle-beta);
         if (speed_t2 <= 0) {
           continue;
         }
         l2 =  d2 * hypot(1, tan(-beta));
         t2 = l2 / speed_t2;
         if (t2 < 0.0) {
           continue;
         }
         t = t1 + t2;
         if (t < t_min) {
           t_min = t;
           b_alpha = alpha;
           b_beta  = beta;
           b_l1 = l1;
           b_l2 = l2;
           b_t1 = t1;
           b_t2 = t2;
         }
       }
     }
 #if DEBUG
     printf("VBVMG: alpha=%.2f, beta=%.2f\n", radToDeg(b_alpha),
            radToDeg(b_beta));
 #endif /* DEBUG */
#endif
   speed_alpha=myBoat->getPolarData()->getSpeed(w_speed,A180(radToDeg(angle-b_alpha)));
   //speed_alpha = find_speed(aboat, w_speed, angle-b_alpha);
   vmg_alpha = speed_alpha * cos(b_alpha);
   speed_beta=myBoat->getPolarData()->getSpeed(w_speed,A180(radToDeg(angle-b_beta)));
   //speed_beta = find_speed(aboat, w_speed, angle-b_beta);
   vmg_beta = speed_beta * cos(b_beta);

 #if DEBUG
     printf("VBVMG: speedalpha=%.2f, speedbeta=%.2f\n", speed_alpha, speed_beta);
     printf("VBVMG: vmgalpha=%.2f, vmgbeta=%.2f\n", vmg_alpha, vmg_beta);
     printf("VBVMG: headingalpha %.2f, headingbeta=%.2f\n",
            radToDeg(fmod(wanted_heading + b_alpha, TWO_PI)),
            radToDeg(fmod(wanted_heading + b_beta, TWO_PI)));
 #endif /* DEBUG */

   if (vmg_alpha > vmg_beta) {
     *heading1 = fmod(wanted_heading + b_alpha, TWO_PI);
     *heading2 = fmod(wanted_heading + b_beta, TWO_PI);
     *time1 = b_t1;
     *time2 = b_t2;
     *dist1 = b_l1;
     *dist2 = b_l2;
   } else {
     *heading2 = fmod(wanted_heading + b_alpha, TWO_PI);
     *heading1 = fmod(wanted_heading + b_beta, TWO_PI);
     *time2 = b_t1;
     *time1 = b_t2;
     *dist2 = b_l1;
     *dist1 = b_l2;
   }
   if (*heading1 < 0 ) {
     *heading1 += TWO_PI;
   }
   if (*heading2 < 0 ) {
     *heading2 += TWO_PI;
   }

   *wangle1 = fmod(*heading1 - w_angle, TWO_PI);
   if (*wangle1 > PI ) {
     *wangle1 -= TWO_PI;
   } else if (*wangle1 < -PI ) {
     *wangle1 += TWO_PI;
   }
   *wangle2 = fmod(*heading2 - w_angle, TWO_PI);
   if (*wangle2 > PI ) {
     *wangle2 -= TWO_PI;
   } else if (*wangle2 < -PI ) {
     *wangle2 += TWO_PI;
   }
 #if 0
   QString s;
   qWarning()<<s.sprintf("VBVMG: wangle1=%.2f, wangle2=%.2f\n", radToDeg(*wangle1),
          radToDeg(*wangle2));
   qWarning()<<s.sprintf("VBVMG: heading1 %.2f, heading2=%.2f\n", radToDeg(*heading1),
          radToDeg(*heading2));
   qWarning()<<s.sprintf("VBVMG: dist=%.2f, l1=%.2f, l2=%.2f, ratio=%.2f\n", dist, *dist1,
          *dist2, (b_l1+b_l2)/dist);
   qWarning()<<s.sprintf("VBVMG: t1 = %.2f, t2=%.2f, total=%.2f\n", *time1, *time2, t_min);
   qWarning()<<s.sprintf("VBVMG: heading %.2f\n", radToDeg(*heading1));
   qWarning()<<s.sprintf("VBVMG: wind angle %.2f\n", radToDeg(*wangle1));
 #endif /* DEBUG */
   *heading1=radToDeg(*heading1);
   *heading2=radToDeg(*heading2);
   *wangle1=radToDeg(*wangle1);
   *wangle2=radToDeg(*wangle2);
 }
void ROUTE::do_vbvmg_buffer(double dist,double wanted_heading,
                             double w_speed,double w_angle,
                             double *heading1, double *heading2,
                             double *wangle1, double *wangle2,
                             double *time1, double *time2,
                             double *dist1, double *dist2)
{
    double alpha, beta;
    double speed, speed_t1, speed_t2, l1, l2, d1, d2;
    double angle, t, t1, t2, t_min;
    double tanalpha, d1hypotratio;
    double b_alpha, b_beta, b_t1, b_t2, b_l1, b_l2;
    double speed_alpha, speed_beta;
    double vmg_alpha, vmg_beta;
    wanted_heading=degToRad(wanted_heading);
    w_angle=degToRad(w_angle);
    int i,j, min_i, min_j, max_i, max_j;

    b_t1 = b_t2 = b_l1 = b_l2 = b_alpha = b_beta = beta = 0.0;
    if(fastSpeed==NULL)
    {
        fastSpeed = new QCache<int,double>;
        fastSpeed->setMaxCost(180);
    }

    fastSpeed->clear();
    if(hypotPos==NULL)
        this->precalculateTan();

    /* first compute the time for the "ortho" heading */
    speed=myBoat->getPolarData()->getSpeed(w_speed,A180(radToDeg(w_angle-wanted_heading)));
    //speed = find_speed(aboat, w_speed, w_angle - wanted_heading);
    if (speed > 0.0)
    {
        t_min = dist / speed;
    }
    else
    {
        t_min = 365.0*24.0; /* one year :) */
    }


    angle = w_angle - wanted_heading;
    if (angle < -PI )
    {
        angle += TWO_PI;
    }
    else if (angle > PI)
    {
        angle -= TWO_PI;
    }
    double guessAngle=A180(radToDeg(angle));
    if (angle < 0.0)
    {
        min_i = 1;
        min_j = -89;
        max_i = 90;
        max_j = 0;
    }
    else
    {
        min_i = -89;
        min_j = 1;
        max_i = 0;
        max_j = 90;
    }
    for (i=min_i; i<max_i; ++i)
    {
        alpha = degToRad((double)i);
        double guessTwa=A180(radToDeg(angle-alpha));
        //if(newVbvmgVlm && (qAbs(guessTwa)<20 || qAbs(guessTwa>175))) continue;
        if(i>0)
        {
            tanalpha = tanPos->at(i);
            d1hypotratio = hypotPos->at(i);
        }
        else
        {
            tanalpha = tanNeg->at(-i);
            d1hypotratio = hypotNeg->at(-i);
        }
        speed_t1=myBoat->getPolarData()->getSpeed(w_speed,A180(radToDeg(angle-alpha)));
        //speed_t1 = find_speed(aboat, w_speed, angle-alpha);
        if (speed_t1 <= 0.0)
        {
            continue;
        }
        int MinJ,MaxJ;
        if(!this->newVbvmgVlm)
        {
            MinJ=min_j;
            MaxJ=max_j;
        }
        else
        {
            int guessInt=qRound(guessAngle+guessTwa);
            MinJ=qMax(guessInt-15,min_j);
            MaxJ=qMin(guessInt+15,max_j);
        }
        for (j=MinJ; j<MaxJ; ++j)
        {
            beta = degToRad((double)j);
            if(-j>0)
            {
//                d1 = dist * (tan(-beta) / (tanalpha + tan(-beta)));
                d1 = dist * tanPos->at(-j) / (tanalpha + tanPos->at(-j));
            }
            else
                d1 = dist * tanNeg->at(j) / (tanalpha + tanNeg->at(j));
            l1 =  d1 * d1hypotratio;
            t1 = l1 / speed_t1;
            if ((t1 < 0.0) || (t1 > t_min))
            {
                continue;
            }
            d2 = dist - d1;
#if 1
            if(fastSpeed->contains(j))
                speed_t2=*fastSpeed->object(j);
            else
            {
                speed_t2=myBoat->getPolarData()->getSpeed(w_speed,A180(radToDeg(angle-beta)));
                fastSpeed->insert(j,new double(speed_t2));
            }
#else
            speed_t2=myBoat->getPolarData()->getSpeed(w_speed,A180(radToDeg(angle-beta)));
#endif
            //speed_t2 = find_speed(aboat, w_speed, angle-beta);
            if (speed_t2 <= 0.0)
            {
                continue;
            }
            if(-j>0)
            {
                //l2 =  d2 * hypot(1, tan(-beta));
                l2 = d2 * hypotPos->at(-j);
            }
            else
                l2 = d2 * hypotNeg->at(j);
            t2 = l2 / speed_t2;
            if (t2 < 0.0)
            {
                continue;
            }
            t = t1 + t2;
            if (t < t_min)
            {
                t_min = t;
                b_alpha = alpha;
                b_beta  = beta;
                b_l1 = l1;
                b_l2 = l2;
                b_t1 = t1;
                b_t2 = t2;
            }
        }
        if(newVbvmgVlm)
        {
            for (j=qMax(-i-15,min_j); j<qMin(-i+15,max_j); ++j)
            {
                beta = degToRad((double)j);
    //            if((angle-alpha>0 && angle-beta>0) ||
    //               (angle-alpha<0 && angle-beta<0)) continue;
                if(-j>0)
                {
    //                d1 = dist * (tan(-beta) / (tanalpha + tan(-beta)));
                    d1 = dist * tanPos->at(-j) / (tanalpha + tanPos->at(-j));
                }
                else
                    d1 = dist * tanNeg->at(j) / (tanalpha + tanNeg->at(j));
                l1 =  d1 * d1hypotratio;
                t1 = l1 / speed_t1;
                if ((t1 < 0.0) || (t1 > t_min))
                {
                    continue;
                }
                d2 = dist - d1;
#if 1
            if(fastSpeed->contains(j))
                speed_t2=*fastSpeed->object(j);
            else
            {
                speed_t2=myBoat->getPolarData()->getSpeed(w_speed,A180(radToDeg(angle-beta)));
                fastSpeed->insert(j,new double(speed_t2));
            }
#else
            speed_t2=myBoat->getPolarData()->getSpeed(w_speed,A180(radToDeg(angle-beta)));
#endif
                //speed_t2 = find_speed(aboat, w_speed, angle-beta);
                if (speed_t2 <= 0.0)
                {
                    continue;
                }
                if(-j>0)
                {
                    //l2 =  d2 * hypot(1, tan(-beta));
                    l2 = d2 * hypotPos->at(-j);
                }
                else
                    l2 = d2 * hypotNeg->at(j);
                t2 = l2 / speed_t2;
                if (t2 < 0.0)
                {
                    continue;
                }
                t = t1 + t2;
                if (t < t_min)
                {
                    t_min = t;
                    b_alpha = alpha;
                    b_beta  = beta;
                    b_l1 = l1;
                    b_l2 = l2;
                    b_t1 = t1;
                    b_t2 = t2;
                }
            }
        }
    }
    speed_alpha=myBoat->getPolarData()->getSpeed(w_speed,A180(radToDeg(angle-b_alpha)));
    //speed_alpha = find_speed(aboat, w_speed, angle-b_alpha);
    vmg_alpha = speed_alpha * cos(b_alpha);
    speed_beta=myBoat->getPolarData()->getSpeed(w_speed,A180(radToDeg(angle-b_beta)));
    //speed_beta = find_speed(aboat, w_speed, angle-b_beta);
    vmg_beta = speed_beta * cos(b_beta);

    if (vmg_alpha > vmg_beta)
    {
        *heading1 = fmod(wanted_heading + b_alpha, TWO_PI);
        *heading2 = fmod(wanted_heading + b_beta, TWO_PI);
        *time1 = b_t1;
        *time2 = b_t2;
        *dist1 = b_l1;
        *dist2 = b_l2;
    }
    else
    {
        *heading2 = fmod(wanted_heading + b_alpha, TWO_PI);
        *heading1 = fmod(wanted_heading + b_beta, TWO_PI);
        *time2 = b_t1;
        *time1 = b_t2;
        *dist2 = b_l1;
        *dist1 = b_l2;
    }
    if (*heading1 < 0 )
    {
        *heading1 += TWO_PI;
    }
    if (*heading2 < 0 )
    {
        *heading2 += TWO_PI;
    }

    *wangle1 = fmod(*heading1 - w_angle, TWO_PI);
    if (*wangle1 > PI )
    {
        *wangle1 -= TWO_PI;
    }
    else if (*wangle1 < -PI )
    {
        *wangle1 += TWO_PI;
    }
    *wangle2 = fmod(*heading2 - w_angle, TWO_PI);
    if (*wangle2 > PI )
    {
        *wangle2 -= TWO_PI;
    } else if (*wangle2 < -PI )
    {
        *wangle2 += TWO_PI;
    }
    fastSpeed->clear();
    delete fastSpeed;
    fastSpeed=NULL;
    *heading1=radToDeg(*heading1);
    *heading2=radToDeg(*heading2);
    *wangle1=radToDeg(*wangle1);
    *wangle2=radToDeg(*wangle2);
}
void ROUTE::precalculateTan()
{
    tanPos=new QList<double>;
    tanNeg=new QList<double>;
    hypotPos=new QList<double>;
    hypotNeg=new QList<double>;
    tanPos->append(0);
    tanNeg->append(0);
    hypotPos->append(0);
    hypotNeg->append(0);
    for (double i=1;i<90;++i)
    {
        double tanG=tan(degToRad(i));
        tanPos->append(tanG);
        hypotPos->append(hypot(1,tanG));
        tanG=tan(degToRad(-i));
        tanNeg->append(tanG);
        hypotNeg->append(hypot(1,tanG));
    }
}
void ROUTE::shiftEtas(QDateTime newStart)
{
    int timeDiff=newStart.toTime_t()-this->startTime.toTime_t();
    this->startTime=newStart;
    foreach(POI * poi,this->my_poiList)
        poi->setRouteTimeStamp(poi->getRouteTimeStamp()+timeDiff);
    this->initialized=false;
    this->setFrozen2(false);
    this->setFrozen2(true);
}
void ROUTE::setAutoAt(bool b)
{
    if(!b && this->autoAt)
    {
        foreach(POI * poi, this->my_poiList)
        {
            poi->setWph(-1);
//            if(poi->getIsWp())
//                poi->slot_setWP();
        }
    }
    autoAt=b;
}
routeStats ROUTE::getStats()
{
    routeStats stats;
    stats.maxBS=-10e6;
    stats.minBS=10e6;
    stats.maxTWS=-10e6;
    stats.minTWS=10e6;
    stats.averageTWS=0;
    stats.averageBS=0;
    stats.totalTime=0;
    stats.totalDistance=0;
    stats.beatingTime=0;
    stats.reachingTime=0;
    stats.largueTime=0;
    stats.nbTacksGybes=0;
    stats.engineTime=0;
    stats.nightTime=0;
    stats.rainTime=0;
    stats.maxWaveHeight=0;
    if(!dataManager) return stats;
    if(this->my_poiList.isEmpty()) return stats;
    QList<vlmPoint> * points=this->getLine()->getPoints();
    if(points->size()<=1) return stats;
    double bs=0;
    double tws=0,twd=0,twa=0,hdg=0;
    double prevTwa=0;
    Orthodromie oo(0,0,0,0);
    for(int n=1;n<points->size();++n)
    {
        double lon=points->at(n).lon;
        double lat=points->at(n).lat;
        time_t date=points->at(n).eta;
        time_t prevDate=points->at(n-1).eta;
        if(!dataManager->getInterpolatedWind(lon, lat,prevDate,&tws,&twd,INTERPOLATION_DEFAULT))
                return stats;
        twd=radToDeg(twd);
        stats.totalTime+=date-prevDate;
        oo.setPoints(points->at(n-1).lon,points->at(n-1).lat,lon,lat);
        stats.totalDistance+=oo.getLoxoDistance();
        stats.averageTWS+=tws;
        stats.maxTWS=qMax(stats.maxTWS,tws);
        stats.minTWS=qMin(stats.minTWS,tws);
        hdg=oo.getLoxoCap();
        twa=Util::A360(hdg-twd);
        if(twa>180) twa-=360;
        if(qAbs(twa)<70.0)
            stats.beatingTime+=date-prevDate;
        else if (qAbs(twa)<130)
            stats.largueTime+=date-prevDate;
        else
            stats.reachingTime+=date-prevDate;
        if(n!=1 && prevTwa*twa<0)
            ++stats.nbTacksGybes;
        prevTwa=twa;
        bool engineUsed=false;
        bs=myBoat->getPolarData()->getSpeed(tws,twa,true,&engineUsed);
        stats.averageBS+=bs;
        stats.maxBS=qMax(stats.maxBS,bs);
        stats.minBS=qMin(stats.minBS,bs);
        if(engineUsed)
            stats.engineTime+=date-prevDate;
        if(!parent->getTerre()->daylight(NULL,points->at(n)))
            stats.nightTime+=date-prevDate;
        stats.rainTime+=dataManager->getInterpolatedValue_1D(DATA_PRECIP_TOT,DATA_LV_GND_SURF,0,lon, lat, prevDate)>0.0?date-prevDate:0;
        stats.maxWaveHeight=qMax(stats.maxWaveHeight,dataManager->getInterpolatedValue_1D(DATA_WAVES_MAX_HGT,DATA_LV_GND_SURF,0,lon,lat, prevDate));
        stats.combWaveHeight=qMax(stats.combWaveHeight,dataManager->getInterpolatedValue_1D(DATA_WAVES_SIG_HGT_COMB,DATA_LV_GND_SURF,0,lon,lat, prevDate));
    }
    stats.averageBS=stats.averageBS/(points->size()-1);
    stats.averageTWS=stats.averageTWS/(points->size()-1);
    return stats;
}

/*****************************************************
 * read POI from file
 *****************************************************/

#define ROOT_NAME             "qtVLM_Route"

/* ROUTE */
#define ROUTE_GROUP_NAME  "ROUTE"
#define ROUTE_NAME        "name"
#define ROUTE_BOAT        "boat"
#define ROUTE_START       "startFromBoat"
#define ROUTE_DATEOPTION  "startTimeOption"
#define ROUTE_DATE        "startTime"
#define ROUTE_WIDTH       "width"
#define ROUTE_COLOR_R     "color_red"
#define ROUTE_COLOR_G     "color_green"
#define ROUTE_COLOR_B     "color_blue"
#define ROUTE_COASTS      "detectCoasts"
#define ROUTE_FROZEN      "frozen"
#define ROUTE_HIDEPOIS    "hidePois"
#define ROUTE_MULTVAC     "vacStep"
#define ROUTE_HIDDEN      "hidden"
#define ROUTE_VBVMG_VLM   "vbvmg-vlm"
#define ROUTE_NEW_VBVMG   "newVbvmg-vlm"
#define ROUTE_SPEED_TACK  "speedTack"
#define ROUTE_REMOVE      "autoRemovePoi"
#define ROUTE_AUTO_AT      "autoAt"
#define ROUTE_SORT_BY_NAME "routeSortByName"
#define ROUTE_ROADMAPINT   "roadMapInterval"
#define ROUTE_ROADMAPHDG   "roadMapHDG"
#define ROUTE_ROADUSEINT   "roadUseInt"


void ROUTE::read_routeData(myCentralWidget * centralWidget) {
    QDomNode node;
    QDomNode subNode;
    QDomNode dataNode;
    bool hasOldSystem=false;

    QString fname = appFolder.value("userFiles")+"poi.dat";

    QDomNode * root=XmlFile::get_dataNodeFromDisk(fname,ROOT_NAME);
    if(!root) {
        qWarning() << "Error reading Route from " << fname <<", try to use alt root name";
        QDomElement * root2=XmlFile::get_fisrtDataNodeFromDisk(fname);

        if(!root2) {
            qWarning() << "Error reading POI from " << fname;
            return ;
        }
        hasOldSystem=true;
        node=root2->firstChild();
    }
    else
        node = root->firstChild();

    while(!node.isNull()) {
        if(node.toElement().tagName() == ROUTE_GROUP_NAME)
        {
            ROUTE * route = centralWidget->addRoute();
            route->setFrozen(true);
            route->setBoat(NULL);
            QColor routeColor=Qt::red;
            bool toBeFreezed=false;
            subNode = node.firstChild();
            bool invalidRoute=true;
            while(!subNode.isNull())
            {
                if(subNode.toElement().tagName() == ROUTE_NAME)
                {
                    dataNode = subNode.firstChild();
                    if(dataNode.nodeType() == QDomNode::TextNode)
                        route->setName(QString(QByteArray::fromBase64(dataNode.toText().data().toUtf8())));
                }
                if(subNode.toElement().tagName() == ROUTE_BOAT)
                {
                    dataNode = subNode.firstChild();
                    if(dataNode.nodeType() == QDomNode::TextNode)
                    {
                        if(centralWidget->getBoats())
                        {
                            QListIterator<boatVLM*> b (*centralWidget->getBoats());

                            while(b.hasNext())
                            {
                                boatVLM * boat = b.next();
                                if(boat->getStatus())
                                {
                                    if(boat->getId()==dataNode.toText().data().toInt())
                                    {
                                        route->setBoat(boat);
                                        invalidRoute=false;
                                        break;
                                    }
                                }
                            }
                        }
                        if(invalidRoute)
                        {
                            if(centralWidget->getSelectedBoat()!=NULL && centralWidget->getSelectedBoat()->get_boatType()==BOAT_REAL)
                            {
                                route->setBoat(centralWidget->getSelectedBoat());
                                invalidRoute=false;
                            }
                        }
                    }
                }
                if(subNode.toElement().tagName() == ROUTE_START)
                {
                    dataNode = subNode.firstChild();
                    if(dataNode.nodeType() == QDomNode::TextNode)
                        route->setStartFromBoat(dataNode.toText().data().toInt()==1);
                }
                if(subNode.toElement().tagName() == ROUTE_SPEED_TACK)
                {
                    dataNode = subNode.firstChild();
                    if(dataNode.nodeType() == QDomNode::TextNode)
                        route->setSpeedLossOnTack((double)dataNode.toText().data().toInt()/100.00);
                }
                if(subNode.toElement().tagName() == ROUTE_DATEOPTION)
                {
                    dataNode = subNode.firstChild();
                    if(dataNode.nodeType() == QDomNode::TextNode)
                        route->setStartTimeOption(dataNode.toText().data().toInt());
                }
                if(subNode.toElement().tagName() == ROUTE_MULTVAC)
                {
                    dataNode = subNode.firstChild();
                    if(dataNode.nodeType() == QDomNode::TextNode)
                        route->setMultVac(dataNode.toText().data().toInt());
                }
                if(subNode.toElement().tagName() == ROUTE_DATE)
                {
                    dataNode = subNode.firstChild();
                    if(dataNode.nodeType() == QDomNode::TextNode)
                    {
                        QDateTime date;
                        date.setTime_t(dataNode.toText().data().toInt());
                        route->setStartTime(date);
                    }
                }
                if(subNode.toElement().tagName() == ROUTE_WIDTH)
                {
                    dataNode = subNode.firstChild();
                    if(dataNode.nodeType() == QDomNode::TextNode)
                        route->setWidth(dataNode.toText().data().toFloat());
                }
                if(subNode.toElement().tagName() == ROUTE_ROADMAPINT)
                {
                    dataNode = subNode.firstChild();
                    if(dataNode.nodeType() == QDomNode::TextNode)
                        route->setRoadMapInterval(dataNode.toText().data().toFloat());
                }
                if(subNode.toElement().tagName() == ROUTE_ROADMAPHDG)
                {
                    dataNode = subNode.firstChild();
                    if(dataNode.nodeType() == QDomNode::TextNode)
                        route->setRoadMapHDG(dataNode.toText().data().toFloat());
                }
                if(subNode.toElement().tagName() == ROUTE_ROADUSEINT)
                {
                    dataNode = subNode.firstChild();
                    if(dataNode.nodeType() == QDomNode::TextNode)
                        route->setUseInterval(dataNode.toText().data().toInt()==1);
                }
                if(subNode.toElement().tagName() == ROUTE_COLOR_R)
                {
                    dataNode = subNode.firstChild();
                    if(dataNode.nodeType() == QDomNode::TextNode)
                    {
                        routeColor.setRed(dataNode.toText().data().toInt());
                        route->setColor(routeColor);
                    }
                }
                if(subNode.toElement().tagName() == ROUTE_COLOR_G)
                {
                    dataNode = subNode.firstChild();
                    if(dataNode.nodeType() == QDomNode::TextNode)
                    {
                        routeColor.setGreen(dataNode.toText().data().toInt());
                        route->setColor(routeColor);
                    }
                }
                if(subNode.toElement().tagName() == ROUTE_COLOR_B)
                {
                    dataNode = subNode.firstChild();
                    if(dataNode.nodeType() == QDomNode::TextNode)
                    {
                        routeColor.setBlue(dataNode.toText().data().toInt());
                        route->setColor(routeColor);
                    }
                }

                if(subNode.toElement().tagName() == ROUTE_COASTS)
                {
                    dataNode = subNode.firstChild();
                    if(dataNode.nodeType() == QDomNode::TextNode)
                        route->setDetectCoasts(dataNode.toText().data().toInt()==1);
                }
                if(subNode.toElement().tagName() == ROUTE_FROZEN)
                {
                    dataNode = subNode.firstChild();
                    if(dataNode.nodeType() == QDomNode::TextNode)
                        toBeFreezed=(dataNode.toText().data().toInt()==1);
                }


                if(subNode.toElement().tagName() == ROUTE_HIDEPOIS)
                {
                    dataNode = subNode.firstChild();
                    if(dataNode.nodeType() == QDomNode::TextNode)
                        route->setHidePois(dataNode.toText().data().toInt()==1);
                }

                if(subNode.toElement().tagName() == ROUTE_HIDDEN)
                {
                    dataNode = subNode.firstChild();
                    if(dataNode.nodeType() == QDomNode::TextNode)
                        route->setHidden(dataNode.toText().data().toInt()==1);
                }
                if(subNode.toElement().tagName() == ROUTE_VBVMG_VLM)
                {
                    dataNode = subNode.firstChild();
                    if(dataNode.nodeType() == QDomNode::TextNode)
                        route->setUseVbVmgVlm(dataNode.toText().data().toInt()==1);
                }

                if(subNode.toElement().tagName() == ROUTE_NEW_VBVMG)
                {
                    dataNode = subNode.firstChild();
                    if(dataNode.nodeType() == QDomNode::TextNode)
                        route->setNewVbvmgVlm(dataNode.toText().data().toInt()==1);
                }

                if(subNode.toElement().tagName() == ROUTE_REMOVE)
                {
                    dataNode = subNode.firstChild();
                    if(dataNode.nodeType() == QDomNode::TextNode)
                        route->setAutoRemove(dataNode.toText().data().toInt()==1);
                }

                if(subNode.toElement().tagName() == ROUTE_AUTO_AT)
                {
                    dataNode = subNode.firstChild();
                    if(dataNode.nodeType() == QDomNode::TextNode)
                        route->setAutoAt(dataNode.toText().data().toInt()==1);
                }

                if(subNode.toElement().tagName() == ROUTE_SORT_BY_NAME)
                {
                    dataNode = subNode.firstChild();
                    if(dataNode.nodeType() == QDomNode::TextNode)
                        route->setSortPoisByName(dataNode.toText().data().toInt()==1);
                }

                subNode = subNode.nextSibling();
            }
            if(invalidRoute) /*route->boat does not exist anymore*/
            {
                route->setBoat(NULL);
            }
            if(!toBeFreezed)
                route->setFrozen(false);
        }
        node=node.nextSibling();
    }
    if(hasOldSystem)
       cleanFile(fname);
}

void ROUTE::cleanFile(QString fname) {
    QDomElement * root=XmlFile::get_fisrtDataNodeFromDisk(fname);
    QDomNode node;

    if(!root) {
        qWarning() << "Error reading route from " << fname << " during clean";
        return ;
    }

    node=root->firstChild();
    while(!node.isNull()) {
        //qWarning() << "Cleaning: " << node.toElement().tagName();
        QDomNode nxtNode=node.nextSibling();
        if(node.toElement().tagName() == ROUTE_GROUP_NAME)
            root->removeChild(node);
        if(node.toElement().tagName() == "Version")
            root->removeChild(node);
        node = nxtNode;
    }
    XmlFile::saveRoot(root,fname);
}

void ROUTE::write_routeData(QList<ROUTE*>& route_list,myCentralWidget * /*centralWidget*/) {
    QString fname = appFolder.value("userFiles")+"poi.dat";

    QDomDocument doc(DOM_FILE_TYPE);
    QDomElement root = doc.createElement(ROOT_NAME);
    doc.appendChild(root);

    QDomElement group;
    QDomElement tag;
    QDomText t;

    QListIterator<ROUTE*> h (route_list);
    while(h.hasNext())
    {
         ROUTE * route=h.next();
         if(route->isImported()) continue;
         if(!route->getBoat() || !route->getBoat()->getStatus()) continue; //if boat has been deactivated do not save route
         group = doc.createElement(ROUTE_GROUP_NAME);
         root.appendChild(group);

         tag = doc.createElement(ROUTE_NAME);
         group.appendChild(tag);
         t = doc.createTextNode(route->getName().toUtf8().toBase64());
         tag.appendChild(t);

         if(route->getBoat()!=NULL)
         {
             tag = doc.createElement(ROUTE_BOAT);
             group.appendChild(tag);
             t = doc.createTextNode(QString().setNum(route->getBoat()->getId()));
             tag.appendChild(t);
         }

         tag = doc.createElement(ROUTE_START);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(route->getStartFromBoat()?1:0));
         tag.appendChild(t);

         tag = doc.createElement(ROUTE_DATEOPTION);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(route->getStartTimeOption()));
         tag.appendChild(t);

         tag = doc.createElement(ROUTE_MULTVAC);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(route->getMultVac()));
         tag.appendChild(t);

         tag = doc.createElement(ROUTE_SPEED_TACK);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(qRound(route->getSpeedLossOnTack()*100)));
         tag.appendChild(t);

         tag = doc.createElement(ROUTE_DATE);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(route->getStartTime().toUTC().toTime_t()));
         tag.appendChild(t);

         tag = doc.createElement(ROUTE_WIDTH);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(route->getWidth()));
         tag.appendChild(t);

         tag = doc.createElement(ROUTE_ROADMAPINT);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(route->getRoadMapInterval()));
         tag.appendChild(t);

         tag = doc.createElement(ROUTE_ROADMAPHDG);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(route->getRoadMapHDG()));
         tag.appendChild(t);

         tag = doc.createElement(ROUTE_ROADUSEINT);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(route->getUseInterval()?1:0));
         tag.appendChild(t);

         tag = doc.createElement(ROUTE_COLOR_R);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(route->getColor().red()));
         tag.appendChild(t);
         tag = doc.createElement(ROUTE_COLOR_G);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(route->getColor().green()));
         tag.appendChild(t);
         tag = doc.createElement(ROUTE_COLOR_B);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(route->getColor().blue()));
         tag.appendChild(t);

         tag = doc.createElement(ROUTE_COASTS);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(route->getDetectCoasts()?1:0));
         tag.appendChild(t);


         tag = doc.createElement(ROUTE_FROZEN);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(route->getFrozen()?1:0));
         tag.appendChild(t);

         tag = doc.createElement(ROUTE_HIDEPOIS);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(route->getHidePois()?1:0));
         tag.appendChild(t);

         tag = doc.createElement(ROUTE_HIDDEN);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(route->getHidden()?1:0));
         tag.appendChild(t);

         tag = doc.createElement(ROUTE_VBVMG_VLM);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(route->getUseVbvmgVlm()?1:0));
         tag.appendChild(t);

         tag = doc.createElement(ROUTE_NEW_VBVMG);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(route->getNewVbvmgVlm()?1:0));
         tag.appendChild(t);

         tag = doc.createElement(ROUTE_REMOVE);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(route->getAutoRemove()?1:0));
         tag.appendChild(t);

         tag = doc.createElement(ROUTE_AUTO_AT);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(route->getAutoAt()?1:0));
         tag.appendChild(t);

         tag = doc.createElement(ROUTE_SORT_BY_NAME);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(route->getSortPoisByName()?1:0));
         tag.appendChild(t);
    }

    if(!XmlFile::set_dataNodeOnDisk(fname,ROOT_NAME,&root,DOM_FILE_TYPE)) {
        /* error in file  => blanking filename in settings */
        qWarning() << "Error saving Route in " << fname;
        return ;
    }
}
