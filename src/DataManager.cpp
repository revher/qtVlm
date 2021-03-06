/**********************************************************************
qtVlm: Virtual Loup de mer GUI
Copyright (C) 2013 - Christophe Thomas aka Oxygen77

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

#include "Grib.h"
#include "GribRecord.h"
#include "GribV1.h"
#include "GribV1Record.h"
#include "GribV2.h"
#include "GribV2Record.h"
#include "settings.h"
#include "Util.h"

#include "DataManager.h"

DataManager::DataManager() {
    // init grib conversion matrix => should only be done once
    GribV1Record::init_conversionMatrix();
    GribV2Record::init_conversionMatrix();

    // init var
    grib=NULL;
    gribCurrent=NULL;
    currentDate=0;
    isoBarsStep = Settings::getSetting("isobarsStep", 2).toDouble();
    isoTherms0Step = Settings::getSetting("isoTherms0Step", 50).toInt();

    QString interpol_name[4] = { "UKN", "TWSA", "selecive TWSA", "Hybride" };
    interpolationMode = INTERPOLATION_DEFAULT;
    qWarning() << "Starting with interpolation: " << interpol_name[interpolationMode];

    load_forcedParam();

}

Grib * DataManager::get_grib(int gribType) {
    switch(gribType) {
        case GRIB_GRIB:
            return grib;
        case GRIB_CURRENT:
            return gribCurrent;
        default: // just in case ...
            qWarning() << "Bad grib type: " << gribType;
            return NULL;
    }
}

Grib * DataManager::get_grib(int dataType,int levelType, int levelValue) {
    switch(hasData(dataType,levelType,levelValue)) {
        case GRIB_GRIB:
            return grib;
        case GRIB_CURRENT:
            return gribCurrent;
        default:
            return NULL;
    }

}

Grib ** DataManager::get_gribPtr(int gribType) {
    switch(gribType) {
        case GRIB_GRIB:
            return &grib;
        case GRIB_CURRENT:
            return &gribCurrent;
        default: // just in case ...
            qWarning() << "Bad grib type: " << gribType;
            return NULL;
    }
}

bool DataManager::isOk(int gribType) {
    if(gribType==GRIB_ANY) {
        Grib * gribPtr=get_grib(GRIB_GRIB);
        if(gribPtr)
            return gribPtr->isOk();
        else {
            gribPtr=get_grib(GRIB_CURRENT);
            if(gribPtr)
                return gribPtr->isOk();
            else
                return false;
        }
    }
    else {
        Grib * gribPtr=get_grib(gribType);
        if(gribPtr)
            return gribPtr->isOk();
        return false;
    }
}

bool DataManager::load_data(QString fileName,int gribType) {
    Grib ** gribPtr=get_gribPtr(gribType);
    if(gribPtr) {
        Grib * ptr=Grib::loadGrib(fileName,this);
        if(!ptr) {
            qWarning() << "Can't load file " << fileName;
            return false;
        }
        if(*gribPtr)
            delete *gribPtr;
        *gribPtr=ptr;
        update_dateList();
        update_levelMap();
        //print_firstRecord_info();
        return true;
    }
    return false;
}

void DataManager::close_data(int gribType) {
    Grib ** gribPtr=get_gribPtr(gribType);
    if(gribPtr) {
        delete *gribPtr;
        *gribPtr=NULL;
        update_dateList();
        update_levelMap();
    }
}

void DataManager::set_currentDate(time_t t) {
    if(t!=currentDate) {
        currentDate=t;
        if(grib) grib->init_isos(t);
        if(gribCurrent) gribCurrent->init_isos(t);
    }
}

void DataManager::update_dateList(void) {
    dateList.clear();
    minDate=-1;
    maxDate=-1;
    if(grib && grib->isOk()) grib->update_dateList(&dateList);
    if(gribCurrent && gribCurrent->isOk()) gribCurrent->update_dateList(&dateList);

    /* update min / max value */
    std::set<time_t>::iterator its;
    for (its=dateList.begin(); its!=dateList.end(); ++its) {
        if(minDate==-1) {
            minDate=*its;
            maxDate=*its;
        }
        else {
            if(minDate>*its)
                minDate=*its;
            if(maxDate<*its)
                maxDate=*its;
        }
    }

}

void DataManager::update_levelMap(void) {
    clear_levelMap();
    if(grib && grib->isOk()) grib->update_levelMap(&levelMap);
    if(gribCurrent && gribCurrent->isOk()) gribCurrent->update_levelMap(&levelMap);
}

void DataManager::clear_levelMap(void) {
    QMapIterator<int, QMap<int, QList<int>*> *> i(levelMap);
    while (i.hasNext()) {
        i.next();
        if(i.value()) {
            QMapIterator<int,QList<int>*> j(*i.value());
            while (j.hasNext()) {
                j.next();
                if(j.value()) delete j.value();
            }
            delete i.value();
        }
    }
    levelMap.clear();
}

QMap<int,QList<int>*> * DataManager::get_levelList(int dataType) {
    return levelMap.value(dataType,NULL);
}

QString DataManager::get_fileName(int gribType) {
    Grib * grb=get_grib(gribType);
    if(grb && grb->isOk()) {
        return grb->get_fileName();
    }
    return "";
}

QString DataManager::get_cartoucheData(void)
{
    if ((!grib || !grib->isOk()) && (!gribCurrent || !gribCurrent->isOk())) return QString();
    return Util::formatDateTimeLong(currentDate);
}

void DataManager::set_isoBarsStep(double step) {
    if(step!=isoBarsStep) {
        isoBarsStep=step;
        if(grib) grib->init_isoBars(currentDate);
        if(gribCurrent) gribCurrent->init_isoBars(currentDate);
    }
}

void DataManager::set_isoTherms0Step(int step) {
    if(step!=isoTherms0Step) {
        isoTherms0Step=step;
        if(grib) grib->init_isoTherms0(currentDate);
        if(gribCurrent) gribCurrent->init_isoTherms0(currentDate);
    }
}

bool DataManager::hasData(int dataType, int levelType, int levelValue,int gribType) {
    Grib * grb=get_grib(gribType);
    if(grb)
        return grb->hasData(dataType,levelType,levelValue);
    return false;
}

int DataManager::hasData(int dataType,int levelType, int levelValue) {
    if(grib && grib->isOk() && grib->hasData(dataType,levelType,levelValue))
        return GRIB_GRIB;
    else if((dataType==DATA_CURRENT_VX || dataType==DATA_CURRENT_VY)
            && gribCurrent && gribCurrent->isOk() && gribCurrent->hasData(dataType,levelType,levelValue))
        return GRIB_CURRENT;
    return GRIB_NONE;
}

bool DataManager::get_data1D(int dataType,int levelType, int levelValue,time_t now,time_t * tPrev,time_t * tNxt,
                             GribRecord ** recPrev,GribRecord ** recNxt) {
    if(grib && grib->isOk()) {
        if(grib->get_recordsAndTime_1D(dataType,levelType,levelValue,now,tPrev,tNxt,recPrev,recNxt))
            return true;
        else if(gribCurrent && gribCurrent->isOk())
            return gribCurrent->get_recordsAndTime_1D(dataType,levelType,levelValue,now,tPrev,tNxt,recPrev,recNxt);
    }
    return false;
}

bool DataManager::get_data2D(int dataType1,int dataType2,int levelType, int levelValue,
                             time_t now,time_t * tPrev,time_t * tNxt,
                             GribRecord ** recU1,GribRecord ** recV1,GribRecord ** recU2,GribRecord ** recV2) {
    if(grib && grib->isOk()) {
        if(grib->get_recordsAndTime_2D(dataType1,dataType2,levelType,levelValue,now,tPrev,tNxt,recU1,recV1,recU2,recV2))
            return true;
        else if(gribCurrent && gribCurrent->isOk())
            return gribCurrent->get_recordsAndTime_2D(dataType1,dataType2,levelType,levelValue,
                                                      now,tPrev,tNxt,recU1,recV1,recU2,recV2);
    }
    return false;
}

double DataManager::getInterpolatedValue_1D(int dataType,int levelType,int levelValue,
                               double d_long, double d_lat, time_t now) {
    double res;
    if(grib && grib->isOk()) {
        if(grib->getInterpolatedValue_1D(dataType,levelType,levelValue,d_long,d_lat,now,&res))
            return res;
        else if(gribCurrent && gribCurrent->isOk())
            if(gribCurrent->getInterpolatedValue_1D(dataType,levelType,levelValue,d_long,d_lat,now,&res))
                return res;
    }
    return false;
}

bool DataManager::getInterpolatedValue_2D(int dataType1,int dataType2,int levelType,int levelValue,
                                   double d_long, double d_lat, time_t now,double * u, double * v,
                                   int interpolation_type,bool UV,bool debug) {
    if(!u || !v) return false;

    if(forceWind && dataType1==DATA_WIND_VX) {
        *u=forcedTWS;
        *v=forcedTWD;
        return true;
    }

    if(forceCurrents && dataType1==DATA_CURRENT_VX) {
        *u=forcedCS;
        *v=forcedCD;
        return true;
    }

    if(interpolation_type == INTERPOLATION_UKN)
        interpolation_type=interpolationMode;

    if(grib && grib->isOk()) {
        if(grib->getInterpolatedValue_2D(dataType1,dataType2,levelType,levelValue,d_long,d_lat,now,u,v,interpolation_type,UV,debug))
            return true;
        else if(gribCurrent && gribCurrent->isOk())
            if(gribCurrent->getInterpolatedValue_2D(dataType1,dataType2,levelType,levelValue,d_long,d_lat,now,u,v,interpolation_type,UV,debug))
                return true;
    }
    return false;
}

bool DataManager::getInterpolatedWind(double d_long, double d_lat, time_t now,double * u, double * v,
                                      int interpolation_type,bool debug) {
    if(forceWind) {
        *u=forcedTWS;
        *v=forcedTWD;
        return true;
    }

    if(interpolation_type == INTERPOLATION_UKN)
        interpolation_type=interpolationMode;

    return getInterpolatedValue_2D(DATA_WIND_VX,DATA_WIND_VY,DATA_LV_ABOV_GND,10,
                                   d_long,d_lat,now,u,v,interpolation_type,true,debug);
}

bool DataManager::getInterpolatedCurrent(double d_long, double d_lat, time_t now,double * u, double * v,
                                      int interpolation_type,bool debug) {
    if(forceCurrents) {
        *u=forcedCS;
        *v=forcedCD;
        return true;
    }

    if(interpolation_type == INTERPOLATION_UKN)
        interpolation_type=interpolationMode;

    return getInterpolatedValue_2D(DATA_CURRENT_VX,DATA_CURRENT_VY,DATA_LV_MSL,0,
                                   d_long,d_lat,now,u,v,interpolation_type,true,debug);
}

bool DataManager::getZoneExtension (int gribType,double *x0,double *y0, double *x1,double *y1) {
    Grib * gribPtr = get_grib(gribType);
    if(gribPtr) {
        return gribPtr->getZoneExtension(x0,y0,x1,y1);
        return true;
    }
    return false;
}

void DataManager::load_forcedParam(void) {
    forceWind=Settings::getSetting("forceWind",0).toInt()==1;
    forcedTWS=Settings::getSetting("forcedTWS",0.0).toDouble();
    forcedTWD=Settings::getSetting("forcedTWD",0.0).toDouble();
    forceCurrents=Settings::getSetting("forceCurrents",0).toInt()==1;
    forcedCS=Settings::getSetting("forcedCS",0.0).toDouble();
    forcedCD=Settings::getSetting("forcedCD",0.0).toDouble();
}

void DataManager::print_firstRecord_bmap(void) {
    if(grib) {
        GribRecord * rec = grib->getFirstRecord();
        if(rec) {
            rec->print_bitmap();
            qWarning() << "bmapSize=" << rec->get_bmapSize() << ", dataSize=" << rec->get_dataSize();
        }
    }
}

void DataManager::print_firstRecord_info(void) {
    if(grib) {
        GribRecord * rec = grib->getFirstRecord();
        if(rec) {
            qWarning() << "edition=" << rec->get_editionNumber() << ", center=" << rec->get_idCenter()
                          << ", model=" << rec->get_idModel() << ", grid=" << rec->get_idGrid();
        }
    }
}
