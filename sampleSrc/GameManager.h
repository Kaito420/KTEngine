#pragma once

#include <memory>
#include <list>
#include "RenderManager.h"
#include "Object.h"
#include "Polygon2D.h"
#include "Camera.h"
#include "Light.h"
#include "Field.h"
#include "Torus.h"

class GameManager
{
private:

	static GameManager* m_Instance;

	RenderManager	m_RenderManger;

	std::list<Object*> m_Objects;
	Polygon2D		m_Polygon2D;
	Camera			m_Camera;
	Light			m_Light;
	Field			m_Field;
	Torus			m_Torus;

public:
	static GameManager* GetInstance() { return m_Instance; }

	GameManager();
	~GameManager();



	void Update();
	void Draw();


};

