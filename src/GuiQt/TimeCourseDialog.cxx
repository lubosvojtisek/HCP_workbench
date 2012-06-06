/*LICENSE_START*/
/* 
 *  Copyright 1995-2011 Washington University School of Medicine 
 * 
 *  http://brainmap.wustl.edu 
 * 
 *  This file is part of CARET. 
 * 
 *  CARET is free software; you can redistribute it and/or modify 
 *  it under the terms of the GNU General Public License as published by 
 *  the Free Software Foundation; either version 2 of the License, or 
 *  (at your option) any later version. 
 * 
 *  CARET is distributed in the hope that it will be useful, 
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of 
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 *  GNU General Public License for more details. 
 * 
 *  You should have received a copy of the GNU General Public License 
 *  along with CARET; if not, write to the Free Software 
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 * 
 */ 
/*LICENSE_END*/

#include <algorithm>
#include "TimeCourseDialog.h"
#include "ui_TimeCourseDialog.h"

#include "TimeCoursePlotter.h"

using namespace caret;

TimeCourseDialog::TimeCourseDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TimeCourseDialog)
{
    ui->setupUi(this);
#ifdef CARET_OS_WINDOWS
    this->setWindowFlags(windowFlags() | Qt::CustomizeWindowHint | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);
#endif
    this->plot = new caret::TimeCoursePlotter();
    isEnabled = true;
    this->ui->TDMinTime->setEnabled(true);
    this->ui->TDMaxTime->setEnabled(true);
    this->ui->TDMinActivity->setEnabled(true);
    this->ui->TDMaxActivity->setEnabled(true);
    this->ui->TDMinTime->setMinimum(-1000000.0);
    this->ui->TDMaxTime->setMinimum(-1000000.0);
    this->ui->TDMinActivity->setMinimum(-1000000.0);
    this->ui->TDMaxActivity->setMinimum(-1000000.0);
    this->ui->TDMinTime->setMaximum(1000000.0);
    this->ui->TDMaxTime->setMaximum(1000000.0);
    this->ui->TDMinActivity->setMaximum(1000000.0);
    this->ui->TDMaxActivity->setMaximum(1000000.0);
    /*QObject::connect(this->plot, SIGNAL(timeStartValueChanged(double)),
                    this->ui->TDMinTime, SLOT(setValue(double)));
    QObject::connect(this->plot, SIGNAL(timeEndValueChanged(double)),
                    this->ui->TDMaxTime, SLOT(setValue(double)));*/
    QObject::connect(this->plot, SIGNAL(activityMinValueChanged(double)),
                    this, SLOT(plotActivityMinValueChanged(double)));
    QObject::connect(this->plot, SIGNAL(activityMaxValueChanged(double)),
                    this, SLOT(plotActivityMaxValueChanged(double)));
    QObject::connect(this->plot, SIGNAL(timeStartValueChanged(double)),
                    this, SLOT(plotTimeStartValueChanged(double)));
    QObject::connect(this->plot, SIGNAL(timeEndValueChanged(double)),
                    this, SLOT(plotTimeEndValueChanged(double)));    
    ui->verticalLayout_4->setContentsMargins(0,0,0,0);
    ui->verticalLayout_4->insertWidget(0,plot,100);    
}



TimeCourseDialog::~TimeCourseDialog()
{
    delete ui;
}

void TimeCourseDialog::addTimeLine(TimeLine &tl)
{    
    tlV.push_back(tl);
    //readjust x values to account for timestep
    for(int j = 0;j<tlV.last().x.size();j++)
    {
        tlV.last().x[j] = startOffset + tlV.last().timeStep*(double)j;
    }
}

void TimeCourseDialog::addTimeLines(QList <TimeLine> &tlVIn)
{
    for(int i = 0;i<tlVIn.size();i++)
    {
        this->addTimeLine(tlV[i]);
    }
}

void TimeCourseDialog::updateDialog(const bool &forceUpdate, const bool &forceDisableAutoScale)
{
    if(tlV.isEmpty() && !forceUpdate) return;
    if(!this->isEnabled) return;
    if(!(this->filename.length())) 
    {   
        this->filename = tlV[0].filename;
        AString temp = filename;
        if(temp.length() > 80) temp = "..." + temp.right(80);
        this->setWindowTitle("Time Course " + AString::number(tlV[0].clmID) + AString(" - ") + temp);
    }
    plot->detachItems();
    plot->populate(tlV,forceDisableAutoScale);    
    this->populateHistory();
    this->setVisible(true);
    this->show();
    this->activateWindow();
    plot->update();
    plot->replot();
    plot->setFocus();
    this->setAttribute(Qt::WA_NoMousePropagation,true);    
}

void TimeCourseDialog::populateHistory()
{
    if(tlV.isEmpty()) return;
    this->ui->TDCoordsText->clear();
    for(int i =0;i<tlV.size();i++)
    { 
        
        this->ui->TDCoordsText->setTextColor(plot->colors.getColor(tlV[i].colorID));
        this->ui->TDCoordsText->append(tlV[i].label);
        this->ui->TDCoordsText->show();
    }
}

void TimeCourseDialog::setAnimationStartTime(const double &time)
{
    startOffset = time;
    if(tlV.isEmpty()) return;
    
    for(int i =0;i<tlV.size();i++)
    {        
        for(int j = 0;j<tlV[i].x.size();j++)
        {
            tlV[i].x[j] = startOffset + tlV[i].timeStep*(double)j;
        }
    }
    plot->detachItems();
    plot->populate(tlV,true);
    plot->update();
    plot->replot();    
}




void TimeCourseDialog::on_TDClose_clicked()
{
    this->hide();
}

void TimeCourseDialog::on_TDClearChart_clicked()
{
    this->plot->clear(tlV);
    tlV.clear();
    this->ui->TDCoordsText->clear();
    this->updateDialog(true);
}

void TimeCourseDialog::on_TDMinTime_valueChanged(double arg1)
{
    QwtScaleDiv *sd = this->plot->axisScaleDiv(QwtPlot::xBottom);
    plot->setAxisScale(QwtPlot::xBottom,arg1,sd->upperBound());
    plot->QwtPlot::replot();
}

void TimeCourseDialog::on_TDMaxTime_valueChanged(double arg1)
{
    QwtScaleDiv *sd = this->plot->axisScaleDiv(QwtPlot::xBottom);
    plot->setAxisScale(QwtPlot::xBottom,sd->lowerBound(),arg1);
    plot->QwtPlot::replot();
}

void TimeCourseDialog::on_TDMinActivity_valueChanged(double arg1)
{
    QwtScaleDiv *sd = this->plot->axisScaleDiv(QwtPlot::yLeft);
    plot->setAxisScale(QwtPlot::yLeft,arg1,sd->upperBound());
    plot->QwtPlot::replot();
}

void TimeCourseDialog::on_TDMaxActivity_valueChanged(double arg1)
{
    QwtScaleDiv *sd = this->plot->axisScaleDiv(QwtPlot::yLeft);
    plot->setAxisScale(QwtPlot::yLeft,sd->lowerBound(),arg1);
    plot->QwtPlot::replot();
}

void TimeCourseDialog::plotTimeEndValueChanged(double time)
{
    this->ui->TDMaxTime->blockSignals(true);
    this->ui->TDMaxTime->setValue(time);
    this->ui->TDMaxTime->blockSignals(false);

}
void TimeCourseDialog::plotTimeStartValueChanged(double time)
{
    this->ui->TDMinTime->blockSignals(true);
    this->ui->TDMinTime->setValue(time);
    this->ui->TDMinTime->blockSignals(false);
}
void TimeCourseDialog::plotActivityMaxValueChanged(double time)
{
    this->ui->TDMaxActivity->blockSignals(true);
    this->ui->TDMaxActivity->setValue(time);
    this->ui->TDMaxActivity->blockSignals(false);
}
void TimeCourseDialog::plotActivityMinValueChanged(double time)
{
    this->ui->TDMinActivity->blockSignals(true);
    this->ui->TDMinActivity->setValue(time);
    this->ui->TDMinActivity->blockSignals(false);
}
void TimeCourseDialog::on_TDShowAverage_toggled(bool checked)
{
    this->plot->setDisplayAverage(checked);
    this->updateDialog(true,true);

}

void TimeCourseDialog::setTimeSeriesGraphEnabled(bool enabled)
{
    this->isEnabled = enabled;
}


void TimeCourseDialog::updateExtents()
{
    double tmin, tmax, amin, amax;
    this->plot->getTimeExtents(tmin,tmax);
    this->plot->getActivityExtents(amin, amax);
    this->ui->TDMinActivity->setMinimum(amin);
    this->ui->TDMinActivity->setMaximum(amax - 1.0);
    this->ui->TDMinTime->setMinimum(tmin);
    this->ui->TDMinTime->setMaximum(tmax - 1.0);
    this->ui->TDMaxActivity->setMinimum(amin + 1.0);
    this->ui->TDMaxActivity->setMaximum(amax);
    this->ui->TDMaxTime->setMinimum(tmin + 1.0);
    this->ui->TDMaxTime->setMaximum(tmax);    
}




void TimeCourseDialog::on_TDKeepLast_valueChanged(int arg1)
{
    this->plot->setMaxTimeLines(arg1);
    this->updateDialog(true,true);
}

void TimeCourseDialog::on_zoomXCheckBox_toggled(bool checked)
{
    this->plot->magnifier->setAxisEnabled(QwtPlot::xBottom,checked);
    this->plot->magnifier->setAxisEnabled(QwtPlot::xTop,checked);
}

void TimeCourseDialog::on_zoomYCheckBox_toggled(bool checked)
{
    this->plot->magnifier->setAxisEnabled(QwtPlot::yLeft,checked);
    this->plot->magnifier->setAxisEnabled(QwtPlot::yRight,checked);
}

void TimeCourseDialog::on_lineWidthSpinBox_valueChanged(int arg1)
{
    this->plot->setTimeLineWidth(arg1);
    this->updateDialog(true,true);
}

void TimeCourseDialog::on_autoFitTimeLinesCheckBox_toggled(bool checked)
{
    this->plot->setAutoScaleEnabled(checked);
    this->updateDialog(true);
}

void TimeCourseDialog::on_resetViewButton_clicked()
{
    this->plot->resetView();
    this->updateDialog(true);
}
