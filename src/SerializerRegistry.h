//=====================================================================================
// SerializerRegistry.h
// Author:Kaito Aoki
// Date:2026/02/12
//=====================================================================================

#ifndef _SERIALIZERREGISTRY_H
#define _SERIALIZERREGISTRY_H

void RegisterAllComponents();

//Cerealの型登録
#include <cereal/types/polymorphic.hpp>

//登録したいシーン
#include "Scene.h"
#include "SceneTitle.h"
#include "SceneGame.h"
#include "SceneResult.h"
#include "ScenePhysicsTest.h"

//マクロ定義によるシーンの登録
CEREAL_REGISTER_TYPE(Scene);
CEREAL_REGISTER_TYPE(SceneTitle);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Scene, SceneTitle);
CEREAL_REGISTER_TYPE(SceneGame);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Scene, SceneGame);
CEREAL_REGISTER_TYPE(SceneResult);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Scene, SceneResult);
CEREAL_REGISTER_TYPE(ScenePhysicsTest);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Scene, ScenePhysicsTest);


//登録したいオブジェクト
//=====エンジンに必須のオブジェクト=====
#include "GameObject.h"
#include "Camera.h"
#include "Sky.h"
#include "Light.h"
#include "CubeObject.h"
#include "SphereObject.h"

//=====ゲーム用に作成したオブジェクト=====
#include "Block.h"
#include "Explosion.h"
#include "FadeObject.h"

//マクロ定義によるオブジェクトの登録
//=====エンジンに必須のオブジェクト=====
CEREAL_REGISTER_TYPE(GameObject);

CEREAL_REGISTER_TYPE(Camera);
CEREAL_REGISTER_POLYMORPHIC_RELATION(GameObject, Camera);

CEREAL_REGISTER_TYPE(Sky);
CEREAL_REGISTER_POLYMORPHIC_RELATION(GameObject, Sky);

CEREAL_REGISTER_TYPE(Light);
CEREAL_REGISTER_POLYMORPHIC_RELATION(GameObject, Light);

CEREAL_REGISTER_TYPE(CubeObject);
CEREAL_REGISTER_POLYMORPHIC_RELATION(GameObject, CubeObject);

CEREAL_REGISTER_TYPE(SphereObject);
CEREAL_REGISTER_POLYMORPHIC_RELATION(GameObject, SphereObject);

//=====ゲーム用に作成したオブジェクト=====
CEREAL_REGISTER_TYPE(Block);
CEREAL_REGISTER_POLYMORPHIC_RELATION(GameObject, Block);

CEREAL_REGISTER_TYPE(Explosion);
CEREAL_REGISTER_POLYMORPHIC_RELATION(GameObject, Explosion);

CEREAL_REGISTER_TYPE(FadeObject);
CEREAL_REGISTER_POLYMORPHIC_RELATION(GameObject, FadeObject);


//登録したいコンポーネント
//=====エンジンに必須のコンポーネント=====
#include "Component.h"
#include "RigidBody.h"
#include "Collider.h"
#include "Cube.h"
#include "Sphere.h"
#include "Shader.h"
#include "Square.h"
#include "SkyDome.h"
#include "ModelRenderer.h"
#include "Particle.h"
#include "Billboard.h"
#include "BillboardEffect.h"
#include "Fade.h"
#include "Wave.h"

//=====ゲーム用に作成したコンポーネント=====
#include "BlockSpawner.h"
#include "CubeKiller.h"
#include "KillY.h"
#include "Piece.h"
#include "ResultManager.h"
#include "TitleManager.h"

//マクロ定義によるコンポーネントの登録

//=====エンジンに必須のコンポーネント=====
CEREAL_REGISTER_TYPE(Component);

CEREAL_REGISTER_TYPE(RigidBody);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Component, RigidBody);

CEREAL_REGISTER_TYPE(Collider);
CEREAL_REGISTER_TYPE(ColliderBox);
CEREAL_REGISTER_TYPE(ColliderSphere);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Component, Collider);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Collider, ColliderBox);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Collider, ColliderSphere);

CEREAL_REGISTER_TYPE(Cube);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Component, Cube);

CEREAL_REGISTER_TYPE(Sphere);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Component, Sphere);

CEREAL_REGISTER_TYPE(Shader);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Component, Shader);

CEREAL_REGISTER_TYPE(Square);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Component, Square);

CEREAL_REGISTER_TYPE(SkyDome);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Component, SkyDome);

CEREAL_REGISTER_TYPE(ModelRenderer);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Component, ModelRenderer);

CEREAL_REGISTER_TYPE(Particle);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Component, Particle);

CEREAL_REGISTER_TYPE(Billboard);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Square, Billboard);

CEREAL_REGISTER_TYPE(BillboardEffect);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Component, BillboardEffect);

CEREAL_REGISTER_TYPE(Fade);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Square, Fade);

CEREAL_REGISTER_TYPE(Wave);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Component, Wave);

//=====ゲーム用に作成したコンポーネント=====
CEREAL_REGISTER_TYPE(BlockSpawner);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Component, BlockSpawner);

CEREAL_REGISTER_TYPE(CubeKiller);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Component, CubeKiller);

CEREAL_REGISTER_TYPE(KillY);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Component, KillY);

CEREAL_REGISTER_TYPE(Piece);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Component, Piece);

CEREAL_REGISTER_TYPE(ResultManager);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Component, ResultManager);

CEREAL_REGISTER_TYPE(TitleManager);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Component, TitleManager);

#endif // !_SERIALIZERREGISTRY_H
