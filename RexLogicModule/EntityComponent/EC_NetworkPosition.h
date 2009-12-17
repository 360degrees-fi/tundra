// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_EC_NetworkPosition_h
#define incl_EC_NetworkPosition_h

#include "Foundation.h"
#include "ComponentInterface.h"
#include "Foundation.h"
#include "RexUUID.h"
#include "RexLogicModuleApi.h"

namespace RexLogic
{
    //! Represents object position/rotation/velocity data received from network, for clientside inter/extrapolation
    /*! Note that currently values are stored in Ogre format axes.
     */ 
    class REXLOGIC_MODULE_API EC_NetworkPosition : public Foundation::ComponentInterface
    {
        Q_OBJECT
            
        DECLARE_EC(EC_NetworkPosition);
    public:
        virtual ~EC_NetworkPosition();
        
        //! Position
        Vector3df position_;
        
        //! Velocity
        Vector3df velocity_;
               
        //! Acceleration
        Vector3df accel_;
        
        //! Orientation
        Quaternion orientation_;
        
        //! Rotational velocity;
        Vector3df rotvel_;
        
        //! Age of current update from network
        f64 time_since_update_;      
        
        //! Previous update interval
        f64 time_since_prev_update_;         
        
        //! Damped position
        Vector3df damped_position_; 
        
        //! Damped orientation
        Quaternion damped_orientation_;
        
        //! Whether update is first
        bool first_update;        
                
        //! Finished an update
        void Updated();
        
        //! Set position forcibly, for example in editing tools
        void SetPosition(const Vector3df& position);
        
        //! Set orientation forcibly, for example in editing tools
        void SetOrientation(const Quaternion& orientation);
                
    private:
        EC_NetworkPosition(Foundation::ModuleInterface* module);        

        //! Disable position damping, called after setting position forcibly
        void NoPositionDamping();

        //! Disable orientation damping, called after setting orientation forcibly
        void NoOrientationDamping();
         
        //! Disable acceleration/velocity, called after setting position forcibly
        void NoVelocity();

        //! Disable rotational , called after setting orientation forcibly
        void NoRotationVelocity();
    };
}

#endif