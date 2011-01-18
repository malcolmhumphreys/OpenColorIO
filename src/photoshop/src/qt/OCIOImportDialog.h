/*
 */

#ifndef OCIOIMPORTDIALOG_H
#define OCIOIMPORTDIALOG_H

#include <QtGui/QDialog>

#include "OCIOFormat.h"
#include "ui_OCIOImportDialog.h"

class OCIOImportDialog : public QDialog, private Ui::OCIOImportDialog
{
    Q_OBJECT
    
public:
    OCIOImportDialog(QWidget *parent = 0, OIIO::ImageSpec* spec = NULL,
                     float* data = NULL, RoleType* in_inputRole = NULL,
                     RoleType* in_outputRole = NULL);
    
    void setInputRole(RoleType rtype);
    void setOutputRole(RoleType rtype);
    
    RoleType m_inputRole;
    RoleType m_outputRole;
    
private slots:
    void on_inputRole_currentIndexChanged(QString item);
    void on_outputRole_currentIndexChanged(QString item);

private:
    bool m_init_gl;
    
};

#endif // OCIOIMPORTDIALOG_H
