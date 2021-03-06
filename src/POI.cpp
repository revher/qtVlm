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

#include <QTimer>
#include <QMessageBox>
#include <QDebug>
#include <QDomDocument>
#include <QFileDialog>

#include "POI.h"
#include "Util.h"
#include "MainWindow.h"
#include "settings.h"
#include "Orthodromie.h"
#include "mycentralwidget.h"
#include "route.h"
#include "boatVLM.h"
#include "Projection.h"
#include "DialogFinePosit.h"
#include "dialogpoiconnect.h"
#include "XmlFile.h"

/**************************/
/* Init & Clean           */
/**************************/

POI::POI(QString name, int type, double lat, double lon,
                 Projection *proj, MainWindow *main, myCentralWidget *parentWindow,
                 double wph,int tstamp,bool useTstamp,boat *myBoat)
    : QGraphicsWidget()
{
    this->parent = parentWindow;
    this->owner = main;
    this->proj = proj;
    this->name = name;
    setLatitude(lat);
    setLongitude(lon);
    this->wph=wph;
    this->timeStamp=tstamp;
    this->useTstamp=useTstamp;
    this->type=type;
    this->typeMask=1<<type;
    this->myBoat=myBoat;
    this->searchRangeLon=1;
    this->searchRangeLat=1;
    this->searchStep=0.01;
    this->abortSearch=false;
    this->navMode=0;
    this->optimizing=true;
    this->wasWP=false;
    this->partOfTwa=false;
    this->notSimplificable=false;
    this->connectedPoi=NULL;
    this->lineBetweenPois=NULL;
    this->lineColor=Qt::blue;
    this->lineWidth=2;
    this->colorPilototo=0;
    this->piloteSelected=false;
    this->piloteDate=-1;
    this->piloteWph=-1;
    this->sequence=0;
    this->autoRange = true;
    useRouteTstamp=false;
    routeTimeStamp=-1;
    route=NULL;
    routeName="";
    this->labelHidden=parentWindow->get_shLab_st();
    this->myLabelHidden=false;
    this->labelTransp=true;
//    qWarning() << "Init POI label: " << loxoCap<<" name: "<<name;

    WPlon=WPlat=-1;
    isWp=false;
    isMoving=false;
    VLMBoardIsBusy=false;

    setZValue(Z_VALUE_POI);
    setFont(QFont(QApplication::font()));
    setCursor(Qt::PointingHandCursor);
    setData(0,POI_WTYPE);

    fgcolor = QColor(0,0,0);
    int gr = 255;
    bgcolor = QColor(gr,gr,gr,150);
    width=20;
    height=10;

    createPopUpMenu();
    slot_paramChanged();
    timerSimp=new QTimer(this);
    timerSimp->setInterval(200);
    timerSimp->setSingleShot(true);
    connect(timerSimp,SIGNAL(timeout()),this,SLOT(slot_timerSimp()));

    connect(this,SIGNAL(addPOI_list(POI*)),parent,SLOT(slot_addPOI_list(POI*)));
    connect(this,SIGNAL(delPOI_list(POI*)),parent,SLOT(slot_delPOI_list(POI*)));

    connect(this,SIGNAL(chgWP(double,double,double)),main,SLOT(slotChgWP(double,double,double)));

    connect(this,SIGNAL(selectPOI(POI*)),main,SLOT(slot_POIselected(POI*)));

    connect(this,SIGNAL(setGribDate(time_t)),main,SLOT(slotSetGribDate(time_t)));

    connect(main,SIGNAL(paramVLMChanged()),this,SLOT(slot_paramChanged()));
    connect(main,SIGNAL(WPChanged(double,double)),this,SLOT(slot_WPChanged(double,double)));
    connect(main,SIGNAL(boatHasUpdated(boat*)),this,SLOT(slot_updateTip(boat*)));
    connect(parent,SIGNAL(stopCompassLine()),this,SLOT(slot_abort()));
    if (main->getSelectedBoat() && main->getSelectedBoat()!=NULL && !parent->getPlayer()->getWrong() && main->getSelectedBoat()->get_boatType()==BOAT_VLM)
        connect(this,SIGNAL(wpChanged()),main,SIGNAL(wpChanged()));

    ((MainWindow*)main)->getBoatWP(&WPlat,&WPlon);
    setName(name);
    slot_updateProjection();
    if(!parentWindow->get_shPoi_st())
        show();
    else
        hide();
    this->setAcceptTouchEvents(true);
}

POI::~POI()
{
    if(route!=NULL) this->setRoute(NULL);
    if(lineBetweenPois!=NULL && !parent->getAboutToQuit())
    {
        this->connectedPoi->setLineBetweenPois(NULL);
        this->connectedPoi->setConnectedPoi(NULL);
        delete lineBetweenPois;
        lineBetweenPois=NULL;
    }
    if(isWp)
    {
        isWp=false;
        emit wpChanged();
    }
    if(popup && !parent->getAboutToQuit())
        popup->deleteLater();
    delete timerSimp;
}
void POI::setLongitude(double lon)
{
    this->lon=(double)(qRound(lon*1000000.0))/1000000.0;

    if(this->lon>180) this->lon=this->lon-360;
    if(this->lon<-180) this->lon=this->lon+360;
}
void POI::setLatitude(double lat)
{
    this->lat=(double)(qRound(lat*1000000.0))/1000000.0;
}
void POI::rmSignal(void)
{
    disconnect(this,SIGNAL(addPOI_list(POI*)),parent,SLOT(slot_addPOI_list(POI*)));
    disconnect(this,SIGNAL(delPOI_list(POI*)),parent,SLOT(slot_delPOI_list(POI*)));
    disconnect(this,SIGNAL(chgWP(double,double,double)),owner,SLOT(slotChgWP(double,double,double)));

    disconnect(this,SIGNAL(setGribDate(time_t)),owner,SLOT(slotSetGribDate(time_t)));

    disconnect(owner,SIGNAL(paramVLMChanged()),this,SLOT(slot_paramChanged()));
    disconnect(owner,SIGNAL(WPChanged(double,double)),this,SLOT(slot_WPChanged(double,double)));
    disconnect(owner,SIGNAL(boatHasUpdated(boat*)),this,SLOT(slot_updateTip(boat*)));

}

void POI::createPopUpMenu(void)
{
    popup = new QMenu(parent);
    connect(this->popup,SIGNAL(aboutToShow()),parent,SLOT(slot_resetGestures()));
    connect(this->popup,SIGNAL(aboutToHide()),parent,SLOT(slot_resetGestures()));

    ac_edit = new QAction(tr("Editer"),popup);
    popup->addAction(ac_edit);
    connect(ac_edit,SIGNAL(triggered()),this,SLOT(slot_editPOI()));

    ac_delPoi = new QAction(tr("Supprimer"),popup);
    popup->addAction(ac_delPoi);
    connect(ac_delPoi,SIGNAL(triggered()),this,SLOT(slot_delPoi()));

    ac_copy = new QAction(tr("Copier"),popup);
    popup->addAction(ac_copy);
    connect(ac_copy,SIGNAL(triggered()),this,SLOT(slot_copy()));

    ac_setGribDate = new QAction(tr("Set Date"),popup);
    popup->addAction(ac_setGribDate);
    connect(ac_setGribDate,SIGNAL(triggered()),this,SLOT(slot_setGribDate()));

    ac_setHorn= new QAction(tr("Activer la corne de brume 10mn avant le passage"),popup);
    popup->addAction(ac_setHorn);
    connect(ac_setHorn,SIGNAL(triggered()),this,SLOT(slot_setHorn()));

    ac_compassLine = new QAction(tr("Tirer un cap"),popup);
    popup->addAction(ac_compassLine);
    connect(ac_compassLine,SIGNAL(triggered()),this,SLOT(slotCompassLine()));
    connect(this,SIGNAL(compassLine(double,double)),owner,SLOT(slotCompassLineForced(double,double)));

    ac_twaLine = new QAction(tr("Tracer une estime TWA"),popup);
    popup->addAction(ac_twaLine);
    connect(ac_twaLine,SIGNAL(triggered()),this,SLOT(slot_twaLine()));

    ac_centerOnPOI = new QAction(tr("Center on POI"),popup);
    popup->addAction(ac_centerOnPOI);
    connect(ac_centerOnPOI,SIGNAL(triggered()),this,SLOT(slot_centerOnBoat()));

    popup->addSeparator();
    ac_setWp = new QAction(tr("Marque->WP"),popup);
    ac_setWp->setCheckable(true);
    popup->addAction(ac_setWp);
    connect(ac_setWp,SIGNAL(triggered()),this,SLOT(slot_setWP_ask()));
    popup->addSeparator();

    ac_routeList = new QMenu(tr("Routes"));
    connect(ac_routeList,SIGNAL(triggered(QAction*)),this,SLOT(slot_routeMenu(QAction*)));
    popup->addMenu(ac_routeList);


    ac_editRoute = new QAction(tr("Editer la route "),popup);
    ac_editRoute->setData(QVariant(QMetaType::VoidStar, &route));
    popup->addAction(ac_editRoute);
    ac_editRoute->setEnabled(false);
    connect(ac_editRoute,SIGNAL(triggered()),this,SLOT(slot_editRoute()));
    ac_poiRoute = new QAction(tr("Montrer les pois intermediaires de la route "),popup);
    ac_poiRoute->setCheckable(true);
    ac_poiRoute->setData(QVariant(QMetaType::VoidStar, &route));
    popup->addAction(ac_poiRoute);
    ac_poiRoute->setEnabled(false);;
    connect(ac_poiRoute,SIGNAL(triggered()),this,SLOT(slot_poiRoute()));
    ac_copyRoute = new QAction(tr("Copier la route "),popup);
    ac_copyRoute->setData(QVariant(QMetaType::VoidStar, &route));
    popup->addAction(ac_copyRoute);
    ac_copyRoute->setEnabled(false);;
    connect(ac_copyRoute,SIGNAL(triggered()),this,SLOT(slot_copyRoute()));

    menuSimplify=new QMenu(tr("Simplifier la route "),popup);
    ac_simplifyRouteMax = new QAction(tr("Maximum"),menuSimplify);
    ac_simplifyRouteMin = new QAction(tr("Minimum"),menuSimplify);
    menuSimplify->addAction(ac_simplifyRouteMax);
    menuSimplify->addAction(ac_simplifyRouteMin);
    menuSimplify->setEnabled(false);
    connect(ac_simplifyRouteMax,SIGNAL(triggered()),this,SLOT(slot_simplifyRouteMax()));
    connect(ac_simplifyRouteMin,SIGNAL(triggered()),this,SLOT(slot_simplifyRouteMin()));
    popup->addMenu(menuSimplify);
    ac_optimizeRoute = new QAction(tr("Optimiser la route "),popup);
    popup->addAction(ac_optimizeRoute);
    ac_optimizeRoute->setEnabled(false);;
    connect(ac_optimizeRoute,SIGNAL(triggered()),this,SLOT(slot_optimizeRoute()));
    ac_zoomRoute = new QAction(tr("Zoom sur la route "),popup);
    ac_zoomRoute->setEnabled(false);
    popup->addAction(ac_zoomRoute);
    connect(ac_zoomRoute,SIGNAL(triggered()),this,SLOT(slot_zoomRoute()));

    ac_delRoute = new QAction(tr("Supprimer la route "),popup);
    ac_delRoute->setData(QVariant(QMetaType::VoidStar, &route));
    ac_delRoute->setEnabled(false);
    connect(ac_delRoute,SIGNAL(triggered()),parent,SLOT(slot_deleteRoute()));

    ac_finePosit = new QAction(tr("Optimiser le placement sur la route"),popup);
    connect(ac_finePosit,SIGNAL(triggered()),this,SLOT(slot_finePosit()));
    popup->addAction(ac_finePosit);

    ac_simplifiable = new QAction(tr("Non simplifiable"),popup);
    ac_simplifiable->setCheckable(true);
    ac_simplifiable->setChecked(this->notSimplificable);
    connect(ac_simplifiable,SIGNAL(toggled(bool)),this,SLOT(slot_notSimplificable(bool)));
    popup->addAction(ac_simplifiable);

    ac_modeList = new QMenu(tr("Mode de navigation vers ce POI"));
    QActionGroup * ptr_group = new QActionGroup(ac_modeList);

    ac_modeList1 = new QAction(tr("VB-VMG"),popup);
    ac_modeList1->setCheckable  (true);
    ac_modeList->addAction(ac_modeList1);
    ptr_group->addAction(ac_modeList1);
    ac_modeList1->setData(0);

    ac_modeList2 = new QAction(tr("B-VMG"),popup);
    ac_modeList2->setCheckable  (true);
    ac_modeList->addAction(ac_modeList2);
    ptr_group->addAction(ac_modeList2);
    ac_modeList2->setData(1);

    ac_modeList3 = new QAction(tr("ORTHO"),popup);
    ac_modeList3->setCheckable  (true);
    ac_modeList->addAction(ac_modeList3);
    ptr_group->addAction(ac_modeList3);
    ac_modeList3->setData(2);
    ac_modeList1->setChecked(true);
    connect(ac_modeList,SIGNAL(triggered(QAction*)),this,SLOT(slot_setMode(QAction*)));
    popup->addMenu(ac_modeList);

    popup->addSeparator();
    popup->addAction(ac_delRoute);
    popup->addSeparator();

    ac_pilot=new QAction(tr("Pre-selectionner pour le pilototo"),popup);
    ac_pilot->setCheckable(true);
    ac_pilot->setChecked(this->piloteSelected);
    connect(ac_pilot,SIGNAL(triggered()),this,SLOT(slot_pilote()));
    popup->addAction(ac_pilot);

    popup->addSeparator();
    ac_routage=new QAction(tr("Routage vers ce POI"),popup);
    connect(ac_routage,SIGNAL(triggered()),this,SLOT(slot_routage()));
    popup->addAction(ac_routage);

    popup->addSeparator();
    ac_connect=new QAction(tr("Tracer/Editer une ligne avec un autre POI"),popup);
    connect(ac_connect,SIGNAL(triggered()),this,SLOT(slot_relier()));
    popup->addAction(ac_connect);
}

/**************************/
/* Events                 */
/**************************/

void POI::mousePressEvent(QGraphicsSceneMouseEvent * e)
{
    if (e->button() == Qt::LeftButton && e->modifiers()==Qt::ShiftModifier)
    {
         if(route!=NULL)
             if(route->getFrozen()||route->isBusy()) return;
         if(VLMBoardIsBusy && this->isWp) return;
         previousLon=lon;
         previousLat=lat;
         this->partOfTwa=false;
         isMoving=true;
//         if(route!=NULL)
//            this->isPartOfBvmg=route->isPartOfBvmg(this);
//         else
//            this->isPartOfBvmg=false;
         if(route!=NULL)
             route->setFastVmgCalc(true);
         mouse_x=e->scenePos().x();
         mouse_y=e->scenePos().y();
         setCursor(Qt::ClosedHandCursor);
         update();
     }
    else if(!((MainWindow*)owner)->get_selPOI_instruction())
        e->ignore();
}

bool POI::tryMoving(int x, int y)
{
    if(isMoving)
    {
        int new_x=this->x()+(x-mouse_x);
        int new_y=this->y()+(y-mouse_y);

        setPos(new_x,new_y);
        mouse_x=x;
        mouse_y=y;


        if(route!=NULL &&!route->isBusy())
        {
            double newlon,newlat;
            new_x=scenePos().x();
            new_y=scenePos().y()+height/2;
            proj->screen2map(new_x,new_y, &newlon, &newlat);
            setLongitude(newlon);
            setLatitude(newlat);
            Util::computePos(proj,lat, lon, &pi, &pj);
            emit poiMoving();
        }
        if(lineBetweenPois!=NULL)
        {
            double newlon,newlat;
            new_x=scenePos().x();
            new_y=scenePos().y()+height/2;
            proj->screen2map(new_x,new_y, &newlon, &newlat);
            setLongitude(newlon);
            setLatitude(newlat);
            Util::computePos(proj,lat, lon, &pi, &pj);
            manageLineBetweenPois();
        }
        return true;
    }
    return false;
}

void POI::mouseReleaseEvent(QGraphicsSceneMouseEvent * e)
{
    if(isMoving)
    {
        double newlon,newlat;
        if(e->modifiers()==Qt::ShiftModifier)
        {
            int new_x=scenePos().x();
            int new_y=scenePos().y()+height/2;
            proj->screen2map(new_x,new_y, &newlon, &newlat);
        }
        else
        {
            newlon=previousLon;
            newlat=previousLat;
        }
        setLongitude(newlon);
        setLatitude(newlat);
        Util::computePos(proj,lat, lon, &pi, &pj);
        setPos(pi, pj-height/2);
        isMoving=false;
        setCursor(Qt::PointingHandCursor);
        setName(this->name);
        update();
        if(isWp && e->modifiers()==Qt::ShiftModifier) slot_setWP();
        if(route!=NULL)
        {
            route->setFastVmgCalc(false);
            route->slot_recalculate();
        }
        manageLineBetweenPois();
        return;
    }

    if(e->pos().x() < 0 || e->pos().x()>width || e->pos().y() < 0 || e->pos().y() > height)
    {
        e->ignore();
        return;
    }
    if (e->button() == Qt::LeftButton)
    {
        emit clearSelection();
        if(((MainWindow*)owner)->get_selPOI_instruction())
            emit selectPOI(this);
        else
        {
//            if(route!=NULL)
//                if(route->getFrozen()) return;
//            emit editPOI(this);
            e->ignore();
        }
    }
}

void POI::contextMenuEvent(QGraphicsSceneContextMenuEvent * e)
{
    bool onlyLineOff = false;
    if(route==NULL || route->getLastPoi()==this || route->getFrozen())
    {
        this->ac_finePosit->setEnabled(false);
    }
    else
    {
        this->ac_finePosit->setEnabled(true);
    }

    if(route==NULL) {
        this->ac_setHorn->setEnabled(false);
        this->ac_routage->setEnabled(true);
    }
    else {
        this->ac_setHorn->setEnabled(true);
        this->ac_routage->setEnabled(false);
    }


    switch(parent->getCompassMode(e->scenePos().x(),e->scenePos().y()))
    {
        case 0:
            /* not showing menu line, default text*/
            ac_compassLine->setText(tr("Tirer un cap"));
            ac_compassLine->setEnabled(true);
            break;
        case 1:
            /* showing text for compass line off*/
            ac_compassLine->setText(tr("Arret du cap"));
            ac_compassLine->setEnabled(true);
            onlyLineOff=true;
            break;
        case 2:
        case 3:
            ac_compassLine->setText(tr("Tirer un cap"));
            ac_compassLine->setEnabled(true);
            break;
    }

    if(onlyLineOff || ((MainWindow*)owner)->get_selPOI_instruction()) /* ie only show the Arret du cap line */
    {
        ac_setWp->setEnabled(false);
        ac_setGribDate->setEnabled(false);
        ac_edit->setEnabled(false);
        ac_delPoi->setEnabled(false);
        ac_copy->setEnabled(false);
        ac_routeList->setEnabled(false);
        ac_delRoute->setEnabled(false);
        ac_delRoute->setData(QVariant(QMetaType::VoidStar, &route));
        ac_editRoute->setEnabled(false);
        ac_poiRoute->setEnabled(false);
        ac_copyRoute->setEnabled(false);
        menuSimplify->setEnabled(false);
        ac_optimizeRoute->setEnabled(false);
        ac_zoomRoute->setEnabled(false);
    }
    else
    {
        ac_setWp->setEnabled(!((MainWindow*)owner)->getBoatLockStatus());
        ac_setGribDate->setEnabled(useTstamp || useRouteTstamp);
        ac_edit->setEnabled(true);
        ac_delPoi->setEnabled(true);
        ac_copy->setEnabled(true);
        ac_routeList->setEnabled(true);
        ac_delRoute->setData(QVariant(QMetaType::VoidStar, &route));
        if(route==NULL)
        {
            ac_delRoute->setEnabled(false);
            ac_editRoute->setEnabled(false);
            ac_poiRoute->setEnabled(false);
            ac_copyRoute->setEnabled(false);
            menuSimplify->setEnabled(false);
            ac_optimizeRoute->setEnabled(false);
            ac_zoomRoute->setEnabled(false);
        }
        else
        {
            QPixmap iconI(20,10);
            iconI.fill(route->getColor());
            QIcon icon(iconI);
            ac_delRoute->setText(tr("Supprimer la route ")+route->getName());
            ac_delRoute->setEnabled(true);
            ac_delRoute->setIcon(icon);
            ac_editRoute->setText(tr("Editer la route ")+route->getName());
            ac_editRoute->setEnabled(true);
            ac_editRoute->setIcon(icon);
            ac_poiRoute->setText(tr("Montrer les POIs intermediaires de la route ")+route->getName());
            ac_poiRoute->setChecked(!route->getHidePois());
            ac_poiRoute->setEnabled(true);
            ac_poiRoute->setIcon(icon);
            ac_copyRoute->setText(tr("Copier la route ")+route->getName());
            ac_copyRoute->setEnabled(true);
            ac_copyRoute->setIcon(icon);
            menuSimplify->setTitle(tr("Simplifier la route ")+route->getName());
            menuSimplify->setEnabled(true);
            menuSimplify->setIcon(icon);
            ac_optimizeRoute->setText(tr("Optimiser la route ")+route->getName());
            ac_optimizeRoute->setEnabled(true);
            ac_optimizeRoute->setIcon(icon);
            ac_zoomRoute->setText(tr("Zoom sur la route ")+route->getName());
            ac_zoomRoute->setEnabled(true);
            ac_zoomRoute->setIcon(icon);
        }
        /*clear current actions */
        ac_routeList->clear();

        QListIterator<ROUTE*> j (parent->getRouteList());
        QAction * ptr;
        ptr=new QAction(tr("Aucune route"),ac_routeList);
        ptr->setCheckable  (true);
        ac_routeList->addAction(ptr);
        ptr->setData(0);

        int k=0;

        if(route==NULL)
            ptr->setChecked(true);
        else
            ptr->setChecked(false);
        QPixmap iconI(20,10);
        while(j.hasNext())
        {
            ROUTE * ptr_route = j.next();
            if(Settings::getSetting("autoHideRoute",1).toInt()==1 && (ptr_route->getBoat()==NULL || !ptr_route->getBoat()->getIsSelected())) continue;
            iconI.fill(ptr_route->getColor());
            QIcon icon(iconI);
            ptr=new QAction(ptr_route->getName(),ac_routeList);
            ptr->setIcon(icon);
            ptr->setCheckable  (true);
            ptr->setData(-1);
            ac_routeList->addAction(ptr);
            if(ptr_route == route)
            {
                QFont font=ptr->font();
                font.setBold(true);
                ptr->setFont(font);
                ptr->setChecked(true);
            }
            else
                ptr->setChecked(false);
            ptr->setData(qVariantFromValue((void *) ptr_route));
            ++k;
        }

    }

    /* Modification du nom du bateau */
    boat * ptr=parent->getSelectedBoat();
    if(ptr)
    {
        ac_setWp->setText(tr("Marque->WP : ")+ptr->getBoatPseudo());
    }
    else
        ac_setWp->setEnabled(false);
    if(this->isWp && ptr)
        ac_setWp->setChecked(true);
    else
        ac_setWp->setChecked(false);
    popup->exec(QCursor::pos());
}

/*********************/
/* data manipulation */
/*********************/

void POI::setNavMode(int mode)
{
    this->navMode=mode;
    switch(navMode)
    {
        case 0:
            ac_modeList1->setChecked(true);
            break;
        case 1:
            ac_modeList2->setChecked(true);
            break;
        case 2:
            ac_modeList3->setChecked(true);
            break;
        default:
            this->navMode=0;
            ac_modeList1->setChecked(true);
            break;
     }
}
void POI::setName(QString name)
{
    if(name.length()>100) name=name.left(100);
    this->name=name;
    update_myStr();
    setTip("");
}
void POI::setTip(QString tip)
{
    boat * w_boat;
    tip=tip.replace(" ","&nbsp;");
    if(route==NULL)
        w_boat=myBoat;
    else
        w_boat=route->getBoat();
    QString pilot;
    switch(this->colorPilototo)
    {
    case 1:
            pilot="<br>"+tr("1ere cible du pilototo");
            break;
    case 2:
            pilot="<br>"+tr("2eme cible du pilototo");
            break;
    case 3:
            pilot="<br>"+tr("3eme cible du pilototo");
            break;
    case 4:
            pilot="<br>"+tr("4eme cible du pilototo");
            break;
    case 5:
            pilot="<br>"+tr("5eme cible du pilototo");
            break;
    }
    QString at;
    if(wph!=-1)
        at=QString().sprintf(" @%.1f",this->wph)+tr("deg");
    QString tt;
    if(w_boat)
    {
        Orthodromie orth2boat(w_boat->getLon(), w_boat->getLat(), lon, lat);
        double   distance=orth2boat.getDistance();
        tt=tr("Ortho a partir de ")+w_boat->getBoatPseudo()+": "+
                   Util::formatDistance(distance)+
                   "/"+QString().sprintf("%.2f",Util::A360(orth2boat.getAzimutDeg()-w_boat->getDeclinaison()))+tr("deg");
        tt=getTypeStr() + " : " + my_str +at+pilot+ "<br>"+tt+tip;
    }
    else
    {
        tt=getTypeStr() + " : "+my_str+at+pilot+"<br>"+tip;
    }
    tt=tt.replace(" ","&nbsp;");
    setToolTip(tt);
}
void POI::update_myStr(void)
{
    QDateTime tm;
    tm.setTimeSpec(Qt::UTC);
    tm.setTime_t(getTimeStamp());
    if(route==NULL) useRouteTstamp=false;
    if(useRouteTstamp || useTstamp)
        if(useRouteTstamp)
        {
            if(this->routeTimeStamp==0)
                my_str=name + " " + tr("Non joignable avec ce Grib");
            else
                my_str=name + " " + tm.toString("dd MMM-hh:mm");
        }
        else
            my_str=name + " " + tm.toString("dd MMM-hh:mm");
    else
        my_str=this->name;
    QFont myFont=(QFont(QApplication::font()));
    if(this->piloteSelected)
        myFont.setBold(true);
    this->setFont(myFont);
    QFontMetrics fm(font());
    prepareGeometryChange();
    width = fm.width(my_str) + 10 +2;
    height = qMax(fm.height()+2,10);
}

void POI::setTimeStamp(time_t date)
{
    this->useTstamp=true;
    this->timeStamp=date;
    update_myStr();

}

void POI::setRouteTimeStamp(time_t date)
{
    this->useRouteTstamp=(date!=-1);
    if(useRouteTstamp) useTstamp=false;
    this->routeTimeStamp=date;
    update_myStr();
    //update();
}

QString POI::getTypeStr(int index)
{
    QString type_str[3] = { "POI", "Marque", "Balise" };
    return type_str[index];
}

void POI::chkIsWP(void)
{
    /*
    if(this->name.contains("R00014"))
    {
        QString debug;
        debug=debug.sprintf("lat %.6f lon %.6f wplat %.6f wplon %.6f",lat,lon,WPlat,WPlon );
        qWarning()<<debug;
    }
    */
//#warning chkIsWp rounded to 4 digits, waiting for a fix by VLM
    //qWarning()<<qRound(lat*1000)<<qRound(lon*1000)<<qRound(WPlat*1000)<<qRound(WPlon*1000);
    double trickLat=qRound(lat*1000);
    double trickLon=qRound(lon*1000);
    double trickWPlat=qRound(WPlat*1000);
    double trickWPlon=qRound(WPlon*1000);
    if(trickLat-1<=trickWPlat && trickLat+1>=trickWPlat &&
       trickLon-1<=trickWPlon && trickLon+1>=trickWPlon)
    {
        if(!isWp)
        {
            isWp=true;
            update();
            emit wpChanged();
        }
    }
    else
    {
        if(isWp)
        {
            isWp=false;
            update();
            parent->getPois().swap(parent->getPois().indexOf(this),0);
            emit wpChanged();
        }
    }
}
void POI::setWph(double wph)
{
    if(qRound(wph*10.0)==qRound(this->wph*10.0)) return;
    this->wph=qRound(wph*10.0)/10.0;
    if(this->isWp)
    {
        emit wpChanged();
    }
}

void POI::setRoute(ROUTE *route)
{
    this->partOfTwa=false;
    if(this->route!=NULL)
    {
        if(this->route->getFrozen()) return;
        //this->notSimplificable=false;
        this->route->removePoi(this);
        this->routeName="";
    }
    this->route=route;
    if(this->route!=NULL)
    {
        this->route->insertPoi(this);
        if(this->route==NULL) /*route has rejected the poi for some reason*/
            setRouteTimeStamp(false);
        else
        {
            this->routeName=route->getName();
            this->route->setHidePois(route->getHidePois());
        }
    }
    else
    {
        //this->notSimplificable=false;
        setRouteTimeStamp(false);
    }
}

/**************************/
/* Slots                  */
/**************************/
void POI::slot_relier()
{
    DialogPoiConnect * box=new DialogPoiConnect(this,parent);
    if(box->exec()==QDialog::Accepted)
    {
        if(connectedPoi==NULL)
        {
            if(lineBetweenPois!=NULL)
            {
                delete lineBetweenPois;
                lineBetweenPois=NULL;
            }
        }
        else
        {
            if(lineBetweenPois!=NULL)
                delete lineBetweenPois;
            lineBetweenPois=new vlmLine(proj,parent->getScene(),Z_VALUE_POI);
            connectedPoi->setLineBetweenPois(lineBetweenPois);
            QPen pen(lineColor);
            pen.setWidthF(lineWidth);
            lineBetweenPois->setLinePen(pen);
            manageLineBetweenPois();
        }
    }
}
void POI::manageLineBetweenPois()
{
    if(lineBetweenPois==NULL) return;
    lineBetweenPois->deleteAll();
    lineBetweenPois->addVlmPoint(vlmPoint(this->lon,this->lat));
    lineBetweenPois->addVlmPoint(vlmPoint(this->connectedPoi->lon,this->connectedPoi->lat));
    lineBetweenPois->slot_showMe();
}
void POI::slot_poiRoute()
{
    if(route==NULL) return;
    route->setHidePois(!route->getHidePois());
    ac_poiRoute->setChecked(!route->getHidePois());
}

void POI::slot_editRoute()
{
    if (this->route==NULL) return;
    parent->slot_editRoute(route);
}
void POI::slot_zoomRoute()
{
   if (this->route == NULL) return;
   route->zoom();
}
void POI::slot_simplifyRouteMax()
{
    if (this->route==NULL) return;
    route->setSimplify (true);
    route->set_strongSimplify(true);
    timerSimp->start();
}
void POI::slot_simplifyRouteMin()
{
    if (this->route==NULL) return;
    route->setSimplify (true);
    route->set_strongSimplify(false);
    timerSimp->start();
}
void POI::slot_timerSimp()
{
    parent->treatRoute(route);
}

void POI::slot_optimizeRoute()
{
    if (this->route==NULL) return;
    route->setOptimize (true);
    timerSimp->start();
}
void POI::slotCompassLine()
{
    double i1,j1;
    proj->map2screenDouble(lon,this->lat,&i1,&j1);
    emit compassLine(i1,j1);
}
void POI::slot_setHorn()
{
    QDateTime time=QDateTime::fromTime_t(this->routeTimeStamp).toUTC();
    time.setTimeSpec(Qt::UTC);
//    qWarning()<<"time before sub "<<time;
//    time.addSecs(-600);
    time=(QDateTime::fromTime_t(time.toTime_t()-600)).toUTC();
//    qWarning()<<"time after sub "<<time;
    parent->setHornDate(time);
    parent->setHornIsActivated(true);
    parent->setHorn();
}

void POI::slot_updateProjection()
{
    Util::computePos(proj,lat, lon, &pi, &pj);
    int dy = height/2;
    setPos(pi, pj-dy);
}
void POI::slot_updateTip(boat * myBoat)
{
    this->myBoat=myBoat;
    if(route==NULL) setTip("");
}

void POI::slot_editPOI()
{
    if(route!=NULL)
        if(route->getFrozen()) return;
    emit editPOI(this);
}

void POI::slot_centerOnBoat(void) {
    proj->setCenterInMap(getLongitude(),getLatitude());
}

void POI::slot_copy()
{
    Util::setWPClipboard(lat,lon,wph);
}
void POI::slot_setWP_ask()
{
    if (parent->getSelectedBoat() && parent->getSelectedBoat()->get_boatType()==BOAT_VLM &&
       ((boatVLM *)parent->getSelectedBoat())->getPilotType()<=2)
    {
        QString mes;
        if(((boatVLM *)parent->getSelectedBoat())->getPilotType()==1)
            mes=tr("Attention: votre bateau est en mode cap fixe");
        else
            mes=tr("Attention: votre bateau est en mode Regulateur d'Allure (angle au vent fixe)");
        int rep = QMessageBox::question (parent,
                tr("Definition du WP-VLM"),
                mes+"\n\n"+tr("Etes-vous sur ?"),
                QMessageBox::Yes | QMessageBox::No);
        if(rep==QMessageBox::Yes)
            slot_setWP();
    }
    else
        slot_setWP();
}

void POI::slot_setWP()
{
    this->partOfTwa=false;
    if(parent->getSelectedBoat()->getLockStatus()){
        chkIsWP();
        return;
    }
    VLMBoardIsBusy=true;
    emit chgWP(lat,lon,wph);
}

void POI::slot_setGribDate()
{
    if (useRouteTstamp) emit setGribDate(routeTimeStamp);
    else if(useTstamp) emit setGribDate(timeStamp);
}

void POI::slot_delPoi()
{
    if(route!=NULL)
        if(route->getFrozen()) return;
    int rep = QMessageBox::question (parent,
                                     tr("Detruire la marque")+" - "+this->getName(),
                                     tr("La destruction d'une marque est definitive.\n\nEtes-vous sur de vouloir supprimer")+" "+getName()+"?",
            QMessageBox::Yes | QMessageBox::No);
    if (rep == QMessageBox::Yes) {

        if(route!=NULL)
        {
            if(route->isBusy())
            {
                QMessageBox::critical(0,tr("Destruction d'une marque")+" - "+this->getName(),tr("Le calcul de la route n'est pas fini, impossible de supprimer ce POI"));
                return;
            }
            setRoute(NULL);
        }
        emit delPOI_list(this);
//        rmSignal();
//        close();
        this->deleteLater();
    }
}

void POI::slot_paramChanged()
{
    poiColor = QColor(Settings::getSetting("POI_color",QColor(Qt::black).name()).toString());
    mwpColor = QColor(Settings::getSetting("Marque_WP_color",QColor(Qt::red).name()).toString());
    wpColor  = QColor(Settings::getSetting("WP_color",QColor(Qt::darkYellow).name()).toString());
    baliseColor  = QColor(Settings::getSetting("Balise_color",QColor(Qt::darkMagenta).name()).toString());
    update();
}

void POI::slot_WPChanged(double tlat,double tlon)
{
    colorPilototo=0;
    WPlat=tlat;
    WPlon=tlon;
    chkIsWP();
    VLMBoardIsBusy=false;
    if (this->isWp)
        this->setWph(parent->getSelectedBoat()->getWPHd());
    if(parent->getSelectedBoat()->get_boatType()!=BOAT_VLM) return;
    boatVLM * b=(boatVLM *)parent->getSelectedBoat();
    if(!b->getHasPilototo()) return;
    QStringList is=b->getPilototo();
    for(int n=0;n<is.count();++n)
    {
        QStringList isi=is.at(n).split(",");
        if(isi.count()<6) continue;
        if(isi.at(5)!="pending") continue;
        if(isi.at(2).toInt()<3) continue;
        double wlat=isi.at(3).toDouble();
        double wlon=isi.at(4).split("@").at(0).toDouble();
        //qWarning()<<lon<<lat<<wlon<<wlat;
        if(qRound(lon*1000)==qRound(wlon*1000) && qRound(lat*1000)==qRound(wlat*1000))
        {
            colorPilototo=n+1;
            //this->setWph(isi.at(4).split("@").at(1).toDouble());
            break;
        }
    }
    update();
}
QString POI::getRouteName()
{
    return routeName;
}

void POI::slot_setMode(QAction* ptr_action)
{
    this->navMode=ptr_action->data().value<int>();
    this->setName(name);
    update();
    if(route!=NULL)
        route->slot_recalculate();
}
void POI::slot_routeMenu(QAction* ptr_action)
{
    ROUTE *  ptr_route = (ROUTE *) ptr_action->data().value<void *>();
    setRoute(ptr_route);
}

/* Helper struct to store a solution for the Simplex optimizer. */
typedef struct {
    double  lon, lat;
    time_t  eta;
    double  remain;
    bool    arrived;
    bool    reached;
} POI_Position;

static inline bool operator< (const POI_Position& a, const POI_Position& b)
{
    return ((a.arrived && (!b.arrived
                           || !b.reached
                           || ((a.eta < b.eta)
                               || ((a.eta == b.eta) && (a.remain < b.remain)))))
            // Note that if a.arrived and b.arrived are true, then a
            // fortiori so are a.reached and b.reached
            ||
            (!a.arrived && !b.arrived
             && (a.reached && (!b.reached
                               || ((a.eta < b.eta)
                                   || ((a.eta == b.eta) && (a.remain < b.remain))))))
            // Note that we need to compare ETAs even when a.arrived
            // and b.arrived are both false because the arrival may be
            // outside the area covered by the grib which might lead
            // the optimization to choose a solution that takes longer
            // to reach the border of the grib if this solution gets
            // closer to the arrival.
            //
            // This is not perfect: the optimizer may choose the wrong
            // grib border (for example, try optimizing the route,
            // moving the arrival to the other side of the grib and
            // optimizing again, there is a fair chance that the route
            // will remain stuck to the wrong side of the
            // grib). However it is the best compromise I can find
            // (and should work just fine if the arrival is inside the
            // area covered by the grib but too far to be reachable).
            ||
            (!a.reached && !b.reached
             && ((a.eta < b.eta)
                 || ((a.eta == b.eta) && (a.remain < b.remain)))));
}

static inline bool operator<= (const POI_Position& a, const POI_Position& b)
{
    return ((a.arrived && (!b.arrived
                           || !b.reached
                           || ((a.eta < b.eta)
                               || ((a.eta == b.eta) && (a.remain <= b.remain)))))
            ||
            (!a.arrived && !b.arrived
             && (a.reached && (!b.reached
                               || ((a.eta < b.eta)
                                   || ((a.eta == b.eta) && (a.remain <= b.remain))))))
            ||
            (!a.reached && !b.reached
             && ((a.eta < b.eta)
                 || ((a.eta == b.eta) && (a.remain <= b.remain)))));
}

void POI::slot_finePosit(bool silent)
{
    if (this->route==NULL) return;
    if (this->route->getFrozen()) return;
    if (route->getLastPoi()==this) return;
    if (route->isBusy()) return;
    if (route->getOptimizing()) return;
    if (route->getUseVbvmgVlm() && !route->getNewVbvmgVlm())
    {
        if(!silent)
            QMessageBox::critical(0,tr("Optimisation du placement d'un POI"),
                                  tr("Vous ne pouvez pas optimiser en mode VBVMG-VLM"));
        return;
    }
    this->abortSearch=false;
    double savedLon=lon;
    double savedLat=lat;
    time_t previousEta=route->getEta();
    //double previousRemain=route->getRemain();
    bool previousHasEta=route->getHas_eta();
    bool detectCoast=route->getDetectCoasts();
    if(!silent)
    {
        DialogFinePosit * dia=new DialogFinePosit(this,parent);
        if(dia->exec()!=QDialog::Accepted)
        {
            delete dia;
            return;
        }
        delete dia;
        if(detectCoast)
            route->setDetectCoasts(false);
    }
    double rangeLon=searchRangeLon;
    double rangeLat=searchRangeLat;
    double step=searchStep;
    QString r;
    route->setOptimizing(!this->optimizing);
    route->setOptimizingPOI(true);
    if(route->getSortPoisByName())
        route->setPoiName(this->name);
    else
        route->setPoiName(QString().setNum(this->sequence));
    POI * best=NULL;
    QDateTime tm;
    tm.setTimeSpec (Qt::UTC);
    //route->setFastVmgCalc(true);
    //Orthodromie fromBoat(route->getStartLon(),route->getStartLat(),lon,lat);
    POI * previousMe=NULL;
    route->slot_recalculate();
    if(!silent)
    {
        if (route->getHas_eta())
        {
            tm.setTime_t(route->getEta());
            previousMe=new POI(tr("ETA du prochain POI: ")+tm.toString("dd MMM-hh:mm"),0,savedLat,savedLon,this->proj,this->owner,this->parent,0,0,false,this->myBoat);
        }
        else
            previousMe=new POI(tr("Dist. restante du prochain POI: ")+r.sprintf("%.2f milles",route->getRemain()),
                               0,savedLat,savedLon,this->proj,this->owner,this->parent,0,0,false,route->getBoat());
        parent->slot_addPOI_list(previousMe);
    }

    POI_Position    simplex[3];

    simplex[0].lon     = lon;
    simplex[0].lat     = lat;
    simplex[0].eta     = route->getEta();
    simplex[0].remain  = route->getRemain();
    simplex[0].arrived = route->getHas_eta();

    /* Note that if the route did not reach the target, then getEta
     * returns the last date of the grib. */
#define TRYPOINT(P) do {                                \
        if ((P).lat > 85)        (P).lat = 85;          \
        else if ((P).lat < -85)  (P).lat = -85;         \
        setLongitude ((P).lon);                         \
        setLatitude ((P).lat);                          \
        route->slot_recalculate();                      \
        (P).eta     = route->getEta();                  \
        (P).remain  = route->getRemain();               \
        (P).arrived = route->getHas_eta();              \
        (P).reached = useRouteTstamp;                   \
        Util::computePos (proj, lat, lon, &pi, &pj);    \
        setPos (pi, pj-height/2);                       \
        update();                                       \
        QApplication::processEvents();                  \
    } while (0)

#define UPDATEBEST  do {                                                \
        if (best != NULL) {                                             \
            parent->slot_delPOI_list (best);                            \
            delete best;                                                \
        }                                                               \
        tm.setTime_t (simplex[0].eta);                                  \
        best = new POI (tr ("Meilleure ETA: ")                          \
                        + tm.toString ("dd MMM-hh:mm")                  \
                        + QString().sprintf(" (%+.3f milles)",simplex[0].remain), \
                        0,                                              \
                        simplex[0].lat, simplex[0].lon,                 \
                        this->proj, this->owner, this->parent, 0, 0, false, route->getBoat()); \
        parent->slot_addPOI_list (best);                                \
    } while (0)

#define SORTSIMPLEX do {                        \
        for (int i = 1; i < 3; ++i) {           \
            POI_Position    tmp = simplex[i];   \
            int             j;                  \
                                                \
            for (j = i; j > 0; --j)             \
                if (tmp < simplex[j-1])         \
                    simplex[j]  = simplex[j-1]; \
                else                            \
                    break;                      \
                                                \
            if (j != i)                         \
                simplex[j] = tmp;               \
        }                                       \
    } while (0)

    if (autoRange || silent) {
        /* if 0 starts from boat, cannot be last*/
        const int    myRank = route->getPoiList().indexOf (this);

        // Get the coordinates of the current POI
        const double cLat = getLatitude();
        const double cLon = getLongitude();

        // Get the coordinates of the previous POI
        const double pLat = ((myRank == 0)
                             ? route->getBoat()->getLat()
                             : route->getPoiList().at (myRank-1)->getLatitude());
        const double pLon = ((myRank == 0)
                             ? route->getBoat()->getLon()
                             : route->getPoiList().at (myRank-1)->getLongitude());

        // Get the coordinates of the next POI
        const double nLat = route->getPoiList().at (myRank+1)->getLatitude();
        const double nLon = route->getPoiList().at (myRank+1)->getLongitude();

        // Select the neighbour that is furthest from the current POI
        const Orthodromie  pOrth (cLon, cLat, pLon, pLat);
        const Orthodromie  nOrth (cLon, cLat, nLon, nLat);

        const bool useNext = (pOrth.getDistance() < nOrth.getDistance());

        const double   rAzimuth
            = useNext ? nOrth.getAzimutDeg() : pOrth.getAzimutDeg();
        const double   distance
            = (useNext ? nOrth.getDistance() : pOrth.getDistance()) / 2;

        // Initialize the candidates
        Util::getCoordFromDistanceAngle (cLat, cLon,
                                         distance, rAzimuth + 30,
                                         &simplex[1].lat, &simplex[1].lon);
        Util::getCoordFromDistanceAngle (cLat, cLon,
                                         distance, rAzimuth - 30,
                                         &simplex[2].lat, &simplex[2].lon);
    } else {
       simplex[1].lon = lon-rangeLon;
       simplex[1].lat = lat;
       simplex[2].lon = lon;
       simplex[2].lat = lat-rangeLat;
    }
    TRYPOINT (simplex[1]);
    TRYPOINT (simplex[2]);

    SORTSIMPLEX;
    UPDATEBEST;

    do {
        //QApplication::processEvents();

        assert ((simplex[0] <= simplex[1]) && (simplex[1] <= simplex[2]));

        /* 1st step: reflection */
        POI_Position    reflect;
        reflect.lon = simplex[0].lon + simplex[1].lon - simplex[2].lon;
        reflect.lat = simplex[0].lat + simplex[1].lat - simplex[2].lat;
        TRYPOINT (reflect);
        if ((simplex[0] <= reflect) && (reflect < simplex[1])) {
            simplex[2] = simplex[1];
            simplex[1] = reflect;
            continue;
        }

        /* 2nd step: expansion */
        if (reflect < simplex[0]) {
            POI_Position    expand;
            expand.lon = 3*(simplex[0].lon + simplex[1].lon)/2 - 2*simplex[2].lon;
            expand.lat = 3*(simplex[0].lat + simplex[1].lat)/2 - 2*simplex[2].lat;
            TRYPOINT (expand);

            simplex[2] = simplex[1];
            simplex[1] = simplex[0];
            simplex[0] = (expand < reflect) ? expand : reflect;

            UPDATEBEST;
            continue;
        }

        /* 3rd step: contraction */
        POI_Position    contract;
        contract.lon = (simplex[0].lon + simplex[1].lon)/4 + simplex[2].lon/2;
        contract.lat = (simplex[0].lat + simplex[1].lat)/4 + simplex[2].lat/2;
        TRYPOINT (contract);

        if (contract < simplex[2]) {
            if (contract < simplex[0]) {
                simplex[2] = simplex[1];
                simplex[1] = simplex[0];
                simplex[0] = contract;
                UPDATEBEST;
            } else if (contract < simplex[1]) {
                simplex[2] = simplex[1];
                simplex[1] = contract;
            } else
                simplex[2] = contract;
            continue;
        }

        /* 4th step: reduction */
        simplex[1].lon = (simplex[0].lon + simplex[1].lon)/2;
        simplex[1].lat = (simplex[0].lat + simplex[1].lat)/2;
        TRYPOINT (simplex[1]);
        simplex[2].lon = (simplex[0].lon + simplex[2].lon)/2;
        simplex[2].lat = (simplex[0].lat + simplex[2].lat)/2;
        TRYPOINT (simplex[2]);

        SORTSIMPLEX;
        UPDATEBEST;

    } while ((!abortSearch || silent)
             /* For some reason, abs gives ridiculous results here... */
             && ((simplex[2].lat - simplex[0].lat >= step)
                 || (simplex[2].lat - simplex[0].lat <= -step)
                 || (simplex[2].lon - simplex[0].lon >= step)
                 || (simplex[2].lon - simplex[0].lon <= -step)));

    if (this->abortSearch && !silent)
    {
        int rep = QMessageBox::question (parent,tr("Abandon du positionnement automatique"), tr("Souhaitez vous conserver la meilleure position deja trouvee?"), QMessageBox::Yes | QMessageBox::No);
        if (rep == QMessageBox::Yes)
        {
            if(best!=NULL)
            {
                parent->slot_delPOI_list(best);
                delete best;
            }
            if(Settings::getSetting("keepOldPoi","0").toInt()==0 && previousMe!=NULL)
            {
                parent->slot_delPOI_list(previousMe);
                delete previousMe;
            }
            setLongitude(simplex[0].lon);
            setLatitude(simplex[0].lat);
            if(isWp) slot_setWP();
        }
        else
        {
            lon=savedLon;
            lat=savedLat;
            if (best != NULL)
            {
                parent->slot_delPOI_list(best);
                delete best;
            }
            if(previousMe!=NULL)
            {
                parent->slot_delPOI_list(previousMe);
                delete previousMe;
            }
        }
    }
    else
    {
        if (best != NULL)
        {
            parent->slot_delPOI_list(best);
            delete best;
        }
        if(Settings::getSetting("keepOldPoi","0").toInt()==0 && !silent && previousMe!=NULL)
        {
            parent->slot_delPOI_list(previousMe);
            delete previousMe;
        }
        setLongitude(simplex[0].lon);
        setLatitude(simplex[0].lat);
        if(isWp && !silent) slot_setWP();
        else this->chkIsWP();
    }
    Util::computePos(proj,lat, lon, &pi, &pj);
    setPos(pi, pj-height/2);
    update();
    route->setOptimizing(false);
    route->setOptimizingPOI(false);
    route->setFastVmgCalc(false);
    if(!silent)
        route->setDetectCoasts(detectCoast);
    route->slot_recalculate();
    if((!route->getHas_eta() && previousHasEta) ||
       (route->getHas_eta() && route->getEta()>previousEta))
    {
        /* We used to also check route->getRemain() when the route
         * does not have an ETA, but if there is a loss it will always
         * be insignificant (since this loss can only happen at the
         * end of the route, i.e at a point where weather uncertainty
         * is greater than the optimization difference) and checking
         * it may prevent route optimization from working when the
         * last POI is outside the grib. */
        qWarning()<<"wrong optimisation, restoring previous POI position";
        lon=savedLon;
        lat=savedLat;
        Util::computePos(proj,lat, lon, &pi, &pj);
        setPos(pi, pj-height/2);
        if(isWp && !silent) slot_setWP();
        update();
        route->slot_recalculate();
    }
    QApplication::processEvents();
}

/***************************/
/* Paint & other qGraphics */
/***************************/

void POI::paint(QPainter * pnt, const QStyleOptionGraphicsItem * , QWidget * )
{
    QFont myFont=(QFont(QApplication::font()));
    if(this->piloteSelected || !labelTransp)
        myFont.setBold(true);
    this->setFont(myFont);
    QFontMetrics fm(font());
    width = fm.width(my_str) + 10 +2;
    height = qMax(fm.height()+2,10);
    int dy = height/2;
    if(!labelHidden && !myLabelHidden)
    {
        if(route==NULL)
            pnt->fillRect(9,0, width-10,height-1, QBrush(bgcolor));
        else
        {
            int alpha=160;
            if(!this->labelTransp)
                alpha=255;

            switch (this->navMode)
            {
            case 0:
                pnt->fillRect(9,0, width-10,height-1, QBrush(QColor(147,255,147,alpha)));
                break;
            case 1:
                pnt->fillRect(9,0, width-10,height-1, QBrush(QColor(168,226,255,alpha)));
                break;
            case 2:
                pnt->fillRect(9,0, width-10,height-1, QBrush(QColor(255,168,198,alpha)));
                break;
            default:
                pnt->fillRect(9,0, width-10,height-1, QBrush(QColor(255,0,0,60)));
                break;
            }
        }
        pnt->setFont(font());
        if(this->notSimplificable)
        {
            QPen pe=pnt->pen();
            pe.setColor(Qt::red);
            pnt->setPen(pe);
        }
        pnt->drawText(10,fm.height()-2,my_str);
    }
    QColor myColor;
    if(isWp)
    {
        myColor=mwpColor;
#if 0
        if(parent->getSelectedBoat()->getWPHd()!=wph)
        {
            //qWarning()<<"wp_at"<<parent->getSelectedBoat()->getWPHd()<<wph;
            if(myColor==Qt::red)
                myColor=QColor(255,75,156);
            else
                myColor=Qt::red;
        }
#endif
    }
    else
    {
        switch(type)
        {
            case 0:
                myColor=poiColor;
                break;
            case 1:
                myColor=wpColor;
                break;
            case 2:
                myColor=baliseColor;
                break;
            case 3:
                myColor=QColor(Qt::white);
                break;
         }
        //qWarning()<<"colorpilototo="<<this->colorPilototo;
        QColor pColor;
        switch (this->colorPilototo)
        {
        case 1:
            pColor=QColor(0,250,0);
            break;
        case 2:
            pColor=QColor(0,220,0);
            break;
        case 3:
            pColor=QColor(0,190,0);
            break;
        case 4:
            pColor=QColor(0,160,0);
            break;
        case 5:
            pColor=QColor(0,130,0);
            break;
        default:
            pColor=myColor;
            break;
        }
        myColor=pColor;
    }
    QPen pen(myColor);
    pen.setWidth(4);
    pnt->setPen(pen);
    if(!myLabelHidden)
        pnt->fillRect(0,dy-3,7,7, QBrush(myColor));
    pen = QPen(QColor(60,60,60));
    pen.setWidth(1);
    pnt->setPen(pen);
    if(!labelHidden && !myLabelHidden)
        pnt->drawRect(9,0,width-10,height-1);
}

QPainterPath POI::shape() const
{    
    QPainterPath path;
    if(this->myLabelHidden)
        path.addRect(0,0,0,0);
    else
        path.addRect(0,0,width,height);
    return path;
}

QRectF POI::boundingRect() const
{
    if(this->myLabelHidden)
        return QRectF(0,0,0,0);
    else
        return QRectF(0,0,width,height);
}
void POI::slot_pilote()
{
    this->piloteSelected=ac_pilot->isChecked();
    update();
}
void POI::slot_copyRoute()
{
    if(this->route==NULL) return;
    parent->exportRouteFromMenuKML(this->route,"",true);
}

/*****************************************************
 * import POI from zyGrib file
 *****************************************************/

void POI::importZyGrib(myCentralWidget * centralWidget) {
    QString filter;
    filter =  tr("Fichiers ini (*.ini)")
            + tr(";;Autres fichiers (*)");
    QString fileName = QFileDialog::getOpenFileName(0,
                         tr("Choisir un fichier ini"),
                         "./",
                         filter);
    QFile file(fileName);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    int nbPOI=0;

    QTextStream stream(&file);

    double lat=0,lon=0;
    QString name;
    bool foundName,foundLat,foundLon;
    int curCode=-1;
    foundName=foundLon=foundLat=false;


    while(true) {
        QString line = stream.readLine();
        if(line.isNull())
            break;

        QStringList list1 = line.split('\\');
        bool ok;
        if(list1.count()<=1) {
            qWarning() << "Wrong line (no code): " <<line << " - nb item:" << list1.count();
            continue;
        }

        int code=list1.at(0).toInt(&ok);
        if(!ok) {
            qWarning() << "Wrong line (code not numeric): " <<line;
            continue;
        }

        if(code!=curCode) {
            qWarning() << "New code " << code;
            curCode=code;
            foundName=foundLon=foundLat=false;
        }

        QStringList list2 = list1.at(1).split('=');

        if(list2.count()<=1) {
            qWarning() << "Wrong line (no = in data part): " <<line << " - nb item:" << list2.count();
            continue;
        }

        if(list2.at(0) == "name") {
            name=list2.at(1);
            foundName=true;
        }

        if(list2.at(0) == "lat") {
            lat=list2.at(1).toDouble();
            foundLat=true;
        }

        if(list2.at(0) == "lon") {
            lon=list2.at(1).toDouble();
            foundLon=true;
        }

        if(foundName && foundLat && foundLon) {
            qWarning() << "All data ok: " << curCode << " - " << name << " - " << lat << "," << lon;
            POI * poi = new POI(name, POI_TYPE_POI,lat,lon,centralWidget->getProj(),centralWidget->getMainWindow(),centralWidget,-1,-1,false,NULL);
            centralWidget->slot_addPOI_list(poi);
            foundName=foundLon=foundLat=false;
            nbPOI++;
        }

    }

    QMessageBox::information(0,tr("Zygrib POI import"),QString().setNum(nbPOI) +" " +tr("POI imported from zyGrib"));

    file.close();
}

/*****************************************************
 * import POI from geoData file
 *****************************************************/
void POI::importGeoData(myCentralWidget * centralWidget) {
    QString filter;
    filter =  tr("Fichiers textes (*.txt)")
            + tr(";;Autres fichiers (*)");
    QString fileName = QFileDialog::getOpenFileName(0,
                         tr("Choisir un fichier GeoData"),
                         "./",
                         filter);
    QFile file(fileName);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    int nbPOI=0;

    QTextStream stream(&file);

    while(true)
    {
        QString line = stream.readLine();
        if(line.isNull())
            break;

        QStringList list1 = line.split(';');
        bool ok;
        if(list1.count()<5)
        {
            qWarning() << "Wrong line (no code): " <<line << " - nb item:" << list1.count();
            continue;
        }
        QString pRank=list1[0];
        QString pId=list1[1];
        QString temp=list1[2];
        QString letter=temp.at(temp.count()-1);
        double  pLat=temp.remove(letter).toDouble(&ok);
        if(!ok)
        {
            qWarning() << "Wrong line (code not numeric): " <<line;
            continue;
        }
        if(letter=="S") pLat=-pLat;
        temp=list1[3];
        letter=temp.at(temp.count()-1);
        double  pLon=temp.remove(letter).toDouble(&ok);
        if(!ok)
        {
            qWarning() << "Wrong line (code not numeric): " <<line;
            continue;
        }
        if(letter=="W") pLon=-pLon;
        QString pTime=list1[4];
        POI * poi = new POI(pId, POI_TYPE_BALISE,pLat,pLon,centralWidget->getProj(),centralWidget->getMainWindow(),centralWidget,-1,-1,false,NULL);
        poi->setTip(tr("Classement ")+pRank+"<br>"+pTime);
        centralWidget->slot_addPOI_list(poi);
        nbPOI++;
    }
    QMessageBox::information(0,tr("GeoData POI import"),QString().setNum(nbPOI) +" " +tr("POI imported from GeoData"));
    file.close();
}

/*****************************************************
 * read POI from file
 *****************************************************/

#define ROOT_NAME         "POIs"

/* POI */
#define POI_GROUP_NAME    "POI"
#define POI_NAME          "name"
#define LAT_NAME          "Lat"
#define LON_NAME          "Lon"
#define LAT_NAME_CONNECTED "LatConnected"
#define LON_NAME_CONNECTED "LonConnected"
#define LINE_COLOR_R      "LineColorR"
#define LINE_COLOR_G      "LineColorG"
#define LINE_COLOR_B      "LineColorB"
#define LINE_WIDTH        "lineWidth"
#define LON_NAME_OLD      "Pass"
#define WPH_NAME          "Wph"
#define TYPE_NAME         "type"
#define TSTAMP_NAME       "timeStamp"
#define USETSTAMP_NAME    "useTimeStamp"
#define POI_ROUTE         "route"
#define POI_NAVMODE       "NavMode"
#define POI_LABEL_HIDDEN  "LabelHidden"
#define POI_NOT_SIMPLIFICABLE "IsSimplificable"
#define POI_PILOTE         "Pilote"
#define POI_SEQUENCE       "Sequence"

void POI::read_POIData(myCentralWidget * centralWidget) {
    QString fname = appFolder.value("userFiles")+"poi.dat";
    QDomNode node;
    QDomNode subNode;
    QDomNode dataNode;

    bool hasOldSystem=false;

    QDomNode * root=XmlFile::get_dataNodeFromDisk(fname,ROOT_NAME);
    if(!root) {
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

    while(!node.isNull())
    {         
         if(node.toElement().tagName() == POI_GROUP_NAME)
         {
             subNode = node.firstChild();
             QString name="";
             QString routeName="";
             double lon=-1, lat=-1,wph=-1;
             double lonConnected=-1, latConnected=-1;
             int type = -1;
             int tstamp=-1;
             bool useTstamp=false;
             bool labelHidden=false;
             int navMode=0;
             bool notSimplificable=false;
             bool pilote=false;
             QColor lineColor=Qt::blue;
             double lineWidth=2;
             int sequence=0;

             while(!subNode.isNull()) {
                  if(subNode.toElement().tagName() == POI_NAME) {
                     dataNode = subNode.firstChild();
                     if(dataNode.nodeType() == QDomNode::TextNode)
                         name = QString(QByteArray::fromBase64(dataNode.toText().data().toUtf8()));
                  }

                  if(subNode.toElement().tagName() == LAT_NAME) {
                     dataNode = subNode.firstChild();
                     if(dataNode.nodeType() == QDomNode::TextNode)
                         lat = dataNode.toText().data().toDouble();
                  }

                  if(subNode.toElement().tagName() == LON_NAME) {
                     dataNode = subNode.firstChild();
                     if(dataNode.nodeType() == QDomNode::TextNode)
                         lon = dataNode.toText().data().toDouble();
                  }

                  if(subNode.toElement().tagName() == LAT_NAME_CONNECTED) {
                     dataNode = subNode.firstChild();
                     if(dataNode.nodeType() == QDomNode::TextNode)
                         latConnected = dataNode.toText().data().toDouble();
                  }

                  if(subNode.toElement().tagName() == LON_NAME_CONNECTED) {
                     dataNode = subNode.firstChild();
                     if(dataNode.nodeType() == QDomNode::TextNode)
                         lonConnected = dataNode.toText().data().toDouble();
                  }

                  if(subNode.toElement().tagName() == LINE_COLOR_R) {
                       dataNode = subNode.firstChild();
                       if(dataNode.nodeType() == QDomNode::TextNode)
                       {
                           lineColor.setRed(dataNode.toText().data().toInt());
                       }
                  }

                  if(subNode.toElement().tagName() == LINE_COLOR_G) {
                       dataNode = subNode.firstChild();
                       if(dataNode.nodeType() == QDomNode::TextNode)
                       {
                           lineColor.setGreen(dataNode.toText().data().toInt());
                       }
                  }

                  if(subNode.toElement().tagName() == LINE_COLOR_B) {
                       dataNode = subNode.firstChild();
                       if(dataNode.nodeType() == QDomNode::TextNode)
                       {
                           lineColor.setBlue(dataNode.toText().data().toInt());
                       }
                  }

                  if(subNode.toElement().tagName() == LINE_WIDTH) {
                       dataNode = subNode.firstChild();
                       if(dataNode.nodeType() == QDomNode::TextNode)
                           lineWidth=dataNode.toText().data().toDouble();
                  }

                  if(subNode.toElement().tagName() == WPH_NAME) {
                     dataNode = subNode.firstChild();
                     if(dataNode.nodeType() == QDomNode::TextNode)
                         wph = dataNode.toText().data().toFloat();
                  }

                  if(subNode.toElement().tagName() == TYPE_NAME) {
                     dataNode = subNode.firstChild();
                     if(dataNode.nodeType() == QDomNode::TextNode)
                         type = dataNode.toText().data().toInt();
                  }

                  if(subNode.toElement().tagName() == POI_SEQUENCE) {
                     dataNode = subNode.firstChild();
                     if(dataNode.nodeType() == QDomNode::TextNode)
                         sequence = dataNode.toText().data().toInt();
                  }

                  if(subNode.toElement().tagName() == TSTAMP_NAME) {
                      dataNode = subNode.firstChild();
                      if(dataNode.nodeType() == QDomNode::TextNode)
                          tstamp = dataNode.toText().data().toInt();
                  }
                  if(subNode.toElement().tagName() == USETSTAMP_NAME) {
                      dataNode = subNode.firstChild();
                      if(dataNode.nodeType() == QDomNode::TextNode)
                          useTstamp = dataNode.toText().data().toInt()==1;
                  }

                  if(subNode.toElement().tagName() == POI_LABEL_HIDDEN) {
                      dataNode = subNode.firstChild();
                      if(dataNode.nodeType() == QDomNode::TextNode)
                          labelHidden = dataNode.toText().data().toInt()==1;
                  }

                  if(subNode.toElement().tagName() == POI_NOT_SIMPLIFICABLE) {
                      dataNode = subNode.firstChild();
                      if(dataNode.nodeType() == QDomNode::TextNode)
                          notSimplificable = dataNode.toText().data().toInt()==1;
                  }

                  if(subNode.toElement().tagName() == POI_PILOTE) {
                      dataNode = subNode.firstChild();
                      if(dataNode.nodeType() == QDomNode::TextNode)
                          pilote = dataNode.toText().data().toInt()==1;
                  }

                  if(subNode.toElement().tagName() == POI_ROUTE) {
                      dataNode = subNode.firstChild();
                      if(dataNode.nodeType() == QDomNode::TextNode)
                         routeName = QString(QByteArray::fromBase64(dataNode.toText().data().toUtf8()));
                  }

                  if(subNode.toElement().tagName() == POI_NAVMODE) {
                      dataNode = subNode.firstChild();
                      if(dataNode.nodeType() == QDomNode::TextNode)
                          navMode = dataNode.toText().data().toInt();
                  }

                  subNode = subNode.nextSibling();
             }
             if(!name.isEmpty() && lat!=-1 && lon != -1) {
                  if(type==-1) type=POI_TYPE_POI;
                  POI * poi = new POI(name,type,lat,lon,centralWidget->getProj(),centralWidget->getMainWindow(),centralWidget,wph,tstamp,useTstamp,NULL);
                  poi->setRouteName(routeName);
                  poi->setNavMode(navMode);
                  poi->setMyLabelHidden(labelHidden);
                  poi->setNotSimplificable(notSimplificable);
                  poi->setPiloteSelected(pilote);
                  poi->setPosConnected(lonConnected,latConnected);
                  poi->setLineColor(lineColor);
                  poi->setLineWidth(lineWidth);
                  poi->setSequence(sequence);
                  centralWidget->slot_addPOI_list(poi);
             }
             else
                  qWarning() << "Incomplete POI info " << name << " "
                       << lat << "," << lon << "@" << wph ;
         }
         node = node.nextSibling();
    }

    if(hasOldSystem)
        POI::cleanFile(fname);
}

void POI::cleanFile(QString fname) {
    QDomElement * root=XmlFile::get_fisrtDataNodeFromDisk(fname);
    QDomNode node;

    if(!root) {
        qWarning() << "Error reading POI from " << fname << " during clean";
        return ;
    }

    node=root->firstChild();
    while(!node.isNull()) {
        //qWarning() << "Cleaning: " << node.toElement().tagName();
        QDomNode nxtNode=node.nextSibling();
        if(node.toElement().tagName() == POI_GROUP_NAME)
            root->removeChild(node);
        if(node.toElement().tagName() == "Version")
            root->removeChild(node);
        node = nxtNode;
    }
    XmlFile::saveRoot(root,fname);
}

void POI::write_POIData(QList<POI*> & poi_list,myCentralWidget * /*centralWidget*/) {
    QString fname = appFolder.value("userFiles")+"poi.dat";

    QDomDocument doc(DOM_FILE_TYPE);
    QDomElement root = doc.createElement(ROOT_NAME);
    doc.appendChild(root);

    QDomElement group;
    QDomElement tag;
    QDomText t;

    QListIterator<POI*> i (poi_list);
    while(i.hasNext())
    {
         POI * poi = i.next();
         if(poi->getRoute()!=NULL && poi->getRoute()->isImported()) continue;
         if(poi->isPartOfTwa()) continue;
         group = doc.createElement(POI_GROUP_NAME);
         root.appendChild(group);

         tag = doc.createElement(POI_NAME);
         group.appendChild(tag);
         t = doc.createTextNode(poi->getName().toUtf8().toBase64());
         tag.appendChild(t);

         tag = doc.createElement(TYPE_NAME);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(poi->getType()));
         tag.appendChild(t);

         tag = doc.createElement(POI_SEQUENCE);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(poi->getSequence()));
         tag.appendChild(t);

         tag = doc.createElement(LAT_NAME);
         group.appendChild(tag);
         t = doc.createTextNode(QString().sprintf("%.10f",poi->getLatitude()));
         tag.appendChild(t);

         tag = doc.createElement(LON_NAME);
         group.appendChild(tag);
         t = doc.createTextNode(QString().sprintf("%.10f",poi->getLongitude()));
         tag.appendChild(t);
         if(poi->getConnectedPoi()!=NULL)
         {
             tag = doc.createElement(LON_NAME_CONNECTED);
             group.appendChild(tag);
             t = doc.createTextNode(QString().sprintf("%.10f",poi->getConnectedPoi()->getLongitude()));
             tag.appendChild(t);

             tag = doc.createElement(LAT_NAME_CONNECTED);
             group.appendChild(tag);
             t = doc.createTextNode(QString().sprintf("%.10f",poi->getConnectedPoi()->getLatitude()));
             tag.appendChild(t);
         }
         tag = doc.createElement(LINE_COLOR_R);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(poi->getLineColor().red()));
         tag.appendChild(t);
         tag = doc.createElement(LINE_COLOR_G);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(poi->getLineColor().green()));
         tag.appendChild(t);
         tag = doc.createElement(LINE_COLOR_B);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(poi->getLineColor().blue()));
         tag.appendChild(t);
         tag = doc.createElement(LINE_WIDTH);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(poi->getLineWidth()));
         tag.appendChild(t);

         tag = doc.createElement(WPH_NAME);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(poi->getWph()));
         tag.appendChild(t);

         tag = doc.createElement(TSTAMP_NAME);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(poi->getTimeStamp()));
         tag.appendChild(t);

         tag = doc.createElement(USETSTAMP_NAME);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(poi->getUseTimeStamp()?1:0));
         tag.appendChild(t);

         tag = doc.createElement(POI_LABEL_HIDDEN);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(poi->getMyLabelHidden()?1:0));
         tag.appendChild(t);

         tag = doc.createElement(POI_NOT_SIMPLIFICABLE);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(poi->getNotSimplificable()?1:0));
         tag.appendChild(t);

         tag = doc.createElement(POI_PILOTE);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(poi->getPiloteSelected()?1:0));
         tag.appendChild(t);

         tag = doc.createElement(POI_ROUTE);
         group.appendChild(tag);
         if(poi->getRoute()!=NULL)
           t = doc.createTextNode(poi->getRoute()->getName().toUtf8().toBase64()); //do not use poi->routeName since route->name might have changed */
         else
             t = doc.createTextNode(QString("").toUtf8().toBase64());
         tag.appendChild(t);

         tag = doc.createElement(POI_NAVMODE);
         group.appendChild(tag);
         t = doc.createTextNode(QString().setNum(poi->getNavMode()));
         tag.appendChild(t);
    }

    if(!XmlFile::set_dataNodeOnDisk(fname,ROOT_NAME,&root,DOM_FILE_TYPE)) {
        /* error in file  => blanking filename in settings */
        qWarning() << "Error saving POI in " << fname;
        return ;
    }
}
