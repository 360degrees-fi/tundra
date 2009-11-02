
#include "StableHeaders.h"
#include "Foundation.h"
#include "UICanvasManager.h"

#include <QtUiTools>
#include <QFile>

#include "OpenSimChat.h"

namespace CommunicationUI
{
	OpenSimChat::OpenSimChat(Foundation::Framework *framework)
		: framework_(framework), internalWidget_(0), visible_(false)
	{
		try
		{
			InitModuleConnections();
			InitUserInterface();
			InitCommConnections();
			Communication::CommunicationModule::LogInfo("[OpenSimUI] Initialisation succesfull...");
		}
		catch (Core::Exception e)
		{
			QString msg( QString("[OpenSimUI] Initialisations threw an exteption: %1").arg(e.what()));
			Communication::CommunicationModule::LogInfo(msg.toStdString());
		}
	}

	OpenSimChat::~OpenSimChat()
	{
		DestroyThis();
	}

	void OpenSimChat::OnOpensimUdpConnectionReady(Communication::ConnectionInterface& connection)
	{
		try
		{
			publicChat_ = connection.OpenChatSession("0"); // public chat channel 0
			ConnectSlotsToChatSession();
		}
		catch (Core::Exception &e)
		{
			QString message = QString("[OpenSimUI] Could not open world chat due to: ").append(connection.GetReason());
			Communication::CommunicationModule::LogError(message.toStdString());
		}
	}

	/// PUBLIC SLOTS ///

	void OpenSimChat::OnOpensimUdpConnectionError(Communication::ConnectionInterface& connection)
	{
		QString message = QString("[OpenSimUI] OpenSim udp connect went to a error state");
		Communication::CommunicationModule::LogDebug(message.toStdString());
	}

	void OpenSimChat::MessageRecieved(const QString& text, const Communication::ChatSessionParticipantInterface& participant)
	{
		
		QString who(participant.GetName());
		QString message(text);
		QString htmlcontent("");
		// opensimConnection_->GetUserID() returns empty ID string "" 
		// coloring + 'me' works when its there
		if ( opensimConnection_->GetUserID() != participant.GetID() )
			htmlcontent.append("<span style='color:#0099FF;'>");
		else
		{
			htmlcontent.append("<span style='color:#F80000;'>");
			who = "Me";
		}
		htmlcontent.append(who);
		htmlcontent.append(": </span><span style='color:white;'>");
		htmlcontent.append(message);
		htmlcontent.append("</span>");
		chatTextBox_->appendHtml(htmlcontent);
	}

	void OpenSimChat::MessageRecieved(const Communication::ChatMessageInterface &msg)
	{
		QString timestamp(msg.GetTimeStamp().toString());
		QString who(msg.GetOriginator()->GetName());
		QString message(msg.GetText());
		QString htmlcontent("<span style='color:#828282;'>[");
		htmlcontent.append(timestamp);
		htmlcontent.append("]</span> <span style='color:#0099FF;'>");
		htmlcontent.append(who);
		htmlcontent.append(" :</span><span style='color:white;'>");
		htmlcontent.append(message);
		htmlcontent.append("</span>");
		chatTextBox_->appendHtml(htmlcontent);
	}

	void OpenSimChat::SendMessage()
	{
		if (!chatInput_->text().isEmpty())
		{
			QString message(chatInput_->text());
			publicChat_->SendMessageW(message);
			chatInput_->clear();
		}
	}

	void OpenSimChat::ParticipantJoined(const Communication::ChatSessionParticipantInterface& participant)
	{
		QString who(participant.GetName());
		QString htmlcontent("<span style='color:#828282;'>");
		htmlcontent.append(who);
		htmlcontent.append(" joined the world chat...</span>");
		chatTextBox_->appendHtml(htmlcontent);
	}

	void OpenSimChat::ParticipantLeft(const Communication::ChatSessionParticipantInterface& participant)
	{
		QString who(participant.GetName());
		QString htmlcontent("<span style='color:#828282;'>");
		htmlcontent.append(who);
		htmlcontent.append(" left the world chat...</span>");
		chatTextBox_->appendHtml(htmlcontent);
	}
	
	void OpenSimChat::ToggleShow()
	{
		if (canvas_)
		{
			if (canvas_->IsHidden())
				canvas_->Hide();
			else
				canvas_->Show();
		}
	}

	void OpenSimChat::DestroyThis()
	{
		if (qtModule_.get())
			qtModule_->DeleteCanvas(canvas_->GetID());
	}

	void OpenSimChat::ToggleChatVisibility()
	{
		if (visible_)
		{
			// Position and resize for widget and canvas
			internalWidget_->resize(500, 20);
			canvas_->SetCanvasSize(500, 20);
			canvas_->SetPosition(0, canvas_->GetRenderWindowSize().height()-20);
			// Hide
			chatInput_->hide();
			chatTextBox_->hide();
			buttonHide_->setText("Show");
			visible_ = false;
		}
		else
		{
			// Position and resize for widget and canvas
			internalWidget_->resize(500, 100);
			canvas_->SetCanvasSize(500, 100);
			canvas_->SetPosition(0, canvas_->GetRenderWindowSize().height()-100);
			// Show
			chatInput_->show();
			chatTextBox_->show();
			buttonHide_->setText("Hide");
			visible_ = true;
		}
	}

	/// PRIVATE METHODS ///

	void OpenSimChat::InitModuleConnections()
	{
		qtModule_ = framework_->GetModuleManager()->GetModule<QtUI::QtModule>(Foundation::Module::MT_Gui).lock();
		if (qtModule_.get())
		{
			canvas_ = qtModule_->CreateCanvas(UICanvas::Internal).lock();
			if (!canvas_.get())
				throw Core::Exception("Could not create new UICanvas");
			else
			{
				canvas_->SetPosition(0, canvas_->GetRenderWindowSize().height()-20);
				canvas_->SetCanvasResizeLock(true);
				canvas_->SetLockPosition(true);
				canvas_->SetAlwaysOnTop(true);
				canvas_->Hide();
			}
		}
		else
			throw Core::Exception("Could not get pointer to QtModule");

	}

	void OpenSimChat::InitCommConnections()
	{
		communicationService_ = Communication::CommunicationService::GetInstance();
		if (communicationService_ != 0)
		{
			Communication::Credentials opensimCredentials;
			opensimCredentials.SetProtocol("opensim_udp");
			opensimConnection_ = communicationService_->OpenConnection(opensimCredentials);
			
			switch ( opensimConnection_->GetState() )
			{
				case Communication::ConnectionInterface::STATE_INITIALIZING:
					QObject::connect(opensimConnection_, SIGNAL( ConnectionReady(Communication::ConnectionInterface&) ), SLOT( OnOpensimUdpConnectionReady(Communication::ConnectionInterface&) ));
					QObject::connect(opensimConnection_, SIGNAL( ConnectionError(Communication::ConnectionInterface&) ), SLOT( OnOpensimUdpConnectionError(Communication::ConnectionInterface&) ));
					break;
				case Communication::ConnectionInterface::STATE_OPEN:
					OnOpensimUdpConnectionReady(*opensimConnection_);
					break;
				case Communication::ConnectionInterface::STATE_ERROR:
					throw Core::Exception("OpenSim chat connection is in a error state, canoot continue");
					break;
			}
		}
		else
			throw Core::Exception("Cannot get CommunicationService object");
	}

	void OpenSimChat::InitUserInterface()
	{
		try
		{
			// Read UI from file
			QUiLoader loader;
			QFile uiFile("./data/ui/communications_opensim_chat.ui");
			internalWidget_ = loader.load(&uiFile);
			uiFile.close();

			// Get widgets
			mainFrame_ = internalWidget_->findChild<QFrame *>("MainFrame");
			chatInput_ = internalWidget_->findChild<QLineEdit *>("chatInput");
			chatTextBox_ = internalWidget_->findChild<QPlainTextEdit *>("chatTextEdit");
			buttonHide_ = internalWidget_->findChild<QPushButton *>("pushButttonHide");

			// Resize and add to UICanvas
			chatInput_->hide();
			chatTextBox_->hide();
			internalWidget_->resize(500, 20);
			canvas_->SetCanvasSize(500, 20);
			canvas_->AddWidget(internalWidget_);
			canvas_->Show();
		}
		catch (std::exception ex)
		{
			throw Core::Exception("Failed to initialise UI from .ui file");
		}
	}

	void OpenSimChat::ConnectSlotsToChatSession()
	{
		// This seems to work
		QObject::connect(publicChat_, SIGNAL( MessageReceived(const QString&, const Communication::ChatSessionParticipantInterface&) ),
						 this, SLOT ( MessageRecieved(const QString&, const Communication::ChatSessionParticipantInterface& ) ));
		// This doesent...
		QObject::connect(publicChat_, SIGNAL( MessageReceived(const Communication::ChatMessageInterface&) ),
						 this, SLOT ( MessageRecieved(const Communication::ChatMessageInterface& ) ));
		// Does not connect
		QObject::connect(publicChat_, SIGNAL( ParticipantJoined(const Communication::ChatSessionParticipantInterface&) ),
						 this, SLOT ( ParticipantJoined(const Communication::ChatSessionParticipantInterface&) ));
		// Does not connect
		QObject::connect(publicChat_, SIGNAL( ParticipantLeft(const Communication::ChatSessionParticipantInterface&) ),
						 this, SLOT ( ParticipantLeft(const Communication::ChatSessionParticipantInterface&) ));
		
		// internal, works
		QObject::connect(chatInput_, SIGNAL( returnPressed() ),
						 this, SLOT( SendMessage() ));
		QObject::connect(buttonHide_, SIGNAL( clicked() ),
						 this, SLOT( ToggleChatVisibility() ));
	}
}