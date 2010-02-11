// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "UiModule.h"
#include "UiProxyWidget.h"
#include "UiWidgetProperties.h"
#include "UiProxyStyle.h"
#include "UiStateMachine.h"

// Private managers
#include "Console/UiConsoleManager.h"
#include "Ether/EtherLogic.h"
#include "Ether/View/EtherScene.h"

#include "NetworkEvents.h"
#include "SceneEvents.h"
#include "ConsoleEvents.h"

#include <QApplication>

namespace UiServices
{
    UiModule::UiModule() 
        : Foundation::ModuleInterfaceImpl(Foundation::Module::MT_UiServices),
          event_query_categories_(QStringList()),
          ui_scene_manager_(0),
          ui_notification_manager_(0)
    {
    }

    UiModule::~UiModule()
    {
        SAFE_DELETE(ui_state_machine_);
        SAFE_DELETE(ui_scene_manager_);
        SAFE_DELETE(ui_notification_manager_);
    }

    /*************** ModuleInterfaceImpl ***************/

    void UiModule::Load()
    {
        QApplication::setStyle(new UiProxyStyle());
        event_query_categories_ << "Framework" << "Scene"  << "Console";
        LogInfo(Name() + " loaded.");
    }

    void UiModule::Unload()
    {
        LogInfo(Name() + " unloaded.");
    }

    void UiModule::Initialize()
    {
        ui_view_ = framework_->GetUIView();
        if (ui_view_)
        {
            LogDebug("Acquired Ogre QGraphicsView shared pointer from framework");

            ui_state_machine_ = new UiStateMachine(0, ui_view_);
            ui_state_machine_->RegisterScene("Inworld", ui_view_->scene());
            LogDebug("State Machine STARTED");

            ui_scene_manager_ = new UiSceneManager(GetFramework(), ui_view_);
            LogDebug("Scene Manager service READY");

            ui_notification_manager_ = new UiNotificationManager(GetFramework(), ui_view_);
            LogDebug("Notification Manager service READY");

            ui_console_manager_ = new CoreUi::UiConsoleManager(GetFramework(), ui_view_);
            LogDebug("Console UI READY");

            LogInfo(Name() + " initialized.");
        }
        else
            LogWarning("Could not accuire QGraphicsView shared pointer from framework, UiServices are disabled");
    }

    void UiModule::PostInitialize()
    {
        SubscribeToEventCategories();
        ui_console_manager_->SendInitializationReadyEvent();

        ether_logic_ = new Ether::Logic::EtherLogic(ui_view_);
        ui_state_machine_->RegisterScene("Ether", ether_logic_->GetScene());
        ether_logic_->Start();
        LogDebug("Ether Logic STARTED");
    }

    void UiModule::Uninitialize()
    {
        LogInfo(Name() + " uninitialized.");
    }

    void UiModule::Update(f64 frametime)
    {
    }

    bool UiModule::HandleEvent(event_category_id_t category_id, event_id_t event_id, Foundation::EventDataInterface* data)
    {
        QString category = service_category_identifiers_.keys().value(service_category_identifiers_.values().indexOf(category_id));
        if (category == "Framework")
        {
            switch (event_id)
            {
                case Foundation::NETWORKING_REGISTERED:
                    event_query_categories_ << "NetworkState";
                    SubscribeToEventCategories();
                    break;
                default:
                    break;
            }
        }
        else if (category == "NetworkState")
        {
            switch (event_id)
            {
                case ProtocolUtilities::Events::EVENT_SERVER_DISCONNECTED:
                    ui_scene_manager_->Disconnected();
                    break;
                case ProtocolUtilities::Events::EVENT_SERVER_CONNECTED:
                {
                    ProtocolUtilities::AuthenticationEventData *auth_data = dynamic_cast<ProtocolUtilities::AuthenticationEventData *>(data);
                    if (auth_data)
                    {
                        current_avatar_ = QString::fromStdString(auth_data->identityUrl);
                        current_server_ = QString::fromStdString(auth_data->hostUrl);
                    }
                    break;
                }
                default:
                    break;
            }
        }
        else if (category == "Console")
        {
            switch (event_id)
            {
            case Console::Events::EVENT_CONSOLE_TOGGLE:
                ui_console_manager_->ToggleConsole();
                break;
            case Console::Events::EVENT_CONSOLE_PRINT_LINE:
                Console::ConsoleEventData* console_data=dynamic_cast<Console::ConsoleEventData*>(data);
                ui_console_manager_->QueuePrintRequest(QString(console_data->message.c_str()));
                break;
            }
        }
        else if (category == "Scene")
        {
            switch (event_id)
            {
                case Scene::Events::EVENT_CONTROLLABLE_ENTITY:
                {
                    ui_scene_manager_->Connected();
                    QString welcome_message;
                    if (!current_avatar_.isEmpty())
                        welcome_message = current_avatar_ + " welcome to " + current_server_;
                    else
                        welcome_message = "Welcome to " + current_server_;
                    ui_notification_manager_->ShowInformationString(welcome_message, 10000);
                    break;
                }
                default:
                    break;
            }
        }

        return false;
    }

    void UiModule::SubscribeToEventCategories()
    {
        service_category_identifiers_.clear();
        foreach (QString category, event_query_categories_)
        {
            service_category_identifiers_[category] = framework_->GetEventManager()->QueryEventCategory(category.toStdString());
            LogDebug(QString("Listening to event category %1").arg(category).toStdString());
        }
    }

}

/************** Poco Module Loading System **************/

extern "C" void POCO_LIBRARY_API SetProfiler(Foundation::Profiler *profiler);
void SetProfiler(Foundation::Profiler *profiler)
{
    Foundation::ProfilerSection::SetProfiler(profiler);
}

using namespace UiServices;

POCO_BEGIN_MANIFEST(Foundation::ModuleInterface)
   POCO_EXPORT_CLASS(UiModule)
POCO_END_MANIFEST

