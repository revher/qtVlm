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

#include <cmath>
#include <QMessageBox>
#include <QDebug>

class POI_Editor;

#include "POI_editor.h"
#include "Util.h"
#include "MainWindow.h"
#include "boatAccount.h"

#define POI_EDT_LAT 1
#define POI_EDT_LON 2

//-------------------------------------------------------
// POI_Editor: Constructor for edit an existing POI
//-------------------------------------------------------
POI_Editor::POI_Editor(MainWindow * main,myCentralWidget * parent)
    : QDialog(parent)
{
    setupUi(this);

    this->poi = NULL;
    this->parent=parent;
    this->main=main;

    lock=true;
    modeCreation=false;

    connect(this,SIGNAL(addPOI_list(POI*)),parent,SLOT(slot_addPOI_list(POI*)));
    connect(this,SIGNAL(delPOI_list(POI*)),parent,SLOT(slot_delPOI_list(POI*)));

    connect(main, SIGNAL(newPOI(float,float,Projection *, boatAccount *)),
            this, SLOT(newPOI(float,float,Projection *, boatAccount *)));
    connect(this,SIGNAL(doChgWP(float,float,float)),main,SLOT(slotChgWP(float,float,float)));
}

void POI_Editor::editPOI(POI * poi_)
{
    //=> set name
    modeCreation = false;
    this->poi = poi_;
    initPOI();
    setWindowTitle(tr("Marque : ")+poi->getName());
    btDelete->setEnabled(true);

    exec();
}

void POI_Editor::newPOI(float lon, float lat,Projection *proj, boatAccount *boat)
{
    //=> set name
    modeCreation = true;
    this->poi = new POI(tr("POI"), POI_TYPE_POI,lat, lon, proj, main,
                        parent, -1,-1,false, boat);
    initPOI();
    setWindowTitle(tr("Nouvelle marque"));
    btDelete->setEnabled(false);
    exec();
}

void POI_Editor::initPOI(void)
{
    editName->setText(poi->getName());
    oldType=poi->getType();
    POI_type_liste->setCurrentIndex(oldType);
    QListIterator<ROUTE*> i (parent->getRouteList());
    cb_routeList->clear();
    cb_routeList->addItem("Not in a route");
    while(i.hasNext())
        cb_routeList->addItem(i.next()->getName());
    if(poi->getRoute()!=NULL)
        cb_routeList->setCurrentIndex(cb_routeList->findText(((ROUTE*) poi->getRoute())->getName()));
    if(poi->getWph()==-1)
        editWph->setText(QString());
    else
        editWph->setText(QString().setNum(poi->getWph()));

    btSaveWP->setEnabled(!main->getBoatLockStatus());

    setValue(POI_EDT_LON,poi->getLongitude());
    setValue(POI_EDT_LAT,poi->getLatitude());

    if(poi->getTimeStamp()!=-1)
    {
        QDateTime tm;
        tm.setTimeSpec(Qt::UTC);
        tm.setTime_t(poi->getTimeStamp());
        editTStamp->setDateTime(tm);
        editTStamp->setTimeSpec(Qt::UTC);
    }
    else
    {
        QDateTime tm = QDateTime::currentDateTime().toUTC();
        editTStamp->setDateTime(tm);
    }

    chk_tstamp->setCheckState(poi->getUseTimeStamp()?Qt::Checked:Qt::Unchecked);
    editTStamp->setEnabled(poi->getUseTimeStamp());
    editName->setEnabled(!poi->getUseTimeStamp());

    boatAccount * ptr=parent->getSelectedBoat();
    if(ptr)
    {
        btSaveWP->setText(tr("Marque->WP\n(")+ptr->getLogin()+")");
        btSaveWP->setEnabled(true);
    }
    else
        btSaveWP->setEnabled(false);
}

//---------------------------------------
void POI_Editor::done(int result)
{
    if(result == QDialog::Accepted)
    {
        poi->setPartOfTwa(false);
        QDateTime tm = editTStamp->dateTime();

        tm.setTimeSpec(Qt::UTC);
        poi->setTimeStamp(tm.toTime_t());
        poi->setUseTimeStamp(chk_tstamp->checkState()==Qt::Checked);

        poi->setLongitude(getValue(POI_EDT_LON));
        poi->setLatitude (getValue(POI_EDT_LAT));
 //       int oldtype=poi->getType();
        poi->setType(POI_type_liste->currentIndex());
        poi->setName((editName->text()).trimmed() );
        if(editWph->text().isEmpty())
            poi->setWph(-1);
        else
            poi->setWph(editWph->text().toFloat());
        poi->slot_updateProjection();

        if (modeCreation) {
            //poi->show();
            emit addPOI_list(poi);
        }
        if(cb_routeList->currentIndex()!=0)
        {
            ROUTE * route=parent->getRouteList()[cb_routeList->currentIndex()-1];
            if(!route->getFrozen())
#warning rajouter un QDialog pour prevenir.
                poi->setRoute(route);
        }
        else
            poi->setRoute(NULL);
        if(poi->getIsWp()) emit doChgWP(poi->getLatitude(),poi->getLongitude(),poi->getWph());


    }

    if(result == QDialog::Rejected)
    {
        if (modeCreation)
                delete poi;
    }
    QDialog::done(result);
}

//---------------------------------------
void POI_Editor::btDeleteClicked()
{
    if (! modeCreation) {
        int rep = QMessageBox::question (this,
            tr("D�truire la marque : %1").arg(poi->getName()),
            tr("La destruction d'une marque est definitive.\n\nEtes-vous sur ?"),
            QMessageBox::Yes | QMessageBox::No);
        if (rep == QMessageBox::Yes)
        {
            emit delPOI_list(poi);
            poi->close();
            QDialog::done(QDialog::Accepted);
        }
    }
}

void POI_Editor::btPasteClicked()
{
    float lat,lon,wph;
    QString name;
    int tstamp;
    if(!Util::getWPClipboard(&name,&lat,&lon,&wph,&tstamp))
        return;

    setValue(POI_EDT_LON,lon);
    setValue(POI_EDT_LAT,lat);

    if(tstamp==-1)
    {
        QDateTime tm = QDateTime::currentDateTime().toUTC();
        editTStamp->setDateTime(tm);
        chk_tstamp->setCheckState(Qt::Unchecked);
    }
    else
    {
        QDateTime tm;
        tm.setTimeSpec(Qt::UTC);
        tm.setTime_t(tstamp);
        editTStamp->setDateTime(tm);
        editTStamp->setTimeSpec(Qt::UTC);
        chk_tstamp->setCheckState(Qt::Checked);
    }

    if(name!="")
        editName->setText(name);

    if(wph<0 || wph > 360)
        editWph->setText(QString());
    else
        editWph->setText(QString().setNum(wph));
}

void POI_Editor::btCopyClicked()
{
    Util::setWPClipboard(getValue(POI_EDT_LAT),getValue(POI_EDT_LON),
        editWph->text().isEmpty()?-1:editWph->text().toFloat());
}

void POI_Editor::btSaveWPClicked()
{
    float wph;
    if(editWph->text().isEmpty())
        wph=-1;
    else
        wph=editWph->text().toFloat();
    emit doChgWP(getValue(POI_EDT_LAT),getValue(POI_EDT_LON),wph);
    done(QDialog::Accepted);
}

void POI_Editor::chkTStamp_chg(int state)
{
    editTStamp->setEnabled(state==Qt::Checked);
    editName->setEnabled(state!=Qt::Checked);
}

void POI_Editor::nameHasChanged(QString newName)
{
    setWindowTitle(tr("Marque : ")+newName);
}

void POI_Editor::lat_deg_chg(int)
{
    data_chg(POI_EDT_LAT);
}

void POI_Editor::lat_min_chg(double)
{
    data_chg(POI_EDT_LAT);
}

void POI_Editor::lon_deg_chg(int)
{
    data_chg(POI_EDT_LON);
}

void POI_Editor::lon_min_chg(double)
{
    data_chg(POI_EDT_LON);
}

void POI_Editor::type_chg(int index)
{
    if(index==oldType)
        return;
    QString oldStr=POI::getTypeStr(oldType);
    QString editStr=editName->text();
    if(oldStr==editStr.left(oldStr.size()))
        editName->setText(POI::getTypeStr(index)+editStr.right(editStr.size()-oldStr.size()));
    oldType=index;
}

void POI_Editor::data_chg(int type)
{
    if(lock)
        return;
    lock=true;
    float val=getValue(type);
    if(type==POI_EDT_LAT)
        lat_val->setValue(val);
    else
        lon_val->setValue(val);
    lock=false;
}

void POI_Editor::lat_val_chg(double)
{
    val_chg(POI_EDT_LAT);
}

void POI_Editor::lon_val_chg(double)
{
    val_chg(POI_EDT_LON);
}

void POI_Editor::val_chg(int type)
{
    float val;
    if(lock)
        return;
    lock=true;
    if(type==POI_EDT_LAT)
        val=lat_val->value();
    else
        val=lon_val->value();
    setValue(type,val);
    lock=false;
}

float POI_Editor::getValue(int type)
{
    float res;
    float deg = (type==POI_EDT_LAT?lat_deg->value():lon_deg->value());
    float min = (type==POI_EDT_LAT?lat_min->value():lon_min->value())/60.0;
    float sig;
    if(type==POI_EDT_LAT)
        sig=lat_sig->currentIndex()==0?1.0:-1.0;
    else
        sig=lon_sig->currentIndex()==0?1.0:-1.0;
    /* if min < 0 or deg < 0 the whole value is < 0 */
    /*if (deg < 0)
    {
        if(min<0)
            res = deg + min;
        else
            res = deg - min;
    }
    else
    {
        if(min<0)
            res=-(deg-min);
        else
            res = deg + min;
    }*/
    res=sig*(deg+min);

    //qWarning() << (type==POI_EDT_LAT?"Lat ":"Lon ") << " set to " << res;

    return res;
}

void POI_Editor::setValue(int type,float val)
{
    int sig=val<0?1:0;
    val=fabs(val);
    int   deg = (int) trunc(val);
    float min = 60.0*fabs(val-trunc(val));

    /*if(deg==0 && val < 0)
        min=-min;*/

    if(type==POI_EDT_LAT)
    {
        lat_deg->setValue(deg);
        lat_min->setValue(min);
        lat_val->setValue(val);
        lat_sig->setCurrentIndex(sig);
    }
    else
    {
        lon_deg->setValue(deg);
        lon_min->setValue(min);
        lon_val->setValue(val);
        lon_sig->setCurrentIndex(sig);
    }
}
