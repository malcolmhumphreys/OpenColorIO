/*
 */

#include <QtGui/QApplication>

#include "OCIOFormat.h"
#include "qt/OCIOImportDialog.h"

int DoImportOptions(OIIO::ImageSpec* spec, float* data,
                    RoleType* intputRole, RoleType* outputRole)
{
    int ret = 1;
    {
        // Setting Qt::AA_MacPluginApplication allows us to bypassing some
        // native menubar initialization that is usually not desired when
        // running Qt in a plugin.
        QApplication::setAttribute(Qt::AA_MacPluginApplication);
        
        int argc = 0;
        char *argv = 0;
        QApplication app(argc, &argv);
        
        OCIOImportDialog dialog(0, spec, data, intputRole, outputRole);
        dialog.show();
        if (QDialog::Accepted == dialog.exec()) ret = 0;
        dialog.close();
        
        *intputRole = dialog.m_inputRole;
        *outputRole = dialog.m_outputRole;
        
    }
    return ret;
}
