// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_Interfaces_ComponentFactoryInterface_h
#define incl_Interfaces_ComponentFactoryInterface_h

namespace Foundation
{
    class ComponentInterfaceAbstract;

    //! A class implements this interface to provide factory functionality for generating one type of entity-components.
    /*! Each EC has its own factory for creating the component.

        \note When creating new entity components, it is not necessary to use this class if DECLARE_EC -macro is used.
    */
    class ComponentFactoryInterface
    {
    public:
        ComponentFactoryInterface() {}
        virtual ~ComponentFactoryInterface() {}

        virtual boost::shared_ptr<ComponentInterfaceAbstract> operator()() = 0;
        virtual boost::shared_ptr<ComponentInterfaceAbstract> operator()(const boost::shared_ptr<ComponentInterfaceAbstract> &) = 0;
    };
}

#endif
