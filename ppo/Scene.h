#pragma once

#include "GameObject.h"
#include "GameTimer.h"
#include "Camera.h"
//#include "DummyApp.h"

class DummyApp;

class Scene
{
public:
    Scene(DummyApp* gametool);
    ~Scene();

    void Update();
    void KeyInput();

    void InitScene();
    void LoadScene();
    void BuildScene();

    void AddGameObject(GameObject* gameObject);
    void AddGameObjects(GameObject** gameObjects, int numGameObjects);

    void DeleteGameObject(GameObject* gameObject);
    void DeleteGameObjects(GameObject** gameObject, int numGameObject);
    void DeleteAllGameObjects();

private:

    GameObject* mAllGameObjects;
    DummyApp* mGameTool;

};

