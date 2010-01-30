// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_Foundation_FrameworkQtApplication_h
#define incl_Foundation_FrameworkQtApplication_h

#include "ForwardDefines.h"

#include <QObject>
#include <QTimer>
#include <QApplication>
#include <QGraphicsView>

namespace Foundation
{
    class Framework;

    /// Bridges QtApplication and Framework objects together and helps drive the continuous
    /// application update ticks using a timer object.
    /// Owns QApplication and QGraphicsView (main window) for its lifetime.

    class FrameworkQtApplication : public QApplication
    {
        Q_OBJECT
        
        public:
            FrameworkQtApplication (Framework *owner, int &argc, char** argv);
            ~FrameworkQtApplication();

            QGraphicsView *GetUIView() const;
            void SetUIView (std::auto_ptr <QGraphicsView> view);

            void Go();

        public slots:
            void UpdateFrame();

        protected:
            bool eventFilter (QObject *obj, QEvent *event);
            
        private:
            Framework   *framework_;
            QTimer      frame_update_timer_;
            bool        app_activated_;

            std::auto_ptr <QGraphicsView> view_;
    };

}

#endif
