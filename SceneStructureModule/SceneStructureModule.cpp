/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 *
 *  @file   SceneStructureModule.cpp
 *  @brief  Provides Scene Structure window and raycast drag-and-drop import of
 *          mesh, .scene, .xml and .nbf files to the main window.
 */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "SceneStructureModule.h"
#include "SceneStructureWindow.h"

#include "Console.h"
#include "UiServiceInterface.h"
#include "Input.h"
#include "RenderServiceInterface.h"
#include "SceneImporter.h"
#include "EC_OgreCamera.h"
#include "EC_Placeable.h"
#include "NaaliUi.h"
#include "NaaliGraphicsView.h"
#include "LoggingFunctions.h"

DEFINE_POCO_LOGGING_FUNCTIONS("SceneStructure");

#ifdef ASSIMP_ENABLED
#include <OpenAssetImport.h>
#endif

//#include <OgreCamera.h>

#include "MemoryLeakCheck.h"

SceneStructureModule::SceneStructureModule() :
    IModule("SceneStructure"),
    sceneWindow(0)
{
}

SceneStructureModule::~SceneStructureModule()
{
    SAFE_DELETE(sceneWindow);
}

void SceneStructureModule::PostInitialize()
{
    framework_->Console()->RegisterCommand("scenestruct", "Shows the Scene Structure window.", this, SLOT(ShowSceneStructureWindow()));
    inputContext = framework_->GetInput()->RegisterInputContext("SceneStructureInput", 90);
    //input->SetTakeKeyboardEventsOverQt(true);
    connect(inputContext.get(), SIGNAL(KeyPressed(KeyEvent *)), this, SLOT(HandleKeyPressed(KeyEvent *)));

    connect(framework_->Ui()->GraphicsView(), SIGNAL(DragEnterEvent(QDragEnterEvent *)), SLOT(HandleDragEnterEvent(QDragEnterEvent *)));
    connect(framework_->Ui()->GraphicsView(), SIGNAL(DragMoveEvent(QDragMoveEvent *)), SLOT(HandleDragMoveEvent(QDragMoveEvent *)));
    connect(framework_->Ui()->GraphicsView(), SIGNAL(DropEvent(QDropEvent *)), SLOT(HandleDropEvent(QDropEvent *)));
}

QList<Scene::Entity *> SceneStructureModule::InstantiateContent(const QString &filename, Vector3df worldPos, bool clearScene, bool queryPosition)
{
    QList<Scene::Entity *> ret;
    const Scene::ScenePtr &scene = framework_->GetDefaultWorldScene();
    if (!scene)
        return ret;

    if (queryPosition)
    {
        bool ok;
        QString posString = QInputDialog::getText(0, tr("Position"), tr("position (x;y;z):"), QLineEdit::Normal, "0.00;0.00;0.00", &ok);
        posString = posString.trimmed();
        if (!ok)
            return ret;

        if (posString.isEmpty())
            posString = "0;0;0";
        posString.replace(',', '.');
        QStringList pos = posString.split(';');
        if (pos.size() > 0)
            worldPos.x = pos[0].toFloat();
        if (pos.size() > 1)
            worldPos.y = pos[1].toFloat();
        if (pos.size() > 2)
            worldPos.z = pos[2].toFloat();
    }

    if (filename.toLower().indexOf(".scene") != -1)
    {
        boost::filesystem::path path(filename.toStdString());
        std::string dirname = path.branch_path().string();

        TundraLogic::SceneImporter importer(framework_);
        ///\todo Take into account asset sources.
        ret = importer.Import(scene, filename.toStdString(), dirname, "./data/assets",
            Transform(worldPos, Vector3df(), Vector3df(1,1,1)), AttributeChange::Default, clearScene, true, false);

        if (ret.empty())
            LogError("Import failed");
        else
            LogInfo("Import succesful. " + ToString(ret.size()) + " entities created.");
    }
    else if (filename.endsWith(".mesh", Qt::CaseInsensitive))
    {
        boost::filesystem::path path(filename.toStdString());
        std::string dirname = path.branch_path().string();

        TundraLogic::SceneImporter importer(framework_);
        Scene::EntityPtr entity = importer.ImportMesh(scene, filename.toStdString(), dirname, "./data/assets",
            Transform(worldPos, Vector3df(), Vector3df(1,1,1)), std::string(), AttributeChange::Default, true);
        if (entity)
        {
            scene->EmitEntityCreated(entity, AttributeChange::Default);
            ret << entity.get();
        }
    }
    else if (filename.toLower().indexOf(".xml") != -1 && filename.toLower().indexOf(".mesh") == -1)
    {
        ret = scene->LoadSceneXML(filename.toStdString(), clearScene, false, AttributeChange::Replicate);
    }
    else if (filename.toLower().indexOf(".nbf") != -1)
    {
        ret = scene->CreateContentFromBinary(filename, true, AttributeChange::Replicate);
    }
    else
    {
#ifdef ASSIMP_ENABLED
        boost::filesystem::path path(filename.toStdString());
        AssImp::OpenAssetImport assimporter;
        QString extension = QString(path.extension().c_str()).toLower();
        if (assimporter.IsSupportedExtension(extension))
        {
            std::string dirname = path.branch_path().string();
            //std::vector<std::string> importedMeshNames;
            //assimporter.Import(filename, importedMeshNames);

            TundraLogic::SceneImporter sceneimporter(framework_);
            //for (size_t i=0 ; i<importedMeshNames.size() ; ++i)
            {
                Scene::EntityPtr entity = sceneimporter.ImportMesh(scene, filename.toStdString()/*importedMeshNames[i]*/, dirname, "./data/assets",
                    Transform(worldPos, Vector3df(0,0,0), Vector3df(1,1,1)), std::string(), AttributeChange::Default, true, true);
                if (entity)
                    scene->EmitEntityCreated(entity, AttributeChange::Default);
            }
        } else
#endif
        {
            LogError("Unsupported file extension: " + filename.toStdString());
        }
    }

    return ret;
}

void SceneStructureModule::CentralizeEntitiesTo(const Vector3df &pos, const QList<Scene::Entity *> &entities)
{
    Vector3df minPos(1e9f, 1e9f, 1e9f);
    Vector3df maxPos(-1e9f, -1e9f, -1e9f);

    foreach(Scene::Entity *e, entities)
    {
        EC_Placeable *p = e->GetComponent<EC_Placeable>().get();
        if (p)
        {
            Vector3df pos = p->transform.Get().position;
            minPos.x = std::min(minPos.x, pos.x);
            minPos.y = std::min(minPos.y, pos.y);
            minPos.z = std::min(minPos.z, pos.z);
            maxPos.x = std::max(maxPos.x, pos.x);
            maxPos.y = std::max(maxPos.y, pos.y);
            maxPos.z = std::max(maxPos.z, pos.z);
        }
    }

    // We assume that world's up axis is Z-coordinate axis.
    Vector3df importPivotPos = Vector3df((minPos.x + maxPos.x) / 2, (minPos.y + maxPos.y) / 2, minPos.z);
    Vector3df offset = pos - importPivotPos;

    foreach(Scene::Entity *e, entities)
    {
        EC_Placeable *p = e->GetComponent<EC_Placeable>().get();
        if (p)
        {
            Transform t = p->transform.Get();
            t.position += offset;
            p->transform.Set(t, AttributeChange::Default);
        }
    }

}

void SceneStructureModule::ShowSceneStructureWindow()
{
    UiServiceInterface *ui = framework_->GetService<UiServiceInterface>();
    if (!ui)
        return;

    if (sceneWindow)
    {
        ui->ShowWidget(sceneWindow);
        return;
    }

    sceneWindow = new SceneStructureWindow(framework_);
    sceneWindow->move(200,200);
    sceneWindow->SetScene(framework_->GetDefaultWorldScene());

    ui->AddWidgetToScene(sceneWindow);
    ui->ShowWidget(sceneWindow);
}

void SceneStructureModule::HandleKeyPressed(KeyEvent *e)
{
    if (e->eventType != KeyEvent::KeyPressed || e->keyPressCount > 1)
        return;

    Input &input = *framework_->GetInput();

    const QKeySequence showSceneStruct = input.KeyBinding("ShowSceneStructureWindow", QKeySequence(Qt::ShiftModifier + Qt::Key_S));
    if (QKeySequence(e->keyCode | e->modifiers) == showSceneStruct)
        ShowSceneStructureWindow();
}

void SceneStructureModule::HandleDragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasUrls())
        e->accept();
}

void SceneStructureModule::HandleDragMoveEvent(QDragMoveEvent *e)
{
    if (e->mimeData()->hasUrls())
        e->accept();
}

void SceneStructureModule::HandleDropEvent(QDropEvent *e)
{
    if (e->mimeData()->hasUrls())
    {
        QList<Scene::Entity *> importedEntities;

        Foundation::RenderServiceInterface *renderer = framework_->GetService<Foundation::RenderServiceInterface>();
        if (!renderer)
            return;

        Vector3df worldPos;
        Foundation::RaycastResult res = renderer->Raycast(e->pos().x(), e->pos().y());
        if (!res.entity_)
        {
            // No entity hit, use camera's position with hard-coded offset.
            const Scene::ScenePtr &scene = framework_->GetDefaultWorldScene();
            if (!scene)
                return;

            Scene::EntityList cameras = scene->GetEntitiesWithComponent(EC_OgreCamera::TypeNameStatic());
            if (cameras.empty())
                return;

            foreach(Scene::EntityPtr cam, cameras)
                if (cam->GetComponent<EC_OgreCamera>()->IsActive())
                {
                    EC_Placeable *placeable = cam->GetComponent<EC_Placeable>().get();
                    if (placeable)
                    {
                        //Ogre::Ray ray = cam->GetComponent<EC_OgreCamera>()->GetCamera()->getCameraToViewportRay(e->pos().x(), e->pos().y());
                        Quaternion q = placeable->GetOrientation();
                        Vector3df v = q * -Vector3df::UNIT_Z;
                        //Ogre::Vector3 oV = ray.getPoint(20);
                        worldPos = /*Vector3df(oV.x, oV.y, oV.z);*/ placeable->GetPosition() + v * 20;
                        break;
                    }
                }
        }
        else
            worldPos = res.pos_;

        foreach (QUrl url, e->mimeData()->urls())
        {
            QString filename = url.path();
#ifdef _WINDOWS
            // We have '/' as the first char on windows and the filename
            // is not identified as a file properly. But on other platforms the '/' is valid/required.
            filename = filename.mid(1);
#endif
            importedEntities.append(InstantiateContent(filename, Vector3df(), false));
        }

        // Calculate import pivot and offset for new content
        if (importedEntities.size())
            CentralizeEntitiesTo(worldPos, importedEntities);

        e->acceptProposedAction();
    }
}

extern "C" void POCO_LIBRARY_API SetProfiler(Foundation::Profiler *profiler);
void SetProfiler(Foundation::Profiler *profiler)
{
    Foundation::ProfilerSection::SetProfiler(profiler);
}

POCO_BEGIN_MANIFEST(IModule)
   POCO_EXPORT_CLASS(SceneStructureModule)
POCO_END_MANIFEST
