//$ HEADER_MOD_FILE $
// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "CommunicationWidget.h"
#include "CommunicationsService.h"
#include "UiProxyWidget.h"
#include "UiModule.h"
#include "ModuleManager.h"
#include "VoiceUsersWidget.h"
#include "UiServiceInterface.h"
#include "VoiceControllerWidget.h"

#include <QWidget>
#include <QStackedLayout>
#include <QTimer>
#include <QGraphicsSceneMouseEvent>
#include <QApplication>
#include <QGraphicsScene>
#include <QTextBrowser>
#include <QSettings>

#include "VoiceToolWidget.h"

#include "DebugOperatorNew.h"


//$ BEGIN_MOD $
#include "TtsServiceInterface.h"
#include "TtsModule.h"


#include "Entity.h"
#include "ServiceInterface.h"
#include "WorldLogicInterface.h"

#include "RexUUID.h"
#include "SceneManager.h"
#include "EC_OpenSimPresence.h"

//$ END_MOD $
namespace
{
    /// HTTP schema indentifier
    const QString &cHttpSchema = "http://";

    /// HTTP schema indentifier
    const QString &cHttpsSchema = "https://";

    /// Hyperlink start tag
    const QString &cLinkStartTag = "<a href=\"";

    /// Hyperlink middle tag
    const QString &cLinkMiddleTag= "\">";

    /// Hyperlink end tag
    const QString &cLinkEndTag= "</a>";

    /// Finds valid hyperlinks in message and generates HTML tags for them
    /// @param message Message to be parsed
    /// @param indentifier Schema indentifier e.g. "http://"
    void GenerateHyperlinks(QString &message, const QString &indentifier)
    {
        QString link;
        int startIndex = 0, endIndex = 0;
        int hyperlinkCount = message.count(indentifier);
        while (hyperlinkCount > 0)
        {
            startIndex = message.indexOf(indentifier, endIndex);
            assert(startIndex != -1);

            endIndex = message.indexOf(' ', startIndex);
            endIndex = endIndex > -1 ? endIndex : message.length();
            assert(endIndex > startIndex);

            link = message.mid(startIndex, endIndex - startIndex);

            message.insert(endIndex, cLinkEndTag);
            message.insert(startIndex, cLinkMiddleTag);
            message.insert(startIndex, link);
            message.insert(startIndex, cLinkStartTag);

            endIndex += link.length();
            --hyperlinkCount;
        }
    }
}

namespace CoreUi
{
    CommunicationWidget::CommunicationWidget(Foundation::Framework* framework) :
        framework_(framework),
        QGraphicsProxyWidget(),
        internal_widget_(new QWidget()),
        im_proxy_(0),
        viewmode_(Normal),
        resizing_horizontal_(false),
        resizing_vertical_(false),
        in_world_chat_session_(0),
        voice_tool_(0),
        tts_other_messages_(false),
        tts_own_messages_(false),
        default_avatar_tts_voice_("")
    {
        Initialise();
        ChangeView(viewmode_);
    }

    // Private

    void CommunicationWidget::Initialise()
    {
        setupUi(internal_widget_);
        setWidget(internal_widget_);

        // Hide IM button by default, leave the container so we dont get ugly clipping. 
        // Shown button when UpdateImWidget() is called with a valid widget.
        imButton->hide();

        // Stacked layout
        stacked_layout_ = new QStackedLayout();
        stacked_layout_->setMargin(0);
        contentContainerLayout->addLayout(stacked_layout_);

        // History view mode
        history_view_text_edit_ = new QTextBrowser(chatContentWidget);
        history_view_text_edit_->setOpenExternalLinks(true);
        history_view_text_edit_->setObjectName("historyViewTextEdit");
        history_view_text_edit_->setStyleSheet(
            "QTextBrowser#historyViewTextEdit {"
                "background-color: rgba(34,34,34,191);"
                "border-radius: 7px; border: 1px solid rgba(255,255,255,50);"
            "}");
        history_view_text_edit_->setFont(QFont("Calibri", 11));
        stacked_layout_->addWidget(history_view_text_edit_);

        // Slim view mode
        normal_view_widget_ = new NormalChatViewWidget(chatContentWidget);
        stacked_layout_->addWidget(normal_view_widget_);

        stacked_layout_->setCurrentWidget(normal_view_widget_);

        connect(viewModeButton, SIGNAL( clicked() ), SLOT( ChangeViewPressed() ));
        connect(imButton, SIGNAL( clicked() ), SLOT( ToggleImWidget() ));
        connect(chatLineEdit, SIGNAL( returnPressed() ), SLOT( SendMessageRequested() ));

        tool_manager_ = new ToolManagerWidget();
        this->voiceLayoutH->addWidget(tool_manager_);
        tool_manager_->show();
        if (framework_ &&  framework_->GetServiceManager())
        {
            Communications::ServiceInterface *comm = framework_->GetService<Communications::ServiceInterface>();
            if (comm)
            {
                connect(comm, SIGNAL(InWorldVoiceAvailable()), SLOT(InitializeInWorldVoice()) );
                connect(comm, SIGNAL(InWorldVoiceUnavailable()), SLOT(UninitializeInWorldVoice()) );
                connect(comm, SIGNAL(InWorldChatAvailable()), SLOT(InitializeInWorldChat()) );
                connect(comm, SIGNAL(InWorldChatUnavailable()), SLOT(InitializeInWorldChat()) );
			}
        }
    }

    void CommunicationWidget::ChangeViewPressed()
    {
        switch (viewmode_)
        {
            case Normal:
                ChangeView(History);
                break;
            case History:
                ChangeView(Normal);
                break;
        }
    }

    void CommunicationWidget::ChangeView(ViewMode new_mode)
    {
        viewmode_ = new_mode; 
        switch (viewmode_)
        {
            case Normal:
                chatContentWidget->setStyleSheet("QWidget#chatContentWidget { background-color: transparent; border-top-left-radius: 0px; border-top-right-radius: 0px; }");
                viewModeButton->setStyleSheet("QPushButton#viewModeButton { background-image: url('./data/ui/images/chat/uibutton_HISTORY_normal.png'); }");
                stacked_layout_->setCurrentWidget(normal_view_widget_);
                break;
            case History:
                chatContentWidget->setStyleSheet("QWidget#chatContentWidget { background-color: rgba(34,34,34,191); border-top-left-radius: 7px; border-top-right-radius: 7px; }");
                viewModeButton->setStyleSheet("QPushButton#viewModeButton { background-image: url('./data/ui/images/chat/uibutton_HISTORY_click.png'); }");
                stacked_layout_->setCurrentWidget(history_view_text_edit_);
                break;
        }
    }
////$ BEGIN_MOD $
//	void CommunicationWidget::ToggleTtsChatWidget()
//    {
//		if(Tts_chat_widget)
//		{
//		   if(!tts_proxy_->isVisible())
//		   {
//			   tts_proxy_->show();
//		        // \todo Find a proper solution to the problem
//                // IM widget doesn't get input without main frame resisizing for unknow reason.
//                // HACK begin
//                tts_proxy_->moveBy(1,1);
//                tts_proxy_->moveBy(-1,-1);
//                // HACK end
//		   }
//		   else
//			   tts_proxy_->AnimatedHide();
//		}
//   }
////$ END_MOD $
    void CommunicationWidget::ToggleImWidget()
    {
        if (im_proxy_)
        {
            if (!im_proxy_->isVisible())
            {
                im_proxy_->show();
                // \todo Find a proper solution to the problem
                // IM widget doesn't get input without main frame resisizing for unknow reason.
                // HACK begin
                im_proxy_->moveBy(1,1);
                im_proxy_->moveBy(-1,-1);
                // HACK end
            }
            else
                im_proxy_->AnimatedHide();
        }
    }

    void CommunicationWidget::ShowIncomingMessage(bool self_sent_message, QString sender, QString timestamp, QString message)
    {
        // History view
        timestamp = timestamp.midRef(timestamp.indexOf(" ")+1).toString(); // Cut the fat from timestamp for now
        QString htmlcontent("<span style='color:grey'>[");
        htmlcontent.append(timestamp);
        if (!self_sent_message)
            htmlcontent.append("]</span> <span style='color:#0099FF;'>");
        else
            htmlcontent.append("]</span> <span style='color:#FF3330;'>");
        htmlcontent.append(sender);
        htmlcontent.append(": </span><span style='color:#EFEFEF;'>");

        // If the message contains hyperlinks, make HTML tags for them.
        if (message.contains(cHttpSchema))
            GenerateHyperlinks(message, cHttpSchema);
        if (message.contains(cHttpsSchema))
            GenerateHyperlinks(message, cHttpsSchema);

        htmlcontent.append(message);
        htmlcontent.append("</span>");

        history_view_text_edit_->append(htmlcontent);

        // Normal view
        if (!self_sent_message)
            normal_view_widget_->ShowChatMessage(self_sent_message, QString("%1: %2").arg(sender, message));
        else
            normal_view_widget_->ShowChatMessage(self_sent_message, QString("Me: %1").arg(message));
    }

    void CommunicationWidget::SendMessageRequested()
    {
        if (chatLineEdit->text().isEmpty())
            return;

        QString message = chatLineEdit->text();
        chatLineEdit->clear();
        if (in_world_chat_session_)
            in_world_chat_session_->SendTextMessage(message);
    }

	void CommunicationWidget::SpeakIncomingMessage(const Communications::InWorldChat::TextMessageInterface &message, const QString& from_uuid)
	{
        /// @todo signal when settings are changed
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, APPLICATION_NAME, "configuration/Tts");
        tts_own_messages_ = settings.value("Tts/play_own_chat_messages", false).toBool();
        tts_other_messages_ = settings.value("Tts/play_other_chat_messages", false).toBool();
        own_tts_voice_ = settings.value("Tts/own_voice", "").toString();
        default_avatar_tts_voice_ = settings.value("Tts/other_default_voice", "").toString();

        if (message.IsOwnMessage())
        {
            if (tts_own_messages_)
            {
                tts_service_->Text2Speech(message.Text(), own_tts_voice_);
            }
        }
        else
        {
            if (tts_other_messages_)
            {
                /// @todo check if there is EC_TTSAgent for this avatar!
                tts_service_->Text2Speech(message.Text(), default_avatar_tts_voice_);
            }
        }
	}

	/*void CommunicationWidget::GetAvatarVoiceComponent()
	{
		// Pick up the EC_TtsVoice of the avatar entity
		boost::shared_ptr<Foundation::WorldLogicInterface> world_logic = framework_->GetServiceManager()->GetService<Foundation::WorldLogicInterface>(Foundation::Service::ST_WorldLogic).lock();
			if(world_logic)
			{
				Scene::EntityPtr user_avatar = world_logic->GetUserAvatarEntity();
				if(user_avatar)
					avatar_voice_=user_avatar->GetComponent<EC_TtsVoice>();
			}
	}

	void CommunicationWidget::UpdateAvatarVoice(Tts::Voice voice)
	{
		if(!avatar_voice_)
				GetAvatarVoiceComponent();
		avatar_voice_->SetMyVoice(voice);
	}*/

    void CommunicationWidget::hoverMoveEvent(QGraphicsSceneHoverEvent *mouse_hover_move_event)
    {
        if (stacked_layout_->currentWidget() == history_view_text_edit_)
        {
            qreal widget_press_pos_x = chatContentWidget->width() - mouse_hover_move_event->scenePos().x();
            qreal widget_press_pos_y = scene()->sceneRect().size().height() - mouse_hover_move_event->scenePos().y() - rect().height();
            if (widget_press_pos_x >= 0 && widget_press_pos_x <= 6)
            {
                if (!QApplication::overrideCursor())
                    QApplication::setOverrideCursor(QCursor(Qt::SizeHorCursor));
            }
            else if (widget_press_pos_y <= 0 && widget_press_pos_y >= -6 && widget_press_pos_x >= 0)
            {
                if (!QApplication::overrideCursor())
                    QApplication::setOverrideCursor(QCursor(Qt::SizeVerCursor));
            }
            else if (QApplication::overrideCursor())
                QApplication::restoreOverrideCursor();
        }
        else if (QApplication::overrideCursor())
            QApplication::restoreOverrideCursor();

        QGraphicsProxyWidget::hoverMoveEvent(mouse_hover_move_event);
    }

    void CommunicationWidget::hoverLeaveEvent(QGraphicsSceneHoverEvent *mouse_hover_leave_event)
    {
        if (QApplication::overrideCursor())
            QApplication::restoreOverrideCursor();
        QGraphicsProxyWidget::hoverLeaveEvent(mouse_hover_leave_event);
    }

    void CommunicationWidget::mousePressEvent(QGraphicsSceneMouseEvent *mouse_press_event)
    {   
        QWidget *w = widget()->childAt(mouse_press_event->pos().toPoint());
        if (w != chatContentWidget)
            scene()->clearFocus();

        resizing_horizontal_ = false;
        resizing_vertical_ = false;
        if (stacked_layout_->currentWidget() == history_view_text_edit_)
        {
            qreal widget_press_pos_x = chatContentWidget->width() - mouse_press_event->scenePos().x();
            qreal widget_press_pos_y = scene()->sceneRect().size().height() - mouse_press_event->scenePos().y() - rect().height();
            if (widget_press_pos_x >= 0 && widget_press_pos_x <= 6)
            {
                mouse_press_event->accept();
                QApplication::setOverrideCursor(QCursor(Qt::SizeHorCursor));
                resizing_horizontal_ = true;
                return;
            }
            else if (widget_press_pos_y <= 0 && widget_press_pos_y >= -6 && widget_press_pos_x >= 0)
            {
                mouse_press_event->accept();
                QApplication::setOverrideCursor(QCursor(Qt::SizeVerCursor));
                resizing_vertical_ = true;
                return;
            }
        }
        QGraphicsProxyWidget::mousePressEvent(mouse_press_event);
    }

    void CommunicationWidget::mouseMoveEvent(QGraphicsSceneMouseEvent *mouse_move_event)
    {
        if (resizing_horizontal_)
            chatContentWidget->setMinimumWidth(mouse_move_event->scenePos().x());
        else if (resizing_vertical_)
            chatContentWidget->setMinimumHeight(scene()->sceneRect().size().height() - mouse_move_event->scenePos().y() - chatControlsWidget->height());
        QGraphicsProxyWidget::mouseMoveEvent(mouse_move_event);
    }

    void CommunicationWidget::mouseReleaseEvent(QGraphicsSceneMouseEvent *mouse_release_event)
    {
        if (resizing_horizontal_ || resizing_vertical_)
        {
            resizing_horizontal_ = false;
            resizing_vertical_ = false;
            QApplication::restoreOverrideCursor();
        }
        QGraphicsProxyWidget::mouseReleaseEvent(mouse_release_event);
    }

    void CommunicationWidget::UpdateImWidget(UiProxyWidget *im_proxy)
    {
        im_proxy_ = im_proxy;
        if (im_proxy_)
            imButton->show();
    }

    void CommunicationWidget::SetFocusToChat()
    {
        chatLineEdit->setFocus(Qt::MouseFocusReason);
    }

    void CommunicationWidget::InitializeInWorldChat()
    {
        if (framework_ &&  framework_->GetServiceManager())
        {
            Communications::ServiceInterface *comm = framework_->GetService<Communications::ServiceInterface>();
            if (comm)
            {
                if (in_world_chat_session_)
                {
                    disconnect(in_world_chat_session_);
                    in_world_chat_session_ = 0;
                    history_view_text_edit_->clear();
                }

				in_world_chat_session_ = comm->InWorldChatSession();
                if (!in_world_chat_session_)
                    return;
                connect(in_world_chat_session_, SIGNAL(TextMessageReceived(const Communications::InWorldChat::TextMessageInterface&,const QString&)),
                    SLOT(UpdateInWorldChatView(const Communications::InWorldChat::TextMessageInterface&,const QString&)) );
            }

       		tts_service_ = framework_->GetService<Tts::TtsServiceInterface>();
	        if (tts_service_)
           		connect(in_world_chat_session_, SIGNAL(TextMessageReceived(const Communications::InWorldChat::TextMessageInterface&,const QString&)), SLOT(SpeakIncomingMessage(const Communications::InWorldChat::TextMessageInterface&,const QString&)) );
        }
    }

    void CommunicationWidget::InitializeInWorldVoice()
    {
        if (voice_tool_)
            UninitializeInWorldVoice();

        voice_tool_ = new CommUI::VoiceToolWidget(framework_);
        tool_manager_->AddToolWidget("Voice", voice_tool_);
    }

    void CommunicationWidget::UpdateInWorldChatView(const Communications::InWorldChat::TextMessageInterface &message,const QString& uuid)
    {
        QString hour_str = QString::number(message.TimeStamp().time().hour());
        QString minute_str = QString::number(message.TimeStamp().time().minute());
        QString time_stamp_str = QString("%1:%2").arg(hour_str, 2, QChar('0')).arg(minute_str, 2, QChar('0'));
    }

    void CommunicationWidget::UninitializeInWorldVoice()
    {
        if (!voice_tool_)
            return;

        tool_manager_->RemoveToolWidget(voice_tool_);
        voice_tool_ = 0; // Object deleted by tool_manager_->RemoveToolWidget() function call
    }
	
    NormalChatViewWidget::NormalChatViewWidget(QWidget *parent) :
        QWidget(parent)
    {
        setObjectName("normalChatViewWidget");
        setStyleSheet("QWidget#normalChatViewWidget { background-color: transparent; }");

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setMargin(0);
        layout->setSpacing(3);
        layout->addSpacerItem(new QSpacerItem(1,1, QSizePolicy::Fixed, QSizePolicy::Expanding));
        setLayout(layout);
    }

    void NormalChatViewWidget::ShowChatMessage(bool own_message, QString message)
    {
        ChatLabel *chat_label = new ChatLabel(own_message, message);
        layout()->addWidget(chat_label);
        connect(chat_label, SIGNAL( DestroyMe(ChatLabel*) ), SLOT( RemoveChatLabel(ChatLabel*) ));
    }

    void NormalChatViewWidget::RemoveChatLabel(ChatLabel *label)
    {
        int index = layout()->indexOf(label);
        if (index != -1)
        {
            layout()->removeItem(layout()->itemAt(index));
            SAFE_DELETE(label);
            if (layout()->count() < 4)
                updateGeometry();
        }
    }

    // ChatLabel

    ChatLabel::ChatLabel(bool own_message, QString message) :
        QLabel(message)
    {
        setFont(QFont("Arial", 12));
        if (own_message)
            setStyleSheet("background-color: rgba(34,34,34,191); color: white; border-radius: 5px; padding: 3px;");
        else
            setStyleSheet("background-color: rgba(34,34,34,150); color: white; border-radius: 5px; padding: 3px;");
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        QTimer::singleShot(10000, this, SLOT(TimeOut()));
    }

    void ChatLabel::TimeOut()
    {
        emit DestroyMe(this);
    }
}

