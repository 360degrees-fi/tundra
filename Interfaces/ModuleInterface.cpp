// For conditions of distribution and use, see copyright notice in license.txt

#include "ModuleInterface.h"
#include <Framework.h>

namespace Foundation
{

ModuleInterfaceImpl::ModuleInterfaceImpl(const std::string &name) :
    name_(name), type_(Module::MT_Unknown), state_(Module::MS_Unloaded), framework_(0)
{
    try
    {
        Poco::Logger::create(Name(),Poco::Logger::root().getChannel(), Poco::Message::PRIO_TRACE);
    }
    catch (std::exception)
    {
        Foundation::RootLogError("Failed to create logger " + Name() + ".");
    }
}

ModuleInterfaceImpl::ModuleInterfaceImpl(Module::Type type) :
    type_(type), state_(Module::MS_Unloaded), framework_(0)
{
    try
    {
        Poco::Logger::create(Name(),Poco::Logger::root().getChannel(),Poco::Message::PRIO_TRACE);
    }
    catch (std::exception)
    {
        Foundation::RootLogError("Failed to create logger " + Name() + ".");
    }
}

ModuleInterfaceImpl::~ModuleInterfaceImpl()
{
    Poco::Logger::destroy(Name());
}

void ModuleInterfaceImpl::AutoRegisterConsoleCommand(const Console::Command &command)
{
    assert (State() == Module::MS_Unloaded && "AutoRegisterConsoleCommand function can only be used when loading the module.");

    for(CommandVector::iterator it = console_commands_.begin() ; it != console_commands_.end(); ++it )
    {
        if (it->name_ == command.name_)
            assert (false && "Registering console command twice");
    }

    console_commands_.push_back(command); 
}
Framework *ModuleInterfaceImpl::GetFramework() const
{
    return framework_;
}

std::string ModuleInterfaceImpl::VersionMajor() const
{
    if (IsInternal())
    {
        static const std::string version("version_major");
        return ( GetFramework()->GetDefaultConfig().GetSetting<std::string>(Framework::ConfigurationGroup(), version) );
    }
    return std::string("0");
}

std::string ModuleInterfaceImpl::VersionMinor() const
{
    if (IsInternal())
    {
        static const std::string version("version_minor");
        return ( GetFramework()->GetDefaultConfig().GetSetting<std::string>(Framework::ConfigurationGroup(), version) );
    }
    return std::string("0");
}
void ModuleInterfaceImpl::InitializeInternal()
{
    assert(framework_ != 0);
    assert(state_ == Module::MS_Loaded);

    //! Register components
    for(size_t n=0 ; n<component_registrars_.size() ; ++n)
        component_registrars_[n]->Register(framework_, this);

    //! Register commands
    for(CommandVector::iterator it = console_commands_.begin() ;  it != console_commands_.end() ; ++it )
    {
        if (GetFramework()->GetServiceManager()->IsRegistered(Service::ST_ConsoleCommand))
        {
            boost::weak_ptr<Console::CommandService> console = framework_->GetService<Console::CommandService>(Service::ST_ConsoleCommand);
            console.lock()->RegisterCommand(*it);
        }
    }
    Initialize();
}

void ModuleInterfaceImpl::UninitializeInternal()
{
    assert(framework_ != 0);
    assert (state_ == Module::MS_Initialized);

    for(size_t n=0 ; n<component_registrars_.size() ; ++n)
        component_registrars_[n]->Unregister(framework_);

    // Unregister commands
    for (CommandVector::iterator it = console_commands_.begin(); it != console_commands_.end(); ++it)
    {
        if (framework_->GetServiceManager()->IsRegistered(Service::ST_ConsoleCommand))
        {
            boost::weak_ptr<Console::CommandService> console = framework_->GetService<Console::CommandService>(Service::ST_ConsoleCommand);
            console.lock()->UnregisterCommand(it->name_);
        }
    }

    Uninitialize();

    // The module is now uninitialized, but it is still loaded in memory.
    // The module can now be initialized again, and InitializeInternal()
    // expects the state to be Module::MS_Loaded.
    state_ = Module::MS_Loaded;
}

}
