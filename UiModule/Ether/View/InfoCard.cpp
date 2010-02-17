// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "InfoCard.h"

#include <QRadialGradient>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsSceneMouseEvent>

#include <QDebug>

namespace Ether
{
    namespace View
    {
        InfoCard::InfoCard(ArragementType type, QRectF bounding_rect, QUuid mapping_id, QString title, QString pixmap_path)
            : QGraphicsWidget(),
              type_(type),
              id_(mapping_id),
              title_(title),
              pixmap_path_(pixmap_path),
              pixmap_(QPixmap()),
              bounding_rectf_(bounding_rect),
              active_animations_()
        {
            resize(bounding_rectf_.size());

            UpdatePixmap(pixmap_path);
            InitPaintHelpers();
            InitDecorations();

            if (type == TopToBottom)
                data_type_ = Avatar;
            else if (type == BottomToTop)
                data_type_ = World;
        }

        void InfoCard::UpdatePixmap(QString pixmap_path)
        {
            pixmap_path_ = pixmap_path;

            // Pixmap
            QSize image_size = bounding_rectf_.size().toSize();
            image_size.setWidth(image_size.width()-20);
            image_size.setHeight(image_size.height()-20);

            pixmap_.load(pixmap_path_);
            if (pixmap_.rect().width() < image_size.width() && pixmap_.rect().height() < image_size.height())
                pixmap_ = pixmap_.scaled(image_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            else
            {
                pixmap_ = pixmap_.scaledToHeight(image_size.height(), Qt::SmoothTransformation);
                pixmap_ = pixmap_.copy(QRect(QPoint(pixmap_.width()/2-image_size.width()/2,0), QPoint(pixmap_.width()/2+image_size.width()/2, pixmap_.height())));
            }
        }

        void InfoCard::InitPaintHelpers()
        {          
            // Font and pen
            font_ = QFont("Helvetica", 10);
            pen_ = QPen(Qt::SolidLine);

            // Background brush
            QRadialGradient bg_grad(QPointF(80, 80), 100);
            bg_grad.setColorAt(0, QColor(255,255,255,255));
            bg_grad.setColorAt(1, QColor(90,110,200));
            bg_brush_ = QBrush(bg_grad);
        }

        void InfoCard::InitDecorations()
        {
            QGraphicsDropShadowEffect *effect = new QGraphicsDropShadowEffect(this);
            effect->setColor(QColor(0,0,0,255));
            effect->setOffset(0,0);
            effect->setBlurRadius(30);

            active_animations_ = new QSequentialAnimationGroup(this);
            blur_animation_ = new QPropertyAnimation(effect, "blurRadius", active_animations_);
            blur_animation_->setEasingCurve(QEasingCurve::Linear);
            blur_animation_->setDuration(750);
            blur_animation_->setStartValue(25);
            blur_animation_->setKeyValueAt(0.5, 5);
            blur_animation_->setEndValue(25);
            blur_animation_->setLoopCount(-1);

            jump_animation_ = new QPropertyAnimation(effect, "blurRadius", active_animations_);
            jump_animation_->setEasingCurve(QEasingCurve::Linear);
            jump_animation_->setDuration(400);
            jump_animation_->setStartValue(0);
            jump_animation_->setKeyValueAt(0.5, 50);
            jump_animation_->setEndValue(25);

            active_animations_->addAnimation(jump_animation_);
            active_animations_->addAnimation(blur_animation_);

            setGraphicsEffect(effect);
            setTransformOriginPoint(QPointF(bounding_rectf_.width()/2, bounding_rectf_.top()));
        }

        void InfoCard::IsActiveItem(bool active)
        {
            QGraphicsDropShadowEffect *effect = dynamic_cast<QGraphicsDropShadowEffect *>(graphicsEffect());
            if (active)
            {          
                if (effect)
                    effect->setColor(Qt::white);
                active_animations_->start();
            }
            else
            {
                if (effect)
                    effect->setColor(Qt::black);
                active_animations_->stop();
            }
        }

        QRectF InfoCard::boundingRect() const
        {
            return bounding_rectf_;
        }

        void InfoCard::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
        {
            pen_.setColor(Qt::blue);
            painter->setPen(pen_);
            painter->setBrush(bg_brush_);
            painter->drawRoundedRect(bounding_rectf_.toRect(), 15, 15);
            painter->drawPixmap(QPoint(10,10), pixmap_);
        }


    }
}
