// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"

#include "ProtocolModuleOpenSim.h"
#include "OpenSimWorldSession.h"

namespace OpenSimProtocol
{

	OpenSimWorldSession::OpenSimWorldSession(Foundation::Framework *framework)
		: framework_(framework), credentials_(0), serverEntryPointUrl_(0)
	{
		networkOpensim_ = framework_->GetModuleManager()->GetModule<OpenSimProtocol::ProtocolModuleOpenSim>(Foundation::Module::MT_OpenSimProtocol);
	}

	OpenSimWorldSession::~OpenSimWorldSession()
	{

	}

	bool OpenSimWorldSession::StartSession(ProtocolUtilities::LoginCredentialsInterface *credentials, QUrl *serverEntryPointUrl)
	{
		bool success = false;
		RexLogic::OpenSimCredentials *testCredentials = dynamic_cast<RexLogic::OpenSimCredentials *>(credentials);
		if (testCredentials)
		{
			// Set Url and Credentials
			serverEntryPointUrl_ = ValidateUrl(serverEntryPointUrl->toString(), WorldSessionInterface::OpenSimServer);
			serverEntryPointUrl = &serverEntryPointUrl_;
			credentials_ = testCredentials;

			// Try do OpenSim login with ProtocolModuleOpenSim
			success = LoginToServer(credentials_->GetFirstName(), 
								    credentials_->GetLastName(), 
								    credentials_->GetPassword(), 
								    serverEntryPointUrl_.host(), 
                                    QString::number(serverEntryPointUrl_.port()), 
								    GetConnectionThreadState());
		}
		else
		{
			ProtocolModuleOpenSim::LogInfo("Invalid credential type, must be OpenSimCredentials for OpenSimWorldSession");
			success = false;
		}

		return success;
	}

	bool OpenSimWorldSession::LoginToServer(const QString& first_name,
											const QString& last_name,
											const QString& password,
											const QString& address,
											const QString& port,
											ProtocolUtilities::ConnectionThreadState *thread_state )
	{
		// Get ProtocolModuleOpenSim
		boost::shared_ptr<OpenSimProtocol::ProtocolModuleOpenSim> spOpenSim = networkOpensim_.lock();

		if (spOpenSim.get())
		{
			spOpenSim->GetLoginWorker()->PrepareOpenSimLogin(first_name, last_name, password, address, port, thread_state);
			spOpenSim->SetAuthenticationType(ProtocolUtilities::AT_OpenSim);
			// Start the thread.
			boost::thread(boost::ref( *spOpenSim->GetLoginWorker() ));
		}
		else
		{
			ProtocolModuleOpenSim::LogInfo("Could not lock ProtocolModuleOpenSim");
			return false;
		}

		return true;
	}


	QUrl OpenSimWorldSession::ValidateUrl(const QString urlString, const UrlType urlType)
	{
		QUrl returnUrl(urlString);
		switch (urlType)
		{
			case WorldSessionInterface::OpenSimServer:
				if (returnUrl.port() == -1)
				{
					returnUrl.setPort(9000);
					ProtocolModuleOpenSim::LogInfo("OpenSimServer url had no port, using defalt 9000");
				}
				break;

			case WorldSessionInterface::OpenSimGridServer:
				if (returnUrl.port() == -1)
				{
					returnUrl.setPort(8002);
					ProtocolModuleOpenSim::LogInfo("OpenSimGridServer url had no port, using defalt 8002");
				}
				break;

			case WorldSessionInterface::RealXtendAuthenticationServer:
				if (returnUrl.port() == -1)
				{
					returnUrl.setPort(10001);
					ProtocolModuleOpenSim::LogInfo("RealXtendAuthenticationServer url had no port, using defalt 10001");
				}
				break;
		}
		if (returnUrl.isValid())
			return returnUrl;
		else
		{
			ProtocolModuleOpenSim::LogInfo("Invalid connection url");
			return QUrl();
		}
	}

	ProtocolUtilities::LoginCredentialsInterface* OpenSimWorldSession::GetCredentials() const
	{
		return credentials_;
	}

	QUrl OpenSimWorldSession::GetServerEntryPointUrl() const
	{
		return serverEntryPointUrl_;
	}

	void OpenSimWorldSession::GetWorldStream() const
	{

	}

	void OpenSimWorldSession::SetCredentials(ProtocolUtilities::LoginCredentialsInterface *newCredentials)
	{
		RexLogic::OpenSimCredentials *testCredentials = dynamic_cast<RexLogic::OpenSimCredentials *>(newCredentials);
		if (testCredentials)
			credentials_ = testCredentials;
		else
			ProtocolModuleOpenSim::LogInfo("Could not set credentials, invalid type. Must be OpenSimCredentials for OpenSimWorldSession");
	}

	void OpenSimWorldSession::SetServerEntryPointUrl(const QUrl &newUrl)
	{
		serverEntryPointUrl_ = newUrl;
	}


}