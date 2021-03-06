/**********************************************************************
qtVlm: Virtual Loup de mer GUI
Copyright (C) 2010 - Christophe Thomas aka Oxygen77

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
#include <QDateTime>

#include "DialogGribValidation.h"
#include "MainWindow.h"
#include "mycentralwidget.h"
#include "Util.h"
#include "dataDef.h"
#include "settings.h"
#include "DataManager.h"

#ifdef __QTVLM_WITH_TEST
extern int nbWarning;
#endif
DialogGribValidation::DialogGribValidation(myCentralWidget * my_centralWidget,MainWindow * mainWindow) :  QDialog(my_centralWidget)
{
    setupUi(this);
    Util::setFontDialog(this);
    this->my_centralWidget=my_centralWidget;
    this->mainWindow=mainWindow;
    dataManager=my_centralWidget->get_dataManager();
    if(dataManager && dataManager->isOk())
        tstamp->setText(QString().setNum(dataManager->get_currentDate()));
    else
        tstamp->setText(QString().setNum(QDateTime::currentDateTime().toTime_t()));
    this->label_date->setText(QDateTime().fromTime_t(tstamp->text().toInt()).toUTC().toString("dd MMM-hh:mm:ss"));
    latitude->setText("0");
    longitude->setText("0");
    this->latitude->blockSignals(true);
#ifdef __QTVLM_WITH_TEST
    nbWarning=0;
#endif
}

DialogGribValidation::~DialogGribValidation()
{
    Settings::setSetting(this->objectName()+".height",this->height());
    Settings::setSetting(this->objectName()+".width",this->width());
}

void DialogGribValidation::setMode(int mode)
{
    type->setCurrentIndex(mode-1);
    curMode=mode;
}

void DialogGribValidation::done(int result)
{
    int newMode;
    if(result == QDialog::Accepted)
        newMode=this->type->currentIndex()+1;
    else
        newMode=curMode;

    if(dataManager)
    {
        dataManager->set_interpolationMode(newMode);
        qWarning() << "Setting interpolation mode to " << newMode;
    }

    hide();
    this->my_centralWidget->send_redrawAll();
    //QDialog::done(result);
}

void DialogGribValidation::doNow(void)
{
    tstamp->setText(QString().setNum(QDateTime::currentDateTime().toUTC().toTime_t()));
}

void DialogGribValidation::interpolationChanged(int newMode)
{
    if(dataManager)
    {
        newMode++;
        dataManager->set_interpolationMode(newMode);
        qWarning() << "Setting interpolation mode to " << newMode;
    }

    this->my_centralWidget->send_redrawAll();
    inputChanged();
}

void DialogGribValidation::inputChanged(void)
{
#ifdef __QTVLM_WITH_TEST
    nbWarning=0;
#endif
    /* recompute interpolation */
    double lat,lon;
    int tstamp;
    bool ok;
    double vit,ang;

    lat=this->latitude->text().toDouble(&ok);
    if(!ok)
    {
        this->vitesse->setText("Err lat");
        return;
    }
    lon=this->longitude->text().toDouble(&ok);
    if(!ok)
    {
        this->vitesse->setText("Err lon");
        return;
    }
    tstamp=this->tstamp->text().toInt(&ok);
    if(!ok)
    {
        this->vitesse->setText("Err tstamp");
        this->label_date->setText("Err tstamp");
        return;
    }
    this->label_date->setText(QDateTime().fromTime_t(this->tstamp->text().toInt()).toUTC().toString("dd MMM-hh:mm:ss"));
    //qWarning() << "Get new param: " << lat << "," << lon << " - " << tstamp;

    if(!dataManager || !dataManager->isOk())
    {
        this->vitesse->setText("No grib");
        return;
    }

    ok=dataManager->getInterpolatedValue_2D(DATA_WIND_VX,DATA_WIND_VY,DATA_LV_ABOV_GND,10,lon,lat,tstamp,&vit,&ang,
                                                      type->currentIndex()+1,true,this->chk_debug->checkState()==Qt::Checked);
    //qWarning() << "Interpolation: v=" << vit << ", ang=" << ang << "(ok=" << ok << ")";

    vitesse->setText(QString().setNum(vit));
    angle->setText(QString().setNum(radToDeg(ang)));

}
