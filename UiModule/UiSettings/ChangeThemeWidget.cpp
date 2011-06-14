// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "ChangeThemeWidget.h"
#include "UiDarkBlueStyle.h"
#include "UiModule.h"

#include <QApplication>
#include <QFontDatabase>
#include <QDebug>
#include <QStyleFactory>
#include <QSettings>

#include "MemoryLeakCheck.h"

namespace CoreUi
{
	ChangeThemeWidget::ChangeThemeWidget(Foundation::Framework *framework) : 
        QWidget(),
		framework_(framework)
    {
        setupUi(this);
        
        comboBox_changeTheme->addItem(QString::fromStdString("Naali dark blue"));
        comboBox_changeTheme->addItems(QStyleFactory::keys());
		
		QString theme;
		int theme_sel = 1;
		if (framework_->IsEditionless())
		{
			QSettings settings(QSettings::IniFormat, QSettings::UserScope, APPLICATION_NAME, "configuration/UiSettings");
			theme = settings.value("default_theme_used", QString("")).toString();
			theme_sel = settings.value("default_theme_index_used", 1).toInt();

		}
		else
		{
			QSettings settings(QSettings::IniFormat, QSettings::UserScope, APPLICATION_NAME, "configuration/UiPlayerSettings");
			theme = settings.value("default_theme_used", QString("")).toString();
			theme_sel = settings.value("default_theme_index_used", 1).toInt();
		}
		
		if (theme == "") {
			//Use the default one. Typically they include "windows", "motif", "cde", "plastique" and "cleanlooks"
			QApplication::setPalette(QApplication::style()->standardPalette());
			int i = 1;
			foreach (QString str, QStyleFactory::keys()) {
				if (str.toLower().replace(" ","") == QApplication::style()->objectName())
					comboBox_changeTheme->setCurrentIndex(i);
				i++;
			}
		}
		else
		{
			comboBox_changeTheme->setCurrentIndex(theme_sel);
			//Default theme
			if (theme.toLower() == "naali dark blue")
			{
				QApplication::setStyle(new UiServices::UiDarkBlueStyle());
				QFontDatabase::addApplicationFont("./media/fonts/FACB.TTF");
				QFontDatabase::addApplicationFont("./media/fonts/FACBK.TTF");
			} 
			else
			{
				QApplication::setStyle(QStyleFactory::create(theme));
			}
			QApplication::setPalette(QApplication::style()->standardPalette());
		}
        connect(comboBox_changeTheme, SIGNAL(currentIndexChanged(int)), SLOT(ChangeTheme()));
    }

    ChangeThemeWidget::~ChangeThemeWidget()
    {
    }

    void ChangeThemeWidget::ChangeTheme()
    {        
        QString theme = comboBox_changeTheme->currentText();

        //If theme not changed, return.
        if (!currentTheme.isEmpty() && theme == currentTheme)
            return;
        currentTheme = theme;

        //
		if (theme.toLower() == "naali dark blue") 
        {
			QApplication::setStyle(new UiServices::UiDarkBlueStyle());
            QFontDatabase::addApplicationFont("./media/fonts/FACEB.TTF");
        } 
        else
        {
            QFontDatabase::removeAllApplicationFonts();
            QApplication::setStyle(QStyleFactory::create(theme));
            QApplication::setPalette(QApplication::style()->standardPalette());
        }
        QApplication::setPalette(QApplication::style()->standardPalette());
		
		QSettings settings;
		if (framework_->IsEditionless())
		{
			QSettings settings(QSettings::IniFormat, QSettings::UserScope, APPLICATION_NAME, "configuration/UiSettings");
			settings.setValue("default_theme_used", theme.toLower());
			settings.setValue("default_theme_index_used", comboBox_changeTheme->currentIndex());
		}
		else
		{
			QSettings settings(QSettings::IniFormat, QSettings::UserScope, APPLICATION_NAME, "configuration/UiPlayerSettings");
			settings.setValue("default_theme_used", theme.toLower());
			settings.setValue("default_theme_index_used", comboBox_changeTheme->currentIndex());
		}
    }
}
