/**********************************************************************
zyGrib: meteorological GRIB file viewer
Copyright (C) 2008 - Jacques Zaninetti - http://www.zygrib.org

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

#include "GisReader.h"
#include "settings.h"
#include "zuFile.h"
#include "dataDef.h"
#include "Projection.h"
#include "Util.h"

//==========================================================
// GisReader
//==========================================================
GisReader::GisReader()
{
}
void GisReader::loadCountries()
{
    QString lang = Settings::getSetting("appLanguage", "none").toString();

    QString fname;
    bool ok1, ok2;
    char *buf;
    long szmax = 10000000;
    buf = new char[szmax];
    assert(buf);
    ZUFILE *f;

    //------------------------------------
    // Read countries file
    //------------------------------------
    QTime t;
    t.start();
    QString dir = Settings::getSetting("mapsFolder",appFolder.value("maps")).toString();
    fname = (lang == "fr") ?
            dir+"/gis/countries_fr.txt.gz" : dir+"/gis/countries_en.txt.gz";
    f = zu_open(qPrintable(fname), "rb");
    if (f != NULL) {
        long sz = zu_read(f, buf, szmax);
        QByteArray barr(buf, sz);
        QList<QByteArray> blist = barr.split('\n');
        for (int i=0; i < blist.size(); i++)
        {
            QByteArray bline = blist.at(i);
            QList<QByteArray> bwords = bline.split(';');
            if (bwords.size() == 4) {
                GisCountry *country = new GisCountry(
                            bwords.at(1),
                            bwords.at(3).toFloat(&ok1),
                            bwords.at(2).toFloat(&ok2)
                        );
                if (ok1 && ok2) {
                    assert(country);
                    lsCountries.push_back(country);
                }
                else
                    delete country;
            }
        }
        zu_close(f);
        //qWarning()<<"time to load countries"<<t.elapsed();
    }
    delete [] buf;
}
void GisReader::loadCities(const int &level)
{
    QTime t;
    t.start();
    QString fname;
    bool ok1, ok2, ok3;
    char *buf;
    long szmax = 10000000;
    buf = new char[szmax];
    assert(buf);
    ZUFILE *f;

    //------------------------------------
    // Read cities file
    //------------------------------------
    t.start();
    QString dir = Settings::getSetting("mapsFolder",appFolder.value("maps")).toString();
    fname = dir+"/gis/cities.txt.gz";
    f = zu_open(qPrintable(fname), "rb");
    if (f != NULL) {
        long sz = zu_read(f, buf, szmax);
        QByteArray barr(buf, sz);
       // barr.replace("\n","\r\n");
        QList<QByteArray> blist = barr.split('\n');
        for (int i=0; i < blist.size(); i++)
        {
            QByteArray bline = blist.at(i);
            QList<QByteArray> bwords = bline.split(';');
            if (bwords.size() == 5) {
                GisCity *city = new GisCity(
                            bwords.at(1),
                            bwords.at(2).toInt(&ok3),
                            bwords.at(4).toFloat(&ok1),
                            bwords.at(3).toFloat(&ok2)
                        );
                if (ok1 && ok2 && ok3 && (int)city->level<=level) {
                    assert(city);
                    lsCities.push_back(city);
                }
                else
                    delete city;
            }
        }
        zu_close(f);
        //qWarning()<<"time to load cities"<<t.elapsed();
    }
    delete [] buf;
}

//-----------------------------------------------------------------------
// Destructeur
GisReader::~GisReader() {
    clearLists();
}

//-----------------------------------------------------------------------
void GisReader::clearLists() {
    std::list<GisPoint*>::iterator itp;
    for (itp=lsCountries.begin(); itp != lsCountries.end(); ++itp) {
        delete *itp;
        *itp = NULL;
    }
    lsCountries.clear();

    std::list<GisCity*>::iterator it2;
    for (it2=lsCities.begin(); it2 != lsCities.end(); ++it2) {
        delete *it2;
        *it2 = NULL;
    }
    lsCities.clear();
}


//-----------------------------------------------------------------------
void GisPoint::draw(QPainter *, Projection *)
{
}
//-----------------------------------------------------------------------
void GisCountry::draw(QPainter *pnt, Projection *proj)
{
    int x0, y0;
    if (proj->isPointVisible(x,y)) {
        proj->map2screen(x, y, &x0, &y0);
        pnt->drawText(QRect(x0,y0,1,1), Qt::AlignCenter|Qt::TextDontClip, name);
    }
}

//-----------------------------------------------------------------------
void GisReader::drawCountriesNames(QPainter &pnt, Projection *proj)
{
    if(lsCountries.size()==0)
        loadCountries();
    pnt.setPen(QColor(120,100,60));
    pnt.setFont(QFont());
    pnt.setBackgroundMode(Qt::OpaqueMode);
    pnt.setBackground(QBrush(QColor(255,255,255,120)));
    std::list<GisPoint*>::iterator itp;
    for (itp=lsCountries.begin(); itp != lsCountries.end(); ++itp) {
        (*itp)->draw(&pnt, proj);
    }
    pnt.setBackgroundMode(Qt::TransparentMode);
}

//-----------------------------------------------------------------------
void GisCity::draw(QPainter *pnt, Projection *proj, int popmin)
{
    if (population < popmin)
        return;
    double x0, y0;
	if (proj->isPointVisible(x,y)) {
        proj->map2screenDouble(x, y, &x0, &y0);
        pnt->drawEllipse(x0-2,y0-2, 5,5);
        pnt->drawText(QRectF(x0,y0-7,1,1), Qt::AlignCenter|Qt::TextDontClip, name);
    }
}
//-----------------------------------------------------------------------
void GisCity::drawCityName (QPainter *pnt, QRect *rectName)
{
	// Warning: getRectName must be called first !!!
	pnt->drawEllipse(x0-2,y0-2, 5,5);
	pnt->drawText(*rectName, Qt::AlignCenter, name);
}
//-----------------------------------------------------------------------
void GisCity::getRectName (QPainter *pnt, Projection *proj, QRect *rectName)
{
	proj->map2screen(x, y, &x0, &y0);
    QFont font;
	pnt->setFont(font);
	QRect prect = QFontMetrics(font).boundingRect(name);
	int x1 = x0-prect.width()/2;
	int y1 = y0-prect.height();
	rectName->setX (x1);
	rectName->setY (y1);
        rectName->setWidth  (prect.width()*1.1);
        rectName->setHeight (prect.height()*1.1);
}
//-----------------------------------------------------------------------
bool compareCities_sup(GisCity *a, GisCity *b)
{
	return a->population > b->population;
}
//-----------------------------------------------------------------------
void GisReader::drawCitiesNames (QPainter &pnt, Projection *proj, int level)
{
    if(lsCities.size()==0)
        loadCities(level);
    pnt.setPen(QColor(40,40,40));
    pnt.setBrush(QColor(0,0,0));

	std::list<GisCity*>::iterator itp;
    std::list<GisCity*> lsVisibleCities;

	std::list<QRect*> lsZonesOccupees;
	std::list<QRect*>::iterator itz;

	for (itp=lsCities.begin(); itp != lsCities.end(); ++itp) {
		GisCity *p = *itp;
        if (proj->isPointVisible(p->x, p->y))
		{
			lsVisibleCities.push_back(p);
		}
    }

	// sort by population
	lsVisibleCities.sort(compareCities_sup);

	// draw if place is free
	bool freePlace;
    for (itp=lsVisibleCities.begin(); itp != lsVisibleCities.end(); ++itp) {
		GisCity *city = *itp;
		QRect *rect = new QRect();
		city->getRectName  (&pnt, proj, rect);
		freePlace = true;
		for (itz = lsZonesOccupees.begin();
					freePlace && itz != lsZonesOccupees.end(); ++itz) {
			QRect *pr = *itz;
			if (rect->intersects(*pr))
				freePlace = false;
		}
		if (freePlace) {
			city->drawCityName (&pnt, rect);
			lsZonesOccupees.push_back(rect);
		}
		else {
			delete rect;
		}
    }

	Util::cleanListPointers(lsZonesOccupees);
	lsVisibleCities.clear();
}
// //-----------------------------------------------------------------------
// void GisReader::drawCitiesNames(QPainter &pnt, Projection *proj, int level)
// {
//     int popmin = 100000;
//     switch (level) {
//         case 0:
//             return;
//         case 1:
//             popmin = 1000000; break;
//         case 2:
//             popmin = 200000; break;
//         case 3:
//             popmin = 50000; break;
//         case 4:
//             popmin = 0; break;
//     }
//     pnt.setPen(QColor(40,40,40));
//     pnt.setBrush(QColor(0,0,0));
//     pnt.setFont(Font::getFont(FONT_MapCity));
//     std::list<GisCity*>::iterator itp;
//     for (itp=lsCities.begin(); itp != lsCities.end(); itp++) {
//         (*itp)->draw(&pnt, proj, popmin);
//     }
// }


