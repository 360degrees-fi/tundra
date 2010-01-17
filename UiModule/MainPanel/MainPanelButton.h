// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_UiModule_MainPanelButton_h
#define incl_UiModule_MainPanelButton_h

#include <QPushButton>
#include <QWidget>
#include <QString>

namespace UiServices
{
    class UiProxyWidget;
}

namespace CoreUi
{
    class MainPanelButton : public QPushButton
    {
        Q_OBJECT

    public:
        MainPanelButton(QWidget *parent, UiServices::UiProxyWidget *widget, const QString &widget_name);
        virtual ~MainPanelButton();

    public slots:
        void ToggleShow();
        void ControlledWidgetHidden();
    
    private:
        QString widget_name_;
        UiServices::UiProxyWidget *controlled_widget_;
    };
}

#endif //incl_UiModule_MainPanelButton_h
