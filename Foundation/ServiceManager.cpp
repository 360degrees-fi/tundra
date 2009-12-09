// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "ServiceManager.h"

namespace Foundation
{
    ServiceManager::ServiceManager(Framework *framework) : framework_(framework)
    {
    }

    void ServiceManager::RegisterService(Core::service_type_t type, const ServiceWeakPtr &service)
    {
        assert(service.expired() == false);

        Foundation::RootLogDebug("Registering service type " + boost::lexical_cast<std::string>(type));

        if (services_.find(type) != services_.end())
        {
            Foundation::RootLogWarning("Service provider already registered!");
            return;
        }
        services_[type] = service;

#ifdef _DEBUG
        ServicePtr upped_service = service.lock();
        Foundation::RootLogDebug("Registering service type " + Core::ToString(type) + " with usage count " + Core::ToString(upped_service.use_count()));
        services_usage_[type] = upped_service.use_count();
#endif
    }

    void ServiceManager::UnregisterService(const ServiceWeakPtr &service)
    {
        assert(service.expired() == false);

        ServicePtr upped_service = service.lock();

        ServicesMap::iterator iter = services_.begin();
        for ( ; iter != services_.end() ; ++iter)
        {
            assert (iter->second.expired() == false);
            if (iter->second.lock().get() == upped_service.get())
            {
                Foundation::RootLogDebug("Unregistering service type " + Core::ToString(iter->first));

#ifdef _DEBUG
            if (upped_service.use_count() > services_usage_[iter->first]) // not efficient according to boost doc, so use only in debug
            {
                // Somewhere, outside the module that contains this service, a reference to the service
                // is maintained. This is an error because usually services rely on their parent modules, and
                // services usually get unregistered when module gets unloaded, thus leaving a service hanging
                // without it's parent module. Crash is a likely result.
                //! \todo It might be worth considering, if services should hold a shared pointer to their parent
                //!       modules. In that way when module gets unloaded, if one of it's services is still in use
                //!       it could keep the module alive. This would take some refactoring of the module system
                //!       thought. -cm
                RootLogError("Unregistering a service type " + Core::ToString(iter->first) + " that is probably still in use!");
            }
#endif

                services_.erase(iter);
                return;
            }
        }
        //! \todo Any possibility to have the service type here nicely, so it can be included with the log warning? -cm
        Foundation::RootLogWarning("Unregistering service provider type that was not registered!");
    }
}
