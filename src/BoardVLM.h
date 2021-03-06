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

#ifndef VLM_BOARD_H
#define VLM_BOARD_H

#include <QApplication>
#include <QLabel>
#include <QHBoxLayout>
#include <QImage>
#include <QPainter>
#include <QMainWindow>

#include "class_list.h"
#include "inetClient.h"

#include "ui_BoardVLM.h"
#include <QAction>
#include "vlmPoint.h"

class boardVLM: public QWidget , public Ui::boardVLM_ui, public inetClient
{ Q_OBJECT
    public:
        boardVLM(MainWindow * mainWin, inetConnexion * inet, board * parent);       
        void boatUpdated(void);
        void setChangeStatus(bool);

        /* inetClient */
        void requestFinished(QByteArray res);
        QString getAuthLogin(bool * ok);
        QString getAuthPass(bool * ok);

        void setCompassVisible(bool status);
        void set_style(QPushButton * button, QColor color=QColor(230,230,230), QColor color2=Qt::white);

    public slots:
        void chgHeading();
        void headingUpdated(double heading);
        void chgAngle();
        void angleUpdated(double angle);
        void doSync();
        void doVirer();
        void doPilotOrtho();
        void doVmg();
        void doVbvmg();
        void doWP_edit();
        void disp_boatInfo();
        void synch_GPS();
        void confirmAndSendCmd(QString,QString,int cmdNum,double,double,double);
        void setWP(double lat,double lon,double wph=-1);

        void paramChanged(void);

        void edtSpinBox_key(void);
        void slot_hideShowCompass();
        void clearPilototo();
        void update_btnWP(void);
        void show_WPdialog(POI * poi);

    signals:
        void VLM_Sync(void);
        void POI_selectAborted(POI*);
        void showMessage(QString,int);

    protected:
        void keyPressEvent ( QKeyEvent * event );
        void contextMenuEvent(QContextMenuEvent  * event);
        bool eventFilter(QObject *obj, QEvent *event);
    private:
        MainWindow * mainWin;
        board * parent;

        boatVLM * currentBoat(void);

        int currentCmdNum;
        int nbRetry;
        bool isWaiting;

        QString cmd_val1,cmd_val2,cmd_val3;
        double computeWPdir(boatVLM * boat);

        void sendCmd(int cmdNum,double  val1,double val2, double val3);

        QTimer * GPS_timer;

        QString default_styleSheet;
        bool classicalButtons;

        /* Dialogs */
        DialogWp * wpDialog;
        bool confirmChange(QString question,QString info);

        /*GPS emul param*/
        QString COM;
        char chkSum(QString data);

        /* heading /angle user update */
        bool isComputing;

        /* contextual menu */
        QMenu *popup;
        QAction * ac_showHideCompass;
};



#endif
