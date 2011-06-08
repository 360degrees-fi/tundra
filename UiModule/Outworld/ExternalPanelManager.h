//$ HEADER_NEW_FILE $
// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_UiModule_ExternalPanelManager_h
#define incl_UiModule_ExternalPanelManager_h

#include "UiModuleApi.h"
#include "ExternalMenuManager.h"
#include "UiWidget.h"

#include <QObject>
#include <QList>
#include <QRectF>
#include <QMap>
#include <QString>
#include <QDockWidget>
#include <QString>

#include <QMainWindow>
#include <QDockWidget>


namespace UiServices
{
    class UI_MODULE_API ExternalPanelManager : public QObject
    {
        Q_OBJECT

    public:
        /*! Constructor.
         *	\param qWin MainWindow pointer.
		 */
		ExternalPanelManager(QMainWindow *qWin, UiModule *owner);

        //! Destructor.
        ~ExternalPanelManager();

		//! Internal list of proxy widgets in scene.
		QList<QDockWidget*> all_qdockwidgets_in_window_;

    public slots:
        /*! \brief	Adds widget to the main Window in a QDockWidget.
         *
         *  \param  widget Widget.
		 *	\param	title Title of the Widget
         *  \param  flags Window flags. Qt::Dialog is used as default		 
         *         
         *  \return widget of the added widget (is a QDockWidget).
         */
		QDockWidget* AddExternalPanel(UiWidget *widget, QString title, Qt::WindowFlags flags = Qt::Dialog);

        //! Adds a already created QDockWidget into the main window.
        /*! Please prefer using AddExternalWidget() with normal QWidget and properties instead of this directly.
         *  \param widget QDockWidget.
         */
        bool AddQDockWidget(QDockWidget *widget);

        /*! Remove a widget from main window if it exist there
         *  Used for removing your widget from main window. The show/hide toggle button will also be removed from the main menu.
         *  \param widget Proxy widget.
         */
        bool RemoveExternalPanel(QDockWidget *widget);

		/*! Shows the widget's DockWidget in the main window.
         *  \param widget Widget.
         */
		void ShowWidget(QWidget *widget);

		/*! Hides the widget's DockWidget in the main window.
         *  \param widget Widget.
         */
		void HideWidget(QWidget *widget);

		/*!Disable all the dockwidgets in the qmainwindow
		 * 
		 */
		void DisableDockWidgets();

		/*!Enable all the dockwidgets in the qmainwindow
		 * 
		 */
		void EnableDockWidgets();

		/*! Returns the QDockWidget where the widget with the name widget is in the QMainWindow. Used (at least) to use WorldBuildingModule with this module.
         *  \param widget Name of the widget.
         */
		QDockWidget* GetExternalMenuPanel(QString &widget);

		void restoreWidget(QDockWidget *widget);

    private slots:
        void ModifyPanelVisibility(bool vis);
        void DockVisibilityChanged(bool vis);
		
    private:

        //! Pointer to main QMainWindow
        QMainWindow *qWin_;

		//Pointer to owner
		UiModule *owner_;

        QMap<QString, bool> controller_panels_visibility_;
    };
}

#endif // incl_UiModule_ExternalPanelManager_h
