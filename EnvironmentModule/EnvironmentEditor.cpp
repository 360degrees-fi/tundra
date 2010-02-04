// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"

#include "EnvironmentEditor.h"
#include "EnvironmentModule.h"
#include "Entity.h"
#include "TerrainDecoder.h"
#include "Terrain.h"
#include "TerrainLabel.h"
#include "Water.h"

#include "Sky.h"
#include "Environment.h"

#include "TextureInterface.h"
#include "TextureServiceInterface.h"

#include <UiModule.h>
#include <UiProxyWidget.h>
#include <UiWidgetProperties.h>

#include <QUiLoader>
#include <QFile>
#include <QPushButton>
#include <QImage>
#include <QLabel>
#include <QColor>
#include <QMouseEvent>
#include <QRadioButton>
#include <QGroupBox>
#include <QTabWidget>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QColorDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QScrollArea>

namespace Environment
{
    EnvironmentEditor::EnvironmentEditor(EnvironmentModule* environment_module):
    environment_module_(environment_module),
    editor_widget_(0),
    action_(Flatten),
    brush_size_(Small),
    sky_type_(OgreRenderer::SKYTYPE_NONE),
    ambient_(false),
    edit_terrain_(false),
    sun_color_picker_(0),
    ambient_color_picker_(0)
    //mouse_press_flag_(no_button)
    {
        // Those two arrays size should always be the same as how many terrain textures we are using.
        terrain_texture_id_list_.resize(cNumberOfTerrainTextures);
        terrain_texture_requests_.resize(cNumberOfTerrainTextures);

        InitEditorWindow();
    }

    EnvironmentEditor::~EnvironmentEditor()
    {
        EnvironmentEditorProxyWidget_ = 0;
        delete sun_color_picker_;
        delete ambient_color_picker_;
        sun_color_picker_ = 0;
        ambient_color_picker_ = 0;
    }

    void EnvironmentEditor::CreateHeightmapImage()
    {
        assert(environment_module_);
        TerrainPtr terrain = environment_module_->GetTerrainHandler();

        if(terrain.get() && editor_widget_)
        {
            Scene::EntityPtr entity = terrain->GetTerrainEntity().lock();
            EC_Terrain *terrain_component = entity->GetComponent<EC_Terrain>().get();
            assert(terrain_component);

            if(terrain_component->AllPatchesLoaded())
            {
                // Find image label in widget so we can get the basic information about the image that we are about to update.
                QLabel *label = editor_widget_->findChild<QLabel *>("map_label");
                if(label)
                {
                    const QPixmap *pixmap = label->pixmap();
                    QImage image = pixmap->toImage();

                    // Make sure that image is in right size.
                    if(image.height() != cHeightmapImageHeight || image.width() != cHeightmapImageWidth)
                        return;

                    // Generate image based on heightmap values. The Highest value on heightmap will show image in white and the lowest in black.
                    MinMaxValue values = GetMinMaxHeightmapValue(*terrain_component);
                    for(int height = 0; height < image.height(); height++)
                    {
                        for(int width = 0; width < image.width(); width++)
                        {
                            float value = (terrain_component->GetPoint(width, height) - values.first);
                            value /= (values.second - values.first);
                            value *= 255;
                            QRgb color_value = qRgb(value, value, value);
                            image.setPixel(width, height, color_value);
                        }
                    }

                    // Set new image into the label.
                    label->setPixmap(QPixmap::fromImage(image));
                }
                else
                    EnvironmentModule::LogError("Cannot find map_label inside the environment editor window.");
            }
        }
    }

    MinMaxValue EnvironmentEditor::GetMinMaxHeightmapValue(EC_Terrain &terrain) const
    {
        float min, max;
        min = 65535.0f;
        max = 0.0f;

        MinMaxValue values;

        if(terrain.AllPatchesLoaded())
        {
            for(int i = 0; i < cHeightmapImageWidth; i++)
            {
                for(int j = 0; j < cHeightmapImageHeight; j++)
                {
                    float height_value = terrain.GetPoint(i, j);
                    if(height_value < min)
                    {
                        min = height_value;
                        values.first = height_value;
                    }
                    else if(height_value > max)
                    {
                        max = height_value;
                        values.second = height_value;
                    }
                }
            }
        }
        return values;
    }

    void EnvironmentEditor::InitEditorWindow()
    {
        assert(environment_module_);
        QUiLoader loader;
        QFile file("./data/ui/environment_editor.ui");
        if(!file.exists())
        {
            EnvironmentModule::LogError("Cannot find terrain editor ui file");
            return;
        }
        editor_widget_ = loader.load(&file);
        file.close();

        boost::shared_ptr<UiServices::UiModule> ui_module = environment_module_->GetFramework()->GetModuleManager()->GetModule<UiServices::UiModule>(Foundation::Module::MT_UiServices).lock();
        if (!ui_module.get())
            return;

        EnvironmentEditorProxyWidget_ = 
            ui_module->GetSceneManager()->AddWidgetToScene(editor_widget_, UiServices::UiWidgetProperties("Environment Editor", UiServices::ModuleWidget));
        //EnvironmentEditorProxyWidget_->updateGeometry();

        InitTerrainTabWindow();
        InitTerrainTextureTabWindow();
        InitWaterTabWindow();
        InitSkyTabWindow();
        InitFogTabWindow();
        InitAmbientTabWindow();

        // Tab window signals
        QTabWidget *tab_widget = editor_widget_->findChild<QTabWidget *>("tabWidget");
        if(tab_widget)
            QObject::connect(tab_widget, SIGNAL(currentChanged(int)), this, SLOT(TabWidgetChanged(int)));

        QObject::connect(&terrain_paint_timer_, SIGNAL(timeout()), this, SLOT(TerrainEditTimerTick()));
    }

    void EnvironmentEditor::InitTerrainTabWindow()
    {
        if(!editor_widget_)
            return;

        QWidget *map_widget = editor_widget_->findChild<QWidget *>("map_widget");
        if(map_widget)
        {
            TerrainLabel *label = new TerrainLabel(map_widget);
            label->setObjectName("map_label");
            QObject::connect(label, SIGNAL(SendMouseEvent(QMouseEvent*)), this, SLOT(HandleMouseEvent(QMouseEvent*)));

            // Create a QImage object and set it in label.
            QImage heightmap(cHeightmapImageWidth, cHeightmapImageHeight, QImage::Format_RGB32);
            heightmap.fill(0);
            label->setPixmap(QPixmap::fromImage(heightmap));
        }

        // Button Signals
        QPushButton *update_button = editor_widget_->findChild<QPushButton *>("button_update");
        if(update_button)
            QObject::connect(update_button, SIGNAL(clicked()), this, SLOT(UpdateTerrain()));

        QPushButton *apply_button_one = editor_widget_->findChild<QPushButton *>("apply_texture_button_1");
        if(apply_button_one)
            QObject::connect(apply_button_one, SIGNAL(clicked()), this, SLOT(ChangeTerrainTexture()));

        QPushButton *apply_button_two = editor_widget_->findChild<QPushButton *>("apply_texture_button_2");
        if(apply_button_two)
            QObject::connect(apply_button_two, SIGNAL(clicked()), this, SLOT(ChangeTerrainTexture()));

        QPushButton *apply_button_three = editor_widget_->findChild<QPushButton *>("apply_texture_button_3");
        if(apply_button_three)
            QObject::connect(apply_button_three, SIGNAL(clicked()), this, SLOT(ChangeTerrainTexture()));

        QPushButton *apply_button_four = editor_widget_->findChild<QPushButton *>("apply_texture_button_4");
        if(apply_button_four)
            QObject::connect(apply_button_four, SIGNAL(clicked()), this, SLOT(ChangeTerrainTexture()));


        // RadioButton Signals
        QRadioButton *rad_button_flatten = editor_widget_->findChild<QRadioButton *>("rad_button_flatten");
        if(rad_button_flatten)
            QObject::connect(rad_button_flatten, SIGNAL(clicked()), this, SLOT(PaintActionChanged()));

        QRadioButton *rad_button_raise = editor_widget_->findChild<QRadioButton *>("rad_button_raise");
        if(rad_button_raise)
            QObject::connect(rad_button_raise, SIGNAL(clicked()), this, SLOT(PaintActionChanged()));

        QRadioButton *rad_button_lower = editor_widget_->findChild<QRadioButton *>("rad_button_lower");
        if(rad_button_lower)
            QObject::connect(rad_button_lower, SIGNAL(clicked()), this, SLOT(PaintActionChanged()));

        QRadioButton *rad_button_smooth = editor_widget_->findChild<QRadioButton *>("rad_button_smooth");
        if(rad_button_smooth)
            QObject::connect(rad_button_smooth, SIGNAL(clicked()), this, SLOT(PaintActionChanged()));

        QRadioButton *rad_button_roughen = editor_widget_->findChild<QRadioButton *>("rad_button_roughen");
        if(rad_button_roughen)
            QObject::connect(rad_button_roughen, SIGNAL(clicked()), this, SLOT(PaintActionChanged()));

        QRadioButton *rad_button_revert = editor_widget_->findChild<QRadioButton *>("rad_button_revert");
        if(rad_button_revert)
            QObject::connect(rad_button_revert, SIGNAL(clicked()), this, SLOT(PaintActionChanged()));

        QRadioButton *rad_button_small = editor_widget_->findChild<QRadioButton *>("rad_button_small");
        if(rad_button_small)
            QObject::connect(rad_button_small, SIGNAL(clicked()), this, SLOT(BrushSizeChanged()));

        QRadioButton *rad_button_medium = editor_widget_->findChild<QRadioButton *>("rad_button_medium");
        if(rad_button_medium)
            QObject::connect(rad_button_medium, SIGNAL(clicked()), this, SLOT(BrushSizeChanged()));

        QRadioButton *rad_button_large = editor_widget_->findChild<QRadioButton *>("rad_button_large");
        if(rad_button_large)
            QObject::connect(rad_button_large, SIGNAL(clicked()), this, SLOT(BrushSizeChanged()));
    }

    void EnvironmentEditor::InitTerrainTextureTabWindow()
    {
        if(!editor_widget_)
            return;

        // Line Edit signals
        QLineEdit *line_edit_one = editor_widget_->findChild<QLineEdit *>("texture_line_edit_1");
        if(line_edit_one)
            QObject::connect(line_edit_one, SIGNAL(returnPressed()), this, SLOT(LineEditReturnPressed()));

        QLineEdit *line_edit_two = editor_widget_->findChild<QLineEdit *>("texture_line_edit_2");
        if(line_edit_two)
            QObject::connect(line_edit_two, SIGNAL(returnPressed()), this, SLOT(LineEditReturnPressed()));

        QLineEdit *line_edit_three = editor_widget_->findChild<QLineEdit *>("texture_line_edit_3");
        if(line_edit_three)
            QObject::connect(line_edit_three, SIGNAL(returnPressed()), this, SLOT(LineEditReturnPressed()));

        QLineEdit *line_edit_four = editor_widget_->findChild<QLineEdit *>("texture_line_edit_4");
        if(line_edit_four)
            QObject::connect(line_edit_four, SIGNAL(returnPressed()), this, SLOT(LineEditReturnPressed()));
    }

    void EnvironmentEditor::InitWaterTabWindow()
    {
        if(!editor_widget_)
            return;

        // Water editing button connections.
        QPushButton* water_apply_button = editor_widget_->findChild<QPushButton *>("water_apply_button");
        QDoubleSpinBox* water_height_box = editor_widget_->findChild<QDoubleSpinBox* >("water_height_doublespinbox");
        QCheckBox* water_toggle_box = editor_widget_->findChild<QCheckBox* >("water_toggle_box");

        if ( water_apply_button != 0 && water_height_box != 0 && water_toggle_box != 0 )
        {
            water_height_box->setMinimum(0.0);
            QObject::connect(water_apply_button, SIGNAL(clicked()), this, SLOT(UpdateWaterHeight()));
            
            WaterPtr water = environment_module_->GetWaterHandler();
            if (water.get() != 0)
            {
                // Initialize
                double height = water->GetWaterHeight();
                water_height_box->setValue(height);
                water_toggle_box->setChecked(true);
                // If water is created after, this connection must be made!
                QObject::connect(water.get(), SIGNAL(HeightChanged(double)), water_height_box, SLOT(setValue(double)));
                // Idea here is that if for some reason server removes water it state is updated to editor correctly.
                QObject::connect(water.get(), SIGNAL(WaterRemoved()), this, SLOT(ToggleWaterCheckButton()));
                QObject::connect(water.get(), SIGNAL(WaterCreated()), this, SLOT(ToggleWaterCheckButton()));
              
            }
            else
            {
                // Water is not created adjust a initial values
                water_toggle_box->setChecked(false);
                water_height_box->setValue(0.0);
            }
            QObject::connect(water_toggle_box, SIGNAL(stateChanged(int)), this, SLOT(UpdateWaterGeometry(int)));
        }
    }

    void EnvironmentEditor::InitSkyTabWindow()
    {
        // Sky tab window connections.
        SkyPtr sky = environment_module_->GetSkyHandler();

        if(sky.get())
        {
            QObject::connect(sky.get(), SIGNAL(SkyTypeChanged()), this, SLOT(UpdateSkyType()));

            QCheckBox *enable_sky_checkbox = editor_widget_->findChild<QCheckBox *>("sky_toggle_box");
            if(enable_sky_checkbox)
            {
                QObject::connect(enable_sky_checkbox, SIGNAL(stateChanged(int)), this, SLOT(UpdateSkyState(int)));
                QObject::connect(sky.get(), SIGNAL(SkyEnabled(bool)), this, SLOT(ToggleSkyCheckButton(bool)));
                enable_sky_checkbox->setChecked(sky->IsSkyEnabled());
            }

            QComboBox *sky_type_combo = editor_widget_->findChild<QComboBox *>("sky_type_combo");
            if(sky_type_combo)
            {
                QObject::connect(sky_type_combo, SIGNAL(activated(int)), this, SLOT(SkyTypeChanged(int)));

                QString sky_type_name;
                sky_type_name = "Sky box";
                sky_type_combo->addItem(sky_type_name);
                sky_type_name = "Sky dome";
                sky_type_combo->addItem(sky_type_name);
                sky_type_name = "Sky plane";
                sky_type_combo->addItem(sky_type_name);

                // Ask what type of sky is in use.
                int index = -1;
                switch(sky->GetSkyType())
                {
                case OgreRenderer::SKYTYPE_BOX:
                    index = 0;
                    break;
                case OgreRenderer::SKYTYPE_DOME:
                    index = 1;
                    break;
                case OgreRenderer::SKYTYPE_PLANE:
                    index = 2;
                    break;
                }

                if(index >= 0)
                {
                    sky_type_combo->setCurrentIndex(index);
                    CreateSkyProperties(sky->GetSkyType());
                }
            }

            // if caelum is in use we need to disable all sky parameters that we dont need.
            QCheckBox *enable_caelum_checkbox = editor_widget_->findChild<QCheckBox *>("use_caelum_check_box");
            if(enable_caelum_checkbox)
            {
                EnvironmentPtr environment = environment_module_->GetEnvironmentHandler();
                if (environment != 0)
                {
                    enable_caelum_checkbox->setChecked(environment->IsCaelum());
                    if(enable_caelum_checkbox->isChecked())
                    {
                        QFrame *frame = editor_widget_->findChild<QFrame *>("sky_edit_frame");
                        if(frame)
                        {
                            frame->setEnabled(false);
                        }
                    }
                }
            }
        }
        UpdateSkyTextureNames();
    }

    void EnvironmentEditor::InitFogTabWindow()
    {
        if(!editor_widget_)
            return;

        // Defines that is override fog color value used. 
        QCheckBox* fog_override_box = editor_widget_->findChild<QCheckBox* >("fog_override_checkBox");
        
        // Ground fog
        QDoubleSpinBox* fog_ground_red = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_red_dSpinBox");
        QDoubleSpinBox* fog_ground_blue = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_blue_dSpinBox");
        QDoubleSpinBox* fog_ground_green = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_green_dSpinBox");
        QPushButton* fog_ground_color_button = editor_widget_->findChild<QPushButton *>("fog_ground_color_apply_button");
        QDoubleSpinBox* fog_ground_start_distance = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_start_distance_dSpinBox");
        QDoubleSpinBox* fog_ground_end_distance = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_end_distance_dSpinBox");
        QPushButton* fog_ground_distance_button = editor_widget_->findChild<QPushButton *>("fog_ground_distance_apply_button");

        // Water fog
        QDoubleSpinBox* fog_water_red = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_red_dSpinBox");
        QDoubleSpinBox* fog_water_blue = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_blue_dSpinBox");
        QDoubleSpinBox* fog_water_green = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_green_dSpinBox");
        QPushButton* fog_water_color_button = editor_widget_->findChild<QPushButton *>("fog_water_color_apply_button");
        QDoubleSpinBox* fog_water_start_distance = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_start_distance_dSpinBox");
        QDoubleSpinBox* fog_water_end_distance = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_end_distance_dSpinBox");
        QPushButton* fog_water_distance_button = editor_widget_->findChild<QPushButton *>("fog_water_distance_apply_button");

        EnvironmentPtr environment = environment_module_->GetEnvironmentHandler();
        if (environment != 0)
        {
            if ( fog_override_box != 0 )
            {
                if ( environment->GetFogColorOverride() )
                    fog_override_box->setChecked(true);
                else
                    fog_override_box->setChecked(false);

                QObject::connect(fog_override_box, SIGNAL(clicked()), this, SLOT(ToggleFogOverride()));
            }

            if ( fog_ground_red != 0 
                && fog_ground_blue != 0
                && fog_ground_green != 0
                && fog_ground_color_button != 0
                && fog_ground_start_distance != 0
                && fog_ground_end_distance != 0
                && fog_ground_distance_button != 0)
            {
                    // Fog ground color. 
                    QVector<float> color = environment->GetFogGroundColor();

                    fog_ground_red->setMinimum(0.0);
                    fog_ground_red->setValue(color[0]);   

                    fog_ground_blue->setMinimum(0.0);
                    fog_ground_blue->setValue(color[1]);

                    fog_ground_green->setMinimum(0.0);
                    fog_ground_green->setValue(color[2]);

                    QObject::connect(environment.get(), SIGNAL(GroundFogAdjusted(float, float, const QVector<float>&)), this, SLOT(UpdateGroundFog(float, float, const QVector<float>&)));
                    QObject::connect(fog_ground_color_button, SIGNAL(clicked()), this, SLOT(SetGroundFog()));

                    fog_ground_start_distance->setMinimum(0.0);
                    fog_ground_end_distance->setMinimum(0.0);

                    QObject::connect(fog_ground_distance_button, SIGNAL(clicked()), this, SLOT(SetGroundFogDistance()));
                    fog_ground_start_distance->setMaximum(1000.0);
                    fog_ground_start_distance->setValue(environment->GetGroundFogStartDistance());
                    fog_ground_end_distance->setMaximum(1000.0);
                    fog_ground_end_distance->setValue(environment->GetGroundFogEndDistance());
            }

            if ( fog_water_red != 0 
                && fog_water_blue != 0
                && fog_water_green != 0
                && fog_water_color_button != 0
                && fog_water_start_distance != 0
                && fog_water_end_distance != 0
                && fog_water_distance_button != 0)
            {
                // Fog water color. 
                QVector<float> color = environment->GetFogWaterColor();

                fog_water_red->setMinimum(0.0);
                fog_water_red->setValue(color[0]);   

                fog_water_blue->setMinimum(0.0);
                fog_water_blue->setValue(color[1]);

                fog_water_green->setMinimum(0.0);
                fog_water_green->setValue(color[2]);

                QObject::connect(environment.get(), SIGNAL(WaterFogAdjusted(float, float, const QVector<float>&)), this, SLOT(UpdateWaterFog(float, float, const QVector<float>&)));
                QObject::connect(fog_water_color_button, SIGNAL(clicked()), this, SLOT(SetWaterFog()));

                fog_water_start_distance->setMinimum(0.0);
                fog_water_end_distance->setMinimum(0.0);

                QObject::connect(fog_water_distance_button, SIGNAL(clicked()), this, SLOT(SetWaterFogDistance()));
                fog_water_start_distance->setMaximum(1000.0);
                fog_water_start_distance->setValue(environment->GetWaterFogStartDistance());
                fog_water_end_distance->setMaximum(1000.0);
                fog_water_end_distance->setValue(environment->GetWaterFogEndDistance());
            }
        }
    }

    void EnvironmentEditor::InitAmbientTabWindow()
    {
        EnvironmentPtr environment = environment_module_->GetEnvironmentHandler();
        if(environment != 0)
        {
            // Sun direction
            QDoubleSpinBox* sun_direction_x = editor_widget_->findChild<QDoubleSpinBox* >("sun_direction_x");
            sun_direction_x->setMinimum(-100.0);
            QDoubleSpinBox* sun_direction_y  = editor_widget_->findChild<QDoubleSpinBox* >("sun_direction_y");
            sun_direction_y->setMinimum(-100.0);
            QDoubleSpinBox* sun_direction_z  = editor_widget_->findChild<QDoubleSpinBox* >("sun_direction_z");
            sun_direction_z->setMinimum(-100.0);
            
          
            if ( sun_color_picker_ == 0)
                sun_color_picker_ = new QColorDialog;
            if ( ambient_color_picker_ == 0)
                ambient_color_picker_ = new QColorDialog;

            if ( sun_direction_x != 0
                 && sun_direction_y != 0
                 && sun_direction_z != 0 )
            {
                // Initialize sun direction value
                QVector<float> sun_direction = environment->GetSunDirection();
                sun_direction_x->setValue(sun_direction[0]);
                sun_direction_y->setValue(sun_direction[1]);
                sun_direction_z->setValue(sun_direction[2]);
                
                QObject::connect(sun_direction_x, SIGNAL(valueChanged(double)), this, SLOT(UpdateSunDirection(double)));
                QObject::connect(sun_direction_y, SIGNAL(valueChanged(double)), this, SLOT(UpdateSunDirection(double)));
                QObject::connect(sun_direction_z, SIGNAL(valueChanged(double)), this, SLOT(UpdateSunDirection(double)));

                QVector<float> sun_color = environment->GetSunColor();
              
                
                QLabel* label = editor_widget_->findChild<QLabel* >("sun_color_img");
                if ( label != 0)
                {
                   
                    QPixmap img(label->width(), 23);
                    QColor color;
                    color.setRedF(sun_color[0]);
                    color.setGreenF(sun_color[1]);
                    color.setBlueF(sun_color[2]);
                    //color.setAlpha(sun_color[3]);
                    //color.setAlpha(0);
                    img.fill(color);
                    label->setPixmap(img);
                }
                
             
              
                QPushButton* sun_color_change = editor_widget_->findChild<QPushButton* >("sun_color_change_button");
                if ( sun_color_change != 0)
                    QObject::connect(sun_color_change, SIGNAL(clicked()), this, SLOT(ShowColorPicker()));

                QPushButton* ambient_color_change = editor_widget_->findChild<QPushButton* >("ambient_color_change_button");
                
                if ( ambient_color_change != 0)
                    QObject::connect(ambient_color_change, SIGNAL(clicked()), this, SLOT(ShowColorPicker()));

                if(sun_color_picker_ != 0)
                    QObject::connect(sun_color_picker_, SIGNAL(currentColorChanged(const QColor& )), this, SLOT(UpdateSunLightColor(const QColor&)));
                
                if ( ambient_color_picker_ != 0)
                    QObject::connect(ambient_color_picker_, SIGNAL(currentColorChanged(const QColor& )), this, SLOT(UpdateAmbientLightColor(const QColor&)));

                QVector<float> ambient_light = environment->GetAmbientLight();
                label = editor_widget_->findChild<QLabel* >("ambient_color_img");
                
                if ( label != 0)
                {
                   
                    QPixmap img(label->width(), 23);
                    QColor color;
                    color.setRedF(ambient_light[0]);
                    color.setGreenF(ambient_light[1]);
                    color.setBlueF(ambient_light[2]);
                    //color.setAlpha(sun_color[3]);
                    //color.setAlpha(0);
                    img.fill(color);
                    label->setPixmap(img);
                }

              
            }
        }
    }

    void EnvironmentEditor::ToggleSkyCheckButton(bool enabled)
    {
        QCheckBox *enable_sky_checkbox = editor_widget_->findChild<QCheckBox *>("sky_toggle_box");
        if(enable_sky_checkbox)
        {
            enable_sky_checkbox->setChecked(enabled);
        }
    }

    void EnvironmentEditor::CreateSkyProperties(OgreRenderer::SkyType sky_type)
    {
        if(!environment_module_)
            return;

        SkyPtr sky = environment_module_->GetSkyHandler();
        if(!sky.get())
            return;

        if(sky_type_ == sky_type)
            return;

        CleanSkyProperties();

        QScrollArea *scroll_area = editor_widget_->findChild<QScrollArea *>("scroll_sky_options");
        scroll_area->setWidgetResizable(true);
        if(sky_type == OgreRenderer::SKYTYPE_BOX)
        {
            QWidget *widget = editor_widget_->findChild<QWidget *>("panel_materials");
            if(scroll_area && widget)
            {
                QLineEdit *edit_line = 0;
                QPushButton *apply_button = 0;
                for(uint i = 0; i < 6; i++)
                {
                    edit_line = new QLineEdit(widget);
                    edit_line->setObjectName("sky_texture_line_edit_" + QString("%1").arg(i + 1));
                    edit_line->move(10, 15 + ((5 + edit_line->rect().height()) * i));
                    edit_line->resize(215, 20);
                    edit_line->show();

                    apply_button = new QPushButton(widget);
                    apply_button->setObjectName("sky_texture_apply_button_" + QString("%1").arg(i + 1));
                    apply_button->setText("Apply");
                    apply_button->move(edit_line->width() + edit_line->x() + 10, edit_line->y());
                    apply_button->resize(40, 20);
                    apply_button->show();

                    QObject::connect(apply_button, SIGNAL(clicked()), this, SLOT(ChangeSkyTextures()));
                }

                QFrame *properties_frame = editor_widget_->findChild<QFrame *>("properties_frame");
                if(!properties_frame)
                    return;
                // Get sky parameters so we can fill our widgets with the spesific sky information.
                OgreRenderer::SkyBoxParameters param = sky->GetSkyBoxParameters();

                QLabel *text_label = new QLabel(properties_frame);
                text_label->setText("Distance");
                text_label->move(150, 10); 
                text_label->show();

                QDoubleSpinBox *d_spin_box = new QDoubleSpinBox(properties_frame);
                d_spin_box->setObjectName("distance_double_spin");
                d_spin_box->move(150, 25);
                d_spin_box->setRange(0, 9999);
                d_spin_box->show();
                d_spin_box->setValue(param.distance);

                apply_button = new QPushButton(properties_frame);
                apply_button->setObjectName("apply_properties_button");
                apply_button->setText("Apply");
                apply_button->move(150, 60);
                apply_button->resize(40, 20);
                apply_button->show();
                QObject::connect(apply_button, SIGNAL(clicked()), this, SLOT(ChangeSkyProperties()));

                //widget->resize(scroll_area->width(), 15 + edit_line->pos().y() + edit_line->height());
            }
        }
        else if(sky_type == OgreRenderer::SKYTYPE_PLANE)
        {
            QWidget *widget = editor_widget_->findChild<QWidget *>("panel_materials");
            if(scroll_area && widget)
            {
                QLineEdit *edit_line = 0;
                QPushButton *apply_button = 0;
                for(uint i = 0; i < 1; i++)
                {
                    edit_line = new QLineEdit(widget);
                    if(edit_line)
                    {
                        edit_line->setObjectName("sky_texture_line_edit_" + QString("%1").arg(i + 1));
                        edit_line->move(10, 15 + ((5 + edit_line->rect().height()) * i));
                        edit_line->resize(215, 20);
                        edit_line->show();
                    }

                    apply_button = new QPushButton(widget);
                    if(apply_button)
                    {
                        apply_button->setObjectName("sky_texture_apply_button_" + QString("%1").arg(i + 1));
                        apply_button->setText("Apply");
                        apply_button->move(edit_line->width() + edit_line->x() + 10, edit_line->y());
                        apply_button->resize(40, 20);
                        apply_button->show();

                        QObject::connect(apply_button, SIGNAL(clicked()), this, SLOT(ChangeSkyTextures()));
                    }
                }

                QFrame *properties_frame = editor_widget_->findChild<QFrame *>("properties_frame");
                if(!properties_frame)
                    return;

                OgreRenderer::SkyPlaneParameters param = sky->GetSkyPlaneParameters();

                // Create properties for sky plane.
                QSpinBox *spin_box = new QSpinBox(properties_frame);
                QLabel *text_label = new QLabel(properties_frame);
                if(text_label)
                {
                    text_label->setText("X seg");
                    text_label->move(10, 10);
                    text_label->show();
                }

                if(spin_box)
                {
                    spin_box->setObjectName("x_segments_spin");
                    spin_box->move(10, 25);
                    spin_box->setRange(0, 999);
                    spin_box->show();
                    spin_box->setValue(param.xSegments);
                }

                text_label = new QLabel(properties_frame);
                if(text_label)
                {
                    text_label->setText("Y seg");
                    text_label->move(50, 10);
                    text_label->show();
                }

                spin_box = new QSpinBox(properties_frame);
                if(spin_box)
                {
                    spin_box->setObjectName("y_segments_spin");
                    spin_box->move(50, 25);
                    spin_box->setRange(0, 999);
                    spin_box->show();
                    spin_box->setValue(param.ySegments);
                }

                text_label = new QLabel(properties_frame);
                if(text_label)
                {
                    text_label->setText("Scale");
                    text_label->move(90, 10);
                    text_label->show();
                }

                QDoubleSpinBox *d_spin_box = new QDoubleSpinBox(properties_frame);
                if(d_spin_box)
                {
                    d_spin_box->setObjectName("scale_double_spin");
                    d_spin_box->move(90, 25);
                    d_spin_box->setRange(0, 999);
                    d_spin_box->show();
                    d_spin_box->setValue(param.scale);
                }

                text_label = new QLabel(properties_frame);
                if(text_label)
                {
                    text_label->setText("Distance");
                    text_label->move(150, 10);
                    text_label->show();
                }

                d_spin_box = new QDoubleSpinBox(properties_frame);
                if(d_spin_box)
                {
                    d_spin_box->setObjectName("distance_double_spin");
                    d_spin_box->move(150, 25);
                    d_spin_box->setRange(0, 9999);
                    d_spin_box->show();
                    d_spin_box->setValue(param.distance);
                }

                text_label = new QLabel(properties_frame);
                if(text_label)
                {
                    text_label->setText("Tiling");
                    text_label->move(10, 45);
                    text_label->show();
                }

                d_spin_box = new QDoubleSpinBox(properties_frame);
                if(d_spin_box)
                {
                    d_spin_box->setObjectName("tiling_double_spin");
                    d_spin_box->move(10, 60);
                    d_spin_box->setRange(0, 999);
                    d_spin_box->show();
                    d_spin_box->setValue(param.tiling);
                }

                text_label = new QLabel(properties_frame);
                if(text_label)
                {
                    text_label->setText("Bow");
                    text_label->move(70, 45);
                    text_label->show();
                }

                d_spin_box = new QDoubleSpinBox(properties_frame);
                if(d_spin_box)
                {
                    d_spin_box->setObjectName("bow_double_spin");
                    d_spin_box->move(70, 60);
                    d_spin_box->setRange(0, 999);
                    d_spin_box->show();
                    d_spin_box->setValue(param.bow);
                }

                apply_button = new QPushButton(properties_frame);
                if(apply_button)
                {
                    apply_button->setObjectName("apply_properties_button");
                    apply_button->setText("Apply");
                    apply_button->move(150, 60);
                    apply_button->resize(40, 20);
                    apply_button->show();
                    QObject::connect(apply_button, SIGNAL(clicked()), this, SLOT(ChangeSkyProperties()));
                }
                //widget->resize(scroll_area->width(), 15 + edit_line->pos().y() + edit_line->height());
            }
        }
        else if(sky_type == OgreRenderer::SKYTYPE_DOME)
        {
            QWidget *widget = editor_widget_->findChild<QWidget *>("panel_materials");
            if(scroll_area && widget)
            {
                QLineEdit *edit_line = 0;
                QPushButton *apply_button = 0;
                for(uint i = 0; i < 1; i++)
                {
                    edit_line = new QLineEdit(widget);
                    if(edit_line)
                    {
                        edit_line->setObjectName("sky_texture_line_edit_" + QString("%1").arg(i + 1));
                        edit_line->move(10, 15 + ((5 + edit_line->rect().height()) * i));
                        edit_line->resize(215, 20);
                        edit_line->show();
                    }

                    apply_button = new QPushButton(widget);
                    if(apply_button)
                    {
                        apply_button->setObjectName("sky_texture_apply_button_" + QString("%1").arg(i + 1));
                        apply_button->setText("Apply");
                        apply_button->move(edit_line->width() + edit_line->x() + 10, edit_line->y());
                        apply_button->resize(40, 20);
                        apply_button->show();

                        QObject::connect(apply_button, SIGNAL(clicked()), this, SLOT(ChangeSkyTextures()));
                    }
                }

                QFrame *properties_frame = editor_widget_->findChild<QFrame *>("properties_frame");
                if(!properties_frame)
                    return;

                OgreRenderer::SkyDomeParameters param = sky->GetSkyDomeParameters();

                // Create properties for sky dome.
                QSpinBox *spin_box = new QSpinBox(properties_frame);
                QLabel *text_label = new QLabel(properties_frame);
                if(text_label)
                {
                    text_label->setText("Y keep");
                    text_label->move(10, 10);
                    text_label->show();
                }

                if(spin_box)
                {
                    spin_box->setObjectName("y_segments_keep_spin");
                    spin_box->move(10, 25);
                    spin_box->setRange(-99, 99);
                    spin_box->show();
                    spin_box->setValue(param.ySegmentsKeep);
                }

                text_label = new QLabel(properties_frame);
                if(text_label)
                {
                    text_label->setText("X seg");
                    text_label->move(50, 10);
                    text_label->show();
                }

                spin_box = new QSpinBox(properties_frame);
                if(spin_box)
                {
                    spin_box->setObjectName("x_segments_spin");
                    spin_box->move(50, 25);
                    spin_box->setRange(0, 99);
                    spin_box->show();
                    spin_box->setValue(param.xSegments);
                }

                text_label = new QLabel(properties_frame);
                if(text_label)
                {
                    text_label->setText("Y seg");
                    text_label->move(90, 10);
                    text_label->show();
                }

                spin_box = new QSpinBox(properties_frame);
                if(spin_box)
                {
                    spin_box->setObjectName("y_segments_spin");
                    spin_box->move(90, 25);
                    spin_box->setRange(0, 99);
                    spin_box->show();
                    spin_box->setValue(param.ySegments);
                }

                text_label = new QLabel(properties_frame);
                if(text_label)
                {
                    text_label->setText("Distance");
                    text_label->move(150, 10);
                    text_label->show();
                }

                QDoubleSpinBox *d_spin_box = new QDoubleSpinBox(properties_frame);
                if(d_spin_box)
                {
                    d_spin_box->setObjectName("distance_double_spin");
                    d_spin_box->move(150, 25);
                    d_spin_box->setRange(0, 9999);
                    d_spin_box->show();
                    d_spin_box->setValue(param.distance);
                }

                text_label = new QLabel(properties_frame);
                if(text_label)
                {
                    text_label->setText("Curvature");
                    text_label->move(10, 45);
                    text_label->show();
                }

                d_spin_box = new QDoubleSpinBox(properties_frame);
                if(d_spin_box)
                {
                    d_spin_box->setObjectName("curvature_double_spin");
                    d_spin_box->move(10, 60);
                    d_spin_box->setRange(0, 999);
                    d_spin_box->show();
                    d_spin_box->setValue(param.curvature);
                }

                text_label = new QLabel(properties_frame);
                if(text_label)
                {
                    text_label->setText("Tiling");
                    text_label->move(70, 45);
                    text_label->show();
                }

                d_spin_box = new QDoubleSpinBox(properties_frame);
                if(d_spin_box)
                {
                    d_spin_box->setObjectName("tiling_double_spin");
                    d_spin_box->move(70, 60);
                    d_spin_box->setRange(0, 999);
                    d_spin_box->show();
                    d_spin_box->setValue(param.tiling);
                }

                apply_button = new QPushButton(properties_frame);
                if(apply_button)
                {
                    apply_button->setObjectName("apply_properties_button");
                    apply_button->setText("Apply");
                    apply_button->move(150, 60);
                    apply_button->resize(40, 20);
                    apply_button->show();
                    QObject::connect(apply_button, SIGNAL(clicked()), this, SLOT(ChangeSkyProperties()));
                }
            }
        }

        sky_type_ = sky_type;
        UpdateSkyTextureNames();
    }

    void EnvironmentEditor::CleanSkyProperties()
    {
        // Clear texture name list on scroll area.
        {
            QWidget *scroll_area = editor_widget_->findChild<QWidget *>("panel_materials");
            if(!scroll_area)
                return;
            const QObjectList &children = scroll_area->children();
            QListIterator<QObject *> it(children);
            while(it.hasNext())
            {
                QObject *object = it.next();
                if(object)
                    delete object;
            }
        }

        // Clear sky type properties.
        {
            QFrame *properties_frame = editor_widget_->findChild<QFrame *>("properties_frame");
            if(!properties_frame)
                return;
            const QObjectList &children = properties_frame->children();
            QListIterator<QObject *> it(children);
            while(it.hasNext())
            {
                QObject *object = it.next();
                if(object)
                    delete object;
            }
        }
    }

    void EnvironmentEditor::UpdateSunLightColor(const QColor& color)
    {
        EnvironmentPtr environment = environment_module_->GetEnvironmentHandler();
        if(!environment.get())
            return;

        QWidget* widget = GetCurrentPage();
        if ( widget != 0 
             && widget->objectName() == "ambient_light")
        {
           
                QVector<float> current_sun_color = environment->GetSunColor();
                QVector<float> new_sun_color(4);
                new_sun_color[0] = color.redF(), new_sun_color[1] = color.greenF(), new_sun_color[2] = color.blueF(), new_sun_color[3] = 1;
                if ( new_sun_color[0] != current_sun_color[0] 
                     && new_sun_color[1] != current_sun_color[1] 
                     && new_sun_color[2] != current_sun_color[2])
                {
                    // Change to new color.
                    environment->SetSunColor(new_sun_color);
                    QLabel* label = editor_widget_->findChild<QLabel* >("sun_color_img");
                    if ( label != 0)
                    {
                        QPixmap img(label->width(), 23);
                        QColor color;
                        color.setRedF(new_sun_color[0]);
                        color.setGreenF(new_sun_color[1]);
                        color.setBlueF(new_sun_color[2]);
                        //color.setAlpha(sun_color[3]);
                        //color.setAlpha(0);
                        img.fill(color);
                        label->setPixmap(img);
                    }
                   
                

                 }
           
         }

    }

    void EnvironmentEditor::UpdateAmbientLightColor(const QColor& color)
    {
        EnvironmentPtr environment = environment_module_->GetEnvironmentHandler();
        if(!environment.get())
            return;

        QWidget* widget = GetCurrentPage();
        if ( widget != 0 
             && widget->objectName() == "ambient_light")
        {

            // ambient light.
            QVector<float> current_ambient_color = environment->GetAmbientLight();
            QVector<float> new_ambient_color(3);
            new_ambient_color[0] = color.redF(), new_ambient_color[1] = color.greenF(), new_ambient_color[2] = color.blueF();
            if ( new_ambient_color[0] != current_ambient_color[0] 
                 && new_ambient_color[1] != current_ambient_color[1] 
                 && new_ambient_color[2] != current_ambient_color[2])
            {
                // Change to new color.
                environment->SetAmbientLight(new_ambient_color);
                QLabel* label = editor_widget_->findChild<QLabel* >("ambient_color_img");
                if ( label != 0)
                {
                    QPixmap img(label->width(), 23);
                    QColor color;
                    color.setRedF(new_ambient_color[0]);
                    color.setGreenF(new_ambient_color[1]);
                    color.setBlueF(new_ambient_color[2]);
                    //color.setAlpha(sun_color[3]);
                    //color.setAlpha(0);
                    img.fill(color);
                    label->setPixmap(img);
                }
             }
        }
    }


    void EnvironmentEditor::ShowColorPicker()
    {
        EnvironmentPtr environment = environment_module_->GetEnvironmentHandler();
        QColorDialog *dialog = 0;

        if( environment == 0)
            return;

        QWidget* widget = GetCurrentPage();
        if ( widget != 0 && widget->objectName() == "ambient_light")
        {
            if (this->sender()->objectName() == "sun_color_change_button")
            {
                
                QVector<float> sun_color = environment->GetSunColor();
                QColor color;
                color.setRedF(sun_color[0]);
                color.setGreenF(sun_color[1]);
                color.setBlueF(sun_color[2]);
                //color.setAlpha(0);
                sun_color_picker_->setCurrentColor(color);
                dialog = sun_color_picker_;  
            }

            if ( this->sender()->objectName() == "ambient_color_change_button")
            {
                QVector<float> ambient_light = environment->GetAmbientLight();
                QColor color;
                color.setRedF(ambient_light[0]);
                color.setGreenF(ambient_light[1]);
                color.setBlueF(ambient_light[2]);
                ambient_color_picker_->setCurrentColor(color);
                dialog = ambient_color_picker_;
            }
         }
            
        if ( dialog != 0)
            dialog->show();
    }

    QWidget* EnvironmentEditor::GetPage(const QString& name)
    {
         
        QTabWidget* tab_widget = editor_widget_->findChild<QTabWidget* >("tabWidget");
        if ( tab_widget != 0 )
        {
            for ( int i = 0; i < tab_widget->count(); ++i)
            {
                QWidget* page = tab_widget->widget(i);
                if ( page != 0 && page->objectName() == name)
                    return page;
                
            }
        }

        return 0;
    }

    QWidget* EnvironmentEditor::GetCurrentPage()
    {
        QWidget* page = 0;
        QTabWidget* tab_widget = editor_widget_->findChild<QTabWidget* >("tabWidget");
        if ( tab_widget != 0 )
            page = tab_widget->currentWidget();
            
        return page;

    }

    void EnvironmentEditor::UpdateSunDirection(double value)
    {
        EnvironmentPtr environment = environment_module_->GetEnvironmentHandler();
        if(!environment.get())
            return;

        // Sun direction
        QDoubleSpinBox* sun_direction_x = editor_widget_->findChild<QDoubleSpinBox* >("sun_direction_x");
        QDoubleSpinBox* sun_direction_y  = editor_widget_->findChild<QDoubleSpinBox* >("sun_direction_y");
        QDoubleSpinBox* sun_direction_z  = editor_widget_->findChild<QDoubleSpinBox* >("sun_direction_z");
       
        if ( sun_direction_x != 0 &&
             sun_direction_y != 0 &&
             sun_direction_z != 0 &&
             environment.get() != 0)
        {
         QVector<float> sun_direction(3);
         sun_direction[0] = static_cast<float>(sun_direction_x->value());
         sun_direction[1] = static_cast<float>(sun_direction_y->value());
         sun_direction[2] = static_cast<float>(sun_direction_z->value());
         environment->SetSunDirection(sun_direction);
        }

    }

    void EnvironmentEditor::UpdateWaterHeight()
    {
        // Sanity check: if water is totally disabled do not update it. 
        QCheckBox* water_toggle_box = editor_widget_->findChild<QCheckBox* >("water_toggle_box");

        if (water_toggle_box != 0 && !water_toggle_box->isChecked())
            return;

        QDoubleSpinBox* water_height_box = editor_widget_->findChild<QDoubleSpinBox* >("water_height_doublespinbox");
        WaterPtr water = environment_module_->GetWaterHandler();
        if (water.get() != 0 && water_height_box != 0)
            water->SetWaterHeight(static_cast<float>(water_height_box->value())); 
    }

    void EnvironmentEditor::UpdateWaterGeometry(int state)
    {
        QDoubleSpinBox* water_height_box = editor_widget_->findChild<QDoubleSpinBox* >("water_height_doublespinbox");
        WaterPtr water = environment_module_->GetWaterHandler();

        switch ( state )
        {
        case Qt::Checked:
            {
                if ( water_height_box != 0 && water.get() != 0 )
                    water->CreateWaterGeometry(static_cast<float>(water_height_box->value()));
                else if ( water.get() != 0)
                    water->CreateWaterGeometry();
                
                break;
            }
        case Qt::Unchecked:
            {
                if ( water.get() != 0)
                    water->RemoveWaterGeometry();
                 break;
            }
        default:
            break;
        }
    }

    void EnvironmentEditor::UpdateSkyState(int state)
   {
        assert(environment_module_);
        SkyPtr sky = environment_module_->GetSkyHandler();
        if(!sky.get())
            return;

        switch(state)
        {
        case Qt::Checked:
            if(!sky->IsSkyEnabled())
                sky->ChangeSkyType(sky_type_, true);
            break;
        case Qt::Unchecked:
            sky->DisableSky();
            break;
        }
    }

    void EnvironmentEditor::UpdateSkyType()
    {
        SkyPtr sky = environment_module_->GetSkyHandler();
        if(!sky.get())
            return;

        // Ask what type of sky is in use and chage it.
        int index = -1;
        switch(sky->GetSkyType())
        {
        case OgreRenderer::SKYTYPE_BOX:
            index = 0;
            break;
        case OgreRenderer::SKYTYPE_DOME:
            index = 1;
            break;
        case OgreRenderer::SKYTYPE_PLANE:
            index = 2;
            break;
        }

        QComboBox *sky_type_combo = editor_widget_->findChild<QComboBox *>("sky_type_combo");
        if(!sky_type_combo)
            return;

        if(index >= 0)
        {
            sky_type_combo->setCurrentIndex(index);
            CreateSkyProperties(sky->GetSkyType());
        }
    }

    void EnvironmentEditor::ChangeSkyTextures()
    {
        assert(environment_module_);
        SkyPtr sky = environment_module_->GetSkyHandler();
        if(!sky.get())
            return;

        const QPushButton *sender = qobject_cast<QPushButton *>(QObject::sender());
        if(sender)
        {
            QString object_name = sender->objectName();
            object_name = object_name[object_name.size() - 1];
            int index = object_name.toInt();
            QLineEdit *text_field = editor_widget_->findChild<QLineEdit *>("sky_texture_line_edit_" + QString("%1").arg(index));
            if(text_field)
            {
                if(text_field->text() != "")
                {
                    if(sky_type_ == OgreRenderer::SKYTYPE_BOX)
                    {
                        RexTypes::RexAssetID sky_textures[6];
                        for(uint i = 0; i < 6; i++)
                            sky_textures[i] = sky->GetSkyTextureID(OgreRenderer::SKYTYPE_BOX, i);
                        sky_textures[index - 1] = text_field->text().toStdString();
                        sky->SetSkyBoxTextures(sky_textures);
                        sky->RequestSkyTextures();
                    }
                    else if(sky_type_ == OgreRenderer::SKYTYPE_DOME || sky_type_ == OgreRenderer::SKYTYPE_PLANE)
                    {
                        sky->SetSkyTexture(text_field->text().toStdString());
                        sky->RequestSkyTextures();
                    }
                }
            }
        }
    }

    void EnvironmentEditor::UpdateSkyTextureNames()
    {
        assert(environment_module_);
        SkyPtr sky = environment_module_->GetSkyHandler();
        if(!sky.get())
            return;

        if(sky_type_ == OgreRenderer::SKYTYPE_BOX)
        {
            for(uint i = 0; i < SKYBOX_TEXTURE_COUNT; i++)
            {
                QString line_edit_name = "sky_texture_line_edit_" + QString("%1").arg(i + 1);
                QLineEdit *texture_line_edit = editor_widget_->findChild<QLineEdit *>(line_edit_name);
                if(texture_line_edit)
                    texture_line_edit->setText(QString::fromStdString(sky->GetSkyTextureID(OgreRenderer::SKYTYPE_BOX, i)));
            }
        }
        else if(sky_type_ == OgreRenderer::SKYTYPE_DOME)
        {
            QString line_edit_name = "sky_texture_line_edit_1";
            QLineEdit *texture_line_edit = editor_widget_->findChild<QLineEdit *>(line_edit_name);
            if(texture_line_edit)
                texture_line_edit->setText(QString::fromStdString(sky->GetSkyTextureID(OgreRenderer::SKYTYPE_DOME, 0)));
        }
        else if(sky_type_ == OgreRenderer::SKYTYPE_PLANE)
        {
            QString line_edit_name = "sky_texture_line_edit_1";
            QLineEdit *texture_line_edit = editor_widget_->findChild<QLineEdit *>(line_edit_name);
            if(texture_line_edit)
                texture_line_edit->setText(QString::fromStdString(sky->GetSkyTextureID(OgreRenderer::SKYTYPE_PLANE, 0)));
        }
    }

    void EnvironmentEditor::ChangeSkyProperties()
    {
        SkyPtr sky = environment_module_->GetSkyHandler();
        if(!sky.get())
            return;

        const QPushButton *sender = qobject_cast<QPushButton *>(QObject::sender());
        int button_number = 0;
        if(sender)
        {
            // Make sure that the signal sender was some of those apply buttons.
            if(sender->objectName() == "apply_properties_button")
            {
                OgreRenderer::SkyType sky_type = sky->GetSkyType();
                switch(sky_type)
                {
                case OgreRenderer::SKYTYPE_BOX:
                    {
                        OgreRenderer::SkyBoxParameters sky_param;
                        QDoubleSpinBox *dspin_box = editor_widget_->findChild<QDoubleSpinBox *>("distance_double_spin");
                        if(dspin_box)
                            sky_param.distance = dspin_box->value();

                        sky->SetSkyBoxParameters(sky_param, sky->IsSkyEnabled());
                    break;
                    }
                case OgreRenderer::SKYTYPE_DOME:
                    {
                        OgreRenderer::SkyDomeParameters sky_param;
                        QSpinBox *spin_box = editor_widget_->findChild<QSpinBox *>("x_segments_spin");
                        if(spin_box)
                            sky_param.xSegments = spin_box->value();
                        spin_box = editor_widget_->findChild<QSpinBox *>("y_segments_spin");
                        if(spin_box)
                            sky_param.ySegments = spin_box->value();
                        spin_box = editor_widget_->findChild<QSpinBox *>("y_segments_keep_spin");
                        if(spin_box)
                            sky_param.ySegmentsKeep = spin_box->value();
                        QDoubleSpinBox *dspin_box = editor_widget_->findChild<QDoubleSpinBox *>("curvature_double_spin");
                        if(dspin_box)
                            sky_param.curvature = dspin_box->value();
                        dspin_box = editor_widget_->findChild<QDoubleSpinBox *>("tiling_double_spin");
                        if(dspin_box)
                            sky_param.tiling = dspin_box->value();
                        dspin_box = editor_widget_->findChild<QDoubleSpinBox *>("distance_double_spin");
                        if(dspin_box)
                            sky_param.distance = dspin_box->value();

                        sky->SetSkyDomeParameters(sky_param, sky->IsSkyEnabled());
                    break;
                    }
                case OgreRenderer::SKYTYPE_PLANE:
                    {
                        OgreRenderer::SkyPlaneParameters sky_param;
                        QSpinBox *spin_box = editor_widget_->findChild<QSpinBox *>("x_segments_spin");
                        if(spin_box)
                            sky_param.xSegments = spin_box->value();
                        spin_box = editor_widget_->findChild<QSpinBox *>("y_segments_spin");
                        if(spin_box)
                            sky_param.ySegments = spin_box->value();
                        QDoubleSpinBox *dspin_box = editor_widget_->findChild<QDoubleSpinBox *>("scale_double_spin");
                        if(dspin_box)
                            sky_param.scale = dspin_box->value();
                        dspin_box = editor_widget_->findChild<QDoubleSpinBox *>("tiling_double_spin");
                        if(dspin_box)
                            sky_param.tiling = dspin_box->value();
                        dspin_box = editor_widget_->findChild<QDoubleSpinBox *>("bow_double_spin");
                        if(dspin_box)
                            sky_param.bow = dspin_box->value();
                        dspin_box = editor_widget_->findChild<QDoubleSpinBox *>("distance_double_spin");
                        if(dspin_box)
                            sky_param.distance = dspin_box->value();

                        sky->SetSkyPlaneParameters(sky_param, sky->IsSkyEnabled());
                    break;
                    }
                }
            }
        }

        QString start_height("Texture_height_doubleSpinBox_" + QString("%1").arg(button_number + 1));
        QString height_range("Texture_height_range_doubleSpinBox_" + QString("%1").arg(button_number + 1));
        QDoubleSpinBox *start_height_spin = editor_widget_->findChild<QDoubleSpinBox*>(start_height);
        QDoubleSpinBox *height_range_spin = editor_widget_->findChild<QDoubleSpinBox*>(height_range);
        if(start_height_spin && height_range_spin)
            environment_module_->SendTextureHeightMessage(start_height_spin->value(), height_range_spin->value(), button_number);
    }

    void EnvironmentEditor::ToggleWaterCheckButton()
    {
        QCheckBox* water_toggle_box = editor_widget_->findChild<QCheckBox* >("water_toggle_box");
        WaterPtr water = environment_module_->GetWaterHandler();

        // Dirty way to check that what is state of water. 
        if ( water.get() != 0 && water_toggle_box != 0)
        {
            if (water->GetWaterEntity().expired())
                water_toggle_box->setChecked(false); 
            else
                water_toggle_box->setChecked(true);
        }
    }

    void EnvironmentEditor::UpdateTerrainTextureRanges()
    {
        assert(environment_module_);
        TerrainPtr terrain = environment_module_->GetTerrainHandler();
        if(!terrain.get())
            return;

        for(uint i = 0; i < cNumberOfTerrainTextures; i++)
        {
            Real start_height_value = terrain->GetTerrainTextureStartHeight(i);
            Real height_range_value = terrain->GetTerrainTextureHeightRange(i);

            QString start_height_name("Texture_height_doubleSpinBox_" + QString("%1").arg(i + 1));
            QString height_range_name("Texture_height_range_doubleSpinBox_" + QString("%1").arg(i + 1));

            QDoubleSpinBox *start_height_spin = editor_widget_->findChild<QDoubleSpinBox *>(start_height_name);
            if(start_height_spin)
                start_height_spin->setValue(start_height_value);

            QDoubleSpinBox *height_range_spin = editor_widget_->findChild<QDoubleSpinBox *>(height_range_name);
            if(height_range_spin)
                height_range_spin->setValue(height_range_value);
        }
    }

    void EnvironmentEditor::UpdateTerrainTextures()
    {
        TerrainPtr terrain = environment_module_->GetTerrainHandler();
        if(!terrain.get())
            return;

        QLineEdit *line_edit = 0;
        for(uint i = 0; i < cNumberOfTerrainTextures; i++)
        {
            //Get terrain texture asset ids so that we can request those image resources.
            RexTypes::RexAssetID terrain_id = terrain->GetTerrainTextureID(i);
            QString line_edit_name("texture_line_edit_" + QString("%1").arg(i + 1));

            // Check if terrain texture hasn't changed for last time, if not we dont need to request a new texture resource and we can continue on next texture.
            if(terrain_texture_id_list_[i] == terrain_id)
                continue;

            terrain_texture_id_list_[i] = terrain_id;

            line_edit = editor_widget_->findChild<QLineEdit *>(line_edit_name);
            if(!line_edit)
                continue;
            line_edit->setText(QString::fromStdString(terrain_id));

            terrain_texture_requests_[i] = RequestTerrainTexture(i);
        }
    }

    void EnvironmentEditor::LineEditReturnPressed()
    {
        assert(environment_module_);
        TerrainPtr terrain = environment_module_->GetTerrainHandler();
        if(!terrain.get())
            return;

        const QLineEdit *sender = qobject_cast<QLineEdit *>(QObject::sender());
        int line_edit_number = 0;
        if(sender)
        {
            // Make sure that the signal sender was some of those line edit widgets.
            if(sender->objectName()      == "texture_line_edit_1") line_edit_number = 0;
            else if(sender->objectName() == "texture_line_edit_2") line_edit_number = 1;
            else if(sender->objectName() == "texture_line_edit_3") line_edit_number = 2;
            else if(sender->objectName() == "texture_line_edit_4") line_edit_number = 3;

            if(line_edit_number >= 0)
            {
                if(terrain_texture_id_list_[line_edit_number] != sender->text().toStdString())
                {
                    terrain_texture_id_list_[line_edit_number] = sender->text().toStdString();
                    RexTypes::RexAssetID texture_id[cNumberOfTerrainTextures];
                    for(uint i = 0; i < cNumberOfTerrainTextures; i++)
                        texture_id[i] = terrain_texture_id_list_[i];
                    terrain->SetTerrainTextures(texture_id);

                    terrain_texture_requests_[line_edit_number] = RequestTerrainTexture(line_edit_number);
                }
            }
        }
    }

    void EnvironmentEditor::ChangeTerrainTexture()
    {
        assert(environment_module_);
        TerrainPtr terrain = environment_module_->GetTerrainHandler();
        if(!terrain.get())
            return;

        const QPushButton *sender = qobject_cast<QPushButton *>(QObject::sender());
        int button_number = 0;
        if(sender)
        {
            // Make sure that the signal sender was some of those apply buttons.
            if(sender->objectName()      == "apply_texture_button_1")
                button_number = 0;
            else if(sender->objectName() == "apply_texture_button_2")
                button_number = 1;
            else if(sender->objectName() == "apply_texture_button_3")
                button_number = 2;
            else if(sender->objectName() == "apply_texture_button_4")
                button_number = 3;

            if(button_number >= 0)
            {
                QString line_edit_name("texture_line_edit_" + QString("%1").arg(button_number + 1));
                QLineEdit *line_edit = editor_widget_->findChild<QLineEdit*>(line_edit_name);
                if(line_edit)
                {
                    if(terrain_texture_id_list_[button_number] != line_edit->text().toStdString())
                    {
                        terrain_texture_id_list_[button_number] = line_edit->text().toStdString();
                        RexTypes::RexAssetID texture_id[cNumberOfTerrainTextures];
                        for(uint i = 0; i < cNumberOfTerrainTextures; i++)
                            texture_id[i] = terrain_texture_id_list_[i];
                        terrain->SetTerrainTextures(texture_id);

                        terrain_texture_requests_[button_number] = RequestTerrainTexture(button_number);

                        environment_module_->SendTextureDetailMessage(texture_id[button_number], button_number);
                    }
                }
            }
        }

        QString start_height("Texture_height_doubleSpinBox_" + QString("%1").arg(button_number + 1));
        QString height_range("Texture_height_range_doubleSpinBox_" + QString("%1").arg(button_number + 1));
        QDoubleSpinBox *start_height_spin = editor_widget_->findChild<QDoubleSpinBox*>(start_height);
        QDoubleSpinBox *height_range_spin = editor_widget_->findChild<QDoubleSpinBox*>(height_range);
        if(start_height_spin && height_range_spin)
            environment_module_->SendTextureHeightMessage(start_height_spin->value(), height_range_spin->value(), button_number);
    }

    void EnvironmentEditor::UpdateTerrain()
    {
        assert(environment_module_);
        if(!environment_module_)
        {
            EnvironmentModule::LogError("Can't update terrain because environment module is not intialized.");
            return;
        }

        TerrainPtr terrain = environment_module_->GetTerrainHandler();
        if(!terrain.get())
            return;

        QObject::connect(terrain.get(), SIGNAL(TerrainTextureChanged()), this, SLOT(UpdateTerrainTextures()));
        CreateHeightmapImage();
    }

    void EnvironmentEditor::HandleMouseEvent(QMouseEvent *ev)
    {
        /*assert(environment_module_);
        TerrainPtr terrain = environment_module_->GetTerrainHandler();
        if(!terrain.get())
            return;*/

        // Ugly this need to be removed when mouse move events are working corretly in Rex UICanvas.
        // Check if mouse has pressed.
        /*if(ev->type() == QEvent::MouseButtonPress)
        {
            QPoint position = ev->pos();
            Scene::EntityPtr entity = terrain->GetTerrainEntity().lock();
            EC_Terrain *terrain_component = entity->GetComponent<EC_Terrain>().get();
            start_height_ = terrain_component->GetPoint(position.x(), position.y());

            switch(ev->button())
            {
                case Qt::LeftButton:
                    mouse_press_flag_ |= left_button;
                    break;
                case Qt::RightButton:
                    mouse_press_flag_ |= right_button;
                    break;
                case Qt::MidButton:
                    mouse_press_flag_ |= middle_button;
                    break;
            }
        }
        else if(ev->type() == QEvent::MouseButtonRelease)
        {
            switch(ev->button())
            {
                case Qt::LeftButton:
                    mouse_press_flag_ -= (mouse_press_flag_ & left_button);
                    break;
                case Qt::RightButton:
                    mouse_press_flag_ -= right_button;
                    break;
                case Qt::MidButton:
                    mouse_press_flag_ -= middle_button;
                    break;
            }
        }

        if((mouse_press_flag_ & button_mask) > 0)
        {
            int time = terrain_paint_timer_.restart();
            if(time <= 0)
                terrain_paint_timer_.start();

            float time_in_ms = time / 1000.0f;
            // update mouse position.
            mouse_position_[0] = ev->pos().x();
            mouse_position_[1] = ev->pos().y();

            QLabel *label = editor_widget_->findChild<QLabel *>("map_label");
            const QPixmap *pixmap = label->pixmap();
            QImage image = pixmap->toImage();
            image.setPixel(ev->pos(), qRgb(255, 255, 255));
            label->setPixmap(QPixmap::fromImage(image));
            label->show();

            QPoint position = ev->pos();
            Scene::EntityPtr entity = terrain->GetTerrainEntity().lock();
            EC_Terrain *terrain_component = entity->GetComponent<EC_Terrain>().get();
            environment_module_->SendModifyLandMessage(position.x(), position.y(), brush_size_, action_, 0.3, terrain_component->GetPoint(position.x(), position.y())); 
        }*/

        if(ev->type() == QEvent::MouseButtonPress || ev->type() == QEvent::MouseMove)
        {
            mouse_position_[0] = ev->pos().x();
            mouse_position_[1] = ev->pos().y();
            if(ev->button() == Qt::LeftButton)
            {
                edit_terrain_ = true;
                terrain_paint_timer_.start(250);
            }
        }
        else if(ev->type() == QEvent::MouseButtonRelease)
        {
            if(ev->button() == Qt::LeftButton)
            {
                terrain_paint_timer_.stop();
                edit_terrain_ = false;
            }
        }
    }

    void EnvironmentEditor::TerrainEditTimerTick()
    {
        if(edit_terrain_)
        {
            assert(environment_module_);
            TerrainPtr terrain = environment_module_->GetTerrainHandler();
            if(!terrain.get())
                return;

            Scene::EntityPtr entity = terrain->GetTerrainEntity().lock();
            EC_Terrain *terrain_component = entity->GetComponent<EC_Terrain>().get();
            if(!terrain_component)
                return;

            environment_module_->SendModifyLandMessage(mouse_position_[0], mouse_position_[1], brush_size_, action_, 0.25, terrain_component->GetPoint(mouse_position_[0], mouse_position_[1])); 
        }
    }

    void EnvironmentEditor::BrushSizeChanged()
    {
        const QRadioButton *sender = qobject_cast<QRadioButton *>(QObject::sender());
        if(sender)
        {
            std::string object_name = sender->objectName().toStdString();
            if(object_name == "rad_button_small")
            {
                brush_size_ = Small;
            }
            else if(object_name == "rad_button_medium")
            {
                brush_size_ = Medium;
            }
            else if(object_name == "rad_button_large")
            {
                brush_size_ = Large;
            }
        }
    }

    void EnvironmentEditor::PaintActionChanged()
    {
        const QRadioButton *sender = qobject_cast<QRadioButton *>(QObject::sender());
        if(sender)
        {
            std::string object_name = sender->objectName().toStdString();
            if(object_name == "rad_button_flatten")
            {
                action_ = Flatten;
            }
            else if(object_name == "rad_button_raise")
            {
                action_ = Raise;
            }
            else if(object_name == "rad_button_lower")
            {
                action_ = Lower;
            }
            else if(object_name == "rad_button_smooth")
            {
                action_ = Smooth;
            }
            else if(object_name == "rad_button_roughen")
            {
                action_ = Roughen;
            }
            else if(object_name == "rad_button_revert")
            {
                action_ = Revert;
            }
        }
    }

    void EnvironmentEditor::TabWidgetChanged(int index)
    {
        const QTabWidget *sender = qobject_cast<QTabWidget *>(QObject::sender());
        if(!sender)
            return;

        QWidget *tab = sender->currentWidget();
        if(!tab)
            return;

        if(tab->objectName() == "edit_terrain") // Map tab
        {
            UpdateTerrain();
        }
        else if(tab->objectName() == "edit_terrain_texture") // Texture tab
        {
            assert(environment_module_);
            TerrainPtr terrain = environment_module_->GetTerrainHandler();
            if(!terrain.get())
                return;

            QLineEdit *line_edit = 0;
            for(uint i = 0; i < cNumberOfTerrainTextures; i++)
            {
                //Get terrain texture asset ids so that we can request those image resources.
                RexTypes::RexAssetID terrain_id = terrain->GetTerrainTextureID(i);

                UpdateTerrainTextureRanges();

                QString line_edit_name("texture_line_edit_" + QString("%1").arg(i + 1));

                // Check if terrain texture hasn't changed for last time, if not we dont need to request a new texture resource and we can continue on next texture.
                if(terrain_texture_id_list_[i] == terrain_id)
                    continue;

                terrain_texture_id_list_[i] = terrain_id;

                line_edit = editor_widget_->findChild<QLineEdit *>(line_edit_name);
                if(!line_edit)
                    continue;
                line_edit->setText(QString::fromStdString(terrain_id));

                terrain_texture_requests_[i] = RequestTerrainTexture(i);
            }
        }
    }

    void EnvironmentEditor::HandleResourceReady(Resource::Events::ResourceReady *res)
    {
        for(uint index = 0; index < terrain_texture_requests_.size(); index++)
        {
            if(terrain_texture_requests_[index] == res->tag_)
            {
                Foundation::TextureInterface *tex = dynamic_cast<Foundation::TextureInterface *>(res->resource_.get());
                if(!tex && tex->GetLevel() != 0)
                    return;

            
                //uint size = tex->GetWidth() * tex->GetHeight() * tex->GetComponents();
                //uint img_components = tex->GetComponents();
          
                //QImage img(tex->GetData(), tex->GetWidth(), tex->GetHeight(), QImage::Format_RGB888);

                //if ( img.isNull() )
                QImage img = ConvertToQImage(*tex);
                    
                QLabel *texture_label = editor_widget_->findChild<QLabel *>("terrain_texture_label_" + QString("%1").arg(index + 1));
                if(texture_label && !img.isNull())
                    texture_label->setPixmap(QPixmap::fromImage(img));
                //texture_label->show();
            }
        }
    }

    QImage EnvironmentEditor::ConvertToQImage(Foundation::TextureInterface &tex)
    {
        uint img_width        = tex.GetWidth(); 
        uint img_height       = tex.GetHeight(); 
        uint img_components   = tex.GetComponents();
        u8 *data              = tex.GetData();
        uint img_width_step   = img_width * img_components;
        QImage image;

        if(img_width > 0 && img_height > 0 && img_components > 0)
        {
            if(img_components == 3)// For RGB32
            {
                image = QImage(QSize(img_width, img_height), QImage::Format_RGB32);
                for(uint height = 0; height < img_height; height++)
                {
                    for(uint width = 0; width < img_width; width++)
                    {
                        u8 color[3];
                        for(uint comp = 0; comp < img_components; comp++)
                        {
                            uint index = (height % img_height) * (img_width_step) + ((width * img_components) % (img_width_step)) + comp;
                            color[comp] = data[index];
                        }
                        image.setPixel(width, height, qRgb(color[0], color[1], color[2]));
                    }
                }
            }
            else if(img_components == 4)// For ARGB32
            {
                image = QImage(QSize(img_width, img_height), QImage::Format_ARGB32);
                for(uint height = 0; height < img_height; height++)
                {
                    for(uint width = 0; width < img_width; width++)
                    {
                        u8 color[4];
                        for(uint comp = 0; comp < img_components; comp++)
                        {
                            uint index = (height % img_height) * (img_width_step) + ((width * img_components) % (img_width_step)) + comp;
                            color[comp] = data[index];
                        }
                        image.setPixel(width, height, qRgba(color[0], color[1], color[2], color[3]));
                    }
                }
            }
        }

        return image;
    }

    request_tag_t EnvironmentEditor::RequestTerrainTexture(uint index)
    {
        if(index > cNumberOfTerrainTextures) index = cNumberOfTerrainTextures;

        Foundation::ServiceManagerPtr service_manager = environment_module_->GetFramework()->GetServiceManager();
        if(service_manager)
        {
            if(service_manager->IsRegistered(Foundation::Service::ST_Texture))
            {
                boost::shared_ptr<Foundation::TextureServiceInterface> texture_service = 
                    service_manager->GetService<Foundation::TextureServiceInterface>(Foundation::Service::ST_Texture).lock();
                if(!texture_service)
                    return 0;

                // Request texture assets.
                return texture_service->RequestTexture(terrain_texture_id_list_[index]);
            }
        }
        return 0;
    }

    void EnvironmentEditor::UpdateGroundFog(float fogStart, float fogEnd, const QVector<float>& color)
    {
        // Adjust editor widget.

        // Ground fog
        QDoubleSpinBox* fog_ground_red = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_red_dSpinBox");
        QDoubleSpinBox* fog_ground_blue = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_blue_dSpinBox");
        QDoubleSpinBox* fog_ground_green = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_green_dSpinBox");

        QDoubleSpinBox* fog_ground_start_distance = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_start_distance_dSpinbox");
        QDoubleSpinBox* fog_ground_end_distance = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_end_distance_dSpinbox");

        if ( fog_ground_red != 0 
            && fog_ground_blue != 0
            && fog_ground_green != 0
            && fog_ground_start_distance != 0
            && fog_ground_end_distance != 0)
        {
            fog_ground_red->setValue(color[0]);   
            fog_ground_blue->setValue(color[1]);
            fog_ground_green->setValue(color[2]);

            fog_ground_start_distance->setValue(fogStart);
            fog_ground_end_distance->setValue(fogEnd);
        }


    }
    
    void EnvironmentEditor::UpdateWaterFog(float fogStart, float fogEnd, const QVector<float>& color)
    {
        // Water fog
        QDoubleSpinBox* fog_water_red = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_red_dSpinBox");
        QDoubleSpinBox* fog_water_blue = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_blue_dSpinBox");
        QDoubleSpinBox* fog_water_green = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_green_dSpinBox");

        QDoubleSpinBox* fog_water_start_distance = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_start_distance_dSpinBox");
        QDoubleSpinBox* fog_water_end_distance = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_end_distance_dSpinBox");

        if ( fog_water_red != 0 
            && fog_water_blue != 0
            && fog_water_green != 0
            && fog_water_start_distance != 0
            && fog_water_end_distance != 0)
        {
            fog_water_red->setValue(color[0]);   
            fog_water_blue->setValue(color[1]);
            fog_water_green->setValue(color[2]);

            fog_water_start_distance->setValue(fogStart);
            fog_water_end_distance->setValue(fogEnd);
        }

    }
    
    void EnvironmentEditor::SetGroundFog()
    {
        EnvironmentPtr environment = environment_module_->GetEnvironmentHandler();
        if(!environment.get())
            return;

        QDoubleSpinBox* fog_ground_red = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_red_dSpinBox");
        QDoubleSpinBox* fog_ground_blue = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_blue_dSpinBox");
        QDoubleSpinBox* fog_ground_green = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_green_dSpinBox");

        if ( fog_ground_red != 0
             && fog_ground_blue != 0
             && fog_ground_green != 0)
        {
            QVector<float> color;
            color << fog_ground_red->value();
            color << fog_ground_blue->value();
            color << fog_ground_green->value();
            environment->SetGroundFogColor(color);
        }

    }
    
    void EnvironmentEditor::SetWaterFog()
    {
        EnvironmentPtr environment = environment_module_->GetEnvironmentHandler();
        if(!environment.get())
            return;

        QDoubleSpinBox* fog_water_red = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_red_dSpinBox");
        QDoubleSpinBox* fog_water_blue = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_blue_dSpinBox");
        QDoubleSpinBox* fog_water_green = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_green_dSpinBox");

        if ( fog_water_red != 0
             && fog_water_blue != 0
             && fog_water_green != 0)
        {
            QVector<float> color;
            color << fog_water_red->value();
            color << fog_water_blue->value();
            color << fog_water_green->value();
            environment->SetGroundFogColor(color);
        }
    }

    void EnvironmentEditor::SetGroundFogDistance()
    {
        EnvironmentPtr environment = environment_module_->GetEnvironmentHandler();
        if(!environment.get())
            return;

        QDoubleSpinBox* fog_ground_start_distance = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_start_distance_dSpinBox");
        QDoubleSpinBox* fog_ground_end_distance = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_end_distance_dSpinBox");   

        if ( fog_ground_start_distance != 0 && fog_ground_end_distance != 0)
            environment->SetGroundFogDistance(fog_ground_start_distance->value(), fog_ground_end_distance->value());

    }

    void EnvironmentEditor::SetWaterFogDistance()
    {
        EnvironmentPtr environment = environment_module_->GetEnvironmentHandler();
        if(!environment.get())
            return;

        QDoubleSpinBox* fog_water_start_distance = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_start_distance_dSpinBox");
        QDoubleSpinBox* fog_water_end_distance = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_end_distance_dSpinBox");   

        if ( fog_water_start_distance != 0 && fog_water_end_distance != 0)
            environment->SetWaterFogDistance(fog_water_start_distance->value(), fog_water_end_distance->value());
    }

    void EnvironmentEditor::ToggleFogOverride()
    {
        EnvironmentPtr environment = environment_module_->GetEnvironmentHandler();
        if(!environment.get())
            return;

        if ( environment->GetFogColorOverride() )
            environment->SetFogColorOverride(false);
        else 
            environment->SetFogColorOverride(true);
    }

    void EnvironmentEditor::SkyTypeChanged(int index)
    {
        assert(environment_module_);
        SkyPtr sky = environment_module_->GetSkyHandler();
        if(!sky.get())
            return;

        const QComboBox *sender = qobject_cast<QComboBox *>(QObject::sender());
        if(sender)
        {
            QString text = sender->itemText(index);
            if(text == "Sky box")
            {
                sky->ChangeSkyType(OgreRenderer::SKYTYPE_BOX, sky->IsSkyEnabled());
                CreateSkyProperties(OgreRenderer::SKYTYPE_BOX);
                UpdateSkyTextureNames();
            }
            else if(text == "Sky dome")
            {
                sky->ChangeSkyType(OgreRenderer::SKYTYPE_DOME, sky->IsSkyEnabled());
                CreateSkyProperties(OgreRenderer::SKYTYPE_DOME);
                UpdateSkyTextureNames();
            }
            else if(text == "Sky plane")
            {
                sky->ChangeSkyType(OgreRenderer::SKYTYPE_PLANE, sky->IsSkyEnabled());
                CreateSkyProperties(OgreRenderer::SKYTYPE_PLANE);
                UpdateSkyTextureNames();
            }
        }
    }

    
}
