// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"

#include <QtCore>
#include <QApplication>
#include <TelepathyQt4/Types>
#include <TelepathyQt4/PendingReady>
#include "ConnectionProvider.h"
#include "FarsightChannel.h"
#include <Poco/Environment.h>

// #include <TelepathyQt4/Debug__> // disabled because header not found on Linux

namespace TelepathyIM
{
	ConnectionProvider::ConnectionProvider(Foundation::Framework* framework): framework_(framework), dbus_daemon_(NULL), state_(STATE_INITIALIZING)
	{
        //QStringList test;
        //test.append("TEST"),
        //test.append("value");
        //QProcess::setEnvironment(test);
        
//        QApplication::set
//        QApplication::addLibraryPath("./gstreamer/bin");

		// We want to start dbus daemon only on Windows platform
#ifdef WIN32
        //! Ensures that gabble and dbus daemon processes are not running 
        //! and start new dbus daemon process

        InitializeDBusAndGabble();
        InitializeGLib();
        InitializeGStreamer();

     //   g_main_loop_.setPriority(QThread::Priority::LowPriority);
        g_main_loop_.start();

#else
      	InitializeTelepathyConnectionManager("gabble");
#endif
	}

    void ConnectionProvider::InitializeGLib()
    {
        g_type_init();
    }

    void ConnectionProvider::InitializeGStreamer()
    {
        std::string fs_plugin_path = Poco::Path::current();
        fs_plugin_path.append("gstreamer\\lib\\farsight2-0.0");
        Poco::Environment env;
        env.set("FS_PLUGIN_PATH", fs_plugin_path);        

        QString gst_plugin_path("--gst-plugin-path=\\gstreamer\\lib\\gstreamer-0.10;");
        int argc=1;
        char* argv[1];
        argv[0] = (char*)gst_plugin_path.toStdString().c_str();
        char** p_argv = &argv[0];
        GError error;
        GError *p_error = &error;
        if (!gst_init_check(&argc, &p_argv, &p_error))
        {
            QString error_message("Cannot initialize GStreamer: ");
            error_message.append(p_error->message);
            LogError(error_message.toStdString());
            return;
        }
        guint major, minor, micro, nano;
        gst_version (&major, &minor, &micro, &nano);
        QString text;
        text.sprintf("GStreamer version %i.%i.%i.%i initialized.", major, minor, micro, nano);
        LogInfo(text.toStdString());
    }

    void ConnectionProvider::InitializeDBusAndGabble()
    {
        // this will start signal&slot chain that will kill gabble and start
        // dbus-daemon and gabble processes
        ClearGabble();
    }


	ConnectionProvider::~ConnectionProvider()
	{
		DeleteConnections();

// We want to stopt dbus daemon only on Windows platform
#ifdef WIN32
		StopDBusDaemon();
        
        //g_main_loop_quit(g_main_loop_.g_main_loop_);  // hack, should be inside the thread class
        g_main_loop_.terminate(); // even more evil hack. Thread should exit by it's own
#endif
	}
               
	void ConnectionProvider::DeleteConnections()
	{
		for (ConnectionVector::iterator i = connections_.begin(); i != connections_.end(); ++i)
		{
            Connection* connection = *i; 
			//! @todo Check that there is enough time to actual close the connection to servers
			connection->Close(); // <--- CRASH HERE when login is failed
            SAFE_DELETE(connection);
		}
		connections_.clear();
	}
		
	QStringList ConnectionProvider::GetSupportedProtocols() const
	{
		return supported_protocols_;
	}

	Communication::ConnectionInterface* ConnectionProvider::OpenConnection(const Communication::CredentialsInterface& credentials)
	{
		if (state_ != STATE_READY)
		{
			LogError("Cannot open IM connection because Telepathy IM connection provider is not ready.");
			throw Exception("Telepathy IM connection provider is not ready.");
		}

		Connection* connection = new Connection(tp_connection_manager_, credentials);
		connections_.push_back(connection);

		//! @todo FIX THESE
		//connect(connection, SIGNAL( ConnectionReady(Communication::ConnectionInterface&) ), SLOT( OnConnectionReady(Communication::ConnectionInterface&) ));
		//connect(connection, SIGNAL( ConnectionClosed(Communication::ConnectionInterface&) ), SLOT( OnConnectionClosed(Communication::ConnectionInterface&) ));
		//connect(connection, SIGNAL( ConnectionError(Communication::ConnectionInterface&) ), SLOT( OnConnectionError(Communication::ConnectionInterface&) ));

		return connection;
	}

	Communication::ConnectionVector ConnectionProvider::GetConnections() const
	{
		Communication::ConnectionVector v;
		for (ConnectionVector::const_iterator i = connections_.begin(); i != connections_.end(); ++i)
		{
            Connection* connection = *i;
			v.push_back(connection);
		}
		return v;
	}

    bool ConnectionProvider::SupportProtocol(QString &protocol) const
    {
        return supported_protocols_.contains(protocol);
    }

	void ConnectionProvider::StartDBusDaemon()
	{
		QString path = "dbus\\dbus-daemon.exe";
		QString arguments = "--config-file=data\\session.conf";

		dbus_daemon_ = new QProcess(this);
		QStringList env = QProcess::systemEnvironment();
		QString env_item = "DBUS_SESSION_BUS_ADDRESS=tcp:host=127.0.0.1,port=";
		env_item.append( QString(DBUS_SESSION_PORT_, 10));
		env << env_item;
		dbus_daemon_->setEnvironment(env);
		
		connect( dbus_daemon_, SIGNAL(readyReadStandardOutput()), SLOT(OnDBusDaemonStdout()) );
		connect( dbus_daemon_, SIGNAL(finished(int)), SLOT(OnDBusDaemonExited(int)) );

		QString command = path.append(" ").append(arguments);
		dbus_daemon_->start(command);
		bool ok = dbus_daemon_->waitForStarted(500);
		if (!ok)
		{
			LogError("Cannot start dbus daemon process.");
			throw Exception("Cannot start up dbus daemon process.");
		}
        LogDebug("DBus daemon is started.");

		// wait some time so that dbus daemon have a time to start up properly
		QTime wait_time = QTime::currentTime().addSecs(1);
		while( QTime::currentTime() < wait_time )
			QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

        InitializeTelepathyConnectionManager("gabble");
	}

    void ConnectionProvider::ClearGabble()
    {
        connect(&kill_old_gabble_, SIGNAL( finished(int, QProcess::ExitStatus) ), SLOT(ClearDBusDaemon() ));
        connect(&kill_old_gabble_, SIGNAL( error(QProcess::ProcessError) ), SLOT(ClearDBusDaemon() ));
		LogDebug("Try to kill gabble.exe process.");
        kill_old_gabble_.start("taskkill /F /FI \"IMAGENAME eq gabble.exe");
    }

    void ConnectionProvider::ClearDBusDaemon()
    {
        connect(&kill_old_dbusdaemon_, SIGNAL( finished(int, QProcess::ExitStatus) ), SLOT(StartDBusDaemon() ));
        connect(&kill_old_dbusdaemon_, SIGNAL( error(QProcess::ProcessError) ), SLOT(StartDBusDaemon() ));
		LogDebug("Try to kill dbus-daemon.exe processs");
        kill_old_dbusdaemon_.start("taskkill /F /FI \"IMAGENAME eq dbus-daemon.exe");
    }

	void ConnectionProvider::StopDBusDaemon()
	{
		if (dbus_daemon_)
		{
			dbus_daemon_->kill(); // terminate seems not to be enough
			bool ok = dbus_daemon_->waitForFinished(1000);
			if (!ok)
			{
				LogError("Cannot terminate dbus daemon process.");
				return;
			}
            SAFE_DELETE(dbus_daemon_);
		}
	}

	//! @todo support more that one connection manager 
	//!       current implementation uses only gabble
	void ConnectionProvider::InitializeTelepathyConnectionManager(const QString &name)
	{
		Tp::registerTypes();
		// Tp::enableDebug(true);
		// Tp::enableWarnings(true);

        qRegisterMetaType<TelepathyIM::FarsightChannel::Status>(); // for streamed media

		tp_connection_manager_ = Tp::ConnectionManager::create(name);
		connect(tp_connection_manager_->becomeReady(Tp::ConnectionManager::FeatureCore), SIGNAL( finished(Tp::PendingOperation *) ), SLOT( OnConnectionManagerReady(Tp::PendingOperation*) ));
	}

	void ConnectionProvider::OnConnectionManagerReady(Tp::PendingOperation *op)
	{
		if (op->isError())
		{
			std::string message = "Cannot initialize Telepathy ConnectionManager object: ";
			message.append( op->errorMessage().toStdString() );
			LogError( message );

			QString reason;
			reason.append(message.c_str());

			state_ = STATE_ERROR;
			return;
		}

		LogInfo("Telepathy connection provider is ready.");
		state_ = STATE_READY;
		supported_protocols_.append("jabber");
		emit( ProtocolListUpdated(supported_protocols_) );
	}

	void ConnectionProvider::OnDBusDaemonStdout()
	{
        QString output = QString(dbus_daemon_->readAllStandardOutput());
        output = output.midRef(0, 1000).toString();
        LogDebug(QString("DBus daemon: ").append(output).toStdString());
	}

	void ConnectionProvider::OnDBusDaemonExited( int )
	{
        LogDebug("DBus daemon is exited.");
	}

	void ConnectionProvider::OnConnectionReady(Communication::ConnectionInterface &connection)
	{

	}

	void ConnectionProvider::OnConnectionClosed(Communication::ConnectionInterface &connection)
	{

	}

	void ConnectionProvider::OnConnectionError(Communication::ConnectionInterface &connection)
	{

	}

} // end of namespace: TelepathyIM
