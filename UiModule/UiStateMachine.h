// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_UiModule_UiStateMachine_h
#define incl_UiModule_UiStateMachine_h

#include "Foundation.h"
#include "UiModuleApi.h"

#include <QStateMachine>
#include <QParallelAnimationGroup>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QMap>

namespace UiServices
{
    class UI_MODULE_API UiStateMachine : public QStateMachine
    {

    Q_OBJECT

    public:
        UiStateMachine(QObject *parent, QGraphicsView *view);

    public slots:
        void RegisterScene(QString name, QGraphicsScene *scene);
        void SwitchToScene(QString name);

    private slots:
        void SetTransitions();
        void ViewKeyEvent(QKeyEvent *key_event);

        void AnimationsStart();
        void AnimationsFinished();

    private:
        QStateMachine *state_machine_;
        QState *state_ether_;
        QState *state_inworld_;
        QState *state_connecting_;
        QState *state_animating_change_;

        QGraphicsView *view_;
        QGraphicsScene *current_scene_;

        QMap<QString, QGraphicsScene*> scene_map_;
        QMap<QGraphicsScene*, QParallelAnimationGroup*> animations_map_;

    signals:
        void EtherTogglePressed();
        void SceneOutAnimationFinised();

    };
}

#endif // ETHERSTATEMACHINE_H
